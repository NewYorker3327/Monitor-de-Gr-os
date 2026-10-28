#include "silo2dwidget.h"
#include "sensorwidget.h"
#include <QPainter>
#include <QPainterPath>
#include <QLinearGradient>
#include <QTime>
#include <QDebug>

Silo2DWidget::Silo2DWidget(QWidget *parent)
    : QWidget(parent),
      m_showGrain(true),
      m_showSensors(true),
      m_showValues(true),
      m_showMotors(true),
      m_motorIsOn(false),
      m_currentTheme(1)
{
    // Carrega as imagens de dia e noite a partir dos recursos.
    m_dayImage.load(":/silo2d.jpg");
    m_nightImage.load(":/silo2d_noite.jpg");

    if (m_dayImage.isNull()) {
        qWarning() << "Nao foi possivel carregar a imagem de recurso ':/silo2d.jpg'. Um placeholder sera usado.";
    }
    if (m_nightImage.isNull()) {
        qWarning() << "Nao foi possivel carregar a imagem de recurso ':/silo2d_noite.jpg'.";
    }

    // Define as posições relativas dos sensores na imagem 2D (valores de 0.0 a 1.0)
    // As posições agora são retilíneas, conforme os novos indicativos.
    const int numPendulums = 5;
    const int sensorsPerPendulum = 13;

    // Mapeamento para garantir que o pêndulo central (índice 0 nos dados)
    // apareça no meio da visualização 2D.
    // Ordem visual (esquerda->direita): Oeste, Sul, Centro, Norte, Leste
    const float x_positions[] = { 0.28f, 0.39f, 0.5f, 0.61f, 0.72f };
    const float data_p_to_x_pos[5] = {
        x_positions[2], // p=0 (Centro) -> Posição visual 2 (0.5f)
        x_positions[3], // p=1 (Norte)  -> Posição visual 3 (0.61f)
        x_positions[4], // p=2 (Leste)  -> Posição visual 4 (0.72f)
        x_positions[1], // p=3 (Sul)    -> Posição visual 1 (0.39f)
        x_positions[0]  // p=4 (Oeste)  -> Posição visual 0 (0.28f)
    };

    const float y_start = 0.245f, y_end = 0.845f; // Ajustado para cima

    for (int p = 0; p < numPendulums; ++p) {
        for (int s = 0; s < sensorsPerPendulum; ++s) {
            Sensor2D sensor;
            float y_rel = (float)s / (sensorsPerPendulum - 1); // 0.0 (topo) a 1.0 (base)

            // Posição X é constante para cada pêndulo, criando uma linha reta.
            sensor.relativePos.setX(data_p_to_x_pos[p]);
            sensor.relativePos.setY(y_start + (y_end - y_start) * y_rel);
            sensor.temperature = 0.0f; // default temp
            m_sensors.append(sensor);
        }
    }
    updateHeatmap();
}

void Silo2DWidget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    // Decide qual imagem de fundo usar com base no tema e na hora.
    // A lógica de dia/noite agora é automática e independente da seleção de tema.
    QImage* imageToDraw = &m_dayImage;
    int hour = QTime::currentTime().hour();
    if (hour < 6 || hour >= 18) {
        if (!m_nightImage.isNull()) { imageToDraw = &m_nightImage; }
    }

    if (!imageToDraw->isNull()) {
        painter.drawImage(rect(), *imageToDraw);
    } else {
        painter.fillRect(rect(), QColor("#334155"));
        painter.setPen(Qt::white);
        painter.drawText(rect(), Qt::AlignCenter, "Imagem 'silo2d.jpg' nao encontrada");
    }

    if (m_showGrain && !m_heatmapImage.isNull()) {
        painter.setOpacity(0.6);
        painter.drawImage(rect(), m_heatmapImage);
        painter.setOpacity(1.0);
    }

    if (m_showMotors) {
        // Desenha indicadores circulares nos motores, conforme solicitado.
        // As posições foram ajustadas para ficarem exatamente sobre a carcaça dos motores.
        float indicatorRadius = width() * 0.015; // Raio do círculo indicador
        QPointF motorPos1(width() * 0.120, height() * 0.820); // Ajustado para cima
        QPointF motorPos2(width() * 0.89, height() * 0.820); // Ajustado para cima
        drawMotorIndicator(painter, motorPos1, indicatorRadius, m_motorIsOn);
        drawMotorIndicator(painter, motorPos2, indicatorRadius, m_motorIsOn);
    }

    if (m_showSensors) {
        for (const auto& sensor : m_sensors) {
            QPointF absolutePos(width() * sensor.relativePos.x(), height() * sensor.relativePos.y());
            drawSensor(painter, absolutePos, sensor.temperature, m_showValues);
        }
    }
}

void Silo2DWidget::resizeEvent(QResizeEvent *event) {
    QWidget::resizeEvent(event);
    updateHeatmap();
}

void Silo2DWidget::setShowGrain(bool show) { m_showGrain = show; update(); }
void Silo2DWidget::setShowSensors(bool show) { m_showSensors = show; update(); }
void Silo2DWidget::setShowValues(bool show) { m_showValues = show; update(); }
void Silo2DWidget::setShowMotors(bool show) { m_showMotors = show; update(); }

void Silo2DWidget::setSensorTemperatures(const QVector<float>& temperatures) {
    if (temperatures.size() != m_sensors.size()) return;
    for (int i = 0; i < m_sensors.size(); ++i) m_sensors[i].temperature = temperatures[i];
    updateHeatmap();
    update();
}

void Silo2DWidget::setMotorStatus(bool isOn) { m_motorIsOn = isOn; update(); }

void Silo2DWidget::setCurrentTheme(int theme) { m_currentTheme = theme; update(); }

void Silo2DWidget::updateSensorPositions(const int mapping[5][13]) {
    const float y_start = 0.245f, y_end = 0.845f;
    for (int p = 0; p < 5; ++p) {
        for (int s = 0; s < 13; ++s) {
            int idx = p * 13 + s;
            if (idx < m_sensors.size()) {
                int pos = mapping[p][s];
                float y_rel = (float)(pos - 1) / 12.0f;
                m_sensors[idx].relativePos.setY(y_start + (y_end - y_start) * y_rel);
            }
        }
    }
    updateHeatmap();
    update();
}

void Silo2DWidget::updateHeatmap() {
    if (size().isEmpty() || m_sensors.isEmpty()) return;

    // Criamos uma imagem de baixa resolução para interpolação rápida e suave (heatmap)
    const int gridW = 50;
    const int gridH = 100;
    QImage lowResHeatmap(gridW, gridH, QImage::Format_ARGB32);
    lowResHeatmap.fill(Qt::transparent);

    // Limites lógicos da massa de grãos correspondentes ao desenho do silo
    const float x_start = 0.26f;
    const float x_end = 0.74f;
    const float y_start = 0.18f;
    const float y_end = 0.88f;

    for (int gy = 0; gy < gridH; ++gy) {
        float y_rel = (float)gy / (gridH - 1);
        float y = y_start + (y_end - y_start) * y_rel;

        for (int gx = 0; gx < gridW; ++gx) {
            float x_rel = (float)gx / (gridW - 1);
            float x = x_start + (x_end - x_start) * x_rel;

            float sumWeights = 0.0f;
            float sumWeightedTemp = 0.0f;
            bool exactMatch = false;
            float exactTemp = 0.0f;

            for (const auto& s : m_sensors) {
                float dx = x - s.relativePos.x();
                float dy = y - s.relativePos.y();
                float distSq = dx * dx + dy * dy;

                if (distSq < 0.0001f) {
                    exactMatch = true;
                    exactTemp = s.temperature;
                    break;
                }

                // IDW com peso de distância à quarta potência para suavização e realce de pontos quentes
                float weight = 1.0f / (distSq * distSq);
                sumWeights += weight;
                sumWeightedTemp += s.temperature * weight;
            }

            float temp = exactMatch ? exactTemp : (sumWeights > 0.0f ? sumWeightedTemp / sumWeights : 20.0f);
            QColor color = getTemperatureColor(temp);
            lowResHeatmap.setPixelColor(gx, gy, color);
        }
    }

    m_heatmapImage = QImage(size(), QImage::Format_ARGB32);
    m_heatmapImage.fill(Qt::transparent);

    QPainter painter(&m_heatmapImage);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::SmoothPixmapTransform); // Habilita interpolação bilinear automática

    // Define o caminho de recorte cilíndrico do silo
    QPainterPath grainPath;
    const float w = width();
    const float h = height();
    const float bodyLeft = w * 0.26f;
    const float bodyRight = w * 0.74f;
    const float bodyTop = h * 0.18f;
    const float bodyBottomY = h * 0.83f;
    const float baseEllipseHeight = h * 0.08f;
    QRectF baseEllipseRect(bodyLeft, bodyBottomY - baseEllipseHeight / 2, bodyRight - bodyLeft, baseEllipseHeight);

    grainPath.moveTo(bodyLeft, bodyBottomY);
    grainPath.lineTo(bodyLeft, bodyTop + h * 0.05);
    grainPath.cubicTo(bodyLeft + w * 0.1, bodyTop, bodyRight - w * 0.1, bodyTop, bodyRight, bodyTop + h * 0.05);
    grainPath.lineTo(bodyRight, bodyBottomY);
    grainPath.arcTo(baseEllipseRect, 0, -180);
    grainPath.closeSubpath();

    painter.setClipPath(grainPath, Qt::ReplaceClip);

    // Desenha o heatmap cobrindo toda a área do silo
    QRectF drawRect(bodyLeft, bodyTop, bodyRight - bodyLeft, h * 0.88f - bodyTop);
    painter.drawImage(drawRect, lowResHeatmap);

    painter.setClipping(false);
}

QColor Silo2DWidget::getTemperatureColor(float temp) {
    return SensorWidget::getTemperatureColor(temp);
}

void Silo2DWidget::drawSensor(QPainter& painter, const QPointF& pos, float temp, bool showValue) {
    painter.save();
    // Nova cor de alto contraste para os sensores, conforme solicitado.
    QColor color(255, 140, 0); // Laranja
    painter.setBrush(color);
    painter.setPen(Qt::white);
    painter.drawEllipse(pos, 4, 4);

    if (showValue) {
        QRectF textRect(pos.x() + 8, pos.y() - 10, 40, 20);
        painter.setFont(QFont("Arial", 8, QFont::Bold));
        painter.setBrush(QColor(0, 0, 0, 150));
        painter.setPen(Qt::NoPen);
        painter.drawRoundedRect(textRect.adjusted(-2, -1, 2, 1), 3, 3);
        painter.setPen(color.lighter(150));
        painter.drawText(textRect, Qt::AlignCenter, QString::number(temp, 'f', 1));
    }
    painter.restore();
}

void Silo2DWidget::drawMotorIndicator(QPainter& painter, const QPointF& center, float radius, bool isOn) {
    painter.save();
    QColor color = isOn ? QColor(34, 197, 94) : QColor(239, 68, 68); // Verde / Vermelho

    // Adiciona um brilho para melhor visibilidade
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(color.red(), color.green(), color.blue(), 80));
    painter.drawEllipse(center, radius * 1.6, radius * 1.6);

    // Círculo principal
    painter.setBrush(color);
    painter.drawEllipse(center, radius, radius);

    painter.restore();
}