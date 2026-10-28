/*
   PLACA: ESTAÇÃO METEOROLÓGICA (ID 2)
   HARDWARE: Sensor DHT22 no Pino 4 | LED no Pino 2
   FUNÇÃO: Lê Temp/Umidade, envia com Jitter e pisca 2x o LED.
*/

#include <esp_now.h>
#include <WiFi.h>
#include <DHT.h>

// ================= CONFIGURAÇÃO =================
#define DHTPIN      4       // Pino do sensor
#define DHTTYPE     DHT22   // Tipo do sensor
#define LED_PIN     2       // LED da placa (Geralmente GPIO 2)
#define NODE_ID     2       // ID 2 = Estação Meteorológica
#define BROADCAST_DATA_REQUEST 16 // Tipo de pacote para solicitar dados

DHT dht(DHTPIN, DHTTYPE);

// ================= ESTRUTURA DE DADOS =================
struct __attribute__((packed)) DataPacket {
    int node_id;
    int msg_type;
    float temps[13]; // <-- AUMENTADO PARA 13 (Consistência)
    float hum;
    bool motor_state;
    float threshold;
};

DataPacket myData;
uint8_t broadcastAddress[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
esp_now_peer_info_t peerInfo;

unsigned long lastTime = 0;
unsigned long interval = 5000; 
volatile bool pendingDataSend = false;

// Callback de Envio (compatível com ESP32 Core v2 e v3)
#if defined(ESP_ARDUINO_VERSION) && ESP_ARDUINO_VERSION >= ESP_ARDUINO_VERSION_VAL(3, 0, 0)
void OnDataSent(const wifi_tx_info_t *info, esp_now_send_status_t status) {}
#else
void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {}
#endif

// NOVO: Callback de Recebimento de Comandos (compatível com ESP32 Core v2 e v3)
#if defined(ESP_ARDUINO_VERSION) && ESP_ARDUINO_VERSION >= ESP_ARDUINO_VERSION_VAL(3, 0, 0)
void OnDataRecv(const esp_now_recv_info_t *info, const uint8_t *incomingData, int len) {
#else
void OnDataRecv(const uint8_t *mac, const uint8_t *incomingData, int len) {
#endif
  if (len == sizeof(DataPacket)) {
    DataPacket commandPacket;
    memcpy(&commandPacket, incomingData, sizeof(commandPacket));

    if (commandPacket.msg_type == BROADCAST_DATA_REQUEST) {
      Serial.println("Requisicao de dados recebida. Agendando envio...");
      pendingDataSend = true;
    }
  }
}

void setup() {
  Serial.begin(9600);
  
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW); // Garante que comece apagado

  dht.begin();
  
  WiFi.mode(WIFI_STA);
  if (esp_now_init() != ESP_OK) {
    Serial.println("Erro ESP-NOW");
    ESP.restart();
  }

  esp_now_register_send_cb(OnDataSent);
  // Registra o callback para receber comandos do Gateway
  esp_now_register_recv_cb(OnDataRecv);

  memcpy(peerInfo.peer_addr, broadcastAddress, 6);
  peerInfo.channel = 0;  
  peerInfo.encrypt = false;
  esp_now_add_peer(&peerInfo);
  
  Serial.println("Estacao Meteorologica (ID 2) Iniciada.");
}

void sendSensorData() {
  // 1. LER DADOS
  float h = dht.readHumidity();
  float t = dht.readTemperature(); 

  if (isnan(h) || isnan(t)) {
    Serial.println("Falha ao ler DHT22!");
    t = -127.0; 
    h = 0.0;
  }

  // 2. PREENCHER PACOTE
  myData.node_id = NODE_ID;
  myData.msg_type = 0;
  myData.hum = h;
  
  // Temperatura ambiente vai na posição 0
  myData.temps[0] = t;
  for(int i=1; i<13; i++) myData.temps[i] = -127.0;

  myData.motor_state = false;
  myData.threshold = 0; // Garante que o campo threshold seja inicializado

  Serial.print("Ambiente -> Temp: ");
  Serial.print(t);
  Serial.print("C | Hum: ");
  Serial.print(h);
  Serial.println("%");

  // 3. ENVIAR
  esp_now_send(broadcastAddress, (uint8_t *) &myData, sizeof(myData));
  
  // --- BLINK DUPLO (FEEDBACK VISUAL) ---
  digitalWrite(LED_PIN, HIGH); delay(50);
  digitalWrite(LED_PIN, LOW);  delay(50);
  digitalWrite(LED_PIN, HIGH); delay(50);
  digitalWrite(LED_PIN, LOW);
  // -------------------------------------
}

void loop() {
  if (pendingDataSend) {
    pendingDataSend = false;
    sendSensorData();
    lastTime = millis();
  }
  if ((millis() - lastTime) > interval) {
    sendSensorData();
    // 4. JITTER (Anti-Colisão)
    lastTime = millis();
    interval = 5000 + random(0, 1000); 
  }
}