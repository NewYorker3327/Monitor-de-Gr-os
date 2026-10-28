#ifndef NETWORKMANAGER_H
#define NETWORKMANAGER_H

#include <QObject>
#include <QUdpSocket>
#include <QVector>
#include <QTimer>
#include <QMap>
#include <QDateTime>

// Enum para os tipos de pacote, para manter consistência com o firmware.
enum class PacketType : int {
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
// IMPORTANTE: Esta estrutura deve ser IDÊNTICA à definida no seu código .ino.
// O #pragma pack garante que o compilador não adicione preenchimento, mantendo o tamanho do pacote consistente.
#pragma pack(push, 1)
struct DataPacket {
    int node_id;
    int msg_type;
    float temps[13]; // <-- AUMENTADO PARA 13
    float hum;
    bool motor_state;
    float threshold;
};
#pragma pack(pop)

class NetworkManager : public QObject
{
    Q_OBJECT
public:
    explicit NetworkManager(QObject *parent = nullptr);
    bool startListening(quint16 port);
    void setAccessPointAddress(const QString& ip, quint16 port);
    void pingAccessPoint(); // Envia um pacote inicial para o AP aprender nosso IP

    // Envia um comando para ligar/desligar o motor de um nó específico
    void sendMotorCommand(int motorId, bool turnOn);
    void sendToggleBeaconCommand();
    void requestGatewayStatus();
    void requestAllData();
    void sendToggleGatewayBeaconCommand(bool turnOn);

signals:
    void newTemperaturesReceived(int siloId, const QVector<float>& temperatures);
    void newMeteoDataReceived(double temperature, double humidity);
    void newMotorStateReceived(int siloId, bool isOn);
    void networkStatusChanged(const QString& status, const QString& details);
    void remoteNodeStatusUpdate(int nodeId, const QString& status, const QString& details);
    // Novo sinal para o status específico da conexão com o Ponto de Acesso
    void accessPointStatusChanged(bool connected);

private slots:
    void onReadyRead();
    void onConnectionCheckTimeout();
    void onNodeTimeoutCheck(); // Novo slot para verificar timeouts dos nodos

private:
    void parseDataPacket(const QByteArray& datagram);
    void resetConnectionTimeout();

    QUdpSocket* m_udpSocket = nullptr;
    QHostAddress m_accessPointAddress;
    quint16 m_accessPointPort;

    // Atributos para monitoramento da conexão
    QTimer* m_connectionCheckTimer = nullptr;
    bool m_isAccessPointConnected = false;

    QTimer* m_nodeTimeoutTimer = nullptr; // Timer para verificar timeouts dos nodos
    QMap<int, QDateTime> m_lastNodePacketTime; // Última vez que um pacote foi recebido de um nó
    QMap<int, bool> m_nodeOnlineStatus; // Status online/offline de cada nó

    // Armazenamento central das temperaturas para o silo atual
    QVector<float> m_currentSiloTemperatures;
    // Mapeamento de IDs de hardware para índices lógicos na aplicação
    QMap<int, int> m_nodeIdToPendulumIndex; // Mapeia ID do Nodo (ex: 3) para índice do pêndulo (ex: 0)
    QMap<int, int> m_motorToNodeIdMap;      // Mapeia ID do Motor (ex: 1) para ID do Nodo (ex: 3)

};

#endif // NETWORKMANAGER_H