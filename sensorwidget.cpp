#include "sensorwidget.h"
#include <QMouseEvent>
#include <QPainter>

int SensorWidget::s_activePalette = 0; // 0 = Ironbow por padrão

QColor SensorWidget::getTemperatureColor(double temp) {
    // 1. Clampa a temperatura na faixa [15.0, 40.0]
    if (temp < 15.0) temp = 15.0;
    if (temp > 40.0) temp = 40.0;

    // 2. Arredonda para o degrau mais próximo de 0.5 graus
    double roundedTemp = qRound(temp * 2.0) / 2.0;

    // 3. Normaliza o valor no intervalo [0.0, 1.0] para a faixa 15°C a 40°C
    double val = (roundedTemp - 15.0) / 25.0;

    struct RGB { double r, g, b; };

    // Escolhe a paleta com base na seleção
    if (s_activePalette == 1) { // 1. Paleta Semáforo (RYG: Verde -> Amarelo -> Vermelho)
        const int NUM_COLORS = 3;
        const RGB controlColors[NUM_COLORS] = {
            {  34.0, 197.0,  94.0 }, // 0.0: Verde (#22c55e)
            { 234.0, 179.0,   8.0 }, // 0.5: Amarelo (#eab308)
            { 239.0,  68.0,  68.0 }  // 1.0: Vermelho (#ef4444)
        };
        double scaledVal = val * (NUM_COLORS - 1);
        int idx = static_cast<int>(scaledVal);
        if (idx >= NUM_COLORS - 1) idx = NUM_COLORS - 2;
        double t = scaledVal - idx;
        const RGB& c0 = controlColors[idx];
        const RGB& c1 = controlColors[idx + 1];
        int r = static_cast<int>(c0.r + t * (c1.r - c0.r) + 0.5);
        int g = static_cast<int>(c0.g + t * (c1.g - c0.g) + 0.5);
        int b = static_cast<int>(c0.b + t * (c1.b - c0.b) + 0.5);
        return QColor(r, g, b);

    } else if (s_activePalette == 2) { // 2. Paleta Divergente (Azul -> Cinza/Branco -> Vermelho)
        const int NUM_COLORS = 3;
        const RGB controlColors[NUM_COLORS] = {
            {  29.0,  78.0, 216.0 }, // 0.0: Azul escuro (#1d4ed8)
            { 241.0, 245.0, 249.0 }, // 0.5: Cinza claro (#f1f5f9)
            { 185.0,  28.0,  28.0 }  // 1.0: Vermelho escuro (#b91c1c)
        };
        double scaledVal = val * (NUM_COLORS - 1);
        int idx = static_cast<int>(scaledVal);
        if (idx >= NUM_COLORS - 1) idx = NUM_COLORS - 2;
        double t = scaledVal - idx;
        const RGB& c0 = controlColors[idx];
        const RGB& c1 = controlColors[idx + 1];
        int r = static_cast<int>(c0.r + t * (c1.r - c0.r) + 0.5);
        int g = static_cast<int>(c0.g + t * (c1.g - c0.g) + 0.5);
        int b = static_cast<int>(c0.b + t * (c1.b - c0.b) + 0.5);
        return QColor(r, g, b);

    } else if (s_activePalette == 3) { // 3. Paleta Viridis (CVD-Friendly)
        const int NUM_COLORS = 5;
        const RGB controlColors[NUM_COLORS] = {
            {  68.0,   1.0,  84.0 }, // 0.0: Roxo escuro (#440154)
            {  49.0, 104.0, 142.0 }, // 0.25: Azul/Teal (#31688e)
            {  53.0, 183.0, 121.0 }, // 0.50: Verde (#35b779)
            { 143.0, 215.0,  68.0 }, // 0.75: Verde claro (#8fd744)
            { 253.0, 231.0,  37.0 }  // 1.0: Amarelo (#fde725)
        };
        double scaledVal = val * (NUM_COLORS - 1);
        int idx = static_cast<int>(scaledVal);
        if (idx >= NUM_COLORS - 1) idx = NUM_COLORS - 2;
        double t = scaledVal - idx;
        const RGB& c0 = controlColors[idx];
        const RGB& c1 = controlColors[idx + 1];
        int r = static_cast<int>(c0.r + t * (c1.r - c0.r) + 0.5);
        int g = static_cast<int>(c0.g + t * (c1.g - c0.g) + 0.5);
        int b = static_cast<int>(c0.b + t * (c1.b - c0.b) + 0.5);
        return QColor(r, g, b);

    } else if (s_activePalette == 4) { // 4. Paleta Normal (Azul -> Amarelo -> Vermelho)
        const int NUM_COLORS = 3;
        const RGB controlColors[NUM_COLORS] = {
            {  29.0,  78.0, 216.0 }, // 0.0: Azul (#1d4ed8)
            { 234.0, 179.0,   8.0 }, // 0.5: Amarelo (#eab308)
            { 239.0,  68.0,  68.0 }  // 1.0: Vermelho (#ef4444)
        };
        double scaledVal = val * (NUM_COLORS - 1);
        int idx = static_cast<int>(scaledVal);
        if (idx >= NUM_COLORS - 1) idx = NUM_COLORS - 2;
        double t = scaledVal - idx;
        const RGB& c0 = controlColors[idx];
        const RGB& c1 = controlColors[idx + 1];
        int r = static_cast<int>(c0.r + t * (c1.r - c0.r) + 0.5);
        int g = static_cast<int>(c0.g + t * (c1.g - c0.g) + 0.5);
        int b = static_cast<int>(c0.b + t * (c1.b - c0.b) + 0.5);
        return QColor(r, g, b);

    } else { // 0. Paleta Ironbow (Padrão)
        const int NUM_COLORS = 6;
        const RGB controlColors[NUM_COLORS] = {
            {   5.0,   5.0,  40.0 }, // 0.0: Preto/Azul escuro
            {  59.0,   0.0, 102.0 }, // 0.2: Roxo escuro
            { 158.0,   0.0,  89.0 }, // 0.4: Magenta/Rubi
            { 230.0,  57.0,  23.0 }, // 0.6: Vermelho/Laranja brilhante
            { 253.0, 187.0,   0.0 }, // 0.8: Amarelo/Âmbar
            { 255.0, 255.0, 255.0 }  // 1.0: Branco
        };
        double scaledVal = val * (NUM_COLORS - 1);
        int idx = static_cast<int>(scaledVal);
        if (idx >= NUM_COLORS - 1) idx = NUM_COLORS - 2;
        double t = scaledVal - idx;
        const RGB& c0 = controlColors[idx];
        const RGB& c1 = controlColors[idx + 1];
        int r = static_cast<int>(c0.r + t * (c1.r - c0.r) + 0.5);
        int g = static_cast<int>(c0.g + t * (c1.g - c0.g) + 0.5);
        int b = static_cast<int>(c0.b + t * (c1.b - c0.b) + 0.5);
        return QColor(r, g, b);
    }
}

SensorWidget::SensorWidget(int gateway, int node, int sensor, QWidget *parent)
    : QFrame(parent)
    , m_gateway(gateway)
    , m_node(node)
    , m_sensor(sensor)
    , m_temperature(0.0)
    , m_online(false)
    , m_alert(false)
    , m_currentZoom(1.0) // Inicializa o zoom em 100%
{
    setupUI();
    setFrameStyle(QFrame::Box | QFrame::Plain);
    setLineWidth(2);
    setMinimumSize(120, 95);
    setMaximumSize(120, 95);
    updateStyle();
}

SensorWidget::~SensorWidget() {}

void SensorWidget::setupUI()
{
    m_layout = new QVBoxLayout(this);
    m_layout->setContentsMargins(5, 5, 5, 5);
    m_layout->setSpacing(2);

    m_idLabel = new QLabel(QString("G%1-N%2-S%3").arg(m_gateway).arg(m_node).arg(m_sensor));
    m_idLabel->setAlignment(Qt::AlignCenter);

    m_temperatureLabel = new QLabel("--°C");
    m_temperatureLabel->setAlignment(Qt::AlignCenter);

    m_statusLabel = new QLabel("Offline");
    m_statusLabel->setAlignment(Qt::AlignCenter);

    m_positionCombo = new QComboBox(this);
    for (int i = 1; i <= 13; ++i) {
        m_positionCombo->addItem(QString::number(i));
    }
    m_positionCombo->setToolTip(tr("Posição física do sensor no cabo (1 = Topo, 13 = Base)"));
    
    m_layout->addWidget(m_idLabel);
    m_layout->addWidget(m_temperatureLabel);
    m_layout->addWidget(m_statusLabel);
    m_layout->addWidget(m_positionCombo);

    connect(m_positionCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int index) {
        emit positionChanged(m_gateway, m_node, m_sensor, index + 1);
    });

    // Remove fixed size constraints - let layout manage size
    setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);
}

void SensorWidget::resizeEvent(QResizeEvent *event)
{
    QFrame::resizeEvent(event);
    // Scale font sizes based on actual widget size
    int minDim = qMin(width(), height());
    double scaleFactor = minDim / 120.0; // Base size is 120x95 (width is still 120)

    int titleSize = qMax(6, (int)(9 * scaleFactor));
    int tempSize = qMax(8, (int)(16 * scaleFactor));

    m_idLabel->setStyleSheet(QString("font-size: %1px;").arg(titleSize));
    if (m_alert) {
        m_temperatureLabel->setStyleSheet(QString("font-size: %1px; font-weight: bold; color: #ef4444;").arg(tempSize));
    } else if (m_online) {
        QColor tempColor = getTemperatureColor(m_temperature);
        m_temperatureLabel->setStyleSheet(QString("font-size: %1px; font-weight: bold; color: %2;").arg(tempSize).arg(tempColor.name()));
    } else {
        m_temperatureLabel->setStyleSheet(QString("font-size: %1px; font-weight: bold; color: #64748b;").arg(tempSize));
    }
    updateStyle();
}

void SensorWidget::setTemperature(double temp)
{
    m_temperature = temp;
    m_temperatureLabel->setText(QString::number(temp, 'f', 1) + "°C");
    updateStyle(); // Garante que a cor do texto e fundo se atualizem com a nova temperatura
}

void SensorWidget::setOnline(bool online)
{
    m_online = online;
    if(online) m_statusLabel->setText("Online");
    else m_statusLabel->setText("Offline");
    updateStyle();
}

void SensorWidget::setAlert(bool alert)
{
    m_alert = alert;
    if(alert) m_statusLabel->setText("ALERTA");
    updateStyle();
}

void SensorWidget::setPhysicalPosition(int pos)
{
    if (m_positionCombo) {
        m_positionCombo->blockSignals(true);
        m_positionCombo->setCurrentIndex(pos - 1);
        m_positionCombo->blockSignals(false);
    }
}

void SensorWidget::setZoom(double factor)
{
    m_currentZoom = factor;
    
    // Atualiza o tamanho físico do widget
    int newWidth = 120 * factor;
    int newHeight = 95 * factor;
    setMinimumSize(newWidth, newHeight);
    setMaximumSize(newWidth, newHeight);
    
    updateStyle(); 
}

void SensorWidget::updateStyle()
{
    // O texto cresce e diminui proporcionalmente ao Zoom
    int titleSize = qMax(6, (int)(9 * m_currentZoom));
    int tempSize = qMax(8, (int)(16 * m_currentZoom));

    QColor tempColor = getTemperatureColor(m_temperature);
    QString tempColorHex = tempColor.name();

    if (m_positionCombo) {
        m_positionCombo->setStyleSheet(QString(
            "QComboBox { "
            "  background-color: #1e293b; "
            "  color: #f1f5f9; "
            "  border: 1px solid #475569; "
            "  border-radius: 3px; "
            "  padding: 1px 3px; "
            "  font-size: %1px; "
            "} "
            "QComboBox::drop-down { "
            "  border: none; "
            "}"
        ).arg(titleSize));
    }

    if (m_alert) {
        m_statusLabel->setStyleSheet(QString("font-size: %1px; color: #ef4444; font-weight: bold;").arg(titleSize));
        m_temperatureLabel->setStyleSheet(QString("font-size: %1px; font-weight: bold; color: #ef4444;").arg(tempSize));
        setStyleSheet("SensorWidget { border-color: #ef4444; background-color: rgba(239, 68, 68, 0.1); }");
    } else if (m_online) {
        m_statusLabel->setStyleSheet(QString("font-size: %1px; color: #10b981;").arg(titleSize));
        m_temperatureLabel->setStyleSheet(QString("font-size: %1px; font-weight: bold; color: %2;").arg(tempSize).arg(tempColorHex));
        setStyleSheet(QString("SensorWidget { border: 2px solid %1; background-color: rgba(%2, %3, %4, 0.1); }")
                      .arg(tempColor.name())
                      .arg(tempColor.red()).arg(tempColor.green()).arg(tempColor.blue()));
    } else {
        m_statusLabel->setStyleSheet(QString("font-size: %1px; color: #64748b;").arg(titleSize));
        m_temperatureLabel->setStyleSheet(QString("font-size: %1px; font-weight: bold; color: #64748b;").arg(tempSize));
        setStyleSheet("SensorWidget { border-color: #334155; background-color: transparent; }");
    }
}

void SensorWidget::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        emit sensorClicked(m_gateway, m_node, m_sensor);
    }
    QFrame::mousePressEvent(event);
}

void SensorWidget::paintEvent(QPaintEvent *event)
{
    QFrame::paintEvent(event);
    
    if (m_alert) {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setBrush(QBrush(QColor("#ef4444")));
        painter.setPen(Qt::NoPen);
        
        // A bolinha vermelha agora cresce/diminui junto com o Zoom
        int ellipseSize = 10 * m_currentZoom;
        painter.drawEllipse(width() - ellipseSize - 5, 5, ellipseSize, ellipseSize);
    }
}