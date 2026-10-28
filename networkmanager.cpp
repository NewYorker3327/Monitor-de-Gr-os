#include "networkmanager.h"
#include <QHostAddress>
#include <QDebug>
#include <QDateTime>

NetworkManager::NetworkManager(QObject *parent) : QObject(parent)
{
    m_accessPointPort = 0;
    m_udpSocket = new QUdpSocket(this);
    m_connectionCheckTimer = new QTimer(this);
    m_connectionCheckTimer->setSingleShot(true); // O timer só dispara uma vez, e é rearmado a cada pacote.
    connect(m_connectionCheckTimer, &QTimer::timeout, this, &NetworkManager::onConnectionCheckTimeout);

    m_nodeTimeoutTimer = new QTimer(this);
    m_nodeTimeoutTimer->setInterval(5000); // Verifica a cada 5 segundos
    connect(m_nodeTimeoutTimer, &QTimer::timeout, this, &NetworkManager::onNodeTimeoutCheck);
    connect(m_udpSocket, &QUdpSocket::readyRead, this, &NetworkManager::onReadyRead);

    // Inicializa o vetor de temperaturas do silo com 65 posições (5 pêndulos x 13 sensores)
    m_currentSiloTemperatures.fill(0.0f, 65);

    // --- MAPEAMENTO CRÍTICO ---
    // Mapeia o ID do hardware (nodo) para o índice lógico do pêndulo na UI (0-4)
    // Ex: O nodo com ID 3 no firmware corresponde ao primeiro pêndulo (índice 0)
    m_nodeIdToPendulumIndex[3] = 0; // Nodo 3 -> Pêndulo 0 (Centro)

    // Mapeia o motor da UI (1-4) para o ID do nodo que o controla
    m_motorToNodeIdMap[1] = 1; // Motor 1 (Alpha) -> é controlado pelo Nodo 1
}

void NetworkManager::setAccessPointAddress(const QString& ip, quint16 port)
{
    m_accessPointAddress.setAddress(ip);
    m_accessPointPort = port;
    qInfo() << "NetworkManager: Endereço do Access Point definido para" << ip << ":" << port;
}

void NetworkManager::pingAccessPoint()
{
    if (m_accessPointAddress.isNull()) return;

    DataPacket pingPacket = {0};
    pingPacket.node_id = -1; // ID para o PC
    pingPacket.msg_type = static_cast<int>(PacketType::PC_PING);

    m_udpSocket->writeDatagram(reinterpret_cast<const char*>(&pingPacket), sizeof(pingPacket), m_accessPointAddress, m_accessPointPort);
    qInfo() << "Enviando PING para o Access Point para registrar o IP do PC.";
}

bool NetworkManager::startListening(quint16 port)
{
    // Se o socket já está conectado e escutando na porta correta, não faz nada.
    if (m_udpSocket->state() == QAbstractSocket::BoundState && m_udpSocket->localPort() == port) {
        return true;
    }
    // Se o socket estiver em qualquer estado que não seja "desconectado", fecha antes de tentar um novo bind.
    if (m_udpSocket->state() != QAbstractSocket::UnconnectedState) { m_udpSocket->close(); }
    if (m_udpSocket->bind(QHostAddress::Any, port)) {
        qInfo() << "NetworkManager: Escutando na porta UDP" << port;
        emit networkStatusChanged("Online", "Escutando na porta " + QString::number(port));
        // Inicia o monitoramento da conexão
        m_isAccessPointConnected = false;
        emit accessPointStatusChanged(false);
        resetConnectionTimeout(); // Inicia o timer pela primeira vez
        m_nodeTimeoutTimer->start(); // Inicia o timer de verificação de nodos
        return true;
    } else {
        qCritical() << "NetworkManager: Falha ao iniciar escuta na porta UDP" << port << ":" << m_udpSocket->errorString();
        emit networkStatusChanged("Offline", "Falha ao escutar na porta " + QString::number(port));
        return false;
    }
}

void NetworkManager::onReadyRead()
{
    while (m_udpSocket->hasPendingDatagrams()) {
        QByteArray datagram;
        datagram.resize(m_udpSocket->pendingDatagramSize());
        QHostAddress sender;
        quint16 senderPort;

        m_udpSocket->readDatagram(datagram.data(), datagram.size(), &sender, &senderPort);

        // Se o pacote veio do IP do nosso Access Point, a conexão está viva.
        if (sender == m_accessPointAddress) {
            resetConnectionTimeout();
        }

        // Validação básica do tamanho do pacote
        if (datagram.size() == sizeof(DataPacket)) {
            parseDataPacket(datagram);
        } else {
            qWarning() << "NetworkManager: Pacote UDP com tamanho incorreto recebido de" << sender.toString()
                       << "Tamanho recebido:" << datagram.size()
                       << "Tamanho esperado:" << sizeof(DataPacket);
        }
    }
}

void NetworkManager::parseDataPacket(const QByteArray& datagram)
{
    const DataPacket* packet = reinterpret_cast<const DataPacket*>(datagram.constData());

    PacketType type = static_cast<PacketType>(packet->msg_type);

    switch(type) {
        case PacketType::SENSOR_DATA: {
            qInfo() << "NetworkManager: Pacote de SENSOR recebido do nó" << packet->node_id;
            // O RSSI agora é enviado pelo Access Point no campo 'threshold'.
            QString details = QString("%1 | RSSI: %2 dBm")
                                  .arg(QDateTime::currentDateTime().toString("hh:mm:ss"))
                                  .arg(packet->threshold);
            m_lastNodePacketTime[packet->node_id] = QDateTime::currentDateTime(); // Atualiza o último visto
            emit remoteNodeStatusUpdate(packet->node_id, "Online", details);

            // 1. Atualiza o estado do motor se o nó estiver mapeado para um motor
            int motorId = m_motorToNodeIdMap.key(packet->node_id, -1);
            if (motorId != -1) {
                emit newMotorStateReceived(motorId - 1, packet->motor_state);
            }

            // 2. Verifica se o pacote veio de um nodo de termometria mapeado
            if (m_nodeIdToPendulumIndex.contains(packet->node_id)) {
                int pendulumIndex = m_nodeIdToPendulumIndex.value(packet->node_id);
                int startIndex = pendulumIndex * 13;

                // Atualiza a seção correspondente no vetor de 65 temperaturas
                for(int i=0; i<13; ++i) {
                    if ((startIndex + i) < m_currentSiloTemperatures.size()) {
                        m_currentSiloTemperatures[startIndex + i] = packet->temps[i];
                    }
                }
                // Emite o vetor completo atualizado para a UI
                emit newTemperaturesReceived(0, m_currentSiloTemperatures); // Assumindo Silo 0
            }

            // 3. Se for o nó da Estação Meteorológica, atualiza os dados da meteorologia
            if (packet->node_id == 2) {
                emit newMeteoDataReceived(packet->temps[0], packet->hum);
            }
            break;
        }

        case PacketType::AP_BEACON:
            qInfo() << "Beacon recebido do Access Point.";
            // A simples recepção do pacote já reinicia o timer em onReadyRead.
            // Apenas garantimos que o status seja "Conectado".
            if (!m_isAccessPointConnected) {
                m_isAccessPointConnected = true;
                emit accessPointStatusChanged(true);
            }
            break;

        case PacketType::GATEWAY_BEACON: {
            qInfo() << "Beacon recebido do Gateway.";
            // O RSSI da comunicação Gateway -> Access Point está no campo 'threshold'.
            QString rssiDetails = QString("%1 dBm").arg(packet->threshold);
            // Usamos o nodeId 0 (definido no firmware do Gateway) para identificar o próprio Gateway.
            emit remoteNodeStatusUpdate(0, "Online", rssiDetails);
            m_lastNodePacketTime[0] = QDateTime::currentDateTime(); // Atualiza o último visto para o Gateway
            break;
        }

        default:
            qWarning() << "NetworkManager: Pacote com msg_type desconhecido:" << packet->msg_type;
            break;
    }
}

void NetworkManager::resetConnectionTimeout() {
    if (!m_isAccessPointConnected) {
        m_isAccessPointConnected = true;
        emit accessPointStatusChanged(true);
    }
    m_connectionCheckTimer->start(10000); // Espera 10 segundos por um novo pacote
}

void NetworkManager::onConnectionCheckTimeout() {
    m_isAccessPointConnected = false;
    emit accessPointStatusChanged(false);
}

void NetworkManager::onNodeTimeoutCheck() {
    const int NODE_TIMEOUT_SECONDS = 15; // Tempo limite para considerar um nó offline

    QDateTime currentTime = QDateTime::currentDateTime();
    for (auto it = m_lastNodePacketTime.begin(); it != m_lastNodePacketTime.end(); ++it) {
        int nodeId = it.key();
        QDateTime lastSeen = it.value();

        if (lastSeen.secsTo(currentTime) > NODE_TIMEOUT_SECONDS) {
            if (m_nodeOnlineStatus.value(nodeId, true)) { // Se estava online e agora está offline
                m_nodeOnlineStatus[nodeId] = false;
                emit remoteNodeStatusUpdate(nodeId, "Offline", "Timeout");
            }
        } else {
            m_nodeOnlineStatus[nodeId] = true; // Node is still online
        }
    }
}

void NetworkManager::sendMotorCommand(int motorId, bool turnOn) // motorId é 1-4
{
    if (m_accessPointAddress.isNull() || m_accessPointPort == 0) {
        qWarning() << "NetworkManager: Endereço do Access Point não configurado. Comando não enviado.";
        return;
    }

    // Encontra para qual ID de nodo o comando do motor deve ser enviado
    int targetNodeId = m_motorToNodeIdMap.value(motorId, -1);
    if (targetNodeId == -1) {
        qWarning() << "NetworkManager: Comando para motor" << motorId << "ignorado. Nenhum nodo mapeado.";
        return;
    }

    DataPacket commandPacket = {0};
    commandPacket.node_id = targetNodeId; // O nó de destino do comando
    commandPacket.msg_type = static_cast<int>(PacketType::MOTOR_COMMAND);
    commandPacket.motor_state = turnOn;

    m_udpSocket->writeDatagram(reinterpret_cast<const char*>(&commandPacket), sizeof(commandPacket), m_accessPointAddress, m_accessPointPort);
    qInfo() << "NetworkManager: Comando de motor (Ligar:" << turnOn << ") enviado para o motor" << motorId << "(Nodo" << targetNodeId << ")";
}

void NetworkManager::sendToggleBeaconCommand()
{
    if (m_accessPointAddress.isNull() || m_accessPointPort == 0) {
        qWarning() << "NetworkManager: Endereço do Access Point não configurado. Comando não enviado.";
        return;
    }

    DataPacket commandPacket = {0};
    commandPacket.node_id = 0; // Não relevante para este comando
    commandPacket.msg_type = static_cast<int>(PacketType::TOGGLE_BEACON);

    m_udpSocket->writeDatagram(reinterpret_cast<const char*>(&commandPacket), sizeof(commandPacket), m_accessPointAddress, m_accessPointPort);
    qInfo() << "NetworkManager: Comando para ligar/desligar beacon enviado.";
}

void NetworkManager::requestGatewayStatus()
{
    if (m_accessPointAddress.isNull() || m_accessPointPort == 0) {
        qWarning() << "NetworkManager: Endereço do Access Point não configurado. Solicitação de status não enviada.";
        return;
    }

    DataPacket requestPacket = {0};
    requestPacket.node_id = -1; // PC
    requestPacket.msg_type = static_cast<int>(PacketType::GATEWAY_STATUS_REQUEST);

    m_udpSocket->writeDatagram(reinterpret_cast<const char*>(&requestPacket), sizeof(requestPacket), m_accessPointAddress, m_accessPointPort);
    qInfo() << "NetworkManager: Solicitacao de status do Gateway enviada para o Access Point.";
}

void NetworkManager::requestAllData()
{
    if (m_accessPointAddress.isNull() || m_accessPointPort == 0) {
        qWarning() << "NetworkManager: Endereço do Access Point não configurado. Requisição de dados não enviada.";
        return;
    }

    DataPacket requestPacket = {0};
    requestPacket.node_id = -1; // PC
    requestPacket.msg_type = static_cast<int>(PacketType::BROADCAST_DATA_REQUEST);

    m_udpSocket->writeDatagram(reinterpret_cast<const char*>(&requestPacket), sizeof(requestPacket), m_accessPointAddress, m_accessPointPort);
    qInfo() << "NetworkManager: Requisição de dados para todos os nodos enviada.";
}

void NetworkManager::sendToggleGatewayBeaconCommand(bool turnOn)
{
    if (m_accessPointAddress.isNull() || m_accessPointPort == 0) {
        qWarning() << "NetworkManager: Endereço do Access Point não configurado. Comando não enviado.";
        return;
    }

    DataPacket commandPacket = {0};
    commandPacket.node_id = 0; // Comando destinado ao Gateway
    commandPacket.msg_type = static_cast<int>(PacketType::TOGGLE_GATEWAY_BEACON);
    commandPacket.motor_state = turnOn; // absolute state!

    m_udpSocket->writeDatagram(reinterpret_cast<const char*>(&commandPacket), sizeof(commandPacket), m_accessPointAddress, m_accessPointPort);
    qInfo() << "NetworkManager: Comando para alterar beacon do GATEWAY para" << (turnOn ? "LIGADO" : "DESLIGADO") << "enviado.";
}