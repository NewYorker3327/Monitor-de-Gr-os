#ifndef SILOGLWIDGET_H
#define SILOGLWIDGET_H

#include <QOpenGLWidget>
#include <QOpenGLFunctions>
#include <QMatrix4x4>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QKeyEvent>
#include <QVector3D>
#include <vector>

class SiloGLWidget : public QOpenGLWidget, protected QOpenGLFunctions
{
    Q_OBJECT

public:
    explicit SiloGLWidget(QWidget *parent = nullptr);
    ~SiloGLWidget();

public slots:
    // Slots de interface e visualização
    void toggleRoof() { m_showRoof = !m_showRoof; update(); }
    void toggleWireframe() { m_wireframe = !m_wireframe; update(); }
    void toggleSensors() { m_showSensors = !m_showSensors; update(); }
    void toggleValues() { m_showValues = !m_showValues; emit showValuesChanged(m_showValues); update(); }
    void setTheme1() { m_currentTheme = 1; update(); }
    void setTheme2() { m_currentTheme = 2; update(); }
    void setTheme3() { m_currentTheme = 3; update(); }
    void toggleHeatmap() { m_showHeatmap = !m_showHeatmap; update(); }
    void resetCamera() { m_xRot = 20.0f; m_yRot = -45.0f; m_xPan = 0.0f; m_yPan = -4.0f; m_zoom = -25.0f; update(); }
    void cycleOpacity() { 
        m_wallOpacity -= 0.10f; // Passos mais suaves
        if (m_wallOpacity < 0.05f) m_wallOpacity = 1.0f; // Mínimo bem mais transparente
        update(); 
    }
    void cycleHeatmapDensity();

    // Slots de controle (Teclado e Zoom) adicionados
    void setKeyboardEnabled(bool enabled);
    void zoomIn();
    void zoomOut();

    // Slots de configuração da aba Opções
    void showTemperatures(bool show);

    // Função para sincronizar temperaturas com a grade
    void setSensorTemperatures(const QVector<float>& temperatures);
    void updateSensorPositions(const int mapping[5][13]);

    // Slots para sliders de opacidade e densidade
    void setOpacity(float opacity) { m_wallOpacity = opacity; update(); }
    void setHeatmapDensitySlider(int density) { m_heatmapDensity = density; update(); }
    void setShowCompass(bool show) { m_showCompass = show; update(); }
    void setCurrentToolBoxPage(int page) { m_currentToolBoxPage = page; }
    void setPendulumValueVisible(int pendulumId, bool visible);
    bool isPendulumValueVisible(int pendulumId) const;

signals:
    void showValuesChanged(bool show);

private:
    int m_currentToolBoxPage; // Página atual do toolBox (0-3)

protected:
    void initializeGL() override;
    void resizeGL(int w, int h) override;
    void paintGL() override;

    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;

private:
    // Navegação da Câmera
    float m_xRot, m_yRot;
    float m_xPan, m_yPan;
    float m_zoom;
    QPoint m_lastPos;
    
    // Configurações Visuais
    bool m_showRoof;
    bool m_wireframe;
    bool m_showSensors;
    bool m_showValues;
    bool m_showHeatmap;
    bool m_showPendulumValues[5];
    bool m_keyboardEnabled;
    bool m_showCompass;
    float m_wallOpacity;
    int m_heatmapDensity; // 1 a 6
    int m_fontSizeIndex; // 0=Padrão, 1=Média, 2=Gigante
    int m_currentTheme; // 1=Padrão, 2=Alto Contraste, 3=Rural
    
    // Dados do Silo
    struct Sensor {
        QVector3D pos;
        float temperature;
        QString name;
    };
    std::vector<Sensor> m_sensors;
    QVector2D m_pendulum_positions[5];
    int m_hoveredSensor;

    struct GrainPoint {
        QVector3D pos;
        QColor color;
    };
    std::vector<GrainPoint> m_pointCloud;

    // Funções auxiliares de desenho e cálculo
    void generatePointCloud();
    void updatePointCloudColors();
    QColor getTemperatureColor(float temp);
    void drawCylinder(float radius, float height, int slices);
    void drawCone(float radius, float height, int slices);
    void drawSphere(float radius, int slices, int stacks);
    void drawCompass();
    void drawCompass3D();
    QVector3D projectToScreen(const QVector3D &worldPos, const QMatrix4x4 &mvp, int width, int height);
};

#endif // SILOGLWIDGET_H