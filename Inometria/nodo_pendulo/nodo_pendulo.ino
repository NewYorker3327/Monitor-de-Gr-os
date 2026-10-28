/*
   PLACA: NODO PÊNDULO (ID 1)
   VERSÃO: Leitura Bruta + Blink Duplo
   HARDWARE: Sensores DS18B20 no GPIO 4 | LED no GPIO 2
*/

#include <DallasTemperature.h>
#include <OneWire.h>
#include <WiFi.h>
#include <esp_now.h>

// ================= CONFIGURAÇÃO =================
#define SENSOR_PIN_4   4    // Barramento GPIO 4 (7 sensores)
#define SENSOR_PIN_17  17   // Barramento GPIO 17 (6 sensores)
#define LED_PIN        2    // LED de status da placa
#define NODE_ID        3    // ID deste Nodo (consistente com o log)
#define INTERVALO      5000 // Envia a cada 5 segundos
#define BROADCAST_DATA_REQUEST 16 // Tipo de pacote para solicitar dados

// Inicialização das bibliotecas Dallas
OneWire oneWire4(SENSOR_PIN_4);
DallasTemperature sensors4(&oneWire4);

OneWire oneWire17(SENSOR_PIN_17);
DallasTemperature sensors17(&oneWire17);

// Estrutura de Dados
struct __attribute__((packed)) DataPacket {
  int node_id;
  int msg_type;    // 0=SENSOR_DATA, 1=MOTOR_COMMAND
  float temps[13]; // <-- AUMENTADO PARA 13 SENSORES
  float hum;
  bool motor_state;
  float threshold;
};

DataPacket myData;
uint8_t broadcastAddress[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
esp_now_peer_info_t peerInfo;

unsigned long lastTime = 0;
volatile bool pendingDataSend = false;

// Callback de Envio (compatível com ESP32 Core v2 e v3)
#if defined(ESP_ARDUINO_VERSION) &&                                            \
    ESP_ARDUINO_VERSION >= ESP_ARDUINO_VERSION_VAL(3, 0, 0)
void OnDataSent(const wifi_tx_info_t *info, esp_now_send_status_t status) {}
#else
void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {}
#endif

// NOVO: Callback de Recebimento de Comandos (compatível com ESP32 Core v2 e v3)
#if defined(ESP_ARDUINO_VERSION) &&                                            \
    ESP_ARDUINO_VERSION >= ESP_ARDUINO_VERSION_VAL(3, 0, 0)
void OnDataRecv(const esp_now_recv_info_t *info, const uint8_t *incomingData,
                int len) {
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

  // 1. Configura Pinos
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  // 2. Inicia Sensores nos dois barramentos
  sensors4.begin();
  sensors4.setResolution(12);          // Resolução de 12 bits para máxima precisão (0.0625°C)
  sensors4.setWaitForConversion(true);

  sensors17.begin();
  sensors17.setResolution(12);          // Resolução de 12 bits para máxima precisão (0.0625°C)
  sensors17.setWaitForConversion(true);

  Serial.print("Nodo Pendulo Iniciado. Sensores no GPIO 4: ");
  Serial.print(sensors4.getDeviceCount());
  Serial.print(" | Sensores no GPIO 17: ");
  Serial.println(sensors17.getDeviceCount());

  // 3. Inicia WiFi e ESP-NOW
  WiFi.mode(WIFI_STA);
  if (esp_now_init() != ESP_OK) {
    Serial.println("Erro ao iniciar ESP-NOW");
    ESP.restart();
  }

  esp_now_register_send_cb(OnDataSent);
  // Registra o callback para receber comandos do Gateway
  esp_now_register_recv_cb(OnDataRecv);

  // 4. Registra o Gateway
  memcpy(peerInfo.peer_addr, broadcastAddress, 6);
  peerInfo.channel = 0;
  peerInfo.encrypt = false;
  esp_now_add_peer(&peerInfo);
}

void sendSensorData() {
  // --- 1. REQUISIÇÃO ---
  sensors4.requestTemperatures();
  sensors17.requestTemperatures();

  // Prepara o pacote
  myData.node_id = NODE_ID;
  myData.msg_type = 0;        // Tipo 0 = Pacote de dados de sensor
  myData.motor_state = false; // Este nodo não possui motor
  myData.hum = 0.0;
  myData.threshold = 0.0;

  // --- 2. LEITURA E IMPRESSÃO (13 Sensores) ---
  // GPIO 4: 7 sensores (índices 0 a 6)
  for (int i = 0; i < 7; i++) {
    myData.temps[i] = sensors4.getTempCByIndex(i);
  }
  // GPIO 17: 6 sensores (índices 7 a 12, mapeados dos índices 0 a 5 do barramento)
  for (int i = 0; i < 6; i++) {
    myData.temps[7 + i] = sensors17.getTempCByIndex(i);
  }

  // Impressão serial em formato de leitura bruta (13 valores separados por vírgula)
  for (int i = 0; i < 13; i++) {
    Serial.print(myData.temps[i], 2);
    if (i < 12) {
      Serial.print(",");
    }
  }
  Serial.println();

  // --- 3. ENVIO ---
  esp_now_send(broadcastAddress, (uint8_t *)&myData, sizeof(myData));

  // --- 4. BLINK DUPLO (FEEDBACK VISUAL) ---
  digitalWrite(LED_PIN, HIGH);
  delay(50);
  digitalWrite(LED_PIN, LOW);
  delay(50);
  digitalWrite(LED_PIN, HIGH);
  delay(50);
  digitalWrite(LED_PIN, LOW);
  // -------------------------------------
}

void loop() {
  if (pendingDataSend) {
    pendingDataSend = false;
    sendSensorData();
  }
  if ((millis() - lastTime) > INTERVALO) {
    sendSensorData();
    lastTime = millis();
  }
}