#include "siloglwidget.h"
#include "sensorwidget.h"
#include <QPainter>
#include <QRandomGenerator>
#include <QtMath>
#include <QDateTime>
#include <QCursor>
#include <QVector2D>
#include <vector>

SiloGLWidget::SiloGLWidget(QWidget *parent)
    : QOpenGLWidget(parent),
      m_xRot(20.0f), m_yRot(-45.0f), m_xPan(0.0f), m_yPan(-4.0f), m_zoom(-25.0f),
      m_showRoof(true), m_wireframe(false), m_showSensors(true), m_showValues(true),
      m_showHeatmap(false), m_keyboardEnabled(true), m_showCompass(false), m_wallOpacity(0.4f), m_heatmapDensity(6),
      m_fontSizeIndex(0), m_currentTheme(1), m_hoveredSensor(-1), m_currentToolBoxPage(-1)
{
    for (int i = 0; i < 5; ++i) {
        m_showPendulumValues[i] = true;
    }
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setMouseTracking(true); 
    setFocusPolicy(Qt::StrongFocus); 

    // Define as posições XZ dos pêndulos.
    // O layout em '+' é mais organizado e profissional. A distância foi ajustada
    // para que os pêndulos fiquem bem distribuídos, mas não muito perto da parede.
    // A ordem foi padronizada para: Centro, Norte, Leste, Sul, Oeste.
    const float pendulum_dist = 1.8f;
    m_pendulum_positions[0] = QVector2D(0.0f, 0.0f);           // Pêndulo 0: Centro
    m_pendulum_positions[1] = QVector2D(0.0f, pendulum_dist);  // Pêndulo 1: Norte (Z+)
    m_pendulum_positions[2] = QVector2D(pendulum_dist, 0.0f);  // Pêndulo 2: Leste (X+)
    m_pendulum_positions[3] = QVector2D(0.0f, -pendulum_dist); // Pêndulo 3: Sul (Z-)
    m_pendulum_positions[4] = QVector2D(-pendulum_dist, 0.0f); // Pêndulo 4: Oeste (X-)

    for (int p = 0; p < 5; ++p) {
        for (int i = 0; i < 13; ++i) { 
            Sensor s;
            // A posição Y foi invertida para garantir consistência com as visualizações 2D e Grade.
            // Agora, o sensor de índice 0 fica no topo (Y=8.0) e o de índice 12 na base (Y=0.0).
            float y_pos = 8.0f - ((8.0f / 12.0f) * i);
            s.pos = QVector3D(m_pendulum_positions[p].x(), y_pos, m_pendulum_positions[p].y());
            s.name = QString("G1-N%1-S%2").arg(p + 1).arg(i + 1); 
            s.temperature = 0.0f;
            m_sensors.push_back(s);
        }
    }
    
    generatePointCloud();
}

SiloGLWidget::~SiloGLWidget() {}

void SiloGLWidget::initializeGL() {
    initializeOpenGLFunctions();
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

void SiloGLWidget::resizeGL(int w, int h) {
    qreal ratio = devicePixelRatio();
    glViewport(0, 0, w * ratio, h * ratio);
}

void SiloGLWidget::paintGL() {
    QColor bgColor;
    if (m_currentTheme == 2) bgColor = QColor(0, 0, 0); 
    else if (m_currentTheme == 3) {
        int hour = QTime::currentTime().hour();
        // Corrige a lógica para que o modo noturno comece às 18:00.
        // Dia: das 6:00 às 17:59. Noite: das 18:00 às 5:59.
        bgColor = (hour >= 6 && hour < 18) ? QColor(135, 206, 235) : QColor(10, 10, 30); 
    } else bgColor = QColor(40, 40, 40); 
    
    glClearColor(bgColor.redF(), bgColor.greenF(), bgColor.blueF(), 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing); // Suaviza as bordas das caixas 2D
    painter.beginNativePainting();

    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE); 

    QMatrix4x4 projection, view, model;
    projection.perspective(45.0f, float(width()) / height(), 0.1f, 100.0f);
    view.translate(m_xPan, m_yPan, m_zoom);
    view.rotate(m_xRot, 1.0f, 0.0f, 0.0f);
    view.rotate(m_yRot, 0.0f, 1.0f, 0.0f);

    glMatrixMode(GL_PROJECTION); glLoadMatrixf(projection.constData());
    glMatrixMode(GL_MODELVIEW); glLoadMatrixf(view.constData());

    // 1. CHÃO
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL); 
    if (m_currentTheme == 3) { 
        glColor3f(0.2f, 0.5f, 0.2f);
        glBegin(GL_QUADS);
        glVertex3f(-20.0f, 0.0f, -20.0f); glVertex3f(20.0f, 0.0f, -20.0f);
        glVertex3f(20.0f, 0.0f, 20.0f); glVertex3f(-20.0f, 0.0f, 20.0f);
        glEnd();
        glColor3f(0.5f, 0.5f, 0.5f);
        drawCylinder(3.2f, 0.1f, 30);
    }

    // 2. SENSORES E CABOS (desenhados antes das paredes para aparecer através da transparência)
    if (m_showSensors) {
        glColor4f(0.3f, 0.3f, 0.3f, 1.0f);
        for (int c = 0; c < 5; ++c) {
            glPushMatrix();
            // Usa as posições de pêndulo definidas no construtor para desenhar os cabos.
            glTranslatef(m_pendulum_positions[c].x(), 0.0f, m_pendulum_positions[c].y());
            drawCylinder(0.05f, 8.0f, 10);
            glPopMatrix();
        }

        for (size_t i = 0; i < m_sensors.size(); ++i) {
            QColor sc = getTemperatureColor(m_sensors[i].temperature);
            glColor4f(sc.redF(), sc.greenF(), sc.blueF(), 1.0f);
            glPushMatrix();
            glTranslatef(m_sensors[i].pos.x(), m_sensors[i].pos.y(), m_sensors[i].pos.z());
            drawSphere((m_hoveredSensor == (int)i) ? 0.25f : 0.15f, 10, 10);
            glPopMatrix();
        }
    }

    // 3. PAREDES E TETO TRANSPARENTES
    // Desabilitar depth test para permitir transparência correcta
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    if (m_wireframe) glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    else glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

    // Desenha paredes com a transparência configurada
    QColor wallColor = (m_currentTheme == 2) ? QColor(0, 255, 0, m_wallOpacity * 255) : QColor(200, 200, 210, m_wallOpacity * 255);
    glColor4f(wallColor.redF(), wallColor.greenF(), wallColor.blueF(), wallColor.alphaF());
    drawCylinder(3.0f, 8.0f, 30);

    if (m_wallOpacity <= 0.2f && !m_wireframe) {
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
        glColor4f(1.0f, 1.0f, 1.0f, 0.15f);
        drawCylinder(3.0f, 8.0f, 30);
    }

    if (m_showRoof) {
        if (!m_wireframe) glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        glPushMatrix(); glTranslatef(0.0f, 8.0f, 0.0f); drawCone(3.2f, 2.0f, 30); glPopMatrix();
    }

    // 3.5. ROSA DOS VENTOS (COMPASS) - apenas desenho 3D
    if (m_showCompass) {
        drawCompass3D();
    }

    // 4. HEATMAP - desenhado por último no espaço 3D (através das paredes transparentes)
    if (m_showHeatmap && !m_pointCloud.empty()) {
        glPointSize(2.0f);
        glBegin(GL_POINTS);
        int step = 7 - m_heatmapDensity;
        for (size_t i = 0; i < m_pointCloud.size(); i += step) {
            glColor4f(m_pointCloud[i].color.redF(), m_pointCloud[i].color.greenF(), m_pointCloud[i].color.blueF(), 0.6f);
            glVertex3f(m_pointCloud[i].pos.x(), m_pointCloud[i].pos.y(), m_pointCloud[i].pos.z());
        }
        glEnd();
    }

    // Restaurar estados do OpenGL para o QPainter
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);

    // =================================================================
    // PREPARAÇÃO CRÍTICA PARA O QPAINTER FUNCIONAR
    // =================================================================
    painter.endNativePainting();

    // Criar novo QPainter para 2D após OpenGL
    QPainter painter2D(this);
    painter2D.setRenderHint(QPainter::Antialiasing);

    // =================================================================
    // 5. CAMADA 2D: TEXTOS DE ALTO CONTRASTE
    // IMPORTANTE: Garantir que o painter possa desenhar corretamente
    // =================================================================

    QMatrix4x4 mvp = projection * view * model;
    int w = width();
    int h = height();

    QFont font = painter2D.font();
    font.setPointSize(10 + (m_fontSizeIndex * 4));
    font.setBold(true);
    painter2D.setFont(font);
    painter2D.setPen(Qt::white); // Cor padrão para legibilidade

    QPoint mousePos = this->mapFromGlobal(QCursor::pos());
    m_hoveredSensor = -1;

    struct LabelData { int px; int py; size_t index; };
    std::vector<LabelData> onScreenLabels;

    for (size_t i = 0; i < m_sensors.size(); ++i) {
        QVector3D screenPos = projectToScreen(m_sensors[i].pos, mvp, w, h);
        if (screenPos.z() >= -1.0f && screenPos.z() <= 1.0f) {
            int px = screenPos.x();
            int py = screenPos.y();
            onScreenLabels.push_back({px, py, i});

            int dx = px - mousePos.x();
            int dy = py - mousePos.y();
            if (qSqrt(dx * dx + dy * dy) < 25) {
                m_hoveredSensor = i;
            }
        }
    }

    // Desenhar valores de temperatura dos sensores (se m_showValues estiver ativo)
    if (m_showSensors && m_showValues) {
        painter2D.setFont(QFont("Segoe UI", 9, QFont::Bold));
        for (const auto& lbl : onScreenLabels) {
            int pendulumId = lbl.index / 13;
            if (pendulumId >= 0 && pendulumId < 5 && !m_showPendulumValues[pendulumId]) continue;
            if (m_hoveredSensor == (int)lbl.index) continue;

            QString tempText = QString::number(m_sensors[lbl.index].temperature, 'f', 1) + "°C";

            QRectF badgeRect(lbl.px + 10, lbl.py - 10, 46, 20);
            painter2D.setBrush(QColor(0, 0, 0, 180));
            painter2D.setPen(Qt::NoPen);
            painter2D.drawRoundedRect(badgeRect, 3, 3);

            QColor txtColor = (m_currentTheme == 2) ? Qt::green : QColor(200, 255, 200);
            painter2D.setPen(txtColor);
            painter2D.drawText(badgeRect, Qt::AlignCenter, tempText);
        }
    }

    // Desenhar tooltip do sensor hovered (sempre no topo, com fundo semi-transparente)
    if (m_hoveredSensor != -1) {
        int pendulumId = m_hoveredSensor / 13;
        bool isValVisible = (pendulumId >= 0 && pendulumId < 5) ? m_showPendulumValues[pendulumId] : true;
        if (isValVisible) {
            int px = 0, py = 0;
            for (const auto& lbl : onScreenLabels) {
                if ((int)lbl.index == m_hoveredSensor) { px = lbl.px; py = lbl.py; break; }
            }

            QString tempText = QString::number(m_sensors[m_hoveredSensor].temperature, 'f', 1);
            QString status = m_sensors[m_hoveredSensor].temperature > 26.0 ? "ALERTA" : "OK";
            QColor borderColor = m_sensors[m_hoveredSensor].temperature > 26.0 ? Qt::red : Qt::green;

            // Fundo do tooltip
            QRectF bgRect(px + 12, py - 68, 115, 62);
            painter2D.setBrush(QColor(15, 15, 25, 250));
            painter2D.setPen(QPen(borderColor, 2));
            painter2D.drawRoundedRect(bgRect, 6, 6);

            // Nome do sensor
            painter2D.setPen(Qt::white);
            painter2D.setFont(QFont("Segoe UI", 10, QFont::Bold));
            painter2D.drawText(QRectF(px + 18, py - 62, 100, 16), m_sensors[m_hoveredSensor].name);

            // Temperatura grande
            painter2D.setFont(QFont("Segoe UI", 14, QFont::Bold));
            painter2D.setPen(borderColor);
            painter2D.drawText(QRectF(px + 18, py - 44, 100, 20), tempText + " °C");

        // Status
        painter2D.setFont(QFont("Segoe UI", 9));
        painter2D.setPen(borderColor);
        painter2D.drawText(QRectF(px + 18, py - 22, 100, 16), status);
        }
    }

    // Rosa dos ventos 2D no canto inferior direito
    if (m_showCompass) {
        drawCompass();
    }
}

void SiloGLWidget::mousePressEvent(QMouseEvent *event) { m_lastPos = event->pos(); }

void SiloGLWidget::mouseMoveEvent(QMouseEvent *event) {
    int dx = event->position().toPoint().x() - m_lastPos.x(); 
    int dy = event->position().toPoint().y() - m_lastPos.y();
    if (event->buttons() & Qt::LeftButton) { m_xRot += 0.5f * dy; m_yRot += 0.5f * dx; }
    else if (event->buttons() & Qt::MiddleButton) { m_xPan += 0.01f * dx; m_yPan -= 0.01f * dy; }
    m_lastPos = event->pos();
    update(); 
}

void SiloGLWidget::wheelEvent(QWheelEvent *event) {
    m_zoom += (event->angleDelta().y() > 0) ? 2.0f : -2.0f; 
    update();
}

void SiloGLWidget::keyPressEvent(QKeyEvent *event) {
    if (!m_keyboardEnabled) { QOpenGLWidget::keyPressEvent(event); return; }
    switch (event->key()) {
        case Qt::Key_1: setTheme1(); break;
        case Qt::Key_2: setTheme2(); break;
        case Qt::Key_3: setTheme3(); break;
        case Qt::Key_R: toggleRoof(); break;
        case Qt::Key_W: toggleWireframe(); break;
        case Qt::Key_S: toggleSensors(); break;
        case Qt::Key_N: toggleValues(); break;
        case Qt::Key_H: toggleHeatmap(); break;
        case Qt::Key_C: resetCamera(); break;
        case Qt::Key_T: cycleOpacity(); break;
        case Qt::Key_G: cycleHeatmapDensity(); break;
    }
}

void SiloGLWidget::cycleHeatmapDensity() { 
    m_heatmapDensity++; 
    if (m_heatmapDensity > 6) m_heatmapDensity = 1; 
    update(); 
}

void SiloGLWidget::setSensorTemperatures(const QVector<float>& temperatures) {
    for (size_t i = 0; i < m_sensors.size() && i < (size_t)temperatures.size(); ++i) {
        m_sensors[i].temperature = temperatures[i];
    }
    updatePointCloudColors();
    update();
}

void SiloGLWidget::updateSensorPositions(const int mapping[5][13]) {
    for (int p = 0; p < 5; ++p) {
        for (int i = 0; i < 13; ++i) {
            int idx = p * 13 + i;
            if (idx < (int)m_sensors.size()) {
                int pos = mapping[p][i];
                float y_pos = 8.0f - ((8.0f / 12.0f) * (pos - 1));
                m_sensors[idx].pos.setY(y_pos);
            }
        }
    }
    updatePointCloudColors();
    update();
}

void SiloGLWidget::generatePointCloud() {
    m_pointCloud.clear();
    const float siloRadius = 3.0f;
    const float siloHeight = 8.0f; // Altura total do cilindro do silo

    // Gera pontos até atingir a densidade desejada, preenchendo todo o cilindro.
    for (int i = 0; i < 15000; ++i) {
        // Gera um ponto candidato dentro do cilindro.
        // A multiplicação por qSqrt() garante uma distribuição uniforme dos pontos no disco,
        // em vez de se concentrarem no centro.
        float r = siloRadius * qSqrt(QRandomGenerator::global()->generateDouble());
        float theta = QRandomGenerator::global()->generateDouble() * 2.0 * M_PI;
        float y = QRandomGenerator::global()->generateDouble() * siloHeight; // Y vai de 0 até o topo do silo

        GrainPoint gp;
        gp.pos = QVector3D(r * qCos(theta), y, r * qSin(theta));
        m_pointCloud.push_back(gp);
    }
    updatePointCloudColors();
}

void SiloGLWidget::updatePointCloudColors() {
    for (auto &p : m_pointCloud) {
        float closestTemp = 22.0f; float minDist = 999.0f;
        for (const auto &s : m_sensors) {
            float d = p.pos.distanceToPoint(s.pos);
            if (d < minDist) { minDist = d; closestTemp = s.temperature; }
        }
        p.color = getTemperatureColor(closestTemp);
    }
}

QColor SiloGLWidget::getTemperatureColor(float temp) {
    return SensorWidget::getTemperatureColor(temp);
}

QVector3D SiloGLWidget::projectToScreen(const QVector3D &worldPos, const QMatrix4x4 &mvp, int width, int height) {
    QVector4D clipPos = mvp * QVector4D(worldPos, 1.0f); if (clipPos.w() == 0.0f) return QVector3D();
    QVector3D ndc = clipPos.toVector3DAffine(); return QVector3D((ndc.x() + 1.0f) * 0.5f * width, (1.0f - ndc.y()) * 0.5f * height, clipPos.z() / clipPos.w());
}

void SiloGLWidget::drawCylinder(float radius, float height, int slices) {
    glBegin(GL_QUAD_STRIP);
    for (int i = 0; i <= slices; i++) {
        float theta = (i == slices ? 0 : i) * 2.0f * M_PI / slices;
        float x = radius * qCos(theta), z = radius * qSin(theta);
        glVertex3f(x, height, z); glVertex3f(x, 0.0f, z);
    } glEnd();
}

void SiloGLWidget::drawCone(float radius, float height, int slices) {
    glBegin(GL_TRIANGLE_FAN); glVertex3f(0.0f, height, 0.0f);
    for (int i = 0; i <= slices; i++) {
        float theta = (i == slices ? 0 : i) * 2.0f * M_PI / slices; glVertex3f(radius * qCos(theta), 0.0f, radius * qSin(theta));
    } glEnd();
}

void SiloGLWidget::drawSphere(float radius, int slices, int stacks) {
    for (int i = 0; i < stacks; i++) {
        float lat0 = M_PI * (-0.5f + (float)(i) / stacks); float z0  = radius * qSin(lat0), zr0 =  radius * qCos(lat0);
        float lat1 = M_PI * (-0.5f + (float)(i+1) / stacks); float z1 = radius * qSin(lat1), zr1 = radius * qCos(lat1);
        glBegin(GL_QUAD_STRIP);
        for (int j = 0; j <= slices; j++) {
            float lng = 2 * M_PI * (float)(j - 1) / slices; float x = qCos(lng), y = qSin(lng);
            glVertex3f(x * zr0, y * zr0, z0); glVertex3f(x * zr1, y * zr1, z1);
        } glEnd();
    }
}

void SiloGLWidget::drawCompass3D() {
    // Desenha rosa dos ventos no chão, afastada e com pétalas menores para não intersectar o silo.
    float compassY = 0.15f;
    float outerRadius = 4.5f; // Afasta do silo (cujo raio da base é ~3.2)
    float petalLength = 1.0f; // Comprimento da pétala
    float petalWidthAngle = 40.0f; // Largura angular da base da pétala

    glDepthMask(GL_FALSE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    auto drawPetal = [=](float angleDeg, const QColor& color) {
        glColor4f(color.redF(), color.greenF(), color.blueF(), 0.85f);

        float baseRadius = outerRadius - petalLength;
        float angleRad = qDegreesToRadians(angleDeg);
        float angleOffsetRad = qDegreesToRadians(petalWidthAngle / 2.0f);

        glBegin(GL_TRIANGLES);
        // Ponta da pétala
        glVertex3f(outerRadius * qCos(angleRad), compassY, outerRadius * qSin(angleRad));
        // Pontos da base
        glVertex3f(baseRadius * qCos(angleRad - angleOffsetRad), compassY, baseRadius * qSin(angleRad - angleOffsetRad));
        glVertex3f(baseRadius * qCos(angleRad + angleOffsetRad), compassY, baseRadius * qSin(angleRad + angleOffsetRad));
        glEnd();
    };

    drawPetal(90.0f, QColor(255, 50, 50));    // N (Z+)
    drawPetal(270.0f, QColor(50, 100, 255));   // S (Z-)
    drawPetal(0.0f, QColor(50, 200, 80));      // L (Leste, X+)
    drawPetal(180.0f, QColor(255, 220, 50));  // O (Oeste, X-)

    // Círculo central
    glColor4f(1.0f, 1.0f, 1.0f, 0.5f);
    glBegin(GL_LINE_LOOP);
    for (int i = 0; i < 32; i++) {
        float angle = i * 2.0f * M_PI / 32;
        // Desenha o círculo onde a base das pétalas começa
        float baseRadius = outerRadius - petalLength;
        glVertex3f(baseRadius * qCos(angle), compassY, baseRadius * qSin(angle));
    }
    glEnd();

    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
}

void SiloGLWidget::drawCompass() {
    // Bússola 2D interativa que gira com a câmera.
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    int cx = width() - 55;
    int cy = height() - 55;
    int r = 28;

    // --- 1. Desenha o fundo e o anel estático ---
    painter.setBrush(QColor(10, 10, 20, 200));
    painter.setPen(QPen(QColor(80, 80, 80), 1));
    painter.drawEllipse(cx - r, cy - r, r * 2, r * 2);

    // --- 2. Desenha a agulha rotativa ---
    painter.save();
    painter.translate(cx, cy);
    painter.rotate(-m_yRot); // Gira o sistema de coordenadas com base na rotação da câmera

    // Parte Norte da agulha (vermelha)
    painter.setPen(Qt::NoPen);
    painter.setBrush(Qt::red);
    QPointF northNeedle[] = { QPointF(0, -r + 4), QPointF(-5, 0), QPointF(5, 0) };
    painter.drawPolygon(northNeedle, 3);

    // Parte Sul da agulha (branca/azul)
    painter.setBrush(QColor(200, 200, 255));
    QPointF southNeedle[] = { QPointF(0, r - 4), QPointF(-5, 0), QPointF(5, 0) };
    painter.drawPolygon(southNeedle, 3);

    painter.restore(); // Restaura o sistema de coordenadas

    // --- 3. Desenha as letras estáticas por cima ---
    painter.setPen(Qt::white);
    painter.setFont(QFont("Segoe UI", 7, QFont::Bold));
    painter.drawText(cx - 4, cy - r + 12, "N");
    painter.drawText(cx - 3, cy + r - 4, "S");
    painter.drawText(cx - r + 6, cy + 4, "O"); // Oeste
    painter.drawText(cx + r - 10, cy + 4, "L"); // Leste
}

// =================================================================
// IMPLEMENTAÇÃO DOS MÉTODOS DE CONTROLE (TECLADO E ZOOM)
// =================================================================

void SiloGLWidget::setKeyboardEnabled(bool enabled) {
    m_keyboardEnabled = enabled;
}

// ================================================
// IMPLEMENTAÇÃO DE EXIBIÇÃO DE TEMPERATURA (ABA OPÇÕES)
// ================================================
void SiloGLWidget::showTemperatures(bool show) {
    m_showValues = show;
    emit showValuesChanged(show);
    update();
}

void SiloGLWidget::zoomIn() {
    m_zoom += 2.0f;
    update();
}

void SiloGLWidget::zoomOut() {
    m_zoom -= 2.0f;
    update();
}

void SiloGLWidget::setPendulumValueVisible(int pendulumId, bool visible) {
    if (pendulumId >= 0 && pendulumId < 5) {
        m_showPendulumValues[pendulumId] = visible;
        update();
    }
}

bool SiloGLWidget::isPendulumValueVisible(int pendulumId) const {
    if (pendulumId >= 0 && pendulumId < 5) {
        return m_showPendulumValues[pendulumId];
    }
    return false;
}