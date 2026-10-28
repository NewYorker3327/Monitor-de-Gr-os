#ifndef SILO2DWIDGET_H
#define SILO2DWIDGET_H

#include <QWidget>
#include <QImage>
#include <QVector>
#include <QColor>

class Silo2DWidget : public QWidget
{
    Q_OBJECT

public:
    explicit Silo2DWidget(QWidget *parent = nullptr);

public slots:
    void setShowGrain(bool show);
    void setShowSensors(bool show);
    void setShowValues(bool show);
    void setShowMotors(bool show);
    void setSensorTemperatures(const QVector<float>& temperatures);
    void setMotorStatus(bool isOn);
    void setCurrentTheme(int theme);
    void updateSensorPositions(const int mapping[5][13]);

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    void updateHeatmap();
    QColor getTemperatureColor(float temp);
    void drawSensor(QPainter& painter, const QPointF& pos, float temp, bool showValue);
    void drawMotorIndicator(QPainter& painter, const QPointF& center, float radius, bool isOn);

    QImage m_dayImage;
    QImage m_nightImage;
    QImage m_heatmapImage;

    bool m_showGrain, m_showSensors, m_showValues, m_showMotors, m_motorIsOn;
    int m_currentTheme;

    struct Sensor2D { QPointF relativePos; float temperature; };
    QVector<Sensor2D> m_sensors;
};

#endif // SILO2DWIDGET_H