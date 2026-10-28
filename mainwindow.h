#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QListWidgetItem>
#include <QLabel>
#include <QPushButton>
#include <QGroupBox>
#include <QMap>
#include <QVector>
#include <QStackedWidget>
#include <QPair>
#include <QTreeWidget>
// #include "databasemanager.h"
#include "networkmanager.h"
#include "thememanager.h"

// Avisa o compilador que essa classe existe (Forward declaration)
class SiloGLWidget; 
class Silo2DWidget;

// Forward-declara as classes do QtCharts para acelerar a compilação e evitar problemas de dependência de includes.
class QChartView;
class QComboBox;
class QPlainTextEdit;
class QSpinBox;
class QTabWidget;
QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

    // Define a aba inicial a ser exibida ao iniciar o programa.
    void setInitialTabByName(const QString& name);
    ThemeManager* getThemeManager() const { return m_themeManager; } // Getter para ThemeManager
    void performPostStartupUIUpdates();
    // void updateGatewayStatus(bool isOnline); // Removido, pois onRemoteNodeStatusUpdate já lida com o Gateway
    void startNetworkInitialization();

public slots:
    void appendToLog(const QString& message);

private slots:
    void randomizeAndUpdateTemperatures();
    void onSiloSelected(QListWidgetItem *item);
    void onViewModeChanged(int index);
    void onThemeChanged(int index);
    void zoomIn();
    void zoomOut();
    void exportarTermometriaJson();
    void exportarAeracaoJson();
    void onDayNightUpdate();
    // Slots para as novas funcionalidades
    // void onLoggingTimerTimeout();
    void onSortearMeteoData();
    void onUpdateRedeStatus();
    void onNewTemperatures(int siloId, const QVector<float>& temperatures);
    void onNewMeteoData(double temperature, double humidity);
    void onLocalNetworkStatusUpdate(const QString& status, const QString& details);
    void onNewMotorState(int siloId, bool isOn);
    void onSaveAndConnectNetwork();
    void onRemoteNodeStatusUpdate(int nodeId, const QString& status, const QString& details);
    void onAccessPointStatusUpdate(bool connected);
    void onToggleBeaconClicked();
    void onRequestGatewayStatus();
    void onRequestAllData();
    void onToggleGatewayBeacon();
    void evaluateAutomaticAeration();
    void onSensorPositionChanged(int gateway, int node, int rawSensorId, int newPosition);
    void onExportSensorOrder();
    void onImportSensorOrder();

private:
    int m_sensorMapping[5][13];
    QMap<int, QVector<float>> m_rawSiloTemperatures;
    void loadSensorMapping();
    void saveSensorMapping();
    void updateSensorGridLayout();

    void updateMotorActiveConfigLabel(int motorId);
    QPushButton* m_btnLockUnlockConfig = nullptr;
    Ui::MainWindow *ui;
    ThemeManager *m_themeManager;
    double m_zoomFactor;

    SiloGLWidget *m_glWidget;       // Ponteiro para controlar o 3D
    Silo2DWidget *m_silo2DWidget;   // Ponteiro para controlar o 2D
    QWidget *m_options3DContainer;  // Contêiner dos botões dinâmicos 3D
    QWidget *m_optionsGridContainer; // Contêiner das opções da Grid View
    QWidget *m_options2DContainer;   // Contêiner das opções da Visualização 2D    
    QComboBox *m_cbPaletteGrid = nullptr;
    QComboBox *m_cbPalette2D = nullptr;
    QComboBox *m_cbPalette3D = nullptr;
    QList<QWidget*> m_overviewPanels; // Lista de todos os painéis de visão geral para mantê-los sincronizados
    QTimer* m_dayNightTimer;        // Timer para atualizar o ciclo dia/noite
    // QTimer* m_loggingTimer;         // Timer para salvar os dados periodicamente
    bool m_gridHorizontal;          // true = pêndulos em colunas, false = pêndulos em linhas
    NetworkManager* m_networkManager = nullptr; // Gerenciador da comunicação de rede
    bool m_isApBeaconActive = true;   // Controla o estado do botão do beacon do AP
    bool m_isGatewayBeaconActive = true; // Controla o estado do botão do beacon do Gateway
    QMap<int, QVector<float>> m_siloTemperatures; // Armazena as temperaturas para cada silo individualmente

    // Widgets para a aba de Meteorologia
    QLabel* m_meteoTempLabel = nullptr;
    QLabel* m_meteoHumidLabel = nullptr;

    // Widgets para a aba de Rede
    QStackedWidget* m_redeNodeStack = nullptr;
    QPlainTextEdit* m_logDisplay = nullptr; // Widget para exibir o log de eventos
    QMap<QString, QLabel*> m_redeStatusLabels; // Mapa para atualizar facilmente os valores de status da rede

    // Widgets para a aba de Estatísticas

    QStackedWidget* m_statsCentralStack = nullptr;
    QStackedWidget* m_statsOptionsStack = nullptr;
    QListWidget* m_statsSelectionList = nullptr;
    QListWidget* m_statsConfigList = nullptr;

    QTabWidget* m_statsTermTabs = nullptr;
    QTabWidget* m_statsAerTabs = nullptr;
    QTabWidget* m_statsRedeTabs = nullptr;
    QTabWidget* m_statsMeteoTabs = nullptr;

    void updateTermometriaStatsChart();
    void updateAeracaoStatsChart();
    void updateMeteorologiaStatsChart();
    void onStatsTabChanged(int pageIndex);

    QDateTime m_statsStartTime;
    QTimer* m_statsRefreshTimer = nullptr;

    // Widgets para a aba de Meteorologia
    // QChartView* m_meteoChartView = nullptr;
    // void updateMeteoChart(int rangeDays);
    // void setupDualAxisMeteoChart(QChartView* chartView, const QDateTime& start, const QDateTime& end);

    void setupSiloSidebar();
    void setupViewTabs();
    void loadNetworkSettings();
    void updateLocalIpAddress();
    QWidget* createGlobalOverviewPanel(); // Fábrica para criar instâncias do painel de visão geral
    void setupConfigTab();
    void setupTermometria3DTab();
    void initialize3DView();
    void setup3DOptionsPanel();     // Nova função para criar os botões
    void setupGridOptionsPanel();    // Opções específicas da Grade
    void setup2DOptionsPanel();      // Opções específicas da Visualização 2D
    void setupAeracaoOptionsPanel(); // Opções do painel de aeração
    void setupMeteorologiaTab();     // Configura a aba de Meteorologia completa
    void setupEstatisticasTab();     // Configura a aba de Estatísticas completa
    void setupRedeTab();             // Configura a aba de Rede completa
    void updateSensorGrid(QString siloName);
    void applyZoom();
    void updateMotorSummary();      // Atualiza o label de resumo dos motores
    void updateAllOverviewPanels(); // Atualiza todos os painéis de visão geral
    void updateMotorOverviewLine(int motorId); // Atualiza uma linha específica no painel de visão geral
    void updateOverviewSummaries(); // Atualiza os totais e resumos no painel de visão geral
    void toggleMotorSummaryPanel(); // Toggle do painel de resumo dos motores
    void toggleGridOrientation();  // Alterna orientação horizontal/vertical do grid
    void showOptionsForViewMode(int mode); // Mostra/esconde opções baseado no modo
    void onPaletteChanged(int index); // Sincroniza e atualiza a paleta de cores
    void exportDatabaseToCsv(const QString& filePath);
    void updateStatsSummaryText();
    QString formatDuration(int seconds);

    QLabel* m_lblStatsSummary = nullptr;

    bool eventFilter(QObject *obj, QEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

protected:
    void closeEvent(QCloseEvent *event) override;
};

#endif // MAINWINDOW_H