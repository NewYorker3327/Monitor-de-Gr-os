#ifndef SENSORWIDGET_H
#define SENSORWIDGET_H

#include <QFrame>
#include <QLabel>
#include <QVBoxLayout>
#include <QComboBox>

class SensorWidget : public QFrame
{
    Q_OBJECT

public:
    explicit SensorWidget(int gateway, int node, int sensor, QWidget *parent = nullptr);
    ~SensorWidget();

    void setTemperature(double temp);
    void setOnline(bool online);
    void setAlert(bool alert);
    void setZoom(double factor);
    void setPhysicalPosition(int pos);

    // Getters para estado interno
    int sensorId() const { return m_sensor; }
    double temperature() const { return m_temperature; }
    bool isOnline() const { return m_online; }

    static QColor getTemperatureColor(double temp);
    static int activePalette() { return s_activePalette; }
    static void setActivePalette(int palette) { s_activePalette = palette; }

protected:
    void resizeEvent(QResizeEvent *event) override;

signals:
    void sensorClicked(int gateway, int node, int sensor);
    void positionChanged(int gateway, int node, int rawSensorId, int newPosition);

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void paintEvent(QPaintEvent *event) override;

private:
    void setupUI();
    void updateStyle();
    

    int m_gateway;
    int m_node;
    int m_sensor;
    double m_temperature;
    double m_currentZoom = 1.0;
    bool m_online;
    bool m_alert;

    QVBoxLayout *m_layout;
    QLabel *m_idLabel;
    QLabel *m_temperatureLabel;
    QLabel *m_statusLabel;
    QComboBox *m_positionCombo = nullptr;
    static int s_activePalette;
};

#endif // SENSORWIDGET_H