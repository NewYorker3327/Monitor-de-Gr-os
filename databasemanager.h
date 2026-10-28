#ifndef DATABASEMANAGER_H
#define DATABASEMANAGER_H

#include <QObject>
#include <QSqlDatabase>
#include <QVector>
#include <QPointF>
#include <QDateTime>

// Estrutura para retornar dados históricos de temperatura agrupados por pêndulo
struct PendulumDataPoint {
    QDateTime timestamp;
    QVector<double> temps; // Array de 13 posições (uma por sensor)
};

// Estrutura para retornar dados históricos de temperatura individuais (médias)
struct TemperatureDataPoint {
    QDateTime timestamp;
    double temperature;
};

// Estrutura para retornar dados históricos de meteorologia
struct MeteorologyDataPoint {
    QDateTime timestamp;
    double temperature;
    double humidity;
};

// Estrutura para retornar dados históricos de aeração
struct MotorEventDataPoint {
    QDateTime timestamp;
    bool isOn;
};

class DatabaseManager : public QObject
{
    Q_OBJECT
public:
    static DatabaseManager& instance();
    ~DatabaseManager();

    bool openDb();
    void logTemperatures(int siloId, const QVector<float>& temperatures);
    void logMeteorology(double temperature, double humidity);
    void logMotorEvent(int motorId, bool isOn);

    QVector<PendulumDataPoint> getPendulumHistory(int siloId, int pendulumId, const QDateTime& start, const QDateTime& end, int maxPoints = 500);
    QVector<TemperatureDataPoint> getAvgTemperatureHistory(int siloId, const QDateTime& start, const QDateTime& end, int maxPoints = 500);
    QVector<MeteorologyDataPoint> getMeteorologyHistory(const QDateTime& start, const QDateTime& end, int maxPoints = 500);
    QVector<MotorEventDataPoint> getMotorHistory(int motorId, const QDateTime& start, const QDateTime& end);

private:
    explicit DatabaseManager(QObject *parent = nullptr);
    DatabaseManager(const DatabaseManager&) = delete;
    DatabaseManager& operator=(const DatabaseManager&) = delete;

    bool initDb();

    QSqlDatabase m_db;
};

#endif // DATABASEMANAGER_H