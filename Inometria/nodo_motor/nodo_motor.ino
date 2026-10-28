/*
   PLACA: NODO MOTOR (ID 1)
   HARDWARE: Saída do Motor no GPIO 2 (LED onboard)
   FUNÇÃO: Recebe comandos de motor via ESP-NOW e controla o GPIO 2.
           Envia periodicamente seu status (incluindo o estado do motor) ao Gateway.
*/

#include <WiFi.h>
#include <esp_now.h>

// ================= CONFIGURAÇÃO =================
#define MOTOR_PIN      2    // GPIO 2 (LED onboard) como saída do motor
#define NODE_ID        1    // ID deste Nodo (ID 1 para o Motor)
#define INTERVALO      5000 // Envia status a cada 5 segundos
#define BROADCAST_DATA_REQUEST 16 // Tipo de pacote para solicitar dados

// Estrutura de Dados (Idêntica à do Gateway/AP/PC)
struct __attribute__((packed)) DataPacket {
  int node_id;
  int msg_type;    // 0=SENSOR_DATA, 1=MOTOR_COMMAND
  float temps[13]; 
  float hum;
  bool motor_state;
  float threshold;
};

DataPacket myData;
uint8_t broadcastAddress[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
esp_now_peer_info_t peerInfo;

unsigned long lastTime = 0;
bool currentMotorState = false;
volatile bool pendingStatusSend = false;

// Função para enviar o status do motor ao Gateway
void sendStatus() {
  myData.node_id = NODE_ID;
  myData.msg_type = 0; // SENSOR_DATA (usado para enviar leituras/status periódico)
  myData.motor_state = currentMotorState;
  myData.hum = 0.0;
  myData.threshold = 0.0;
  
  // Como é um nó de motor, preenchemos as temperaturas com -127.0 (inválido/sem sensor)
  for (int i = 0; i < 13; i++) {
    myData.temps[i] = -127.0;
  }

  Serial.print("[STATUS] Enviando estado do motor: ");
  Serial.println(currentMotorState ? "LIGADO" : "DESLIGADO");

  esp_now_send(broadcastAddress, (uint8_t *)&myData, sizeof(myData));
}

// Callback de Envio (compatível com ESP32 Core v2 e v3)
#if defined(ESP_ARDUINO_VERSION) && ESP_ARDUINO_VERSION >= ESP_ARDUINO_VERSION_VAL(3, 0, 0)
void OnDataSent(const wifi_tx_info_t *info, esp_now_send_status_t status) {}
#else
void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {}
#endif

// Callback de Recebimento de Comandos (compatível com ESP32 Core v2 e v3)
#if defined(ESP_ARDUINO_VERSION) && ESP_ARDUINO_VERSION >= ESP_ARDUINO_VERSION_VAL(3, 0, 0)
void OnDataRecv(const esp_now_recv_info_t *info, const uint8_t *incomingData, int len) {
#else
void OnDataRecv(const uint8_t *mac, const uint8_t *incomingData, int len) {
#endif
  if (len == sizeof(DataPacket)) {
    DataPacket commandPacket;
    memcpy(&commandPacket, incomingData, sizeof(commandPacket));

    // Verifica se o comando é direcionado a este nodo
    if (commandPacket.node_id == NODE_ID) {
      if (commandPacket.msg_type == 1) { // MOTOR_COMMAND
        currentMotorState = commandPacket.motor_state;
        digitalWrite(MOTOR_PIN, currentMotorState ? HIGH : LOW);
        Serial.print("[COMANDO] Motor alterado para: ");
        Serial.println(currentMotorState ? "LIGADO" : "DESLIGADO");
        
        // Solicita envio de status fora do callback
        pendingStatusSend = true;
      }
    } else if (commandPacket.msg_type == BROADCAST_DATA_REQUEST) {
      Serial.println("[COMANDO] Requisição de dados recebida. Agendando envio de status...");
      pendingStatusSend = true;
    }
  }
}

void setup() {
  Serial.begin(9600);

  // 1. Configura Pinos
  pinMode(MOTOR_PIN, OUTPUT);
  digitalWrite(MOTOR_PIN, currentMotorState ? HIGH : LOW);

  // 2. Inicia WiFi e ESP-NOW
  WiFi.mode(WIFI_STA);
  if (esp_now_init() != ESP_OK) {
    Serial.println("Erro ao iniciar ESP-NOW");
    ESP.restart();
  }

  esp_now_register_send_cb(OnDataSent);
  esp_now_register_recv_cb(OnDataRecv);

  // 3. Registra o Gateway / Broadcast
  memcpy(peerInfo.peer_addr, broadcastAddress, 6);
  peerInfo.channel = 0;
  peerInfo.encrypt = false;
  esp_now_add_peer(&peerInfo);

  Serial.println("Nodo Motor (ID 1) Iniciado.");
}

void loop() {
  if (pendingStatusSend) {
    pendingStatusSend = false;
    sendStatus();
  }
  
  if ((millis() - lastTime) > INTERVALO) {
    sendStatus();
    lastTime = millis();
  }
}
