/*
   PLACA: ACCESS POINT (Ponte LoRa <-> WiFi UDP)
   FUNÇÃO: Recebe dados do Gateway via LoRa e entrega ao PC via WiFi.
           Recebe comandos do PC via WiFi e entrega ao Gateway via LoRa.
*/

#include <SPI.h>
#include <LoRa.h>
#include <WiFi.h>
#include <WiFiUdp.h>

// ================= CONFIGURAÇÕES DE USUÁRIO =================
const char* ssid     = "Vilarino_Engenharia"; // <--- NOME DA SUA NOVA REDE WIFI
const char* password = "22161497";      // <--- SENHA DA SUA NOVA REDE WIFI
const int   udpPort  = 4210;                // Porta UDP padrão do sistema

// ================= PINAGEM LORA (ESP32 Padrão) =================
// Verifique se sua placa usa esses pinos. Se for a Heltec ou TTGO, muda.
#define SCK     18
#define MISO    19
#define MOSI    23
#define SS      5
#define RST     14
#define DIO0    2 

#define LED_PIN 2 // Led interno para feedback visual

// ================= TIPOS DE PACOTE =================
enum PacketType {
    SENSOR_DATA = 0,
    MOTOR_COMMAND = 1,
    PC_PING = 10,
    AP_BEACON = 12,
    TOGGLE_BEACON = 13,
    GATEWAY_BEACON = 14,
    GATEWAY_STATUS_REQUEST = 15,
    BROADCAST_DATA_REQUEST = 16, // Novo: Solicita dados de todos os nodos
    TOGGLE_GATEWAY_BEACON = 17   // Novo: Controla o beacon do Gateway
};

// ================= ESTRUTURA DE DADOS =================
// Deve ser idêntica ao PC e aos outros Nodos para não "quebrar" os dados
struct __attribute__((packed)) DataPacket {
    int node_id;
    int msg_type;
    float temps[13]; // <-- AUMENTADO PARA 13
    float hum;
    bool motor_state;
    float threshold;
};

DataPacket buffer;   // Buffer para armazenar dados temporariamente
WiFiUDP udp;         // Objeto UDP

// Variáveis de estado e controle de tempo (Multitasking)
unsigned long lastWifiCheck = 0;
unsigned long lastBeaconSend = 0;
bool wifiConnected = false;
IPAddress remotePcIP;
bool pcIpKnown = false;
bool beaconEnabled = true;

void setup() {
  Serial.begin(9600);
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  // 1. INICIALIZAÇÃO LORA (Crítico)
  Serial.println("\n[BOOT] Iniciando Modulo LoRa...");
  SPI.begin(SCK, MISO, MOSI, SS);
  LoRa.setPins(SS, RST, DIO0);
  
  // CORREÇÃO CRÍTICA: A frequência deve ser 433MHz para ser compatível com o Gateway.
  if (!LoRa.begin(433E6)) { 
    Serial.println("[ERRO] Falha ao iniciar LoRa! Verifique as conexoes.");
    while (1) { 
      // Pisca rápido para indicar erro de hardware
      digitalWrite(LED_PIN, !digitalRead(LED_PIN)); 
      delay(100); 
    }
  }

  // CORREÇÃO CRÍTICA: Os parâmetros de rádio devem ser idênticos aos do Gateway.
  LoRa.setSpreadingFactor(9);     
  LoRa.setSignalBandwidth(500E3);
  LoRa.setCodingRate4(5);         
  LoRa.setSyncWord(0xF3);
  LoRa.setTxPower(20); // Define a potência máxima de transmissão para comandos alcançarem o Gateway

  Serial.println("[OK] LoRa Iniciado e Escutando.");

  // 2. INICIALIZAÇÃO WIFI
  // Não usamos while(status != connected) aqui para não travar se o roteador estiver desligado
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  
  // 3. INICIALIZAÇÃO UDP
  udp.begin(udpPort);
}

void loop() {
  // ================= TAREFA 1: GERENCIAR CONEXÃO WIFI (ROBUSTEZ) =================
  unsigned long currentMillis = millis();
  
  // Verifica conexão a cada 5 segundos
  if ((WiFi.status() != WL_CONNECTED) && (currentMillis - lastWifiCheck >= 5000)) {
    Serial.print("[WIFI] Tentando reconectar a ");
    Serial.println(ssid);
    WiFi.disconnect();
    WiFi.reconnect();
    lastWifiCheck = currentMillis;
    wifiConnected = false;
  } else if (WiFi.status() == WL_CONNECTED && !wifiConnected) {
    Serial.println("[WIFI] Conectado! IP: " + WiFi.localIP().toString());
    wifiConnected = true;
    lastWifiCheck = currentMillis;
  }

  // Se não tem WiFi, continuamos rodando o loop para não travar o processador,
  // mas não podemos enviar UDP. O LoRa continua sendo escutado.

  // ================= TAREFA 2: RECEBER DO LORA -> ENVIAR PRO PC =================
  int packetSize = LoRa.parsePacket();
  if (packetSize) {
    // Verifica se o tamanho do pacote bate com a estrutura (Filtro de Ruído Básico)
    if (packetSize == sizeof(DataPacket)) {
      // Lê o RSSI ANTES de ler o pacote, pois a leitura pode afetar o valor.
      int rssi = LoRa.packetRssi();
      LoRa.readBytes((uint8_t*)&buffer, sizeof(buffer));
      
      // INJEÇÃO DO RSSI: Coloca o valor do RSSI no campo 'threshold' do pacote.
      // Isso permite que o PC receba a qualidade do sinal sem alterar a estrutura do pacote.
      buffer.threshold = rssi;
      
      // Feedback Serial
      Serial.print("[LORA->PC] Recebido ID: ");
      Serial.print(buffer.node_id);
      Serial.print(" | RSSI: ");
      Serial.println(rssi);

      // Se WiFi está OK, repassa para o PC
      if (wifiConnected && pcIpKnown) {
        udp.beginPacket(remotePcIP, udpPort); // Envia para o IP do PC que já conhecemos
        udp.write((const uint8_t*)&buffer, sizeof(buffer));
        udp.endPacket();
        blinkLed(); // Pisca para indicar sucesso
      } else if (!wifiConnected) {
        Serial.println("[ERRO] Pacote LoRa perdido: WiFi desconectado.");
      } else {
        Serial.println("[ALERTA] Pacote LoRa recebido, mas IP do PC desconhecido. Pacote descartado.");
      }
    } else {
      // Limpa o buffer de pacotes corrompidos ou de tamanho errado
      while (LoRa.available()) LoRa.read();
      Serial.print("[ALERTA] Pacote LoRa com tamanho incorreto ignorado. Recebido: ");
      Serial.print(packetSize);
      Serial.print(" bytes, Esperado: ");
      Serial.print(sizeof(DataPacket));
      Serial.println(" bytes.");
    }
  }

  // ================= TAREFA 3: RECEBER DO PC -> ENVIAR PRO GATEWAY =================
  if (wifiConnected) {
    int udpSize = udp.parsePacket();
    if (udpSize) {
      // Aprende ou atualiza o IP do PC que enviou o pacote
      if (!pcIpKnown || udp.remoteIP() != remotePcIP) {
          remotePcIP = udp.remoteIP();
          pcIpKnown = true;
          Serial.print("[WIFI] IP do PC registrado: ");
          Serial.println(remotePcIP);
      }

      if (udpSize == sizeof(DataPacket)) {
        udp.read((char*)&buffer, sizeof(buffer)); // Lê do UDP
        
        if (buffer.msg_type == TOGGLE_BEACON) {
          beaconEnabled = !beaconEnabled;
          Serial.print("[WIFI] Comando para Beacon recebido. Novo estado: ");
          Serial.println(beaconEnabled ? "LIGADO" : "DESLIGADO");
          blinkLed();
        } else if (buffer.msg_type == MOTOR_COMMAND || 
                   buffer.msg_type == GATEWAY_STATUS_REQUEST || 
                   buffer.msg_type == BROADCAST_DATA_REQUEST ||
                   buffer.msg_type == TOGGLE_GATEWAY_BEACON) {
          Serial.print("[PC->LORA] Comando/Requisicao recebida. Tipo: ");
          Serial.print(buffer.msg_type);
          Serial.println(". Repassando para o Gateway...");
          // Envia via LoRa para o Gateway (Broadcast)
          LoRa.beginPacket();
          LoRa.write((uint8_t*)&buffer, sizeof(buffer));
          LoRa.endPacket();
          blinkLed();
        } else if (buffer.msg_type == PC_PING) {
          Serial.println("[WIFI] Ping do PC recebido.");
          // Um ping do PC indica uma nova tentativa de conexão.
          // Devemos reativar o beacon para garantir que o PC nos veja.
          if (!beaconEnabled) {
            beaconEnabled = true;
            Serial.println("       -> Beacon foi reativado.");
          }
        } else {
          Serial.print("[PC->WIFI] Pacote com tipo desconhecido recebido: ");
          Serial.println(buffer.msg_type);
        }
      } else {
         // Limpa pacote UDP inválido
         udp.flush();
      }
    }

    // Envia um beacon periódico para o PC, se o IP dele for conhecido
    if (pcIpKnown && beaconEnabled && (millis() - lastBeaconSend >= 5000)) {
      DataPacket beaconPacket;
      beaconPacket.node_id = 0; // ID 0 para o próprio Access Point
      beaconPacket.msg_type = AP_BEACON;

      udp.beginPacket(remotePcIP, udpPort);
      udp.write((const uint8_t*)&beaconPacket, sizeof(beaconPacket));
      udp.endPacket();

      lastBeaconSend = millis();
      Serial.println("[BEACON] Enviando beacon para o PC.");
      blinkLed();
    }
  }
}

// Função auxiliar para piscar o LED brevemente
void blinkLed() {
  digitalWrite(LED_PIN, HIGH);
  delay(50); // Delay curto imperceptível para a lógica geral
  digitalWrite(LED_PIN, LOW);
}