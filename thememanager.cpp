#include "thememanager.h"
#include <QApplication>
#include <QDebug>

ThemeManager::ThemeManager(QObject *parent)
    : QObject(parent)
    , m_currentTheme(DARK_THEME)
{
    updateCurrentColors();
}

ThemeManager::~ThemeManager() {}

void ThemeManager::setTheme(ThemeType theme)
{
    m_currentTheme = theme;
    updateCurrentColors();
    applyThemeToApplication();
}

ThemeManager::ThemeType ThemeManager::getCurrentTheme() const
{
    return m_currentTheme;
}

const ThemeManager::ThemeColors& ThemeManager::getCurrentColors() const
{
    return m_currentColors;
}

void ThemeManager::updateCurrentColors()
{
    switch (m_currentTheme) {
        case DARK_THEME:
            m_currentColors.background = QColor("#0f172a");
            m_currentColors.surface = QColor("#334155"); // Mais claro para contraste
            m_currentColors.primary = QColor("#3b82f6");
            m_currentColors.textPrimary = QColor("#f8fafc");
            m_currentColors.border = QColor("#475569"); // Borda visível na superfície
            m_currentColors.hover = QColor("#2563eb");
            break;
            
        case LIGHT_THEME:
            m_currentColors.background = QColor("#f1f5f9"); // Cinza claro para menos brilho
            m_currentColors.surface = QColor("#ffffff");
            m_currentColors.primary = QColor("#2563eb");
            m_currentColors.textPrimary = QColor("#0f172a");
            m_currentColors.border = QColor("#cbd5e1"); // Borda mais escura
            m_currentColors.hover = QColor("#3b82f6");
            break;
            
        case FAMILIAR_THEME:
            m_currentColors.background = QColor("#fefce8");
            m_currentColors.surface = QColor("#fef9c3");
            m_currentColors.primary = QColor("#a16207"); // Ouro mais escuro
            m_currentColors.textPrimary = QColor("#44403c"); // Marrom/cinza escuro
            m_currentColors.border = QColor("#facc15"); // Amarelo mais forte
            m_currentColors.hover = QColor("#eab308");
            break;
    }
}

void ThemeManager::applyThemeToApplication()
{
    // Define o estilo base como Fusion para garantir consistência entre Windows/Linux/Mac
    // Define o estilo base da aplicação. QMacStyle só é efetivo no macOS.
    QApplication::setStyle("Fusion");
    
    // Aplica as cores na paleta global do sistema
    QPalette palette;
    palette.setColor(QPalette::Window, m_currentColors.background);
    palette.setColor(QPalette::WindowText, m_currentColors.textPrimary);
    palette.setColor(QPalette::Base, m_currentColors.surface);
    palette.setColor(QPalette::AlternateBase, m_currentColors.background);
    palette.setColor(QPalette::Text, m_currentColors.textPrimary);
    palette.setColor(QPalette::Button, m_currentColors.surface);
    palette.setColor(QPalette::ButtonText, m_currentColors.textPrimary);
    palette.setColor(QPalette::Highlight, m_currentColors.primary);

    QApplication *app = qobject_cast<QApplication*>(QApplication::instance());
    if (app) {
        app->setPalette(palette);
        // A aplicação do stylesheet global será adiada para depois da exibição da janela principal,
        // para evitar bloqueios durante a inicialização da UI.
    }
}

QString ThemeManager::getStyleSheet() const
{
    return generateMainStyleSheet();
}

QString ThemeManager::generateMainStyleSheet() const
{
    // Este é o CSS global que vai estilizar todas as abas, botões e caixas da sua UI
    return QString(
        "QMainWindow, QWidget { background-color: %1; color: %2; font-family: 'Segoe UI', Arial; }"
        
        "QGroupBox {"
        "    font-weight: bold;"
        "    margin-top: 1ex;"
        "}"
        "QGroupBox::title {"
        "    subcontrol-origin: margin;"
        "    padding: 0 3px;"
        "}"
        
        "QPushButton {"
        "    background-color: %5;" /* surface */
        "    color: %2;" /* textPrimary */
        "    border: 1px solid %3;" /* border */
        "    border-radius: 6px;"
        "    padding: 8px 16px;"
        "    font-weight: bold;"
        "    outline: none;"
        "}"
        "QPushButton:hover {"
        "    border-color: %4;" /* primary */
        "}"
        "QPushButton:pressed {"
        "    background-color: %1;" /* background */
        "}"
        "QPushButton:disabled {"
        "    background-color: %3;"
        "    color: #888;"
        "    border-color: %3;"
        "}"
        
        /* Remove o padding dos botões de toggle que são só ícones */
        "QPushButton[objectName=\"panelToggleButton\"] {"
        "    padding: 0px;"
        "}"

        "QTabWidget::pane { border-top: 1px solid %3; }" /* Estilo mais sutil para QMacStyle */
        "QTabBar::tab {"
        "    padding: 10px 12px;" /* Padding horizontal reduzido ainda mais para garantir que as abas caibam sem setas */
        "}"
        "QTabBar::tab:selected {"
        "    color: %4;" /* Cor primária para o texto da aba selecionada */
        "    font-weight: bold;"
        "}"

        /* Estilos para o Painel de Visão Geral */
        "QFrame#globalOverviewPanel {"
        "    background-color: %1;" /* Cor de fundo do tema */
        "    border: 1px solid %3;"
        "    border-radius: 8px;"
        "}"
        "QLabel#overviewTitleLabel {"
        "    font-weight: bold; font-size: 13px; color: %2;"
        "    padding-bottom: 4px; border-bottom: 1px solid %3;"
        "}"
        "QLabel#lblMotorSummary {"
        "    font-weight: bold; padding: 5px; color: %4;" /* Cor primária */
        "}"
        "QLabel#overviewSectionTitle {"
        "    font-weight: bold; font-size: 9px; color: #475569; letter-spacing: 1px;" /* Cinza mais escuro para subtítulos */
        "    margin-top: 6px;"
        "}"
        "QFrame[frameShape=\"4\"] { /* QFrame::HLine */"
        "    border: 1px solid %3;"
        "}"
        "QLabel[objectName^=\"lblMotorSummaryLeft\"] {"
        "    font-size: 10px; padding: 3px; font-weight: bold;" /* Adicionado negrito */
        "}"
        "QLabel[objectName^=\"lblMotorSummaryLeft\"][state=\"on\"] {"
        "    color: #22c55e;" /* Verde para ligado */
        "}"
        "QLabel[objectName^=\"lblMotorSummaryLeft\"][state=\"off\"] {"
        "    color: #475569;" /* Cinza mais escuro para desligado */
        "}"
        "QLabel#lblMotorTotalLeft {"
        "    font-weight: bold; font-size: 11px; padding: 4px; margin-top: 4px;"
        "}"
        "QLabel#lblMotorTotalLeft[state=\"on\"] { color: #22c55e; }"
        "QLabel#lblMotorTotalLeft[state=\"off\"] { color: #475569; }"
        "QLabel[objectName^=\"lblTempPendulum\"] {"
        "    color: #d97706; font-size: 10px; padding: 2px; font-weight: bold;" /* Laranja escuro e negrito para temperaturas */
        "}"
        "QLabel[objectName^=\"lblMeteo\"] {"
        "    color: #0ea5e9; font-size: 10px; padding: 2px; font-weight: bold;" /* Azul para meteorologia */
        "}"
        "QLabel#lblTempGeral {"
        "    color: %4; font-weight: bold; font-size: 11px; padding: 4px; margin-top: 4px;" /* Cor primária */
        "}"
    ).arg(m_currentColors.background.name(),
          m_currentColors.textPrimary.name(),
          m_currentColors.border.name(),
          m_currentColors.primary.name(),
          m_currentColors.surface.name());
}