#include "databasemanager.h"
#include <QSqlQuery>
#include <QSqlError>
#include <QDebug>
#include <QStandardPaths>
#include <QDir>
#include <QTimeZone>

DatabaseManager& DatabaseManager::instance()
{
    static DatabaseManager instance;
    return instance;
}

DatabaseManager::DatabaseManager(QObject *parent) : QObject(parent)
{
    openDb();
}

DatabaseManager::~DatabaseManager()
{
    if (m_db.isOpen()) {
        m_db.close();
        qInfo() << "Database connection closed.";
    }
}

bool DatabaseManager::openDb()
{
    if (m_db.isOpen()) {
        return true;
    }

    m_db = QSqlDatabase::addDatabase("QSQLITE");
    // Salva o banco de dados em um local padrão para dados de aplicativo
    QString path = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir dir(path);
    if (!dir.exists()) {
        dir.mkpath(".");
    }
    QString dbPath = path + "/history.sqlite";
    m_db.setDatabaseName(dbPath);

    if (!m_db.open()) {
        qCritical() << "Database connection failed:" << m_db.lastError().text();
        return false;
    }

    qInfo() << "Database opened successfully at" << dbPath;
    return initDb();
}

bool DatabaseManager::initDb()
{
    QSqlQuery query;
    bool success = true;

    // Tabela de leituras de termometria
    if (!query.exec("CREATE TABLE IF NOT EXISTS temperature_readings ("
                    "id INTEGER PRIMARY KEY AUTOINCREMENT, "
                    "timestamp DATETIME NOT NULL, "
                    "silo_id INTEGER NOT NULL, "
                    "pendulum_id INTEGER NOT NULL, " // Pêndulo canônico (0-4)
                    "sensor_idx INTEGER NOT NULL, " // Sensor dentro do pêndulo (0-12)
                    "temperature REAL NOT NULL"
                    ")")) {
        qCritical() << "Failed to create temperature_readings table:" << query.lastError().text();
        success = false;
    }

    // Tabela de leituras de meteorologia
    if (!query.exec("CREATE TABLE IF NOT EXISTS meteorology_readings ("
                    "id INTEGER PRIMARY KEY AUTOINCREMENT, "
                    "timestamp DATETIME NOT NULL, "
                    "temperature REAL NOT NULL, "
                    "humidity REAL NOT NULL"
                    ")")) {
        qCritical() << "Failed to create meteorology_readings table:" << query.lastError().text();
        success = false;
    }
    
    // Tabela de eventos de aeração
    if (!query.exec("CREATE TABLE IF NOT EXISTS motor_events ("
                    "id INTEGER PRIMARY KEY AUTOINCREMENT, "
                    "timestamp DATETIME NOT NULL, "
                    "motor_id INTEGER NOT NULL, " // 1-4
                    "is_on BOOLEAN NOT NULL"
                    ")")) {
        qCritical() << "Failed to create motor_events table:" << query.lastError().text();
        success = false;
    }

    if (success) {
        // Criar índices para otimizar pesquisas
        if (!query.exec("CREATE INDEX IF NOT EXISTS idx_temp_readings_query ON temperature_readings (silo_id, pendulum_id, timestamp)")) {
            qWarning() << "Failed to create index idx_temp_readings_query:" << query.lastError().text();
        }
        if (!query.exec("CREATE INDEX IF NOT EXISTS idx_meteo_readings_timestamp ON meteorology_readings (timestamp)")) {
            qWarning() << "Failed to create index idx_meteo_readings_timestamp:" << query.lastError().text();
        }
        if (!query.exec("CREATE INDEX IF NOT EXISTS idx_motor_events_query ON motor_events (motor_id, timestamp)")) {
            qWarning() << "Failed to create index idx_motor_events_query:" << query.lastError().text();
        }

        // Limpeza retroativa de logs redundantes e sucessivos de motores (evita travamento do gráfico)
        qInfo() << "Cleaning up consecutive redundant motor events from database...";
        if (!query.exec("DELETE FROM motor_events "
                        "WHERE id NOT IN ("
                        "  SELECT id FROM ("
                        "    SELECT id, is_on, "
                        "           LAG(is_on) OVER (PARTITION BY motor_id ORDER BY timestamp) as prev_on "
                        "    FROM motor_events"
                        "  ) "
                        "  WHERE is_on IS NOT prev_on OR prev_on IS NULL"
                        ")")) {
            qWarning() << "Failed to clean up redundant motor events:" << query.lastError().text();
        }

        qInfo() << "Database tables and indices initialized successfully.";
    }
    return success;
}

void DatabaseManager::logTemperatures(int siloId, const QVector<float>& temperatures)
{
    if (!m_db.isOpen() || temperatures.size() != 65) {
        return;
    }

    QSqlQuery query;
    query.prepare("INSERT INTO temperature_readings (timestamp, silo_id, pendulum_id, sensor_idx, temperature) "
                  "VALUES (?, ?, ?, ?, ?)");

    QDateTime now = QDateTime::currentDateTime();

    m_db.transaction();
    for (int i = 0; i < temperatures.size(); ++i) {
        int pendulumId = i / 13;
        int sensorIdx = i % 13;
        
        query.bindValue(0, now);
        query.bindValue(1, siloId);
        query.bindValue(2, pendulumId);
        query.bindValue(3, sensorIdx);
        query.bindValue(4, temperatures[i]);
        
        if (!query.exec()) {
            qWarning() << "Failed to log temperature for sensor" << i << ":" << query.lastError().text();
            m_db.rollback();
            return;
        }
    }
    m_db.commit();
}

void DatabaseManager::logMeteorology(double temperature, double humidity)
{
    if (!m_db.isOpen()) {
        return;
    }

    QSqlQuery query;
    query.prepare("INSERT INTO meteorology_readings (timestamp, temperature, humidity) VALUES (?, ?, ?)");
    query.addBindValue(QDateTime::currentDateTime());
    query.addBindValue(temperature);
    query.addBindValue(humidity);

    if (!query.exec()) {
        qWarning() << "Failed to log meteorology data:" << query.lastError().text();
    }
}

void DatabaseManager::logMotorEvent(int motorId, bool isOn)
{
    if (!m_db.isOpen()) {
        return;
    }

    QSqlQuery query;
    query.prepare("INSERT INTO motor_events (timestamp, motor_id, is_on) VALUES (?, ?, ?)");
    query.addBindValue(QDateTime::currentDateTime());
    query.addBindValue(motorId);
    query.addBindValue(isOn);

    if (!query.exec()) {
        qWarning() << "Failed to log motor event:" << query.lastError().text();
    }
}

QVector<PendulumDataPoint> DatabaseManager::getPendulumHistory(int siloId, int pendulumId, const QDateTime& start, const QDateTime& end, int maxPoints)
{
    QVector<PendulumDataPoint> rawData;
    if (!m_db.isOpen()) return rawData;

    qint64 diffSecs = start.secsTo(end);
    QString queryStr;
    bool useGrouping = false;
    if (diffSecs > 5 * 24 * 3600) { // mais de 5 dias: agrupa por hora
        queryStr = "SELECT strftime('%Y-%m-%d %H:00:00', timestamp) as time_group, sensor_idx, AVG(temperature) "
                   "FROM temperature_readings "
                   "WHERE silo_id = :silo_id AND pendulum_id = :pendulum_id AND timestamp BETWEEN :start AND :end "
                   "GROUP BY time_group, sensor_idx "
                   "ORDER BY time_group ASC";
        useGrouping = true;
    } else if (diffSecs > 12 * 3600) { // mais de 12 horas: agrupa por minuto
        queryStr = "SELECT strftime('%Y-%m-%d %H:%M:00', timestamp) as time_group, sensor_idx, AVG(temperature) "
                   "FROM temperature_readings "
                   "WHERE silo_id = :silo_id AND pendulum_id = :pendulum_id AND timestamp BETWEEN :start AND :end "
                   "GROUP BY time_group, sensor_idx "
                   "ORDER BY time_group ASC";
        useGrouping = true;
    } else { // menos de 12 horas: busca todos os dados brutos
        queryStr = "SELECT timestamp, sensor_idx, temperature "
                   "FROM temperature_readings "
                   "WHERE silo_id = :silo_id AND pendulum_id = :pendulum_id AND timestamp BETWEEN :start AND :end "
                   "ORDER BY timestamp ASC";
    }

    QSqlQuery query;
    query.prepare(queryStr);
    query.bindValue(":silo_id", siloId);
    query.bindValue(":pendulum_id", pendulumId);
    query.bindValue(":start", start);
    query.bindValue(":end", end);

    if (query.exec()) {
        PendulumDataPoint currentPoint;
        bool hasData = false;
        QTimeZone localZone = QTimeZone::systemTimeZone();
        
        while (query.next()) {
            QDateTime ts;
            if (useGrouping) {
                ts = QDateTime::fromString(query.value(0).toString(), "yyyy-MM-dd hh:mm:ss");
            } else {
                ts = query.value(0).toDateTime();
            }
            if (ts.timeSpec() != Qt::LocalTime) {
                ts.setTimeZone(localZone);
            }
            int sIdx = query.value(1).toInt();
            double temp = query.value(2).toDouble();
            
            if (hasData && ts != currentPoint.timestamp) {
                rawData.append(currentPoint);
                currentPoint.temps.fill(-100.0, 13);
                currentPoint.timestamp = ts;
            } else if (!hasData) {
                currentPoint.timestamp = ts;
                currentPoint.temps.fill(-100.0, 13);
                hasData = true;
            }
            
            if (sIdx >= 0 && sIdx < 13) {
                currentPoint.temps[sIdx] = temp;
            }
        }
        if (hasData) {
            rawData.append(currentPoint);
        }
    } else {
        qWarning() << "Failed to query pendulum history:" << query.lastError().text();
    }
    
    // Decimation (Amostragem Visual para Performance do Gráfico)
    if (useGrouping || rawData.size() <= maxPoints || maxPoints <= 0) {
        return rawData;
    }
    
    QVector<PendulumDataPoint> decimated;
    int n = rawData.size();
    double step = (double)n / maxPoints;
    
    for (int i = 0; i < maxPoints; ++i) {
        int startIdx = (int)(i * step);
        int endIdx = (int)((i + 1) * step);
        if (endIdx > n) endIdx = n;
        
        PendulumDataPoint avgPoint;
        avgPoint.timestamp = rawData[startIdx].timestamp; // Usa o timestamp do inicio do bucket
        avgPoint.temps.fill(0.0, 13);
        QVector<int> counts(13, 0);
        
        for (int j = startIdx; j < endIdx; ++j) {
            for (int s = 0; s < 13; ++s) {
                if (rawData[j].temps[s] > -90.0) {
                    avgPoint.temps[s] += rawData[j].temps[s];
                    counts[s]++;
                }
            }
        }
        for (int s = 0; s < 13; ++s) {
            if (counts[s] > 0) avgPoint.temps[s] /= counts[s];
            else avgPoint.temps[s] = -100.0;
        }
        decimated.append(avgPoint);
    }
    return decimated;
}

QVector<MotorEventDataPoint> DatabaseManager::getMotorHistory(int motorId, const QDateTime& start, const QDateTime& end)
{
    QVector<MotorEventDataPoint> data;
    if (!m_db.isOpen()) return data;

    QSqlQuery query;
    query.prepare("SELECT timestamp, is_on FROM motor_events "
                  "WHERE motor_id = :motor_id AND timestamp BETWEEN :start AND :end "
                  "ORDER BY timestamp ASC");
    query.bindValue(":motor_id", motorId);
    query.bindValue(":start", start);
    query.bindValue(":end", end);

    if (query.exec()) {
        QTimeZone localZone = QTimeZone::systemTimeZone();
        while (query.next()) {
            MotorEventDataPoint point;
            point.timestamp = query.value(0).toDateTime();
            if (point.timestamp.timeSpec() != Qt::LocalTime) {
                point.timestamp.setTimeZone(localZone);
            }
            point.isOn = query.value(1).toBool();
            data.append(point);
        }
    } else {
        qWarning() << "Failed to query motor history:" << query.lastError().text();
    }
    return data;
}

QVector<TemperatureDataPoint> DatabaseManager::getAvgTemperatureHistory(int siloId, const QDateTime& start, const QDateTime& end, int maxPoints)
{
    QVector<TemperatureDataPoint> rawData;
    if (!m_db.isOpen()) return rawData;

    qint64 diffSecs = start.secsTo(end);
    QString queryStr;
    bool useGrouping = false;
    if (diffSecs > 5 * 24 * 3600) { // mais de 5 dias: agrupa por hora
        queryStr = "SELECT strftime('%Y-%m-%d %H:00:00', timestamp) as time_group, AVG(temperature) as avg_temp "
                   "FROM temperature_readings "
                   "WHERE silo_id = :silo_id AND timestamp BETWEEN :start AND :end "
                   "GROUP BY time_group "
                   "ORDER BY time_group ASC";
        useGrouping = true;
    } else if (diffSecs > 12 * 3600) { // mais de 12 horas: agrupa por minuto
        queryStr = "SELECT strftime('%Y-%m-%d %H:%M:00', timestamp) as time_group, AVG(temperature) as avg_temp "
                   "FROM temperature_readings "
                   "WHERE silo_id = :silo_id AND timestamp BETWEEN :start AND :end "
                   "GROUP BY time_group "
                   "ORDER BY time_group ASC";
        useGrouping = true;
    } else { // menos de 12 horas: busca todos os dados brutos
        queryStr = "SELECT timestamp, AVG(temperature) as avg_temp "
                   "FROM temperature_readings "
                   "WHERE silo_id = :silo_id AND timestamp BETWEEN :start AND :end "
                   "GROUP BY timestamp "
                   "ORDER BY timestamp ASC";
    }

    QSqlQuery query;
    query.prepare(queryStr);
    query.bindValue(":silo_id", siloId);
    query.bindValue(":start", start);
    query.bindValue(":end", end);

    if (query.exec()) {
        QTimeZone localZone = QTimeZone::systemTimeZone();
        while (query.next()) {
            TemperatureDataPoint point;
            if (useGrouping) {
                point.timestamp = QDateTime::fromString(query.value(0).toString(), "yyyy-MM-dd hh:mm:ss");
            } else {
                point.timestamp = query.value(0).toDateTime();
            }
            if (point.timestamp.timeSpec() != Qt::LocalTime) {
                point.timestamp.setTimeZone(localZone);
            }
            point.temperature = query.value(1).toDouble();
            rawData.append(point);
        }
    } else {
        qWarning() << "Failed to query average temperature history:" << query.lastError().text();
    }
    
    if (useGrouping || rawData.size() <= maxPoints || maxPoints <= 0) return rawData;
    QVector<TemperatureDataPoint> decimated;
    double step = (double)rawData.size() / maxPoints;
    for (int i = 0; i < maxPoints; ++i) {
        int startIdx = (int)(i * step);
        int endIdx = (int)((i + 1) * step);
        if (endIdx > rawData.size()) endIdx = rawData.size();
        
        TemperatureDataPoint pt;
        pt.timestamp = rawData[startIdx].timestamp;
        double sum = 0;
        for (int j = startIdx; j < endIdx; ++j) sum += rawData[j].temperature;
        pt.temperature = sum / (endIdx - startIdx);
        decimated.append(pt);
    }
    return decimated;
}

QVector<MeteorologyDataPoint> DatabaseManager::getMeteorologyHistory(const QDateTime& start, const QDateTime& end, int maxPoints)
{
    QVector<MeteorologyDataPoint> rawData;
    if (!m_db.isOpen()) return rawData;

    qint64 diffSecs = start.secsTo(end);
    QString queryStr;
    bool useGrouping = false;
    if (diffSecs > 5 * 24 * 3600) { // mais de 5 dias: agrupa por hora
        queryStr = "SELECT strftime('%Y-%m-%d %H:00:00', timestamp) as time_group, AVG(temperature), AVG(humidity) "
                   "FROM meteorology_readings "
                   "WHERE timestamp BETWEEN :start AND :end "
                   "GROUP BY time_group "
                   "ORDER BY time_group ASC";
        useGrouping = true;
    } else if (diffSecs > 12 * 3600) { // mais de 12 horas: agrupa por minuto
        queryStr = "SELECT strftime('%Y-%m-%d %H:%M:00', timestamp) as time_group, AVG(temperature), AVG(humidity) "
                   "FROM meteorology_readings "
                   "WHERE timestamp BETWEEN :start AND :end "
                   "GROUP BY time_group "
                   "ORDER BY time_group ASC";
        useGrouping = true;
    } else { // menos de 12 horas: busca todos os dados brutos
        queryStr = "SELECT timestamp, temperature, humidity FROM meteorology_readings "
                   "WHERE timestamp BETWEEN :start AND :end "
                   "ORDER BY timestamp ASC";
    }

    QSqlQuery query;
    query.prepare(queryStr);
    query.bindValue(":start", start);
    query.bindValue(":end", end);

    if (query.exec()) {
        QTimeZone localZone = QTimeZone::systemTimeZone();
        while (query.next()) {
            MeteorologyDataPoint point;
            if (useGrouping) {
                point.timestamp = QDateTime::fromString(query.value(0).toString(), "yyyy-MM-dd hh:mm:ss");
            } else {
                point.timestamp = query.value(0).toDateTime();
            }
            if (point.timestamp.timeSpec() != Qt::LocalTime) {
                point.timestamp.setTimeZone(localZone);
            }
            point.temperature = query.value(1).toDouble();
            point.humidity = query.value(2).toDouble();
            rawData.append(point);
        }
    } else {
        qWarning() << "Failed to query meteorology history:" << query.lastError().text();
    }
    
    if (useGrouping || rawData.size() <= maxPoints || maxPoints <= 0) return rawData;
    QVector<MeteorologyDataPoint> decimated;
    double step = (double)rawData.size() / maxPoints;
    for (int i = 0; i < maxPoints; ++i) {
        int startIdx = (int)(i * step);
        int endIdx = (int)((i + 1) * step);
        if (endIdx > rawData.size()) endIdx = rawData.size();
        
        MeteorologyDataPoint pt;
        pt.timestamp = rawData[startIdx].timestamp;
        double sumT = 0, sumH = 0;
        for (int j = startIdx; j < endIdx; ++j) {
            sumT += rawData[j].temperature;
            sumH += rawData[j].humidity;
        }
        pt.temperature = sumT / (endIdx - startIdx);
        pt.humidity = sumH / (endIdx - startIdx);
        decimated.append(pt);
    }
    return decimated;
}