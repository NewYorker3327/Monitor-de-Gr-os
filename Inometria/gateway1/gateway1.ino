/*
   PLACA: GATEWAY (Repetidor)
   FREQUENCIA: 433 MHz
   ROTA: Nodo (ESP-NOW) -> Gateway (LoRa) -> Access Point
*/

#include <SPI.h>
#include <LoRa.h>
#include <esp_now.h>
#include <WiFi.h>

// ================= PINAGEM LORA =================
#define SCK     18
#define MISO    19
#define MOSI    23
#define SS      5
#define RST     14
#define DIO0    2
#define LED_PIN 2
#define GATEWAY_ID 0 // ID 0 para o próprio Gateway
#define GATEWAY_BEACON_TYPE 14 // Tipo de pacote para o beacon do gateway
#define GATEWAY_STATUS_REQUEST 15
#define BROADCAST_DATA_REQUEST 16 // Novo: Solicita dados de todos os nodos
#define TOGGLE_GATEWAY_BEACON 17  // Novo: Controla o beacon do Gateway

struct __attribute__((packed)) DataPacket {
    int node_id;
    int msg_type;
    float temps[13]; // <-- AUMENTADO PARA 13
    float hum;
    bool motor_state;
    float threshold;
};

DataPacket buffer;         
DataPacket commandPacket;  

uint8_t broadcastAddress[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
esp_now_peer_info_t peerInfo;
bool hasNewDataFromNode = false;
unsigned long lastBeaconTime = 0;
unsigned long beaconInterval = 5000; // Envia beacon a cada 5 segundos
bool gatewayBeaconEnabled = true;    // Novo: Controla o beacon do Gateway

// CALLBACK ESP-NOW (compatível com ESP32 Core v2 e v3)
#if defined(ESP_ARDUINO_VERSION) && ESP_ARDUINO_VERSION >= ESP_ARDUINO_VERSION_VAL(3, 0, 0)
void OnDataRecv(const esp_now_recv_info_t *info, const uint8_t *incomingData, int len) {
#else
void OnDataRecv(const uint8_t *mac, const uint8_t *incomingData, int len) {
#endif
  memcpy(&buffer, incomingData, sizeof(buffer));
  hasNewDataFromNode = true;
}

// Novo: Função para enviar o beacon do gateway
void sendGatewayBeacon() {
  DataPacket beaconPacket = {0};
  beaconPacket.node_id = GATEWAY_ID;
  beaconPacket.msg_type = GATEWAY_BEACON_TYPE;

  Serial.println("[BEACON] Enviando beacon de saude do Gateway via LoRa...");
  LoRa.beginPacket();
  LoRa.write((uint8_t*)&beaconPacket, sizeof(beaconPacket));
  LoRa.endPacket();
}

void setup() {
  Serial.begin(9600);
  pinMode(LED_PIN, OUTPUT);
  
  // 1. ESP-NOW
  WiFi.mode(WIFI_STA);
  if (esp_now_init() != ESP_OK) ESP.restart();
  esp_now_register_recv_cb(OnDataRecv);

  memcpy(peerInfo.peer_addr, broadcastAddress, 6);
  peerInfo.channel = 0;  
  peerInfo.encrypt = false;
  esp_now_add_peer(&peerInfo);

  // 2. LORA (433MHz)
  Serial.println("[BOOT] LoRa Gateway (433MHz)...");
  SPI.begin(SCK, MISO, MOSI, SS);
  LoRa.setPins(SS, RST, DIO0);
  
  if (!LoRa.begin(433E6)) { // <--- CORREÇÃO AQUI
    Serial.println("[ERRO] LoRa falhou!");
    while (1) { digitalWrite(LED_PIN, !digitalRead(LED_PIN)); delay(100); }
  }
  
  LoRa.setTxPower(20);            // 20dBm = Potência Máxima
  LoRa.setGain(0);                // 0 = AGC (Melhor sensibilidade automática)

  LoRa.setSpreadingFactor(9);     
  LoRa.setSignalBandwidth(125E3);
  LoRa.setCodingRate4(5);         
  LoRa.setSyncWord(0xF3);        
  
  Serial.println("[OK] Gateway Pronto.");
}

void loop() {
  unsigned long currentTime = millis();

  // Tarefa 1: Enviar beacon periódico do Gateway
  if (gatewayBeaconEnabled && (currentTime - lastBeaconTime > beaconInterval)) {
    sendGatewayBeacon();
    lastBeaconTime = currentTime;
  }

  // Tarefa 2: Repasse Nodo -> LoRa
  if (hasNewDataFromNode) {
    Serial.print("[REPASSE] Nodo -> LoRa | ID: ");
    Serial.println(buffer.node_id);
    LoRa.beginPacket();
    LoRa.write((uint8_t*)&buffer, sizeof(buffer));
    LoRa.endPacket();
    digitalWrite(LED_PIN, HIGH); delay(50); digitalWrite(LED_PIN, LOW);
    hasNewDataFromNode = false;
  }

  // Tarefa 3: Repasse LoRa -> Nodo (Comando) ou Responder a Pedido de Status
  int packetSize = LoRa.parsePacket();
  if (packetSize == sizeof(DataPacket)) {
      LoRa.readBytes((uint8_t*)&commandPacket, sizeof(commandPacket));
      
      // Verifica o tipo de pacote recebido
      if (commandPacket.msg_type == GATEWAY_STATUS_REQUEST) {
        Serial.println("[COMANDO] Solicitacao de status recebida do AP. Respondendo...");
        sendGatewayBeacon(); // Responde imediatamente com um beacon
      } else if (commandPacket.msg_type == TOGGLE_GATEWAY_BEACON) {
        gatewayBeaconEnabled = commandPacket.motor_state; // absolute state!
        Serial.print("[COMANDO] Beacon do Gateway ");
        Serial.println(gatewayBeaconEnabled ? "ATIVADO" : "DESATIVADO");
      } else if (commandPacket.msg_type == BROADCAST_DATA_REQUEST) {
        Serial.println("[COMANDO] Repassando requisicao de dados para todos os nodos via ESP-NOW...");
        // Simplesmente repassa o pacote de requisição para todos os nodos escutando
        esp_now_send(broadcastAddress, (uint8_t *) &commandPacket, sizeof(commandPacket));
      } else {
        // Se não for um pedido de status, é um comando para um nodo
        Serial.println("[COMANDO] LoRa -> Nodo");
        esp_now_send(broadcastAddress, (uint8_t *) &commandPacket, sizeof(commandPacket));
      }
      digitalWrite(LED_PIN, HIGH); delay(50); digitalWrite(LED_PIN, LOW); // Pisca para qualquer comando LoRa recebido
  } else if (packetSize > 0) {
      while (LoRa.available()) LoRa.read();
  }
}