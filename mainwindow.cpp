// Inclui os cabeçalhos completos do QtCharts primeiro para garantir que suas definições estejam disponíveis.
#include <QtCharts/QChartView>
#include <QtCharts/QChart>
#include <QtCharts/QCategoryAxis>
#include <QtCharts/QLineSeries>
#include <QtCharts/QSplineSeries>
#include <QtCharts/QDateTimeAxis>
#include <QtCharts/QValueAxis>

#include "mainwindow.h"
#include "ui_mainwindow.h"

#include "sensorwidget.h"
#include "siloglwidget.h"
#include "silo2dwidget.h"
#include <QRandomGenerator>
#include <QTimer>
#include <QVBoxLayout>
#include <QGroupBox>
#include <QCheckBox>
#include <QPushButton>
#include <QAbstractButton>
#include <QMouseEvent>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QNetworkInterface>
#include <QFileDialog>
#include <QMessageBox>
#include <QTreeWidget>
#include <QDateTime>
#include <QFormLayout>
#include <QSettings>
#include <QSet>
#include <QLineEdit>
#include <QToolTip>
#include <QPlainTextEdit>
#include <QTabWidget>
#include <QCloseEvent>
#include <QSpinBox>
#include "databasemanager.h"
#include <QButtonGroup>
#include <QIcon>

namespace {
    // Estrutura para centralizar as informações dos pêndulos, garantindo consistência.
    struct PendulumInfo {
        int id;
        QString name;
        QColor color;
    };

    const QList<PendulumInfo> PENDULUM_INFO_LIST = {
        {0, "Pêndulo Centro", QColor("#a855f7")}, // Roxo
        {1, "Pêndulo Norte",  QColor("#3b82f6")}, // Azul
        {2, "Pêndulo Leste",  QColor("#22c55e")}, // Verde
        {3, "Pêndulo Sul",    QColor("#ef4444")}, // Vermelho
        {4, "Pêndulo Oeste",  QColor("#f97316")}  // Laranja
    };
}

// Estrutura para agrupar os widgets de controle de um motor.
// Isso evita o uso repetido de findChild e torna o código mais limpo.
struct MotorControls {
    QComboBox* cbOperationMode = nullptr;
    QComboBox* cbAeracaoMode = nullptr;
    QWidget* widgetAutoConfig = nullptr;
    QWidget* wAutoTemp = nullptr;
    QWidget* wAutoTime = nullptr;
    QWidget* widgetManualConfig = nullptr;
    QLabel* lblCurrentActiveConfig = nullptr;
    QDoubleSpinBox* spinBoxMaxTemp = nullptr;
    QTimeEdit* timeEditOn = nullptr;
    QTimeEdit* timeEditOff = nullptr;
    QPushButton* btnManualAction = nullptr;
    QLabel* lblMotorStatus = nullptr;
};

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_themeManager(new ThemeManager(this))
    , m_zoomFactor(1.0)
    , m_silo2DWidget(nullptr)
    , m_glWidget(nullptr) // Initialized in setupTermometria3DTab()
    , m_options3DContainer(nullptr) // Will be assigned to ui->toolBoxOptions
    , m_optionsGridContainer(nullptr)
    , m_options2DContainer(nullptr)
    // m_overviewPanels é inicializado vazio por padrão
    , m_dayNightTimer(nullptr)
    // , m_loggingTimer(nullptr)
    , m_gridHorizontal(true)
{
    ui->setupUi(this);


    loadSensorMapping();


    // =================================================================================
    // SOLUÇÃO PARA O REDIMENSIONAMENTO DA JANELA
    // Impede que a grade de sensores (que pode ser muito grande) force a janela
    // principal a se redimensionar. Ao definir o tamanho mínimo do conteúdo da
    // QScrollArea como zero, garantimos que a janela mantenha um tamanho estável,
    // e a QScrollArea exibirá barras de rolagem conforme necessário.
    QWidget* gridPage = ui->viewStackedWidget->widget(0);
    if (gridPage) {
        // Encontra a QScrollArea dentro da página da grade.
        // Isso é mais robusto do que usar um nome fixo que pode não existir.
        if (QScrollArea* scrollArea = gridPage->findChild<QScrollArea*>()) {
            if (scrollArea->widget()) {
                scrollArea->widget()->setMinimumSize(0, 0);
            }
        }
    }

    // =================================================================================

    // Adiciona o painel de visão geral à barra lateral esquerda da aba de termometria,
    // removendo qualquer painel antigo que possa existir.
    if (ui->leftSidebarLayout && ui->grpVisaoGeral) {
        // Itera sobre o layout para remover tanto o painel antigo quanto o espaçador.
        for (int i = ui->leftSidebarLayout->count() - 1; i >= 0; --i) {
            QLayoutItem* item = ui->leftSidebarLayout->itemAt(i);
            if (item->spacerItem()) {
                // Remove o espaçador para que o novo painel possa "encostar" no widget acima.
                ui->leftSidebarLayout->removeItem(item);
                delete item;
            } else if (item->widget() && item->widget() == ui->grpVisaoGeral) {
                // Remove o painel antigo.
                ui->leftSidebarLayout->removeWidget(ui->grpVisaoGeral);
                ui->grpVisaoGeral->deleteLater();
                ui->grpVisaoGeral = nullptr;
            }
        }
        // Adiciona o novo painel de visão geral e um novo espaçador no final.
        QWidget* panel = createGlobalOverviewPanel();
        ui->leftSidebarLayout->addWidget(panel);
        ui->leftSidebarLayout->addStretch(); // Garante que tudo fique alinhado ao topo.

    }

    // Adiciona o painel de visão geral à aba de Aeração
    // A busca pelo layout da barra lateral da aeração foi tornada mais robusta usando findChild,
    // pois a abordagem anterior de navegar pelo pai do widget falhava.
    if (QVBoxLayout* aeracaoSidebarLayout = ui->aeracaoTab->findChild<QVBoxLayout*>("vboxLayout5")) {
        // Remove o spacer existente para que o painel fique encostado na lista, mantendo a consistência visual.
        for (int i = aeracaoSidebarLayout->count() - 1; i >= 0; --i) {
            QLayoutItem* item = aeracaoSidebarLayout->itemAt(i);
            if (item->spacerItem()) {
                aeracaoSidebarLayout->removeItem(item);
                delete item;
                break; // Assume-se que há apenas um espaçador no final
            }
        }
        QWidget* panel = createGlobalOverviewPanel();
        aeracaoSidebarLayout->addWidget(panel);
        aeracaoSidebarLayout->addStretch();

    }

    m_themeManager->setTheme(ThemeManager::DARK_THEME);


    // Configuração original das abas, restaurada para centralizar a lógica.

    setupSiloSidebar();


    setupViewTabs();


    setupConfigTab();


    setupAeracaoOptionsPanel();


    setupMeteorologiaTab();


    setupEstatisticasTab();


    setupRedeTab();


    // Inicialização da visualização 3D
    setUpdatesEnabled(false);
    if (!m_glWidget) {

        setupTermometria3DTab();
        setup3DOptionsPanel();
    }
    setUpdatesEnabled(true);
    showOptionsForViewMode(0);


    // Histórico agora é carregado via SQLite sob demanda

    // Timer para o ciclo dia/noite
    m_dayNightTimer = new QTimer(this);
    connect(m_dayNightTimer, &QTimer::timeout, this, &MainWindow::onDayNightUpdate);
    m_dayNightTimer->start(60000); // Atualiza a cada minuto


    // Timer para avaliação da aeração automática (modo Automático por tempo/temp)
    QTimer* autoAeracaoTimer = new QTimer(this);
    connect(autoAeracaoTimer, &QTimer::timeout, this, &MainWindow::evaluateAutomaticAeration);
    autoAeracaoTimer->start(5000); // Avalia a cada 5 segundos


    // ==========================================
    // PADRONIZAÇÃO DOS PAINÉIS MINIMIZÁVEIS
    // ==========================================

    // --- Termometria ---
    // Painel de Seleção (Esquerda): Lógica simples para não quebrar o layout.
    ui->btnToggleLeftSidebar->setCheckable(true);
    ui->btnToggleLeftSidebar->setObjectName("panelToggleButton");
    ui->btnToggleLeftSidebar->setFixedSize(18, 18); // Reduz o tamanho em 25% (de 24x24 para 18x18)
    ui->btnToggleLeftSidebar->setChecked(true);
    connect(ui->btnToggleLeftSidebar, &QPushButton::toggled, ui->siloList, &QWidget::setVisible);

    // Padroniza o estilo do título do painel de seleção.
    if (ui->leftSidebarLayout && ui->leftSidebarLayout->count() > 0) {
        // Abordagem robusta: procura o QLabel pelo conteúdo, não pela posição.
        // Isso evita falhas se a ordem dos widgets no .ui for alterada.
        if (QWidget* parent = ui->leftSidebarLayout->parentWidget()) {
            QList<QLabel*> labels = parent->findChildren<QLabel*>();
            for (QLabel* label : labels) {
                if (label->text().contains("Seleção")) {
                    QFont font = label->font();
                    font.setBold(false);
                    font.setPointSize(11);
                    label->setFont(font);
                    break; // Encontrou e estilizou, pode parar.
                }
            }
        }
    }

    // Painel de Opções (Direita): Lógica simplificada para seguir o padrão do setupRedeTab.
    {
        // ABORDAGEM DE LAYOUT ROBUSTA:
        // Define os "fatores de expansão" (stretch factors) para o layout principal da aba.
        // Isso força o painel central a se expandir para preencher o espaço vazio,
        // enquanto as barras laterais mantêm um tamanho controlado, resolvendo o problema
        // da interface "aglomerada".
        if (auto* mainHLayout = qobject_cast<QHBoxLayout*>(ui->termometriaTab->layout())) {
            if (mainHLayout->count() == 3) { // Garante que temos 3 painéis (esquerda, centro, direita)
                // Define uma largura máxima para as barras laterais para evitar que cresçam demais.
                if (QWidget* leftSidebar = mainHLayout->itemAt(0)->widget()) {
                    leftSidebar->setMaximumWidth(250);
                }
                if (QWidget* rightSidebar = mainHLayout->itemAt(2)->widget()) {
                    rightSidebar->setMaximumWidth(250);
                }
                mainHLayout->setStretch(0, 0); // Barra lateral esquerda (não expande)
                mainHLayout->setStretch(1, 1); // Conteúdo central (expande)
                mainHLayout->setStretch(2, 0); // Barra lateral direita (não expande)
            }
        }

        // Pega o layout do cabeçalho original do .ui
        if (auto* headerLayout = qobject_cast<QHBoxLayout*>(ui->rightSidebarLayout->itemAt(0)->layout())) {
            // Limpa o layout, removendo e deletando os widgets antigos.
            QLayoutItem* item;
            while ((item = headerLayout->takeAt(0)) != nullptr) {
                delete item->widget();
                delete item;
            }

            // Cria e adiciona os novos widgets, como no exemplo.
            QLabel* titleLabel = new QLabel("Opções");
            titleLabel->setStyleSheet("font-weight: bold; font-size: 14px;");

            QPushButton* toggleButton = new QPushButton("");
            toggleButton->setObjectName("panelToggleButton");
            toggleButton->setFixedSize(24, 24);
            toggleButton->setToolTip("Minimizar/Maximizar Opções");
            toggleButton->setCheckable(true);
            toggleButton->setChecked(true);

            headerLayout->addWidget(titleLabel);
            headerLayout->addStretch();
            headerLayout->addWidget(toggleButton);

            // Conecta o botão para controlar a visibilidade de todo o painel de conteúdo.
            connect(toggleButton, &QPushButton::toggled, ui->optionsPanel, &QWidget::setVisible);
        }
    }

    // --- Aeração ---
    // Painel de Seleção (Esquerda): Lógica simples.
    ui->btnToggleAeracaoLeftSidebar->setCheckable(true);
    ui->btnToggleAeracaoLeftSidebar->setObjectName("panelToggleButton");
    ui->btnToggleAeracaoLeftSidebar->setFixedSize(18, 18); // Reduz o tamanho em 25% (de 24x24 para 18x18)
    ui->btnToggleAeracaoLeftSidebar->setChecked(true);
    connect(ui->btnToggleAeracaoLeftSidebar, &QPushButton::toggled, ui->aeracaoSiloList, &QWidget::setVisible);

    // Padroniza o estilo do título do painel de seleção de aeração.
    if (auto* aeracaoSidebarLayout = ui->aeracaoTab->findChild<QVBoxLayout*>("vboxLayout5")) {
        // Aplica a mesma abordagem robusta para a aba de aeração.
        if (QWidget* parent = aeracaoSidebarLayout->parentWidget()) {
            QList<QLabel*> labels = parent->findChildren<QLabel*>();
            for (QLabel* label : labels) {
                if (label->text().contains("Seleção")) {
                    QFont font = label->font();
                    font.setBold(false);
                    font.setPointSize(11);
                    label->setFont(font);
                    break; // Encontrou e estilizou, pode parar.
                }
            }
        }
    }

    // Painel de Opções (Direita): Lógica simplificada.
    {
        // Aplica a mesma abordagem de layout robusta para a aba de Aeração.
        if (auto* mainHLayout = qobject_cast<QHBoxLayout*>(ui->aeracaoTab->layout())) {
            if (mainHLayout->count() == 3) {
                // Define uma largura máxima para as barras laterais.
                if (QWidget* leftSidebar = mainHLayout->itemAt(0)->widget()) {
                    leftSidebar->setMaximumWidth(250);
                }
                if (QWidget* rightSidebar = mainHLayout->itemAt(2)->widget()) {
                    rightSidebar->setMaximumWidth(250);
                }
                mainHLayout->setStretch(0, 0); // Barra lateral esquerda (não expande)
                mainHLayout->setStretch(1, 1); // Conteúdo central (expande)
                mainHLayout->setStretch(2, 0); // Barra lateral direita (não expande)
            }
        }

        // Encontra o layout da barra lateral direita pelo nome do layout pai
        if (auto* rightSidebarLayout = ui->aeracaoTab->findChild<QVBoxLayout*>("vboxLayout23")) {
            // Pega o layout do cabeçalho
            if (auto* headerLayout = qobject_cast<QHBoxLayout*>(rightSidebarLayout->itemAt(0)->layout())) {
                // Limpa o layout, removendo e deletando os widgets antigos.
                QLayoutItem* item;
                while ((item = headerLayout->takeAt(0)) != nullptr) {
                    delete item->widget();
                    delete item;
                }

                // Cria e adiciona os novos widgets.
                QLabel* titleLabel = new QLabel("Opções");
                titleLabel->setStyleSheet("font-weight: bold; font-size: 14px;");

                QPushButton* toggleButton = new QPushButton("");
                toggleButton->setObjectName("panelToggleButton");
                toggleButton->setFixedSize(24, 24);
                toggleButton->setToolTip("Minimizar/Maximizar Opções");
                toggleButton->setCheckable(true);
                toggleButton->setChecked(true);

                headerLayout->addWidget(titleLabel);
                headerLayout->addStretch();
                headerLayout->addWidget(toggleButton);

                // Conecta o botão para controlar a visibilidade do painel de conteúdo.
                connect(toggleButton, &QPushButton::toggled, ui->aeracaoOptionsPanel, &QWidget::setVisible);
            }
        }
    }

    // --- Barras Inferiores ---
    ui->btnToggleBottomBar->setText("");
    ui->btnToggleBottomBar->setObjectName("panelToggleButton");
    ui->btnToggleBottomBar->setFixedSize(24, 24);
    connect(ui->btnToggleBottomBar, &QPushButton::clicked, this, [this]() {
        ui->viewTabBar->setVisible(!ui->viewTabBar->isVisible());
    });

    ui->btnToggleAeracaoBottomBar->setText("");
    ui->btnToggleAeracaoBottomBar->setObjectName("panelToggleButton");
    ui->btnToggleAeracaoBottomBar->setFixedSize(24, 24);
    connect(ui->btnToggleAeracaoBottomBar, &QPushButton::clicked, this, [this]() {
        ui->aeracaoViewTabBar->setVisible(!ui->aeracaoViewTabBar->isVisible());
    });

    // --- Lógica específica da aba de Aeração (restaurada) ---
    ui->aeracaoSiloList->addItems({"Motor Alpha", "Motor Beta", "Motor Charlie", "Motor Delta"});
    ui->aeracaoSiloList->setCurrentRow(0);

    ui->aeracaoViewTabBar->addTab("Um");
    ui->aeracaoViewTabBar->addTab("Dois");
    ui->aeracaoViewTabBar->addTab("Três");
    ui->aeracaoViewTabBar->addTab("Quatro");

    if (ui->aeracaoViewTabBar->parentWidget()) {
        if (QHBoxLayout* bottomLayout = qobject_cast<QHBoxLayout*>(ui->aeracaoViewTabBar->parentWidget()->layout())) {
            bottomLayout->setStretchFactor(ui->aeracaoViewTabBar, 1);
            bottomLayout->setStretchFactor(ui->btnToggleAeracaoBottomBar, 0);
        }
    }

    // CORREÇÃO: Garante a distribuição vertical correta no painel central da aba de aeração.
    // Isso força a área de visualização dos motores a se expandir para preencher o espaço,
    // resolvendo o problema de "aglomerado na parte de cima".
    if (auto* mainHLayout = qobject_cast<QHBoxLayout*>(ui->aeracaoTab->layout())) {
        if (mainHLayout->count() == 3) {
            if (auto* centralWidget = mainHLayout->itemAt(1)->widget()) {
                if (auto* centralVLayout = qobject_cast<QVBoxLayout*>(centralWidget->layout())) {
                    // Define que o primeiro item (área de visualização) deve se expandir,
                    // enquanto o segundo (barra de abas inferior) não.
                    centralVLayout->setStretch(0, 1); // Área de visualização dos motores (expande)
                    centralVLayout->setStretch(1, 0); // Barra de abas inferior (não expande)
                }
            }
        }
    }


    for (int i = 1; i <= 4; ++i) {
        // --- LÓGICA DE CONFIGURAÇÃO DOS MOTORES RESTAURADA ---

        // 1. Encontra todos os widgets de controle para o motor atual.

        MotorControls controls;
        controls.cbOperationMode = findChild<QComboBox*>(QString("cbOperationMode_%1").arg(i));
        controls.cbAeracaoMode = findChild<QComboBox*>(QString("cbAeracaoMode_%1").arg(i));
        controls.widgetAutoConfig = findChild<QWidget*>(QString("widgetAutoConfig_%1").arg(i));
        controls.wAutoTemp = findChild<QWidget*>(QString("wAutoTemp_%1").arg(i));
        controls.wAutoTime = findChild<QWidget*>(QString("wAutoTime_%1").arg(i));
        controls.widgetManualConfig = findChild<QWidget*>(QString("widgetManualConfig_%1").arg(i));
        controls.lblCurrentActiveConfig = findChild<QLabel*>(QString("lblCurrentActiveConfig_%1").arg(i));
        controls.spinBoxMaxTemp = findChild<QDoubleSpinBox*>(QString("spinBoxMaxTemp_%1").arg(i));
        controls.timeEditOn = findChild<QTimeEdit*>(QString("timeEditOn_%1").arg(i));
        controls.timeEditOff = findChild<QTimeEdit*>(QString("timeEditOff_%1").arg(i));
        controls.btnManualAction = findChild<QPushButton*>(QString("btnManualAction_%1").arg(i));
        controls.lblMotorStatus = findChild<QLabel*>(QString("lblMotorStatus_%1").arg(i));

        // Verificação robusta de todos os ponteiros para encontrar a causa do travamento.
        if (!controls.cbOperationMode) { qCritical() << "CRASH IMMINENT: Widget 'cbOperationMode_" << i << "' not found!"; }
        if (!controls.cbAeracaoMode) { qCritical() << "CRASH IMMINENT: Widget 'cbAeracaoMode_" << i << "' not found!"; }
        if (!controls.widgetAutoConfig) { qCritical() << "CRASH IMMINENT: Widget 'widgetAutoConfig_" << i << "' not found!"; }
        if (!controls.wAutoTemp) { qCritical() << "CRASH IMMINENT: Widget 'wAutoTemp_" << i << "' not found!"; }
        if (!controls.wAutoTime) { qCritical() << "CRASH IMMINENT: Widget 'wAutoTime_" << i << "' not found!"; }
        if (!controls.widgetManualConfig) { qCritical() << "CRASH IMMINENT: Widget 'widgetManualConfig_" << i << "' not found!"; }
        if (!controls.lblCurrentActiveConfig) { qCritical() << "CRASH IMMINENT: Widget 'lblCurrentActiveConfig_" << i << "' not found!"; }
        if (!controls.spinBoxMaxTemp) { qCritical() << "CRASH IMMINENT: Widget 'spinBoxMaxTemp_" << i << "' not found!"; }
        if (!controls.timeEditOn) { qCritical() << "CRASH IMMINENT: Widget 'timeEditOn_" << i << "' not found!"; }
        if (!controls.timeEditOff) { qCritical() << "CRASH IMMINENT: Widget 'timeEditOff_" << i << "' not found!"; }
        if (!controls.btnManualAction) { qCritical() << "CRASH IMMINENT: Widget 'btnManualAction_" << i << "' not found!"; }
        if (!controls.lblMotorStatus) { qCritical() << "CRASH IMMINENT: Widget 'lblMotorStatus_" << i << "' not found!"; }


        if (!controls.cbOperationMode || !controls.btnManualAction) {
            qCritical() << "Constructor: Motor" << i << ": Critical widgets not found. Skipping configuration for this motor.";
            continue;
        }


        // 4. Adiciona o botão "Salvar Configuração".

        QPushButton* saveButton = new QPushButton("Salvar Configuração");
        saveButton->setStyleSheet("font-size: 12px; background-color: #3b82f6; color: white; padding: 8px; margin-top: 10px;");
        QWidget* configContainer = findChild<QWidget*>(QString("configContainerWidget_%1").arg(i));
        if (configContainer && configContainer->layout()) {
            configContainer->layout()->addWidget(saveButton);
        }
        connect(saveButton, &QPushButton::clicked, this, [this, motorId = i, saveButton]() {
            updateMotorOverviewLine(motorId);
            updateOverviewSummaries();
            saveButton->setText("Salvo!");
            QTimer::singleShot(2000, this, [saveButton](){ saveButton->setText("Salvar Configuração"); });
        });

        // 5. Conecta o botão de ação manual com a lógica de delay e verificação de modo.
        controls.btnManualAction->setProperty("isMotorOn", false);

        connect(controls.btnManualAction, &QPushButton::clicked, this, [this, c = controls, motorId = i]() {
            if (!m_networkManager) return;

            // Garante que a ação só funcione no modo manual.
            if (c.cbOperationMode->currentIndex() != 1) {
                QMessageBox::warning(this, "Modo Inválido", "A ação manual só pode ser executada no Modo Manual.");
                return;
            }

            bool isCurrentlyOn = c.btnManualAction->property("isMotorOn").toBool();
            m_networkManager->sendMotorCommand(motorId, !isCurrentlyOn);

            // Atualiza a UI para o estado "enviando".
            c.btnManualAction->setText("Enviando...");
            c.btnManualAction->setEnabled(false);

            // Timer de segurança para reativar o botão caso não haja resposta da rede.
            QTimer::singleShot(3000, this, [c](){
                if (!c.btnManualAction->isEnabled()) {
                    c.btnManualAction->setEnabled(true);
                    bool isOn = c.btnManualAction->property("isMotorOn").toBool();
                    c.btnManualAction->setText(isOn ? "DESLIGAR MOTOR" : "LIGAR MOTOR");
                }
            });
        });


        // 6. Define o estado inicial da UI para cada motor.

        if (controls.lblMotorStatus) {
            controls.lblMotorStatus->setText("DESLIGADO");
            controls.lblMotorStatus->setStyleSheet("font-size: 14px; font-weight: bold; color: #64748b; background-color: rgba(100, 116, 139, 0.1); padding: 8px 16px; border-radius: 20px; border: 2px solid #64748b;");
        }


        // Define o estado inicial dos widgets ANTES de conectar os sinais.
        // Isso evita que a UI seja atualizada prematuramente durante a construção, o que causa o travamento.
        controls.cbOperationMode->setCurrentIndex(0);
        controls.cbAeracaoMode->setCurrentIndex(0);
        if (controls.widgetAutoConfig) controls.widgetAutoConfig->setVisible(true);
        if (controls.widgetManualConfig) controls.widgetManualConfig->setVisible(false);
        if (controls.wAutoTemp) controls.wAutoTemp->setVisible(false);
        if (controls.wAutoTime) controls.wAutoTime->setVisible(true);

        // O painel de configuração começa travado (invisível).
        if (configContainer) configContainer->setVisible(false);

        
        // A atualização do label de resumo foi movida para fora do construtor
        // para evitar travamentos na inicialização.

        // 7. AGORA, conecta os sinais para responder a futuras interações do usuário.

        connect(controls.cbOperationMode, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this, c = controls, motorId = i](int index) {
            if (c.widgetAutoConfig) c.widgetAutoConfig->setVisible(index == 0);
            if (c.widgetManualConfig) c.widgetManualConfig->setVisible(index == 1);
            updateMotorActiveConfigLabel(motorId);
            updateMotorOverviewLine(motorId); // Atualiza o painel de visão geral
        });

        connect(controls.cbAeracaoMode, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this, c = controls, motorId = i](int index) {
            if (c.wAutoTemp) c.wAutoTemp->setVisible(index == 1);
            if (c.wAutoTime) c.wAutoTime->setVisible(index == 0);
            updateMotorActiveConfigLabel(motorId);
            updateMotorOverviewLine(motorId); // Atualiza o painel de visão geral
        });

        // Conecta as mudanças de valor para atualizar o resumo.
        auto updateLabelOnValueChange = [this, motorId = i]() {
            updateMotorActiveConfigLabel(motorId);
            updateMotorOverviewLine(motorId); // Atualiza o painel de visão geral
        };
        if (controls.spinBoxMaxTemp) connect(controls.spinBoxMaxTemp, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, updateLabelOnValueChange);
        if (controls.timeEditOn) connect(controls.timeEditOn, &QTimeEdit::timeChanged, this, updateLabelOnValueChange);
        if (controls.timeEditOff) connect(controls.timeEditOff, &QTimeEdit::timeChanged, this, updateLabelOnValueChange);


    }

    auto updateViews = [this]() {
        QWidget* container = ui->motorFrame_1->parentWidget();
        QGridLayout* gridLayout = qobject_cast<QGridLayout*>(container ? container->layout() : nullptr);
        if (!gridLayout) {
            qWarning("O container de visualização da aeração não possui um QGridLayout!");
            return;
        }
        // CORREÇÃO: Limpa o layout antigo para evitar que widgets duplicados ou
        // itens de layout antigos (como splitters) permaneçam.
        QLayoutItem* item;
        while ((item = gridLayout->takeAt(0)) != nullptr) {
            // Apenas removemos o item do layout. O widget em si (motorFrame) não é deletado,
            // pois vamos readicioná-lo. Itens que não são widgets (como spacers) são deletados.
            if (!item->widget()) {
                delete item;
            }
        }

        QList<QWidget*> motorFrames = {ui->motorFrame_1, ui->motorFrame_2, ui->motorFrame_3, ui->motorFrame_4};

        // Desanexa todos os widgets do layout e os esconde.
        for (QWidget* frame : motorFrames) {
            gridLayout->removeWidget(frame);
            frame->setVisible(false);
        }

        int mode = ui->aeracaoViewTabBar->currentIndex();
        int selected = ui->aeracaoSiloList->currentRow();

        // Adiciona os widgets de volta ao grid na configuração correta.
        if (mode == 0) { // "Um"
            if (selected >= 0 && selected < 4) {
                gridLayout->addWidget(motorFrames[selected], 0, 0, 1, 2);
                motorFrames[selected]->setVisible(true);
            }
        } else if (mode == 1) { // "Dois"
            gridLayout->addWidget(ui->motorFrame_1, 0, 0);
            gridLayout->addWidget(ui->motorFrame_2, 0, 1);
            ui->motorFrame_1->setVisible(true); ui->motorFrame_2->setVisible(true);
        } else if (mode == 2) { // "Três"
            gridLayout->addWidget(ui->motorFrame_1, 0, 0);
            gridLayout->addWidget(ui->motorFrame_2, 0, 1);
            gridLayout->addWidget(ui->motorFrame_3, 1, 0);
            ui->motorFrame_1->setVisible(true); ui->motorFrame_2->setVisible(true); ui->motorFrame_3->setVisible(true);
        } else if (mode == 3) { // "Quatro"
            gridLayout->addWidget(ui->motorFrame_1, 0, 0);
            gridLayout->addWidget(ui->motorFrame_2, 0, 1);
            gridLayout->addWidget(ui->motorFrame_3, 1, 0);
            gridLayout->addWidget(ui->motorFrame_4, 1, 1);
            ui->motorFrame_1->setVisible(true); ui->motorFrame_2->setVisible(true); ui->motorFrame_3->setVisible(true); ui->motorFrame_4->setVisible(true);
        }

        // Restaura o fator de expansão para que as células da grade preencham o espaço,
        // corrigindo o problema dos painéis "aglomerados".
        for (int i = 0; i < 2; ++i) {
            gridLayout->setRowStretch(i, 1);
            gridLayout->setColumnStretch(i, 1);
        }

        // CORREÇÃO: Força o recalculamento da geometria da barra de abas.
        // Isso resolve o bug onde as setas de navegação aparecem desnecessariamente.
        // Ao chamar updateGeometry(), informamos ao sistema de layout que o tamanho
        // ideal da barra de abas pode ter mudado, forçando-o a reavaliar e
        // alocar o espaço correto, eliminando as setas.
        ui->aeracaoViewTabBar->updateGeometry();

    };

    connect(ui->aeracaoViewTabBar, &QTabBar::currentChanged, this, updateViews);
    connect(ui->aeracaoSiloList, &QListWidget::currentRowChanged, this, updateViews);

    updateViews();


    updateMotorSummary(); // Atualiza todos os painéis após a configuração inicial.


    // ==========================================
    // INICIALIZAÇÃO DO BANCO DE DADOS E LOGGING
    // ==========================================
    // DatabaseManager::instance(); // Garante que o banco de dados seja aberto e inicializado
    // m_loggingTimer = new QTimer(this);
    // connect(m_loggingTimer, &QTimer::timeout, this, &MainWindow::onLoggingTimerTimeout);
    // m_loggingTimer->start(30000); // Salva os dados a cada 30 segundos
    // qInfo() << "Periodic data logging started (every 30 seconds).";

    // ==========================================
    // INICIALIZAÇÃO DO GERENCIADOR DE REDE
    // ==========================================
    m_networkManager = new NetworkManager(this);
    connect(m_networkManager, &NetworkManager::newTemperaturesReceived, this, &MainWindow::onNewTemperatures);
    connect(m_networkManager, &NetworkManager::newMeteoDataReceived, this, &MainWindow::onNewMeteoData);
    connect(m_networkManager, &NetworkManager::newMotorStateReceived, this, &MainWindow::onNewMotorState);
    connect(m_networkManager, &NetworkManager::networkStatusChanged, this, &MainWindow::onLocalNetworkStatusUpdate);
    connect(m_networkManager, &NetworkManager::remoteNodeStatusUpdate, this, &MainWindow::onRemoteNodeStatusUpdate);
    connect(m_networkManager, &NetworkManager::accessPointStatusChanged, this, &MainWindow::onAccessPointStatusUpdate);

    // Carrega as configurações de rede salvas e tenta conectar

}

void MainWindow::performPostStartupUIUpdates()
{
    // Esta função é chamada via QTimer após o loop de eventos ter iniciado.
    // É seguro realizar atualizações de UI aqui.

    // Aplica o stylesheet global
    if (QApplication* app = qobject_cast<QApplication*>(QApplication::instance())) {
        app->setStyleSheet(m_themeManager->getStyleSheet());
    }

    // Atualiza os labels de resumo de configuração dos motores com seu estado inicial.
    for (int i = 1; i <= 4; ++i) {
        updateMotorActiveConfigLabel(i);
    }

    // Conecta o botão de travar/destravar configurações da aba Aeração.
    // Isso é feito aqui para garantir que a UI esteja totalmente inicializada,
    // evitando o deadlock que ocorria ao conectar no construtor.
    if (m_btnLockUnlockConfig) {
        connect(m_btnLockUnlockConfig, &QPushButton::clicked, this, [this]() {
            // A lógica de toggle: verifica o estado do primeiro painel e inverte para todos.
            QWidget* firstCfg = findChild<QWidget*>("configContainerWidget_1");
            bool shouldBecomeVisible = firstCfg ? !firstCfg->isVisible() : false;

            for (int i = 1; i <= 4; ++i) {
                QWidget* cfg = findChild<QWidget*>(QString("configContainerWidget_%1").arg(i));
                if (cfg) cfg->setVisible(shouldBecomeVisible);
            }
            m_btnLockUnlockConfig->setText(shouldBecomeVisible ? "Travar Configurações" : "Destravar Configurações");
        });
    }
}

MainWindow::~MainWindow() { delete ui; } // O QObject parent/child system cuidará dos outros ponteiros.

void MainWindow::setInitialTabByName(const QString& name) {
    for (int i = 0; i < ui->mainTabWidget->count(); ++i) {
        if (ui->mainTabWidget->tabText(i) == name) {
            ui->mainTabWidget->setCurrentIndex(i);
            return;
        }
    }
    qWarning() << "Aba com o nome '" << name << "' não foi encontrada.";
}

void MainWindow::startNetworkInitialization() {
    // Carrega as configurações de rede salvas e tenta conectar
    loadNetworkSettings();
    updateLocalIpAddress();
}

void MainWindow::appendToLog(const QString& message) {
    if (m_logDisplay) {
        // A atualização é feita via QueuedConnection a partir do customLogHandler,
        // o que a torna segura entre threads. Esta função apenas adiciona o texto
        // ao widget quando o evento é processado na thread principal da UI.
        m_logDisplay->appendPlainText(message);
    }
}

void MainWindow::updateMotorActiveConfigLabel(int motorId)
{
    QLabel* lbl = findChild<QLabel*>(QString("lblCurrentActiveConfig_%1").arg(motorId));
    QComboBox* cbOpMode = findChild<QComboBox*>(QString("cbOperationMode_%1").arg(motorId));
    QComboBox* cbAeracaoMode = findChild<QComboBox*>(QString("cbAeracaoMode_%1").arg(motorId));
    QDoubleSpinBox* spinTemp = findChild<QDoubleSpinBox*>(QString("spinBoxMaxTemp_%1").arg(motorId));
    QTimeEdit* timeOn = findChild<QTimeEdit*>(QString("timeEditOn_%1").arg(motorId));
    QTimeEdit* timeOff = findChild<QTimeEdit*>(QString("timeEditOff_%1").arg(motorId));

    if (!lbl || !cbOpMode || !cbAeracaoMode || !spinTemp || !timeOn || !timeOff) return;

    if (cbOpMode->currentIndex() == 1) { // Manual
        lbl->setText("Configuração Atual: MODO MANUAL");
    } else { // Automático
        if (cbAeracaoMode->currentIndex() == 0) { // Por Horário
            lbl->setText(QString("Atual: Automático (Das %1 às %2)").arg(timeOn->time().toString("HH:mm")).arg(timeOff->time().toString("HH:mm")));
        } else { // Por Temperatura
            lbl->setText(QString("Atual: Automático (Temp. Máx: %1 °C)").arg(spinTemp->value()));
        }
    }
}


// ==========================================
// FUNÇÃO AUXILIAR: Atualizar resumo dos motores
// ==========================================
void MainWindow::updateMotorSummary() {
    // Esta função agora é um atalho para atualizar todos os painéis de visão geral.
    updateAllOverviewPanels(); // Atualiza o painel de visão geral
}

// ==========================================
// ATUALIZA O RESUMO DOS MOTORES NO PAINEL DE INFORMAÇÕES
// ==========================================
void MainWindow::updateAllOverviewPanels() {
    for (int i = 1; i <= 4; ++i) {
        updateMotorOverviewLine(i);
    }
    updateOverviewSummaries();
}

void MainWindow::updateMotorOverviewLine(int motorId) {
    if (m_overviewPanels.isEmpty() || motorId < 1 || motorId > 4) return;

    QStringList motorNames = {"Motor Alpha", "Motor Beta", "Motor Charlie", "Motor Delta"};

    for (QWidget* panel : m_overviewPanels) {
        if (!panel) continue;

        QLabel* lbl = panel->findChild<QLabel*>(QString("lblMotorSummaryLeft_%1").arg(motorId));
        if (!lbl) continue;

        QPushButton* btn = findChild<QPushButton*>(QString("btnManualAction_%1").arg(motorId));
        QComboBox* cbOpMode = findChild<QComboBox*>(QString("cbOperationMode_%1").arg(motorId));
        QComboBox* cbAutoMode = findChild<QComboBox*>(QString("cbAeracaoMode_%1").arg(motorId));
        QDoubleSpinBox* spinTemp = findChild<QDoubleSpinBox*>(QString("spinBoxMaxTemp_%1").arg(motorId));
        QTimeEdit* timeOn = findChild<QTimeEdit*>(QString("timeEditOn_%1").arg(motorId));
        QTimeEdit* timeOff = findChild<QTimeEdit*>(QString("timeEditOff_%1").arg(motorId));

        bool isOn = btn ? btn->property("isMotorOn").toBool() : false;
        QString icon = isOn ? "●" : "○";
        QString stateStr = isOn ? "LIGADO" : "DESLIGADO";

        QString modeStr = "?";
        QString cfgStr;
        if (cbOpMode) {
            if (cbOpMode->currentIndex() == 0) { // Automático
                modeStr = "Automático";
                if (cbAutoMode) {
                    if (cbAutoMode->currentIndex() == 0) { // Horário
                        QString on = timeOn ? timeOn->time().toString("HH:mm") : "--:--";
                        QString off = timeOff ? timeOff->time().toString("HH:mm") : "--:--";
                        cfgStr = QString("Das %1 às %2").arg(on).arg(off);
                    } else { // Temperatura
                        cfgStr = QString("TempMáx %1°C").arg(spinTemp ? QString::number(spinTemp->value(), 'f', 0) : QString("--"));
                    }
                }
            } else { // Manual
                modeStr = "Manual";
            }
        }

        QString text = QString("%1 %2: %3 — %4").arg(icon, motorNames.at(motorId-1), stateStr, modeStr);
        if (!cfgStr.isEmpty()) text = QString("%1 %2: %3 — %4 (%5)").arg(icon, motorNames.at(motorId-1), stateStr, modeStr, cfgStr);
        lbl->setText(text);
        lbl->setProperty("state", isOn ? "on" : "off");
        lbl->style()->unpolish(lbl);
        lbl->style()->polish(lbl);
    }
}

void MainWindow::updateOverviewSummaries() {
    if (m_overviewPanels.isEmpty()) return;

    int totalOn = 0;
    for (int i = 1; i <= 4; ++i) {
        QPushButton* btn = findChild<QPushButton*>(QString("btnManualAction_%1").arg(i));
        if (btn && btn->property("isMotorOn").toBool()) totalOn++;
    }

    for (QWidget* panel : m_overviewPanels) {
        if (!panel) continue;

        // Atualiza o contador total de motores ligados
        QLabel* lblTotal = panel->findChild<QLabel*>("lblMotorTotalLeft");
        if (lblTotal) {
            QString statusTotal = QString("TOTAL: %1/4").arg(totalOn);
            lblTotal->setText(statusTotal);
            lblTotal->setProperty("state", totalOn > 0 ? "on" : "off");
            lblTotal->style()->unpolish(lblTotal);
            lblTotal->style()->polish(lblTotal);
        }

        // Atualiza o status resumido
        QLabel* lblSummary = panel->findChild<QLabel*>("lblMotorSummary");
        if (lblSummary) {
            QString status = totalOn == 0 ? "DESLIGADOS" : QString("LIGADOS: %1/4").arg(totalOn);
            lblSummary->setText(QString("Todos os motores: %1").arg(status));
        }
    }
}

void MainWindow::toggleMotorSummaryPanel() {
    // Não faz nada - o toggle agora é gerenciado pelo GroupBox automaticamente
}

QWidget* MainWindow::createGlobalOverviewPanel() {
    QFrame* panel = new QFrame();
    panel->setObjectName("globalOverviewPanel");
    panel->setFixedWidth(250);

    QVBoxLayout *panelLayout = new QVBoxLayout(panel);
    panelLayout->setSpacing(6);
    panelLayout->setContentsMargins(8, 8, 8, 8);

    QLabel* titleLabel = new QLabel("Visão Geral");
    titleLabel->setObjectName("overviewTitleLabel"); // Nome para estilização global
    panelLayout->addWidget(titleLabel);

    // Label resumida de status geral dos motores
    QLabel* lblMotorSummary = new QLabel(QStringLiteral("Todos os motores: DESLIGADOS"));
    lblMotorSummary->setObjectName("lblMotorSummary");
    panelLayout->addWidget(lblMotorSummary);

    // Título interno - Status dos Motores
    QLabel *lblMotorsTitle = new QLabel("Status dos Motores:");
    lblMotorsTitle->setObjectName("overviewSectionTitle");
    panelLayout->addWidget(lblMotorsTitle);

    // Labels para cada motor
    QLabel *lblMotor1 = new QLabel("Motor Alpha: DESLIGADO");
    lblMotor1->setObjectName("lblMotorSummaryLeft_1");
    lblMotor1->setMouseTracking(true);
    lblMotor1->installEventFilter(this);
    panelLayout->addWidget(lblMotor1);

    QLabel *lblMotor2 = new QLabel("Motor Beta: DESLIGADO");
    lblMotor2->setObjectName("lblMotorSummaryLeft_2");
    lblMotor2->setMouseTracking(true);
    lblMotor2->installEventFilter(this);
    panelLayout->addWidget(lblMotor2);

    QLabel *lblMotor3 = new QLabel("Motor Charlie: DESLIGADO");
    lblMotor3->setObjectName("lblMotorSummaryLeft_3");
    lblMotor3->setMouseTracking(true);
    lblMotor3->installEventFilter(this);
    panelLayout->addWidget(lblMotor3);

    QLabel *lblMotor4 = new QLabel("Motor Delta: DESLIGADO");
    lblMotor4->setObjectName("lblMotorSummaryLeft_4");
    lblMotor4->setMouseTracking(true);
    lblMotor4->installEventFilter(this);
    panelLayout->addWidget(lblMotor4);

    // Total de motores
    QLabel *lblTotal = new QLabel("TOTAL: 0/4");
    lblTotal->setObjectName("lblMotorTotalLeft");
    panelLayout->addWidget(lblTotal);

    // Divisor visual
    QFrame *divider = new QFrame();
    divider->setFrameShape(QFrame::HLine);
    divider->setFrameShadow(QFrame::Sunken);
    panelLayout->addWidget(divider);

    // Título - Médias de Temperatura
    QLabel *lblTempTitle = new QLabel("Médias de Temperatura:");
    lblTempTitle->setObjectName("overviewSectionTitle");
    panelLayout->addWidget(lblTempTitle);

    // Labels de temperatura dos pêndulos
    QLabel *lblTempP1 = new QLabel("Pêndulo 1: -- °C");
    lblTempP1->setObjectName("lblTempPendulum_1");
    panelLayout->addWidget(lblTempP1);

    QLabel *lblTempP2 = new QLabel("Pêndulo 2: -- °C");
    lblTempP2->setObjectName("lblTempPendulum_2");
    panelLayout->addWidget(lblTempP2);

    QLabel *lblTempP3 = new QLabel("Pêndulo 3: -- °C");
    lblTempP3->setObjectName("lblTempPendulum_3");
    panelLayout->addWidget(lblTempP3);

    QLabel *lblTempP4 = new QLabel("Pêndulo 4: -- °C");
    lblTempP4->setObjectName("lblTempPendulum_4");
    panelLayout->addWidget(lblTempP4);

    QLabel *lblTempP5 = new QLabel("Pêndulo 5: -- °C");
    lblTempP5->setObjectName("lblTempPendulum_5");
    panelLayout->addWidget(lblTempP5);

    // Média geral
    QLabel *lblTempGeral = new QLabel("Média Geral: -- °C");
    lblTempGeral->setObjectName("lblTempGeral");
    panelLayout->addWidget(lblTempGeral);

    // Divisor visual
    QFrame *divider2 = new QFrame();
    divider2->setFrameShape(QFrame::HLine);
    divider2->setFrameShadow(QFrame::Sunken);
    panelLayout->addWidget(divider2);

    // Título - Meteorologia
    QLabel *lblMeteoTitle = new QLabel("Estação Meteorológica:");
    lblMeteoTitle->setObjectName("overviewSectionTitle");
    panelLayout->addWidget(lblMeteoTitle);

    QLabel *lblMeteoTemp = new QLabel("Temp. Amb.: -- °C");
    lblMeteoTemp->setObjectName("lblMeteoTempOverview");
    panelLayout->addWidget(lblMeteoTemp);

    QLabel *lblMeteoHumid = new QLabel("Umidade Rel.: -- %");
    lblMeteoHumid->setObjectName("lblMeteoHumidOverview");
    panelLayout->addWidget(lblMeteoHumid);

    panelLayout->addStretch();

    // Adiciona o painel recém-criado à lista de painéis gerenciados.
    m_overviewPanels.append(panel);
    return panel;
}

void MainWindow::setupMeteorologiaTab() {
    QWidget *tab = ui->meteorologiaTab;
    if (!tab) return;

    // Limpa o conteúdo placeholder do .ui
    qDeleteAll(tab->findChildren<QWidget*>(QString(), Qt::FindDirectChildrenOnly));
    delete tab->layout();

    // Layout principal em Grid para máxima flexibilidade
    QGridLayout *mainLayout = new QGridLayout(tab);
    mainLayout->setSpacing(5);
    mainLayout->setContentsMargins(5, 5, 5, 5);

    // === 1. PAINEL ESQUERDO (Seleções) ===
    QWidget *leftSidebar = new QWidget();
    leftSidebar->setObjectName("leftSidebarPanel");
    leftSidebar->setFixedWidth(250);
    QVBoxLayout *leftLayout = new QVBoxLayout(leftSidebar);
    leftLayout->setContentsMargins(0, 0, 0, 0);
    leftLayout->setSpacing(0);
    
    // --- Title Row with Toggle Button ---
    QWidget* titleWidget = new QWidget();
    QHBoxLayout* titleLayout = new QHBoxLayout(titleWidget);
    titleLayout->setContentsMargins(4, 4, 4, 4);
    QLabel *lblListTitle = new QLabel("Seleção de Estações");
    lblListTitle->setStyleSheet("font-weight: bold; font-size: 14px;");
    QPushButton* btnToggleLeft = new QPushButton("");
    btnToggleLeft->setObjectName("panelToggleButton"); // Kept for consistency, though not styled
    btnToggleLeft->setFixedSize(24, 24);
    btnToggleLeft->setToolTip("Minimizar/Maximizar Seleções");
    titleLayout->addWidget(lblListTitle);
    titleLayout->addStretch();
    titleLayout->addWidget(btnToggleLeft);
    leftLayout->addWidget(titleWidget);

    // --- Conteúdo do Painel Esquerdo ---
    QListWidget *list = new QListWidget();
    list->setObjectName("meteorologiaStationList");
    list->addItem("Estação 1");
    list->setCurrentRow(0);
    leftLayout->addWidget(list);
    QWidget* panel = createGlobalOverviewPanel();
    leftLayout->addWidget(panel);
    leftLayout->addStretch();

    // === 2. CONTEÚDO CENTRAL ===
    QStackedWidget* centerStack = new QStackedWidget();
    QWidget *meteoDataPage = new QWidget();
    meteoDataPage->setObjectName("meteoCenterFrame");
    QVBoxLayout *centerLayout = new QVBoxLayout(meteoDataPage);
    centerLayout->setContentsMargins(20, 20, 20, 20);

    QGroupBox* meteoGroup = new QGroupBox("Dados da Estação Meteorológica");
    QFormLayout* formLayout = new QFormLayout(meteoGroup);
    formLayout->setSpacing(10);

    m_meteoTempLabel = new QLabel("-- °C");
    m_meteoHumidLabel = new QLabel("-- %");
    m_meteoTempLabel->setStyleSheet("font-weight: bold;");
    m_meteoHumidLabel->setStyleSheet("font-weight: bold;");

    formLayout->addRow("Temperatura Ambiente:", m_meteoTempLabel);
    formLayout->addRow("Umidade Relativa:", m_meteoHumidLabel);

    centerLayout->addWidget(meteoGroup);
    centerLayout->addStretch();
    centerStack->addWidget(meteoDataPage);

    // === 3. PAINEL DIREITO (Opções) - Reestruturado para ser minimizável ===
    QWidget *rightSidebar = new QWidget();
    rightSidebar->setObjectName("rightSidebarPanel");
    rightSidebar->setFixedWidth(250); // Garante que a largura não mude
    QVBoxLayout* rightMainLayout = new QVBoxLayout(rightSidebar);
    rightMainLayout->setContentsMargins(0, 0, 0, 0);
    rightMainLayout->setSpacing(0);

    // Cria o widget de conteúdo que será o painel "sanfona" (accordion)
    QWidget* accordionContent = new QWidget();
    QVBoxLayout* accordionLayout = new QVBoxLayout(accordionContent);
    accordionLayout->setContentsMargins(0, 0, 0, 0);
    accordionLayout->setSpacing(5);

    // Cria o cabeçalho com o botão de minimizar, que controla o accordionContent
    QWidget* headerWidget = new QWidget();
    QHBoxLayout* headerLayout = new QHBoxLayout(headerWidget);
    headerLayout->setContentsMargins(4, 4, 4, 4);
    QLabel* titleLabel = new QLabel("Opções");
    titleLabel->setStyleSheet("font-weight: bold; font-size: 14px;");
    QPushButton* toggleButton = new QPushButton("");
    toggleButton->setObjectName("panelToggleButton");
    toggleButton->setFixedSize(24, 24);
    toggleButton->setToolTip("Minimizar/Maximizar Opções");
    toggleButton->setCheckable(true);
    toggleButton->setChecked(true);
    headerLayout->addWidget(titleLabel);
    headerLayout->addStretch();
    headerLayout->addWidget(toggleButton);
    connect(toggleButton, &QPushButton::toggled, accordionContent, &QWidget::setVisible);

    // Helper para criar páginas do acordeão.
    auto createAccordionPage = [&](const QString& title, QWidget* contentPage, bool startChecked = false) {
        QWidget* sectionWidget = new QWidget();
        QVBoxLayout* sectionLayout = new QVBoxLayout(sectionWidget);
        sectionLayout->setContentsMargins(0, 0, 0, 0);
        sectionLayout->setSpacing(2);
 
        QPushButton* headerButton = new QPushButton();
        headerButton->setFlat(true);
        headerButton->setCursor(Qt::PointingHandCursor);
        headerButton->setStyleSheet("text-align: left;");
 
        QHBoxLayout* headerLayout = new QHBoxLayout(headerButton);
        headerLayout->setContentsMargins(8, 4, 4, 4);
 
        QLabel* titleLabel = new QLabel(title);
        titleLabel->setStyleSheet("font-weight: bold;");
        titleLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
 
        headerLayout->addWidget(titleLabel);
        headerLayout->addStretch();
 
        QScrollArea* scrollArea = new QScrollArea();
        scrollArea->setWidgetResizable(true);
        scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        scrollArea->setFrameShape(QFrame::NoFrame);
        scrollArea->setWidget(contentPage);
 
        sectionLayout->addWidget(headerButton);
        sectionLayout->addWidget(scrollArea);
        scrollArea->setVisible(startChecked);
 
        connect(headerButton, &QPushButton::clicked, this, [scrollArea](){
            scrollArea->setVisible(!scrollArea->isVisible());
        });
 
        return sectionWidget;
    };

    // --- Página de Ações ---
    QWidget *pageAcoes = new QWidget();
    QVBoxLayout *layoutAcoes = new QVBoxLayout(pageAcoes);
    layoutAcoes->setContentsMargins(12, 16, 12, 12);
    layoutAcoes->setSpacing(10);
 
    QLabel *lblSecao = new QLabel("AÇÕES");
    lblSecao->setStyleSheet("font-weight: bold; font-size: 10px; color: #888; letter-spacing: 1.5px; margin-bottom: 8px;");
    layoutAcoes->addWidget(lblSecao);
 
    QPushButton* btnSortear = new QPushButton("Requisitar Dados");
    btnSortear->setMinimumHeight(32);
    connect(btnSortear, &QPushButton::clicked, this, &MainWindow::onRequestAllData);
    layoutAcoes->addWidget(btnSortear);
    layoutAcoes->addStretch();
 
    accordionLayout->addWidget(createAccordionPage("Ações", pageAcoes, true));
    accordionLayout->addStretch();

    // Adiciona o cabeçalho e o conteúdo ao layout principal da barra lateral
    rightMainLayout->addWidget(headerWidget);
    rightMainLayout->addWidget(accordionContent);
    rightMainLayout->addStretch();

    // === 4. BARRA INFERIOR (Modos de Visualização) ===
    QHBoxLayout* bottomBarLayout = new QHBoxLayout();
    bottomBarLayout->setSpacing(5);
    bottomBarLayout->setContentsMargins(0,0,0,0);
    QPushButton* btnToggleBottom = new QPushButton("");
    btnToggleBottom->setObjectName("panelToggleButton");
    btnToggleBottom->setFixedSize(24, 24);
    btnToggleBottom->setToolTip("Minimizar/Maximizar Modos");
    QTabBar* bottomBar = new QTabBar();
    bottomBar->setObjectName("viewModeTabBar");
    bottomBar->addTab("Dados Atuais");
    // bottomBar->addTab("Histórico"); // Removido temporariamente
    bottomBarLayout->addWidget(btnToggleBottom);
    bottomBarLayout->addWidget(bottomBar, 1);

    // --- A funcionalidade de histórico foi removida temporariamente ---
    /*
    m_meteoChartView = new QChartView();
    m_meteoChartView->setRenderHint(QPainter::Antialiasing);

    QWidget* graphPage = new QWidget(); // Wrapper para o gráfico e os controles
    QVBoxLayout* graphLayout = new QVBoxLayout(graphPage);
    graphLayout->setContentsMargins(5,5,5,5);

    QHBoxLayout* timeControlsLayout = new QHBoxLayout();
    timeControlsLayout->setSpacing(10);
    timeControlsLayout->addStretch();

    QButtonGroup* rangeGroup = new QButtonGroup(this);
    QPushButton* btn24h = new QPushButton("Últimas 24h");
    QPushButton* btn7d = new QPushButton("Últimos 7 dias");
    QPushButton* btn30d = new QPushButton("Últimos 30 dias");
    btn24h->setCheckable(true);
    btn7d->setCheckable(true);
    btn30d->setCheckable(true);
    btn24h->setChecked(true);
    rangeGroup->addButton(btn24h, 1);
    rangeGroup->addButton(btn7d, 7);
    rangeGroup->addButton(btn30d, 30);

    timeControlsLayout->addWidget(btn24h);
    timeControlsLayout->addWidget(btn7d);
    timeControlsLayout->addWidget(btn30d);
    timeControlsLayout->addStretch();

    graphLayout->addLayout(timeControlsLayout);
    graphLayout->addWidget(m_meteoChartView, 1);
    centerStack->addWidget(graphPage);
    connect(rangeGroup, QOverload<int>::of(&QButtonGroup::idClicked), this, &MainWindow::updateMeteoChart);
    */

    // O QStackedWidget agora só tem uma página, então a conexão da barra de abas é desnecessária se houver apenas uma aba.
    connect(bottomBar, &QTabBar::currentChanged, centerStack, &QStackedWidget::setCurrentIndex);

    // Wrapper layout for the central column to stabilize it
    QVBoxLayout* centerVLayout = new QVBoxLayout();
    centerVLayout->setContentsMargins(0,0,0,0);
    centerVLayout->setSpacing(5);
    centerVLayout->addWidget(centerStack, 1); // Content expands
    centerVLayout->addLayout(bottomBarLayout); // Bottom bar stays fixed

    // === 6. MONTAGEM DO GRID LAYOUT ===
    mainLayout->addWidget(leftSidebar, 0, 0, 2, 1, Qt::AlignTop);
    mainLayout->addLayout(centerVLayout, 0, 1, 2, 1); // Central column spans 2 rows
    mainLayout->addWidget(rightSidebar, 0, 2, 2, 1, Qt::AlignTop);

    mainLayout->setColumnStretch(1, 1);

    // === 7. CONEXÕES E ESTADO INICIAL ===
    connect(btnToggleLeft, &QPushButton::clicked, this, [list, panel](){
        list->setVisible(!list->isVisible());
        // O painel de visão geral (panel) permanece visível, pois não é afetado.
    });
    connect(btnToggleBottom, &QPushButton::clicked, this, [bottomBar](){
        bottomBar->setVisible(!bottomBar->isVisible());
    });

    // onSortearMeteoData(); // Não sorteia mais dados iniciais, aguarda a rede.
    // updateMeteoChart(1); // Histórico desativado
}

void MainWindow::onSortearMeteoData()
{
    // Esta função foi desativada. Os dados de meteorologia agora são recebidos
    // exclusivamente pela rede através do NetworkManager e do slot onNewMeteoData.
    qInfo() << "onSortearMeteoData foi chamado, mas está desativado.";
}

/*
 * As funções relacionadas ao histórico e estatísticas (`updateMeteoChart`, `setupEstatisticasTab`, etc.)
 * foram temporariamente desativadas para focar na funcionalidade de rede.
 * O código foi comentado para facilitar a reativação no futuro.
 */

// ==========================================
// FUNÇÕES ADICIONAIS DE PAINEL LATERAL
// ==========================================
void MainWindow::setupTermometria3DTab() {
    if (m_glWidget) return;

    m_glWidget = new SiloGLWidget(this);
    m_glWidget->setObjectName("siloGLWidget");
    m_glWidget->updateSensorPositions(m_sensorMapping);

    QWidget *page3D = ui->viewStackedWidget->widget(2);

    if (page3D) {
        if (page3D->layout() == nullptr) {
            QVBoxLayout *layout = new QVBoxLayout(page3D);
            layout->setContentsMargins(0, 0, 0, 0);
            layout->addWidget(m_glWidget);
        } else {
            page3D->layout()->addWidget(m_glWidget);
        }
    }
}

void MainWindow::onUpdateRedeStatus() {
    if (m_redeStatusLabels.isEmpty()) return;

    auto setLabel = [&](const QString& key, const QString& value) {
        // Helper para atualizar todas as instâncias de um label
        // Esta função foi desativada. O status da rede agora é atualizado
        // em tempo real pelo NetworkManager e pelo slot onNetworkStatusUpdate.
    };
}

void MainWindow::updateLocalIpAddress()
{
    QString ipAddress = "Não encontrado";
    // Itera sobre todas as interfaces de rede
    for (const QNetworkInterface &interface : QNetworkInterface::allInterfaces()) {
        // Filtra por interfaces ativas, não-loopback e que sejam Wi-Fi ou Ethernet
        if (interface.flags().testFlag(QNetworkInterface::IsUp) &&
            !interface.flags().testFlag(QNetworkInterface::IsLoopBack) &&
            (interface.type() == QNetworkInterface::Wifi || interface.type() == QNetworkInterface::Ethernet))
        {
            // Itera sobre os endereços IP da interface
            for (const QNetworkAddressEntry &entry : interface.addressEntries()) {
                // Pega o primeiro endereço IPv4 que encontrar
                if (entry.ip().protocol() == QAbstractSocket::IPv4Protocol) {
                    ipAddress = entry.ip().toString();
                    goto found_ip; // Sai dos loops aninhados assim que encontrar um IP válido
                }
            }
        }
    }
found_ip:
    // Atualiza o label na aba Rede
    if (m_redeStatusLabels.contains("config_local_ip")) {
        m_redeStatusLabels["config_local_ip"]->setText(ipAddress);
    }
}

void MainWindow::initialize3DView() {
    if (m_glWidget) return;
    setupTermometria3DTab();
    setup3DOptionsPanel();
}

void MainWindow::setupRedeTab() {
    QWidget *tab = ui->redeTab;
    if (!tab) return;

    // Limpa o conteúdo placeholder do .ui
    qDeleteAll(tab->findChildren<QWidget*>(QString(), Qt::FindDirectChildrenOnly));
    delete tab->layout();

    // Limpa labels de status antigos para evitar ponteiros inválidos
    m_redeStatusLabels.clear();

    // Layout principal em Grid
    QGridLayout *mainLayout = new QGridLayout(tab);
    mainLayout->setSpacing(10);
    mainLayout->setContentsMargins(10, 10, 10, 10);

    // === 1. PAINEL ESQUERDO (Seleção) ===
    QWidget *leftSidebar = new QWidget();
    leftSidebar->setObjectName("leftSidebarPanel");
    leftSidebar->setFixedWidth(250);
    QVBoxLayout *leftMainLayout = new QVBoxLayout(leftSidebar);
    leftMainLayout->setContentsMargins(0, 0, 0, 0);
    leftMainLayout->setSpacing(0);
    
    // --- Title Row with Toggle Button ---
    QWidget* leftTitleWidget = new QWidget();
    QHBoxLayout* leftTitleLayout = new QHBoxLayout(leftTitleWidget);
    leftTitleLayout->setContentsMargins(4, 4, 4, 4);
    QLabel *lblListTitle = new QLabel("Seleção");
    lblListTitle->setStyleSheet("font-weight: bold; font-size: 14px;");
    QPushButton* btnToggleLeft = new QPushButton("");
    btnToggleLeft->setObjectName("panelToggleButton");
    btnToggleLeft->setFixedSize(24, 24);
    btnToggleLeft->setToolTip("Minimizar/Maximizar Seleção");
    leftTitleLayout->addWidget(lblListTitle);
    leftTitleLayout->addStretch();
    leftTitleLayout->addWidget(btnToggleLeft);
    leftMainLayout->addWidget(leftTitleWidget);

    // --- Conteúdo do Painel Esquerdo (que será minimizado) ---
    QListWidget *selectionList = new QListWidget();
    selectionList->setObjectName("redeSelectionList");
    selectionList->addItems({"Computador Pessoal", "Ponto de Acesso", "Gateway (LoRa)", "Estação Meteorológica", "Nodo Termometria", "Nodo Motor"});
    leftMainLayout->addWidget(selectionList);
    leftMainLayout->addStretch();

    // === 2. CONTEÚDO CENTRAL (QStackedWidget) ===
    QStackedWidget* centralStack = new QStackedWidget();
    centralStack->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    // --- PÁGINA 0: Computador Pessoal / Wi-Fi ---
    QWidget* wifiPage = new QWidget();
    QVBoxLayout* wifiVLayout = new QVBoxLayout(wifiPage);

    QGroupBox* pcGroup = new QGroupBox("Status do Computador Pessoal");
    QFormLayout* pcLayout = new QFormLayout(pcGroup);
    QLabel* pcIpLabel = new QLabel("Detectando...");
    pcIpLabel->setStyleSheet("font-weight: bold;");
    pcLayout->addRow("IP deste Computador (IPv4):", pcIpLabel);
    m_redeStatusLabels.insert("config_local_ip", pcIpLabel);
    wifiVLayout->addWidget(pcGroup);

    QGroupBox* wifiConfigGroup = new QGroupBox("Configuração Wi-Fi (para o Ponto de Acesso)");
    QFormLayout* wifiConfigLayout = new QFormLayout(wifiConfigGroup);
    QLineEdit* ssidEdit = new QLineEdit();
    ssidEdit->setObjectName("wifiSsidEdit");
    ssidEdit->setPlaceholderText("Nome da rede Wi-Fi (Ex: Vilarino_Engenharia)");
    wifiConfigLayout->addRow("SSID Wi-Fi:", ssidEdit);
    QLineEdit* passwordEdit = new QLineEdit();
    passwordEdit->setObjectName("wifiPasswordEdit");
    passwordEdit->setPlaceholderText("Senha da rede Wi-Fi");
    passwordEdit->setEchoMode(QLineEdit::Password);
    wifiConfigLayout->addRow("Senha Wi-Fi:", passwordEdit);
    QPushButton* saveWifiButton = new QPushButton("Salvar Credenciais Wi-Fi");
    wifiConfigLayout->addRow(saveWifiButton);
    wifiVLayout->addWidget(wifiConfigGroup);
    wifiVLayout->addStretch();

    centralStack->addWidget(wifiPage);

    // --- PÁGINA 1: Ponto de Acesso ---
    QWidget* apPage = new QWidget();
    QVBoxLayout* apVLayout = new QVBoxLayout(apPage);
    
    QGroupBox* apGroup = new QGroupBox("Conexão com o Ponto de Acesso (AP)");
    QFormLayout* apLayout = new QFormLayout(apGroup);
    QLineEdit* apIpEdit = new QLineEdit();
    apIpEdit->setObjectName("apIpAddressEdit");
    apIpEdit->setPlaceholderText("Ex: 192.168.1.104");
    apLayout->addRow("Endereço IP do AP:", apIpEdit);
    
    QPushButton* connectApButton = new QPushButton("Salvar IP e Conectar");
    connectApButton->setMinimumHeight(40);

    QLabel* apStatusLabel = new QLabel("Desconectado");
    apStatusLabel->setStyleSheet("font-weight: bold; color: #ef4444;");
    m_redeStatusLabels.insert("config_local_status", apStatusLabel);

    apLayout->addRow(connectApButton);
    apLayout->addRow("Status da Conexão:", apStatusLabel);
    
    apVLayout->addWidget(apGroup);
    apVLayout->addStretch();

    centralStack->addWidget(apPage);

    // --- PÁGINA 2: Gateway ---
    QWidget* gatewayPage = new QWidget();
    QVBoxLayout* gatewayVLayout = new QVBoxLayout(gatewayPage);

    QGroupBox* gatewayGroup = new QGroupBox("Status do Gateway (LoRa)");
    QFormLayout* gatewayLayout = new QFormLayout(gatewayGroup);

    QLabel* gatewayStatusLabel = new QLabel("Aguardando Beacon...");
    gatewayStatusLabel->setStyleSheet("font-style: italic; color: #888;");
    gatewayLayout->addRow("Status da Conexão:", gatewayStatusLabel);
    m_redeStatusLabels.insert("status_gateway_status", gatewayStatusLabel);

    QLabel* rssiLabel = new QLabel("N/A");
    rssiLabel->setStyleSheet("font-style: italic; color: #888;");
    gatewayLayout->addRow("Sinal LoRa (GW <-> AP):", rssiLabel);
    m_redeStatusLabels.insert("status_gateway_details", rssiLabel);

    QLabel* gatewayLastSeenLabel = new QLabel("N/A");
    gatewayLayout->addRow("Última Recepção:", gatewayLastSeenLabel);
    m_redeStatusLabels.insert("status_gateway_last_seen", gatewayLastSeenLabel);

    QPushButton* requestStatusButton = new QPushButton("Solicitar Status Agora");
    requestStatusButton->setToolTip("Envia uma requisição para o Gateway responder seu status imediatamente.");
    gatewayLayout->addRow(requestStatusButton);
    connect(requestStatusButton, &QPushButton::clicked, this, &MainWindow::onRequestGatewayStatus);

    gatewayVLayout->addWidget(gatewayGroup);
    gatewayVLayout->addStretch();
    centralStack->addWidget(gatewayPage);

    // --- PÁGINA 3: Estação Meteorológica ---
    QWidget* meteoPage = new QWidget();
    QVBoxLayout* meteoVLayout = new QVBoxLayout(meteoPage);
    QGroupBox* meteoGroup = new QGroupBox("Status da Estação Meteorológica");
    QFormLayout* meteoLayout = new QFormLayout(meteoGroup);
    QLabel* meteoStatusLabel = new QLabel("Offline");
    meteoStatusLabel->setStyleSheet("font-weight: bold; color: #ef4444;");
    meteoLayout->addRow("Status da Conexão:", meteoStatusLabel);
    m_redeStatusLabels.insert("status_meteo_status", meteoStatusLabel);
    QLabel* meteoDetailsLabel = new QLabel("N/A");
    meteoLayout->addRow("Sinal/Detalhes:", meteoDetailsLabel);
    m_redeStatusLabels.insert("status_meteo_details", meteoDetailsLabel);
    QLabel* meteoLastSeenLabel = new QLabel("N/A");
    meteoLayout->addRow("Última Recepção:", meteoLastSeenLabel);
    m_redeStatusLabels.insert("status_meteo_last_seen", meteoLastSeenLabel);
    QLabel* meteoInfoLabel = new QLabel("N/A");
    meteoLayout->addRow("Informação Recebida:", meteoInfoLabel);
    m_redeStatusLabels.insert("status_meteo_info", meteoInfoLabel);
    meteoVLayout->addWidget(meteoGroup);
    meteoVLayout->addStretch();
    centralStack->addWidget(meteoPage);

    // --- PÁGINA 4: Nodo Termometria ---
    QWidget* termPage = new QWidget();
    QVBoxLayout* termVLayout = new QVBoxLayout(termPage);
    QGroupBox* termGroup = new QGroupBox("Status do Nodo de Termometria");
    QFormLayout* termLayout = new QFormLayout(termGroup);
    QLabel* termStatusLabel = new QLabel("Offline");
    termStatusLabel->setStyleSheet("font-weight: bold; color: #ef4444;");
    termLayout->addRow("Status da Conexão:", termStatusLabel);
    m_redeStatusLabels.insert("status_termometria_status", termStatusLabel);
    QLabel* termDetailsLabel = new QLabel("N/A");
    termLayout->addRow("Sinal/Detalhes:", termDetailsLabel);
    m_redeStatusLabels.insert("status_termometria_details", termDetailsLabel);
    QLabel* termLastSeenLabel = new QLabel("N/A");
    termLayout->addRow("Última Recepção:", termLastSeenLabel);
    m_redeStatusLabels.insert("status_termometria_last_seen", termLastSeenLabel);
    QLabel* termInfoLabel = new QLabel("N/A");
    termLayout->addRow("Informação Recebida:", termInfoLabel);
    m_redeStatusLabels.insert("status_termometria_info", termInfoLabel);
    termVLayout->addWidget(termGroup);
    termVLayout->addStretch();
    centralStack->addWidget(termPage);

    // --- PÁGINA 5: Nodo Motor ---
    QWidget* motorPage = new QWidget();
    QVBoxLayout* motorVLayout = new QVBoxLayout(motorPage);
    QGroupBox* motorGroup = new QGroupBox("Status do Nodo Motor");
    QFormLayout* motorLayout = new QFormLayout(motorGroup);
    QLabel* motorStatusLabel = new QLabel("Offline");
    motorStatusLabel->setStyleSheet("font-weight: bold; color: #ef4444;");
    motorLayout->addRow("Status da Conexão:", motorStatusLabel);
    m_redeStatusLabels.insert("status_motor_status", motorStatusLabel);
    QLabel* motorDetailsLabel = new QLabel("N/A");
    motorLayout->addRow("Sinal/Detalhes:", motorDetailsLabel);
    m_redeStatusLabels.insert("status_motor_details", motorDetailsLabel);
    QLabel* motorLastSeenLabel = new QLabel("N/A");
    motorLayout->addRow("Última Recepção:", motorLastSeenLabel);
    m_redeStatusLabels.insert("status_motor_last_seen", motorLastSeenLabel);
    QLabel* motorInfoLabel = new QLabel("N/A");
    motorLayout->addRow("Informação Recebida:", motorInfoLabel);
    m_redeStatusLabels.insert("status_motor_info", motorInfoLabel);
    motorVLayout->addWidget(motorGroup);
    motorVLayout->addStretch();
    centralStack->addWidget(motorPage);

    // === 3. PAINEL DIREITO (Opções Dinâmicas) ===
    QStackedWidget* optionsStack = new QStackedWidget();
    optionsStack->setObjectName("redeOptionsStack");
    optionsStack->setFixedWidth(250);

    // Helper para criar um painel de opções padrão
    auto createOptionsPage = [&](const QString& title, QLayout* contentLayout) -> QWidget* {
        QWidget* page = new QWidget();
        QVBoxLayout* mainLayout = new QVBoxLayout(page);
        mainLayout->setContentsMargins(0, 0, 0, 0);
        mainLayout->setSpacing(0);

        QGroupBox* group = new QGroupBox(title);
        group->setLayout(contentLayout);

        mainLayout->addWidget(group);
        mainLayout->addStretch();
        return page;
    };

    // --- PÁGINA DE OPÇÕES 0: Computador Pessoal ---
    QVBoxLayout* pcOptionsLayout = new QVBoxLayout();
    QPushButton* requestDataButton = new QPushButton("Requisitar Dados Agora");
    requestDataButton->setToolTip("Envia uma requisição para todos os nodos responderem seu status imediatamente.");
    requestDataButton->setMinimumHeight(32);
    connect(requestDataButton, &QPushButton::clicked, this, &MainWindow::onRequestAllData);
    pcOptionsLayout->addWidget(requestDataButton);
    pcOptionsLayout->addStretch();
    optionsStack->addWidget(createOptionsPage("Ações", pcOptionsLayout));

    // --- PÁGINA DE OPÇÕES 1: Ponto de Acesso ---
    QVBoxLayout* apOptionsLayout = new QVBoxLayout();
    QPushButton* toggleApBeaconButton = new QPushButton("Desligar Beacon (AP)");
    toggleApBeaconButton->setObjectName("toggleBeaconButton"); // Nome antigo para manter o estilo
    toggleApBeaconButton->setToolTip("Liga/Desliga o pacote de 'sinal de vida' que o Access Point envia para este computador.");
    toggleApBeaconButton->setEnabled(false); // Começa desabilitado
    toggleApBeaconButton->setMinimumHeight(32);
    connect(toggleApBeaconButton, &QPushButton::clicked, this, &MainWindow::onToggleBeaconClicked);
    apOptionsLayout->addWidget(toggleApBeaconButton);
    apOptionsLayout->addStretch();
    optionsStack->addWidget(createOptionsPage("Controle do Ponto de Acesso", apOptionsLayout));

    // --- PÁGINA DE OPÇÕES 2: Gateway ---
    QVBoxLayout* gwOptionsLayout = new QVBoxLayout();
    QPushButton* toggleGwBeaconButton = new QPushButton("Desligar Beacon (Gateway)");
    toggleGwBeaconButton->setObjectName("toggleGatewayBeaconButton");
    toggleGwBeaconButton->setToolTip("Liga/Desliga o pacote de 'sinal de vida' que o Gateway envia para o Access Point via LoRa.");
    toggleGwBeaconButton->setMinimumHeight(32);
    connect(toggleGwBeaconButton, &QPushButton::clicked, this, &MainWindow::onToggleGatewayBeacon);
    gwOptionsLayout->addWidget(toggleGwBeaconButton);
    gwOptionsLayout->addStretch();
    optionsStack->addWidget(createOptionsPage("Controle do Gateway", gwOptionsLayout));

    // --- PÁGINA DE OPÇÕES 3: Estação Meteorológica ---
    QVBoxLayout* meteoOptionsLayout = new QVBoxLayout();
    QPushButton* requestMeteoDataButton = new QPushButton("Requisitar Dados Agora");
    requestMeteoDataButton->setToolTip("Envia uma requisição para a estação meteorológica responder imediatamente.");
    requestMeteoDataButton->setMinimumHeight(32);
    connect(requestMeteoDataButton, &QPushButton::clicked, this, &MainWindow::onRequestAllData);
    meteoOptionsLayout->addWidget(requestMeteoDataButton);
    meteoOptionsLayout->addStretch();
    optionsStack->addWidget(createOptionsPage("Ações", meteoOptionsLayout));

    // --- PÁGINA DE OPÇÕES 4: Termometria ---
    QVBoxLayout* termOptionsLayout = new QVBoxLayout();
    QPushButton* requestTermDataButton = new QPushButton("Requisitar Dados Agora");
    requestTermDataButton->setToolTip("Envia uma requisição para o nodo de termometria responder imediatamente.");
    requestTermDataButton->setMinimumHeight(32);
    connect(requestTermDataButton, &QPushButton::clicked, this, &MainWindow::onRequestAllData);
    termOptionsLayout->addWidget(requestTermDataButton);
    termOptionsLayout->addStretch();
    optionsStack->addWidget(createOptionsPage("Ações", termOptionsLayout));

    // --- PÁGINA DE OPÇÕES 5: Motor ---
    QVBoxLayout* motorOptionsLayout = new QVBoxLayout();
    QPushButton* requestMotorDataButton = new QPushButton("Requisitar Dados Agora");
    requestMotorDataButton->setToolTip("Envia uma requisição para o nodo motor responder imediatamente.");
    requestMotorDataButton->setMinimumHeight(32);
    connect(requestMotorDataButton, &QPushButton::clicked, this, &MainWindow::onRequestAllData);
    motorOptionsLayout->addWidget(requestMotorDataButton);
    motorOptionsLayout->addStretch();
    optionsStack->addWidget(createOptionsPage("Ações", motorOptionsLayout));

    // === 4. LOG DE EVENTOS (Abaixo) ===
    QGroupBox* logGroup = new QGroupBox("Log de Eventos do Sistema");
    QVBoxLayout* logLayout = new QVBoxLayout(logGroup);
    m_logDisplay = new QPlainTextEdit();
    m_logDisplay->setReadOnly(true);
    m_logDisplay->setFont(QFont("Consolas", 9));
    m_logDisplay->setStyleSheet("background-color: #020617; color: #e2e8f0;"); // Fundo bem escuro
    logLayout->addWidget(m_logDisplay);
    logGroup->setMinimumHeight(150);
    logGroup->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::MinimumExpanding);

    // === 5. MONTAGEM DO GRID LAYOUT ===
    mainLayout->addWidget(leftSidebar, 0, 0);
    mainLayout->addWidget(centralStack, 0, 1);
    mainLayout->addWidget(optionsStack, 0, 2);
    mainLayout->addWidget(logGroup, 1, 0, 1, 3); // Span log across all columns
    mainLayout->setColumnStretch(1, 1); // Central content expands
    mainLayout->setRowStretch(0, 1); // Top row (content) expands
    mainLayout->setRowStretch(1, 1); // Bottom row (log) also expands

    // === 6. CONEXÕES E ESTADO INICIAL ===
    connect(selectionList, &QListWidget::currentRowChanged, centralStack, &QStackedWidget::setCurrentIndex);
    connect(selectionList, &QListWidget::currentRowChanged, optionsStack, &QStackedWidget::setCurrentIndex);
    
    // Botão na página "Computador Pessoal" salva apenas as credenciais Wi-Fi para referência.
    connect(saveWifiButton, &QPushButton::clicked, this, [this]() {
        QLineEdit* ssidEdit = findChild<QLineEdit*>("wifiSsidEdit");
        QLineEdit* passwordEdit = findChild<QLineEdit*>("wifiPasswordEdit");
        if (ssidEdit && passwordEdit) {
            QSettings settings("VilarinoEngenharia", "GrainMonitor");
            settings.setValue("network/ssid", ssidEdit->text().trimmed());
            settings.setValue("network/password", passwordEdit->text());
            qInfo() << "Credenciais Wi-Fi salvas (para referência do Ponto de Acesso).";
            QMessageBox::information(this, "Salvo", "Credenciais Wi-Fi salvas com sucesso.");
        }
    });

    // Botão na página "Ponto de Acesso" salva o IP e inicia a conexão.
    connect(connectApButton, &QPushButton::clicked, this, &MainWindow::onSaveAndConnectNetwork);
    // Conexão para minimizar/maximizar o painel de seleção
    connect(btnToggleLeft, &QPushButton::clicked, this, [selectionList](){
        selectionList->setVisible(!selectionList->isVisible());
    });

    selectionList->setCurrentRow(0);
}

void MainWindow::setup3DOptionsPanel() {
    // =======================================================================================
    // ROBUST ACCORDION IMPLEMENTATION USING QGROUPBOX
    // This replaces the problematic QToolBox with a custom, reliable accordion menu.
    // =======================================================================================

    // 1. Create a new container widget for our custom accordion.
    // This will replace the QToolBox from the UI file.
    QWidget* accordionContainer = new QWidget();
    accordionContainer->setObjectName("accordion3DContainer");
    // Define uma largura fixa para o painel de opções, garantindo estabilidade visual.
    accordionContainer->setFixedWidth(250);

    QVBoxLayout* accordionLayout = new QVBoxLayout(accordionContainer);
    accordionLayout->setContentsMargins(0, 0, 0, 0);
    accordionLayout->setSpacing(5);

    QList<QScrollArea*> scrollAreas;

    // 2. Helper function to create each collapsible page.
    auto createAccordionPage = [&](const QString& title, QWidget* contentPage, bool startChecked = false) {
        QWidget* sectionWidget = new QWidget();
        QVBoxLayout* sectionLayout = new QVBoxLayout(sectionWidget);
        sectionLayout->setContentsMargins(0, 0, 0, 0);
        sectionLayout->setSpacing(2);

        QPushButton* headerButton = new QPushButton();
        headerButton->setFlat(true);
        headerButton->setCursor(Qt::PointingHandCursor);
        headerButton->setStyleSheet("text-align: left;");

        QHBoxLayout* headerLayout = new QHBoxLayout(headerButton);
        headerLayout->setContentsMargins(8, 4, 4, 4);

        QLabel* titleLabel = new QLabel(title);
        titleLabel->setStyleSheet("font-weight: bold;");
        titleLabel->setAttribute(Qt::WA_TransparentForMouseEvents);

        headerLayout->addWidget(titleLabel);
        headerLayout->addStretch();

        QScrollArea* scrollArea = new QScrollArea();
        scrollArea->setWidgetResizable(true);
        scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        scrollArea->setFrameShape(QFrame::NoFrame);
        scrollArea->setWidget(contentPage);

        sectionLayout->addWidget(headerButton);
        sectionLayout->addWidget(scrollArea);
        scrollArea->setVisible(startChecked);

        connect(headerButton, &QPushButton::clicked, this, [scrollArea](){
            scrollArea->setVisible(!scrollArea->isVisible());
        });

        titleLabel->setStyleSheet("font-weight: bold;");
        // Permite que cliques no label passem para o botão pai.
        titleLabel->setAttribute(Qt::WA_TransparentForMouseEvents);

        headerLayout->addWidget(titleLabel);
        headerLayout->addStretch();

        scrollAreas.append(scrollArea);
        return sectionWidget;
    };

    QSlider *localSliderOpacidade = new QSlider(Qt::Horizontal);
    localSliderOpacidade->setMinimum(0);
    localSliderOpacidade->setMaximum(100);
    localSliderOpacidade->setValue(40);
    localSliderOpacidade->setObjectName("sliderOpacidadeLocal");
    localSliderOpacidade->setMinimumHeight(24);

    QSlider *localSliderDensidade = new QSlider(Qt::Horizontal);
    localSliderDensidade->setMinimum(0);
    localSliderDensidade->setMaximum(100);
    localSliderDensidade->setValue(100);
    localSliderDensidade->setObjectName("sliderDensidadeLocal");
    localSliderDensidade->setMinimumHeight(24);

    // ===== PÁGINA 0: TEMAS =====
    QWidget *pageTemas = new QWidget();
    QVBoxLayout *layoutTemas = new QVBoxLayout(pageTemas);
    layoutTemas->setSpacing(10);
    layoutTemas->setContentsMargins(12, 16, 12, 12);

    QLabel *lblSecao = new QLabel("APARÊNCIA");
    lblSecao->setStyleSheet("font-weight: bold; font-size: 10px; color: #888; letter-spacing: 1.5px; margin-bottom: 8px;");
    layoutTemas->addWidget(lblSecao);

    QPushButton *btnTheme1 = new QPushButton("Tema Padrão (Escuro)");
    QPushButton *btnTheme2 = new QPushButton("Tema Alto Contraste");
    QPushButton *btnTheme3 = new QPushButton("Tema Rural (Dia/Noite)");
    layoutTemas->addWidget(btnTheme1);
    layoutTemas->addWidget(btnTheme2);
    layoutTemas->addWidget(btnTheme3);
    layoutTemas->addStretch();

    // ===== PÁGINA 2: VISIBILIDADE =====
    QWidget *pageMostrar = new QWidget();
    QVBoxLayout *layoutMostrar = new QVBoxLayout(pageMostrar);
    layoutMostrar->setSpacing(8);
    layoutMostrar->setContentsMargins(12, 16, 12, 12);

    lblSecao = new QLabel("OBJETOS");
    lblSecao->setStyleSheet("font-weight: bold; font-size: 10px; color: #888; letter-spacing: 1.5px; margin-bottom: 8px;");
    layoutMostrar->addWidget(lblSecao);

    QPushButton *btnSensors = new QPushButton("Sensores [S]");
    QPushButton *btnValues = new QPushButton("Valores [N]");
    btnValues->setCheckable(true);
    btnValues->setChecked(true);

    // Sub-painel com os checkboxes para cada pêndulo individual
    QWidget *containerPendulums = new QWidget();
    QVBoxLayout *layoutPendulums = new QVBoxLayout(containerPendulums);
    layoutPendulums->setContentsMargins(15, 4, 4, 4); // Indentado
    layoutPendulums->setSpacing(6);

    for (int i = 0; i < 5; ++i) {
        QCheckBox *chkPendulum = new QCheckBox(QString("Pêndulo %1").arg(i + 1));
        chkPendulum->setChecked(true);
        chkPendulum->setStyleSheet("QCheckBox { font-size: 11px; color: #ccc; padding: 2px; }");
        layoutPendulums->addWidget(chkPendulum);

        connect(chkPendulum, &QCheckBox::toggled, this, [this, i](bool checked) {
            if (m_glWidget) {
                m_glWidget->setPendulumValueVisible(i, checked);
            }
        });
    }

    QPushButton *btnHeatmap = new QPushButton("Heatmap [H]");
    QPushButton *btnRoof = new QPushButton("Teto [R]");
    QPushButton *btnWireframe = new QPushButton("Wireframe [W]");
    QPushButton *btnOpacity = new QPushButton("Opacidade [T]");
    for (QPushButton *b : {btnSensors, btnValues, btnHeatmap, btnRoof, btnWireframe, btnOpacity}) {
        b->setMaximumHeight(30);
    }

    QLabel *lblOpacidade = new QLabel("Opacidade da Parede");
    lblOpacidade->setStyleSheet("font-size: 10px; color: #ccc; margin-top: 4px; margin-left: 10px;");
    QLabel *lblDensidade = new QLabel("Densidade do Heatmap");
    lblDensidade->setStyleSheet("font-size: 10px; color: #ccc; margin-top: 4px; margin-left: 10px;");

    layoutMostrar->addWidget(btnSensors);
    layoutMostrar->addWidget(btnValues);
    layoutMostrar->addWidget(containerPendulums);
    layoutMostrar->addWidget(btnHeatmap);
    layoutMostrar->addWidget(lblDensidade);
    layoutMostrar->addWidget(localSliderDensidade);
    layoutMostrar->addWidget(btnRoof);
    layoutMostrar->addWidget(btnWireframe);
    layoutMostrar->addWidget(btnOpacity);
    layoutMostrar->addWidget(lblOpacidade);
    layoutMostrar->addWidget(localSliderOpacidade);
    layoutMostrar->addStretch();

    // ===== PÁGINA 3: CÂMERA =====
    QWidget *pageZoom = new QWidget();
    QVBoxLayout *layoutZoom = new QVBoxLayout(pageZoom);
    layoutZoom->setSpacing(8);
    layoutZoom->setContentsMargins(12, 16, 12, 12);

    lblSecao = new QLabel("CONTROLE");
    lblSecao->setStyleSheet("font-weight: bold; font-size: 10px; color: #888; letter-spacing: 1.5px; margin-bottom: 8px;");
    layoutZoom->addWidget(lblSecao);

    QPushButton *btnZoomIn = new QPushButton("Zoom +");
    QPushButton *btnZoomOut = new QPushButton("Zoom -");
    QPushButton *btnResetCam = new QPushButton("Resetar Câmera [C]");
    btnZoomIn->setMaximumHeight(36);
    btnZoomOut->setMaximumHeight(36);
    btnResetCam->setMaximumHeight(32);

    layoutZoom->addWidget(btnZoomIn);
    layoutZoom->addWidget(btnZoomOut);
    layoutZoom->addSpacing(6);
    layoutZoom->addWidget(btnResetCam);
    layoutZoom->addStretch();

    // ===== PÁGINA 4: SISTEMA =====
    QWidget *pageSistema = new QWidget();
    QVBoxLayout *layoutSistema = new QVBoxLayout(pageSistema);
    layoutSistema->setSpacing(8);
    layoutSistema->setContentsMargins(12, 16, 12, 12);

    QLabel *lblSecaoPaleta = new QLabel("PALETA DE CORES");
    lblSecaoPaleta->setStyleSheet("font-weight: bold; font-size: 10px; color: #888; letter-spacing: 1.5px; margin-bottom: 8px;");
    layoutSistema->addWidget(lblSecaoPaleta);

    m_cbPalette3D = new QComboBox();
    m_cbPalette3D->setMinimumHeight(32);
    m_cbPalette3D->addItem("Ironbow", 0);
    m_cbPalette3D->addItem("Semáforo (RYG)", 1);
    m_cbPalette3D->addItem("Divergente (Azul-Vermelho)", 2);
    m_cbPalette3D->addItem("Plasma / Viridis (CVD)", 3);
    m_cbPalette3D->addItem("Normal", 4);
    m_cbPalette3D->setObjectName("cbPaletteSelector3D");
    m_cbPalette3D->setCurrentIndex(SensorWidget::activePalette());
    m_cbPalette3D->setStyleSheet(
        "QComboBox { "
        "  background-color: #1e293b; "
        "  color: #f1f5f9; "
        "  border: 1px solid #475569; "
        "  border-radius: 6px; "
        "  padding: 4px 8px; "
        "} "
        "QComboBox::drop-down { "
        "  border: none; "
        "}"
    );
    layoutSistema->addWidget(m_cbPalette3D);
    connect(m_cbPalette3D, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MainWindow::onPaletteChanged);

    layoutSistema->addSpacing(10);

    lblSecao = new QLabel("AÇÕES");
    lblSecao->setStyleSheet("font-weight: bold; font-size: 10px; color: #888; letter-spacing: 1.5px; margin-bottom: 8px;");
    layoutSistema->addWidget(lblSecao);

    QPushButton *btnRandom = new QPushButton("Requisitar Dados");
    QPushButton *btnExportTermo = new QPushButton("Exportar JSON");
    btnRandom->setMaximumHeight(32);
    btnExportTermo->setMaximumHeight(32);

    layoutSistema->addWidget(btnRandom);
    layoutSistema->addWidget(btnExportTermo);
    layoutSistema->addStretch();

    connect(btnSensors, &QPushButton::clicked, this, [this] { if (m_glWidget) m_glWidget->toggleSensors(); });
    connect(btnValues, &QPushButton::toggled, this, [this](bool checked) {
        if (m_glWidget) m_glWidget->showTemperatures(checked);
    });
    connect(m_glWidget, &SiloGLWidget::showValuesChanged, this, [btnValues, containerPendulums](bool show) {
        btnValues->blockSignals(true);
        btnValues->setChecked(show);
        btnValues->blockSignals(false);
        containerPendulums->setVisible(show);
    });
    connect(btnHeatmap, &QPushButton::clicked, this, [this] { if (m_glWidget) m_glWidget->toggleHeatmap(); });
    connect(btnRoof, &QPushButton::clicked, this, [this] { if (m_glWidget) m_glWidget->toggleRoof(); });
    connect(btnWireframe, &QPushButton::clicked, this, [this] { if (m_glWidget) m_glWidget->toggleWireframe(); });
    connect(btnOpacity, &QPushButton::clicked, this, [this] { if (m_glWidget) m_glWidget->cycleOpacity(); });
    connect(btnResetCam, &QPushButton::clicked, this, [this] { if (m_glWidget) m_glWidget->resetCamera(); });
    connect(btnZoomIn, &QPushButton::clicked, this, [this] { if (m_glWidget) m_glWidget->zoomIn(); });
    connect(btnZoomOut, &QPushButton::clicked, this, [this] { if (m_glWidget) m_glWidget->zoomOut(); });
    connect(btnRandom, &QPushButton::clicked, this, &MainWindow::onRequestAllData);
    connect(btnExportTermo, &QPushButton::clicked, this, &MainWindow::exportarTermometriaJson);
    // Conecta os botões de tema para atualizar ambas as visualizações (2D e 3D)
    connect(btnTheme1, &QPushButton::clicked, this, [this] { if (m_glWidget) m_glWidget->setTheme1(); if (m_silo2DWidget) m_silo2DWidget->setCurrentTheme(1); });
    connect(btnTheme2, &QPushButton::clicked, this, [this] { if (m_glWidget) m_glWidget->setTheme2(); if (m_silo2DWidget) m_silo2DWidget->setCurrentTheme(2); });
    connect(btnTheme3, &QPushButton::clicked, this, [this] { if (m_glWidget) m_glWidget->setTheme3(); if (m_silo2DWidget) m_silo2DWidget->setCurrentTheme(3); });
    connect(localSliderOpacidade, &QSlider::valueChanged, this, [=](int val) {
        if (m_glWidget) m_glWidget->setOpacity(val / 100.0f);
    });
    connect(localSliderDensidade, &QSlider::valueChanged, this, [=](int val) {
        if (m_glWidget) m_glWidget->setHeatmapDensitySlider(6 - (val / 20));
    });

    // ===== PÁGINA 5: EXTRAS =====
    {
        QWidget *pageExtras = new QWidget();
        QVBoxLayout *layoutExtras = new QVBoxLayout(pageExtras);
        layoutExtras->setSpacing(12);
        layoutExtras->setContentsMargins(12, 16, 12, 12);

        QLabel *lblExtras = new QLabel("Configurações Extras");
        lblExtras->setStyleSheet("font-weight: bold; font-size: 12px; color: #888; margin-bottom: 6px;");
        layoutExtras->addWidget(lblExtras);

        QString chkStyle = "QCheckBox { padding: 6px; font-size: 12px; }";

        QCheckBox *chkKeyboard = new QCheckBox("Habilitar Atalhos de Teclado");
        chkKeyboard->setChecked(true);
        chkKeyboard->setStyleSheet(chkStyle);
        chkKeyboard->setMinimumHeight(32);

        QCheckBox *chkCompass = new QCheckBox("Habilitar Rosa dos Ventos");
        chkCompass->setChecked(false);
        chkCompass->setStyleSheet(chkStyle);
        chkCompass->setMinimumHeight(32);

        layoutExtras->addWidget(chkKeyboard);
        layoutExtras->addWidget(chkCompass);
        layoutExtras->addStretch();

        connect(chkKeyboard, &QCheckBox::toggled, this, [this](bool enabled) { if (m_glWidget) m_glWidget->setKeyboardEnabled(enabled); });
        connect(chkCompass, &QCheckBox::toggled, this, [this](bool enabled) { if (m_glWidget) m_glWidget->setShowCompass(enabled); });

        // 3. Adiciona as páginas criadas como GroupBoxes ao layout do acordeão
        accordionLayout->addWidget(createAccordionPage("Temas", pageTemas, true)); // Começa com a primeira aba aberta
        accordionLayout->addWidget(createAccordionPage("Visibilidade", pageMostrar));
        accordionLayout->addWidget(createAccordionPage("Câmera", pageZoom));
        accordionLayout->addWidget(createAccordionPage("Sistema", pageSistema));
        accordionLayout->addWidget(createAccordionPage("Extras", pageExtras));

        // =======================================================================================
        // PADRONIZA A ALTURA DAS PÁGINAS E ADICIONA SCROLL PARA EVITAR OVERFLOW
        // Define uma altura MÁXIMA para a área de conteúdo de cada aba.
        // Isso permite que abas com pouco conteúdo sejam pequenas, enquanto abas com
        // muito conteúdo se tornam roláveis em vez de forçar a janela a crescer.
        // =======================================================================================
        const int contentMaxHeight = 350; // Altura máxima pré-estabelecida
        for (QScrollArea* sa : scrollAreas) {
            sa->setMaximumHeight(contentMaxHeight);
        }
    }

    // 4. Conecta os sinais para o comportamento de acordeão exclusivo.
    // O comportamento de acordeão exclusivo foi removido para permitir que múltiplas
    // seções fiquem abertas simultaneamente, oferecendo mais flexibilidade ao usuário.

    // 5. Adiciona um espaçador no final para empurrar todos os itens para o topo.
    accordionLayout->addStretch();

    // 6. PASSO CRÍTICO: Substitui o QToolBox antigo pelo nosso novo container de acordeão.
    QLayout* parentLayout = ui->toolBoxOptions->parentWidget()->layout();
    if (parentLayout) {
        // Replace the widget in the layout.
        parentLayout->replaceWidget(ui->toolBoxOptions, accordionContainer);

        // The old widget is now outside the layout system. Hide it and mark for deletion.
        ui->toolBoxOptions->setVisible(false);
        ui->toolBoxOptions->deleteLater();

        // Update the member pointer to our new container so it can be managed (shown/hidden).
        m_options3DContainer = accordionContainer;
    } else {
        qWarning() << "Could not find parent layout for toolBoxOptions to replace it.";
    }
}


// ==========================================
// FUNÇÕES ORIGINAIS DE SETUP
// ==========================================
void MainWindow::setupSiloSidebar() {
    ui->siloList->clear();
    ui->siloList->addItem("Silo Alpha");
    ui->siloList->addItem("Silo Beta");
    ui->siloList->addItem("Silo Charlie");
    ui->siloList->addItem("Silo Delta");

    connect(ui->siloList, &QListWidget::itemClicked, this, &MainWindow::onSiloSelected);
    ui->siloList->setCurrentRow(0);
    updateSensorGrid("Silo Alpha");
}

void MainWindow::setupViewTabs() {
    ui->viewTabBar->addTab("Sensores (Grade)");
    ui->viewTabBar->addTab("Visualização 2D");
    ui->viewTabBar->addTab("Simulação 3D (OpenGL)");
    connect(ui->viewTabBar, &QTabBar::currentChanged, this, &MainWindow::onViewModeChanged);

    // Adiciona o widget de visualização 2D, substituindo o placeholder
    m_silo2DWidget = new Silo2DWidget(this);
    m_silo2DWidget->updateSensorPositions(m_sensorMapping);
    QWidget *page2D = ui->viewStackedWidget->widget(1);
    if (page2D) {
        // Limpa o layout antigo (que continha o QLabel)
        QLayoutItem* item;
        while ((item = page2D->layout()->takeAt(0)) != nullptr) {
            if (item->widget()) {
                item->widget()->deleteLater();
            }
            delete item;
        }
        // Adiciona o novo widget 2D
        page2D->layout()->addWidget(m_silo2DWidget);
    }

    // Criar containers separados para cada modo de visualização
    setupGridOptionsPanel();
    setup2DOptionsPanel();
}

void MainWindow::setupGridOptionsPanel() {
    // Cria container para opções da Grid View, seguindo o padrão de "acordeão".
    m_optionsGridContainer = new QWidget();
    m_optionsGridContainer->setObjectName("m_optionsGridContainer");
    m_optionsGridContainer->setFixedWidth(250);

    QVBoxLayout* accordionLayout = new QVBoxLayout(m_optionsGridContainer);
    accordionLayout->setContentsMargins(0, 0, 0, 0);
    accordionLayout->setSpacing(5);

    // Helper para criar páginas do acordeão.
    auto createAccordionPage = [&](const QString& title, QWidget* contentPage, bool startChecked = false) {
        QWidget* sectionWidget = new QWidget();
        QVBoxLayout* sectionLayout = new QVBoxLayout(sectionWidget);
        sectionLayout->setContentsMargins(0, 0, 0, 0);
        sectionLayout->setSpacing(2);

        QPushButton* headerButton = new QPushButton();
        headerButton->setFlat(true);
        headerButton->setCursor(Qt::PointingHandCursor);
        headerButton->setStyleSheet("text-align: left;");

        QHBoxLayout* headerLayout = new QHBoxLayout(headerButton);
        headerLayout->setContentsMargins(8, 4, 4, 4);

        QLabel* titleLabel = new QLabel(title);
        titleLabel->setStyleSheet("font-weight: bold;");
        titleLabel->setAttribute(Qt::WA_TransparentForMouseEvents);

        headerLayout->addWidget(titleLabel);
        headerLayout->addStretch();

        QScrollArea* scrollArea = new QScrollArea();
        scrollArea->setWidgetResizable(true);
        scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        scrollArea->setFrameShape(QFrame::NoFrame);
        scrollArea->setWidget(contentPage);

        sectionLayout->addWidget(headerButton);
        sectionLayout->addWidget(scrollArea);
        scrollArea->setVisible(startChecked);

        connect(headerButton, &QPushButton::clicked, this, [scrollArea](){
            scrollArea->setVisible(!scrollArea->isVisible());
        });

        return sectionWidget;
    };

    // --- Página de Controle da Grade ---
    QWidget *pageControl = new QWidget();
    QVBoxLayout *layoutControl = new QVBoxLayout(pageControl);
    layoutControl->setContentsMargins(12, 16, 12, 12);
    layoutControl->setSpacing(10);

    QLabel *lblSecao = new QLabel("CONTROLE");
    lblSecao->setStyleSheet("font-weight: bold; font-size: 10px; color: #888; letter-spacing: 1.5px; margin-bottom: 8px;");
    layoutControl->addWidget(lblSecao);

    QPushButton *btnZoomInGrid = new QPushButton("Zoom +");
    btnZoomInGrid->setMinimumHeight(36);
    QPushButton *btnZoomOutGrid = new QPushButton("Zoom -");
    btnZoomOutGrid->setMinimumHeight(36);
    QPushButton *btnToggleOrientation = new QPushButton("Alternar Orientação (H/V)");
    btnToggleOrientation->setMinimumHeight(32);
    btnToggleOrientation->setObjectName("btnToggleGridOrientation");

    layoutControl->addWidget(btnZoomInGrid);
    layoutControl->addWidget(btnZoomOutGrid);
    layoutControl->addSpacing(6);
    layoutControl->addWidget(btnToggleOrientation);
    layoutControl->addStretch();

    // --- Página de Sistema ---
    QWidget *pageSistema = new QWidget();
    QVBoxLayout *layoutSistema = new QVBoxLayout(pageSistema);
    layoutSistema->setContentsMargins(12, 16, 12, 12);
    layoutSistema->setSpacing(10);

    QLabel *lblSecaoPaleta = new QLabel("PALETA DE CORES");
    lblSecaoPaleta->setStyleSheet("font-weight: bold; font-size: 10px; color: #888; letter-spacing: 1.5px; margin-bottom: 8px;");
    layoutSistema->addWidget(lblSecaoPaleta);

    QComboBox *cbPalette = new QComboBox();
    cbPalette->setMinimumHeight(32);
    cbPalette->addItem("Ironbow", 0);
    cbPalette->addItem("Semáforo (RYG)", 1);
    cbPalette->addItem("Divergente (Azul-Vermelho)", 2);
    cbPalette->addItem("Plasma / Viridis (CVD)", 3);
    cbPalette->addItem("Normal", 4);
    cbPalette->setObjectName("cbPaletteSelector");
    cbPalette->setCurrentIndex(SensorWidget::activePalette());
    cbPalette->setStyleSheet(
        "QComboBox { "
        "  background-color: #1e293b; "
        "  color: #f1f5f9; "
        "  border: 1px solid #475569; "
        "  border-radius: 6px; "
        "  padding: 4px 8px; "
        "} "
        "QComboBox::drop-down { "
        "  border: none; "
        "}"
    );
    layoutSistema->addWidget(cbPalette);

    layoutSistema->addSpacing(10);

    lblSecao = new QLabel("AÇÕES");
    lblSecao->setStyleSheet("font-weight: bold; font-size: 10px; color: #888; letter-spacing: 1.5px; margin-bottom: 8px;");
    layoutSistema->addWidget(lblSecao);

    QPushButton *btnRandomize = new QPushButton("Requisitar Dados");
    btnRandomize->setMinimumHeight(32);
    layoutSistema->addWidget(btnRandomize);
    layoutSistema->addStretch();

    // --- Página de Importação/Exportação da Ordem dos Sensores ---
    QWidget *pageOrdem = new QWidget();
    QVBoxLayout *layoutOrdem = new QVBoxLayout(pageOrdem);
    layoutOrdem->setContentsMargins(12, 16, 12, 12);
    layoutOrdem->setSpacing(10);

    lblSecao = new QLabel("ORDEM");
    lblSecao->setStyleSheet("font-weight: bold; font-size: 10px; color: #888; letter-spacing: 1.5px; margin-bottom: 8px;");
    layoutOrdem->addWidget(lblSecao);

    QPushButton *btnExportOrdem = new QPushButton("Exportar Ordem");
    btnExportOrdem->setMinimumHeight(32);
    btnExportOrdem->setObjectName("btnExportOrdem");

    QPushButton *btnImportOrdem = new QPushButton("Importar Ordem");
    btnImportOrdem->setMinimumHeight(32);
    btnImportOrdem->setObjectName("btnImportOrdem");

    layoutOrdem->addWidget(btnExportOrdem);
    layoutOrdem->addWidget(btnImportOrdem);
    layoutOrdem->addStretch();

    // Adiciona as páginas ao acordeão
    accordionLayout->addWidget(createAccordionPage("Controle da Grade", pageControl, true));
    accordionLayout->addWidget(createAccordionPage("Ordem dos Sensores", pageOrdem));
    accordionLayout->addWidget(createAccordionPage("Sistema", pageSistema));
    accordionLayout->addStretch();

    // Conexões
    connect(btnZoomInGrid, &QPushButton::clicked, this, [this]() {
        if (m_zoomFactor < 2.0) { m_zoomFactor += 0.2; applyZoom(); }
    });
    connect(btnZoomOutGrid, &QPushButton::clicked, this, [this]() {
        if (m_zoomFactor > 0.4) { m_zoomFactor -= 0.2; applyZoom(); }
    });
    connect(btnToggleOrientation, &QPushButton::clicked, this, &MainWindow::toggleGridOrientation);
    connect(btnRandomize, &QPushButton::clicked, this, &MainWindow::onRequestAllData);
    connect(btnExportOrdem, &QPushButton::clicked, this, &MainWindow::onExportSensorOrder);
    connect(btnImportOrdem, &QPushButton::clicked, this, &MainWindow::onImportSensorOrder);
    m_cbPaletteGrid = cbPalette;
    connect(m_cbPaletteGrid, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MainWindow::onPaletteChanged);

    // Oculta o container inicialmente (será visível quando Grid View estiver ativa)
    m_optionsGridContainer->setVisible(false);

    // Adiciona ao layout do optionsPanel
    if (QVBoxLayout *optsPanelLayout = qobject_cast<QVBoxLayout*>(ui->optionsPanel->layout())) {
        optsPanelLayout->insertWidget(0, m_optionsGridContainer);
    }
}

void MainWindow::setup2DOptionsPanel() {
    // Cria container para opções da Visualização 2D
    m_options2DContainer = new QWidget();
    m_options2DContainer->setObjectName("options2DContainer");
    m_options2DContainer->setFixedWidth(250);

    QVBoxLayout* opts2DLayout = new QVBoxLayout(m_options2DContainer);
    opts2DLayout->setContentsMargins(0, 0, 0, 0);
    opts2DLayout->setSpacing(5);

    // Helper para criar páginas do acordeão, seguindo o padrão do setup3DOptionsPanel.
    QList<QScrollArea*> scrollAreas;
    auto createAccordionPage = [&](const QString& title, QWidget* contentPage, bool startChecked = false) {
        QWidget* sectionWidget = new QWidget();
        QVBoxLayout* sectionLayout = new QVBoxLayout(sectionWidget);
        sectionLayout->setContentsMargins(0, 0, 0, 0);
        sectionLayout->setSpacing(2);

        QPushButton* headerButton = new QPushButton();
        headerButton->setFlat(true);
        headerButton->setCursor(Qt::PointingHandCursor);
        headerButton->setStyleSheet("text-align: left;");

        QHBoxLayout* headerLayout = new QHBoxLayout(headerButton);
        headerLayout->setContentsMargins(8, 4, 4, 4);

        QLabel* titleLabel = new QLabel(title);
        titleLabel->setStyleSheet("font-weight: bold;");
        titleLabel->setAttribute(Qt::WA_TransparentForMouseEvents);

        QPushButton* arrowIndicator = new QPushButton(startChecked ? "▼" : "▲");
        arrowIndicator->setFlat(true);
        arrowIndicator->setFixedSize(24, 24);
        arrowIndicator->setAttribute(Qt::WA_TransparentForMouseEvents);

        headerLayout->addWidget(titleLabel);
        headerLayout->addStretch();
        headerLayout->addWidget(arrowIndicator);

        QScrollArea* scrollArea = new QScrollArea();
        scrollArea->setWidgetResizable(true);
        scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        scrollArea->setFrameShape(QFrame::NoFrame);
        scrollArea->setWidget(contentPage);

        sectionLayout->addWidget(headerButton);
        sectionLayout->addWidget(scrollArea);
        scrollArea->setVisible(startChecked);

        connect(headerButton, &QPushButton::clicked, this, [scrollArea, arrowIndicator](){
            bool isVisible = !scrollArea->isVisible();
            scrollArea->setVisible(isVisible);
            arrowIndicator->setText(isVisible ? "▼" : "▲");
        });

        scrollAreas.append(scrollArea);
        return sectionWidget;
    };

    // --- Página de Visibilidade ---
    QWidget *pageVisibility = new QWidget();
    QVBoxLayout *layoutVisibility = new QVBoxLayout(pageVisibility);
    layoutVisibility->setContentsMargins(12, 16, 12, 12);
    layoutVisibility->setSpacing(10);

    QString chkStyle = "QCheckBox { padding: 4px; font-size: 12px; }";
    QCheckBox *chkShowGrain = new QCheckBox("Massa de Grãos (Heatmap)");
    chkShowGrain->setChecked(true);
    chkShowGrain->setStyleSheet(chkStyle);
    layoutVisibility->addWidget(chkShowGrain);

    QCheckBox *chkShowSensors = new QCheckBox("Pontos dos Sensores");
    chkShowSensors->setChecked(true);
    chkShowSensors->setStyleSheet(chkStyle);
    layoutVisibility->addWidget(chkShowSensors);

    QCheckBox *chkShowValues = new QCheckBox("Valores de Temperatura");
    chkShowValues->setChecked(true);
    chkShowValues->setStyleSheet(chkStyle);
    layoutVisibility->addWidget(chkShowValues);

    QCheckBox *chkShowMotors = new QCheckBox("Indicadores dos Motores");
    chkShowMotors->setChecked(true);
    chkShowMotors->setStyleSheet(chkStyle);
    layoutVisibility->addWidget(chkShowMotors);

    // --- Página de Sistema ---
    QWidget *pageSistema = new QWidget();
    QVBoxLayout *layoutSistema = new QVBoxLayout(pageSistema);
    layoutSistema->setContentsMargins(12, 16, 12, 12);
    layoutSistema->setSpacing(10);

    QLabel *lblSecaoPaleta = new QLabel("PALETA DE CORES");
    lblSecaoPaleta->setStyleSheet("font-weight: bold; font-size: 10px; color: #888; letter-spacing: 1.5px; margin-bottom: 8px;");
    layoutSistema->addWidget(lblSecaoPaleta);

    m_cbPalette2D = new QComboBox();
    m_cbPalette2D->setMinimumHeight(32);
    m_cbPalette2D->addItem("Ironbow", 0);
    m_cbPalette2D->addItem("Semáforo (RYG)", 1);
    m_cbPalette2D->addItem("Divergente (Azul-Vermelho)", 2);
    m_cbPalette2D->addItem("Plasma / Viridis (CVD)", 3);
    m_cbPalette2D->addItem("Normal", 4);
    m_cbPalette2D->setObjectName("cbPaletteSelector2D");
    m_cbPalette2D->setCurrentIndex(SensorWidget::activePalette());
    m_cbPalette2D->setStyleSheet(
        "QComboBox { "
        "  background-color: #1e293b; "
        "  color: #f1f5f9; "
        "  border: 1px solid #475569; "
        "  border-radius: 6px; "
        "  padding: 4px 8px; "
        "} "
        "QComboBox::drop-down { "
        "  border: none; "
        "}"
    );
    layoutSistema->addWidget(m_cbPalette2D);
    connect(m_cbPalette2D, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MainWindow::onPaletteChanged);

    layoutSistema->addSpacing(10);

    QPushButton *btnRandomize = new QPushButton("Requisitar Dados");
    layoutSistema->addWidget(btnRandomize);

    opts2DLayout->addWidget(createAccordionPage("Visibilidade", pageVisibility, true));
    opts2DLayout->addWidget(createAccordionPage("Sistema", pageSistema));

    // Conecta os checkboxes aos slots do Silo2DWidget
    if (m_silo2DWidget) {
        connect(chkShowGrain, &QCheckBox::toggled, m_silo2DWidget, &Silo2DWidget::setShowGrain);
        connect(chkShowSensors, &QCheckBox::toggled, m_silo2DWidget, &Silo2DWidget::setShowSensors);
        connect(chkShowValues, &QCheckBox::toggled, m_silo2DWidget, &Silo2DWidget::setShowValues);
        connect(chkShowMotors, &QCheckBox::toggled, m_silo2DWidget, &Silo2DWidget::setShowMotors);
    }
    // Conecta o novo botão
    connect(btnRandomize, &QPushButton::clicked, this, &MainWindow::onRequestAllData);

    opts2DLayout->addStretch();

    // Oculta inicialmente
    m_options2DContainer->setVisible(false);

    // Adiciona ao layout do optionsPanel
    if (QVBoxLayout *optsPanelLayout = qobject_cast<QVBoxLayout*>(ui->optionsPanel->layout())) {
        optsPanelLayout->insertWidget(0, m_options2DContainer);
    }
}

void MainWindow::showOptionsForViewMode(int mode) {
    // Grid View (0): mostra apenas opções da grade
    // 2D View (1): mostra apenas opções 2D
    // 3D View (2): mostra toolBox e opções 3D
    if (m_optionsGridContainer) {
        m_optionsGridContainer->setVisible(mode == 0);
    }
    if (m_options2DContainer) {
        m_options2DContainer->setVisible(mode == 1);
    }
    if (m_options3DContainer) {
        m_options3DContainer->setVisible(mode == 2);
    }
}

void MainWindow::onPaletteChanged(int index) {
    SensorWidget::setActivePalette(index);

    // Sincroniza a seleção de todos os comboboxes de paleta
    if (m_cbPaletteGrid) {
        m_cbPaletteGrid->blockSignals(true);
        m_cbPaletteGrid->setCurrentIndex(index);
        m_cbPaletteGrid->blockSignals(false);
    }
    if (m_cbPalette2D) {
        m_cbPalette2D->blockSignals(true);
        m_cbPalette2D->setCurrentIndex(index);
        m_cbPalette2D->blockSignals(false);
    }
    if (m_cbPalette3D) {
        m_cbPalette3D->blockSignals(true);
        m_cbPalette3D->setCurrentIndex(index);
        m_cbPalette3D->blockSignals(false);
    }

    // Atualiza a visualização atual para redesenhar as cores correspondentes
    if (ui->siloList->currentItem()) {
        updateSensorGrid(ui->siloList->currentItem()->text());
    }
}

void MainWindow::setupConfigTab() {
    ui->themeComboBox->addItem("Tema Escuro");
    ui->themeComboBox->addItem("Tema Claro");
    ui->themeComboBox->addItem("Tema Familiar");
    connect(ui->themeComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MainWindow::onThemeChanged);

    // Adiciona o painel de visão geral à aba de Configurações
    if (ui->configTab) {
        QLayout* currentLayout = ui->configTab->layout();
        if (currentLayout) {
            // Para evitar o warning "setLayout: Attempting to set QLayout, which already has a layout",
            // nós movemos o conteúdo existente para um novo widget e, em seguida, reconstruímos o layout da aba.

            // 1. Cria um widget para conter o conteúdo original da aba.
            QWidget* originalContentWidget = new QWidget();
            originalContentWidget->setLayout(currentLayout); // Move o layout existente para o novo widget.

            // 2. Cria o novo layout principal para a aba (horizontal).
            QHBoxLayout* newMainLayout = new QHBoxLayout();

            // 3. Cria a barra lateral com o painel de visão geral.
            QVBoxLayout* sidebarLayout = new QVBoxLayout();
            QWidget* panel = createGlobalOverviewPanel();
            sidebarLayout->addWidget(panel);
            sidebarLayout->addStretch();

            // 4. Adiciona a barra lateral e o conteúdo original ao novo layout principal.
            newMainLayout->addLayout(sidebarLayout);
            newMainLayout->addWidget(originalContentWidget, 1); // O '1' dá mais espaço ao conteúdo original.

            // 5. Define o novo layout para a aba de configurações.
            ui->configTab->setLayout(newMainLayout);
        }
    }
}

void MainWindow::onThemeChanged(int index) {
    m_themeManager->setTheme(static_cast<ThemeManager::ThemeType>(index));

    // CORREÇÃO: O stylesheet global precisa ser reaplicado para que as
    // mudanças de cor do novo tema tenham efeito em toda a UI.
    if (QApplication* app = qobject_cast<QApplication*>(QApplication::instance())) {
        app->setStyleSheet(m_themeManager->getStyleSheet());
    }

    // Sincroniza o tema das visualizações 2D e 3D com a seleção global.
    if (m_silo2DWidget) {
        // O widget 2D usa temas 1, 2, 3, enquanto o índice do ComboBox é 0, 1, 2.
        m_silo2DWidget->setCurrentTheme(index + 1);
    }
    if (m_glWidget) {
        // O widget 3D usa métodos separados para cada tema.
        if (index == 0) m_glWidget->setTheme1();
        else if (index == 1) m_glWidget->setTheme2();
        else if (index == 2) m_glWidget->setTheme3();
    }
}

void MainWindow::onSiloSelected(QListWidgetItem *item) {
    ui->lblCurrentSilo->setText(item->text());
    updateSensorGrid(item->text());

    // Atualiza o status do motor na visão 2D
    int siloIndex = ui->siloList->row(item);
    if (siloIndex >= 0 && siloIndex < 4) {
        QPushButton* motorButton = findChild<QPushButton*>(QString("btnManualAction_%1").arg(siloIndex + 1));
        if (motorButton && m_silo2DWidget) {
            bool isMotorOn = motorButton->property("isMotorOn").toBool();
            m_silo2DWidget->setMotorStatus(isMotorOn);
        }
    }
}

void MainWindow::onViewModeChanged(int index) {
    // 3D já foi inicializado em background no construtor
    ui->viewStackedWidget->setCurrentIndex(index);
    showOptionsForViewMode(index);

    // A chamada a adjustSize() foi removida. A estabilidade do tamanho da janela
    // agora é garantida pela configuração da QScrollArea da grade de sensores,
    // que foi ajustada no construtor da MainWindow para não impor um tamanho
    // mínimo excessivo à janela principal.
    // adjustSize();
}

void MainWindow::onNewTemperatures(int siloId, const QVector<float>& temperatures)
{
    if (siloId < 0 || siloId >= 4) return;

    // Armazena os dados recebidos
    m_siloTemperatures[siloId] = temperatures;

    // Se o silo que recebeu dados é o que está selecionado na UI, atualiza tudo
    if (siloId == ui->siloList->currentRow()) {
        updateSensorGrid(ui->siloList->currentItem()->text());
    }

    // Log para o banco de dados (Estatísticas)
    DatabaseManager::instance().logTemperatures(siloId, temperatures);
    
    if (m_statsCentralStack) {
        if (m_statsCentralStack->currentIndex() == 4) {
            updateStatsSummaryText();
        }
    }


    QLabel* infoLabel = m_redeStatusLabels.value("status_termometria_info", nullptr);
    if (infoLabel) {
        QStringList tempList;
        for (int i = 0; i < 13; ++i) {
            tempList.append(QString::number(temperatures[i], 'f', 1));
        }
        infoLabel->setText("Temps: " + tempList.join(", "));
    }
}

void MainWindow::onNewMeteoData(double temperature, double humidity)
{
    if (!m_meteoTempLabel || !m_meteoHumidLabel) return;

    m_meteoTempLabel->setText(QString::number(temperature, 'f', 1) + " °C");
    m_meteoHumidLabel->setText(QString::number(humidity, 'f', 1) + " %");

    // Atualiza os painéis de visão geral
    for (QWidget* panel : m_overviewPanels) {
        if (!panel) continue;
        QLabel* tempLabel = panel->findChild<QLabel*>("lblMeteoTempOverview");
        if (tempLabel) tempLabel->setText(QString("Temp. Amb.: %1 °C").arg(QString::number(temperature, 'f', 1)));
        QLabel* humidLabel = panel->findChild<QLabel*>("lblMeteoHumidOverview");
        if (humidLabel) humidLabel->setText(QString("Umidade Rel.: %1 %").arg(QString::number(humidity, 'f', 1)));
    }

    // Log para o banco de dados (Estatísticas)
    DatabaseManager::instance().logMeteorology(temperature, humidity);
    
    if (m_statsCentralStack) {
        if (m_statsCentralStack->currentIndex() == 4) {
            updateStatsSummaryText();
        }
    }


    QLabel* infoLabel = m_redeStatusLabels.value("status_meteo_info", nullptr);
    if (infoLabel) {
        infoLabel->setText(QString("Temp: %1 °C, Umidade: %2 %").arg(temperature, 0, 'f', 1).arg(humidity, 0, 'f', 1));
    }
}

void MainWindow::onLocalNetworkStatusUpdate(const QString& status, const QString& details)
{
    // Esta função agora lida apenas com o status da escuta UDP do próprio PC,
    // que não é mais exibido diretamente na UI principal da rede.
    // Poderíamos logar isso, mas por enquanto não faremos nada.
}

void MainWindow::onAccessPointStatusUpdate(bool connected)
{
    QLabel* statusLabel = m_redeStatusLabels.value("config_local_status", nullptr);

    if (statusLabel) {
        statusLabel->setText(connected ? "Conectado" : "Desconectado");
        if (connected) {
            statusLabel->setStyleSheet("font-weight: bold; color: #22c55e;"); // Verde
        } else {
            statusLabel->setStyleSheet("font-weight: bold; color: #ef4444;"); // Vermelho
        }
    }

    // Habilita ou desabilita o botão de controle do beacon
    QPushButton* toggleBeaconButton = findChild<QPushButton*>("toggleBeaconButton");
    if (toggleBeaconButton) {
        toggleBeaconButton->setEnabled(connected); // Botão do AP
        if (!connected) {
            m_isApBeaconActive = true; // Reseta o estado quando desconecta
            toggleBeaconButton->setText("Desligar Beacon (AP)");
        }
    }
}

void MainWindow::onNewMotorState(int siloId, bool isOn)
{
    // O motorId na UI é 1-4, o siloId é 0-3.
    int motorId = siloId + 1;
    if (motorId < 1 || motorId > 4) return;

    QPushButton* btn = findChild<QPushButton*>(QString("btnManualAction_%1").arg(motorId));
    QLabel* statusLabel = findChild<QLabel*>(QString("lblMotorStatus_%1").arg(motorId));

    if (!btn || !statusLabel) return;

    // Log para o banco de dados apenas em transição de estado para evitar inundação do banco
    bool wasOn = btn->property("isMotorOn").toBool();
    bool hasProperty = btn->property("isMotorOn").isValid();
    if (!hasProperty || wasOn != isOn) {
        DatabaseManager::instance().logMotorEvent(motorId, isOn);
    }
    
    if (m_statsCentralStack) {
        if (m_statsCentralStack->currentIndex() == 4) {
            updateStatsSummaryText();
        }
    }

    // Atualiza o estado do botão e o texto
    btn->setProperty("isMotorOn", isOn);
    btn->setEnabled(true); // Reabilita o botão

    if (isOn) {
        btn->setText("DESLIGAR MOTOR");
        btn->setStyleSheet("font-size: 14px; font-weight: bold; background-color: #c62828; color: white;"); // Vermelho
        statusLabel->setText("LIGADO");
        statusLabel->setStyleSheet("font-size: 14px; font-weight: bold; color: #22c55e; background-color: rgba(34, 197, 94, 0.15); padding: 8px 16px; border-radius: 20px; border: 2px solid #22c55e;");
    } else {
        btn->setText("LIGAR MOTOR");
        btn->setStyleSheet("font-size: 14px; font-weight: bold; background-color: #2e7d32; color: white;"); // Verde
        statusLabel->setText("DESLIGADO");
        statusLabel->setStyleSheet("font-size: 14px; font-weight: bold; color: #64748b; background-color: rgba(100, 116, 139, 0.1); padding: 8px 16px; border-radius: 20px; border: 2px solid #64748b;");
    }

    // Atualiza a visão 2D se o silo correspondente estiver visível
    if (m_silo2DWidget && (siloId == ui->siloList->currentRow())) {
        m_silo2DWidget->setMotorStatus(isOn);
    }

    updateMotorSummary(); // Atualiza o painel de visão geral

    QLabel* infoLabel = m_redeStatusLabels.value("status_motor_info", nullptr);
    if (infoLabel) {
        infoLabel->setText(QString("Motor %1: %2").arg(motorId).arg(isOn ? "LIGADO" : "DESLIGADO"));
    }
}

void MainWindow::onDayNightUpdate()
{
    // Força a atualização dos widgets que dependem do tempo (tema rural).
    // Isso garante que a mudança de dia/noite seja refletida sem interação do usuário.
    if (m_glWidget) {
        m_glWidget->update();
    }
    if (m_silo2DWidget) {
        m_silo2DWidget->update();
    }
}

void MainWindow::evaluateAutomaticAeration()
{
    if (!m_networkManager) return;

    QTime currentTime = QTime::currentTime();

    for (int i = 1; i <= 4; ++i) {
        QComboBox* cbOpMode = findChild<QComboBox*>(QString("cbOperationMode_%1").arg(i));
        if (!cbOpMode || cbOpMode->currentIndex() != 0) {
            // Se não estiver no modo automático, pula
            continue;
        }

        QComboBox* cbAutoMode = findChild<QComboBox*>(QString("cbAeracaoMode_%1").arg(i));
        QTimeEdit* timeOn = findChild<QTimeEdit*>(QString("timeEditOn_%1").arg(i));
        QTimeEdit* timeOff = findChild<QTimeEdit*>(QString("timeEditOff_%1").arg(i));
        QDoubleSpinBox* spinTemp = findChild<QDoubleSpinBox*>(QString("spinBoxMaxTemp_%1").arg(i));
        QPushButton* btn = findChild<QPushButton*>(QString("btnManualAction_%1").arg(i));

        if (!cbAutoMode || !btn) continue;

        bool shouldBeOn = false;

        if (cbAutoMode->currentIndex() == 0) { // Por Horário
            if (timeOn && timeOff) {
                QTime tOn = timeOn->time();
                QTime tOff = timeOff->time();

                if (tOn < tOff) {
                    shouldBeOn = (currentTime >= tOn && currentTime <= tOff);
                } else if (tOn > tOff) {
                    // Período que atravessa a meia-noite (ex: 22:00 às 06:00)
                    shouldBeOn = (currentTime >= tOn || currentTime <= tOff);
                } else {
                    // tOn == tOff
                    shouldBeOn = false;
                }
            }
        } else { // Por Temperatura
            if (spinTemp) {
                int siloId = i - 1;
                float maxVal = -127.0f;
                if (m_siloTemperatures.contains(siloId)) {
                    const QVector<float>& temps = m_siloTemperatures.value(siloId);
                    for (float t : temps) {
                        if (t > -100.0f && t > maxVal) {
                            maxVal = t;
                        }
                    }
                }
                // Se temos leituras válidas, compara. Se não temos, mantém desligado.
                if (maxVal > -100.0f) {
                    shouldBeOn = (maxVal > spinTemp->value());
                } else {
                    shouldBeOn = false;
                }
            }
        }

        bool isCurrentlyOn = btn->property("isMotorOn").toBool();
        if (isCurrentlyOn != shouldBeOn) {
            qInfo() << "MainWindow: Desvio automático detectado no motor" << i
                    << "| Atual:" << isCurrentlyOn << "| Desejado:" << shouldBeOn
                    << "| Enviando comando...";
            m_networkManager->sendMotorCommand(i, shouldBeOn);
        }
    }
}

void MainWindow::randomizeAndUpdateTemperatures()
{
    // Esta função foi desativada. Os dados de termometria agora são recebidos
    // exclusivamente pela rede através do NetworkManager e do slot onNewTemperatures.
    // qInfo() << "randomizeAndUpdateTemperatures foi chamado, mas está desativado.";
}

void MainWindow::onToggleBeaconClicked()
{
    if (!m_networkManager) return;

    QPushButton* btn = findChild<QPushButton*>("toggleBeaconButton");
    if (btn) {
        m_networkManager->sendToggleBeaconCommand();
        m_isApBeaconActive = !m_isApBeaconActive; // Assume que o comando funcionou e atualiza a UI
        btn->setText(m_isApBeaconActive ? "Desligar Beacon (AP)" : "Ligar Beacon (AP)");
    }
}

// ZOOM SENSÍVEL
void MainWindow::zoomIn() {
    if (ui->viewTabBar->currentIndex() == 2 && m_glWidget) { m_glWidget->zoomIn(); }
    else if (m_zoomFactor < 2.0) { m_zoomFactor += 0.2; applyZoom(); }
}

void MainWindow::zoomOut() {
    if (ui->viewTabBar->currentIndex() == 2 && m_glWidget) { m_glWidget->zoomOut(); }
    else if (m_zoomFactor > 0.4) { m_zoomFactor -= 0.2; applyZoom(); }
}

void MainWindow::toggleGridOrientation() {
    m_gridHorizontal = !m_gridHorizontal;
    updateSensorGridLayout();
}

void MainWindow::applyZoom() {
    for (int i = 0; i < ui->sensorGridLayout->count(); ++i) {
        QWidget *w = ui->sensorGridLayout->itemAt(i)->widget();
        if (SensorWidget *sw = qobject_cast<SensorWidget*>(w)) {
            sw->setZoom(m_zoomFactor);
        }
    }
}

void MainWindow::setupAeracaoOptionsPanel() {
    // Limpa o conteúdo placeholder do painel.
    // Define a largura fixa diretamente no painel de conteúdo para evitar que ele encolha.
    ui->aeracaoOptionsPanel->setFixedWidth(250);

    QLayout* oldLayout = ui->aeracaoOptionsPanel->layout();
    if (oldLayout) {
        QLayoutItem* item;
        while ((item = oldLayout->takeAt(0)) != nullptr) {
            delete item->widget();
            delete item;
        }
        delete oldLayout;
    }

    QVBoxLayout* accordionLayout = new QVBoxLayout(ui->aeracaoOptionsPanel);
    accordionLayout->setContentsMargins(0, 0, 0, 0);
    accordionLayout->setSpacing(5);

    // Helper para criar páginas do acordeão.
    auto createAccordionPage = [&](const QString& title, QWidget* contentPage, bool startChecked = false) {
        QWidget* sectionWidget = new QWidget();
        QVBoxLayout* sectionLayout = new QVBoxLayout(sectionWidget);
        sectionLayout->setContentsMargins(0, 0, 0, 0);
        sectionLayout->setSpacing(2);

        QPushButton* headerButton = new QPushButton();
        headerButton->setFlat(true);
        headerButton->setCursor(Qt::PointingHandCursor);
        headerButton->setStyleSheet("text-align: left;");

        QHBoxLayout* headerLayout = new QHBoxLayout(headerButton);
        headerLayout->setContentsMargins(8, 4, 4, 4);

        QLabel* titleLabel = new QLabel(title);
        titleLabel->setStyleSheet("font-weight: bold;");
        titleLabel->setAttribute(Qt::WA_TransparentForMouseEvents);

        headerLayout->addWidget(titleLabel);
        headerLayout->addStretch();

        QScrollArea* scrollArea = new QScrollArea();
        scrollArea->setWidgetResizable(true);
        scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        scrollArea->setFrameShape(QFrame::NoFrame);
        scrollArea->setWidget(contentPage);

        sectionLayout->addWidget(headerButton);
        sectionLayout->addWidget(scrollArea);
        scrollArea->setVisible(startChecked);

        connect(headerButton, &QPushButton::clicked, this, [scrollArea](){
            scrollArea->setVisible(!scrollArea->isVisible());
        });

        return sectionWidget;
    };

    // ===== PÁGINA 1: AÇÕES GLOBAIS =====
    QWidget *pageAcoes = new QWidget();
    QVBoxLayout *layoutAcoes = new QVBoxLayout(pageAcoes);
    layoutAcoes->setSpacing(10);
    layoutAcoes->setContentsMargins(12, 16, 12, 12);

    QPushButton *btnEmergencyStop = new QPushButton("PARADA DE EMERGÊNCIA");
    btnEmergencyStop->setStyleSheet("background-color: #dc2626; color: white; font-weight: bold; padding: 12px;");
    btnEmergencyStop->setMinimumHeight(50);

    QPushButton *btnStartAll = new QPushButton("INICIAR TODOS");
    btnStartAll->setStyleSheet("background-color: #22c55e; color: white; font-weight: bold; padding: 8px;");
    btnStartAll->setMinimumHeight(40);

    QPushButton *btnStopAll = new QPushButton("PARAR TODOS");
    btnStopAll->setStyleSheet("background-color: #dc2626; color: white; font-weight: bold; padding: 8px;");
    btnStopAll->setMinimumHeight(40);

    QPushButton *btnRequestData = new QPushButton("REQUISITAR DADOS");
    btnRequestData->setStyleSheet("background-color: #3b82f6; color: white; font-weight: bold; padding: 8px;");
    btnRequestData->setMinimumHeight(40);

    layoutAcoes->addWidget(btnEmergencyStop);
    layoutAcoes->addWidget(btnStartAll);
    layoutAcoes->addWidget(btnStopAll);
    layoutAcoes->addWidget(btnRequestData);
    layoutAcoes->addStretch();

    // ===== PÁGINA 2: CONFIGURAÇÃO DOS PAINÉIS =====
    QWidget *pageConfig = new QWidget();
    QVBoxLayout *layoutConfig = new QVBoxLayout(pageConfig);
    layoutConfig->setSpacing(10);
    layoutConfig->setContentsMargins(12, 16, 12, 12);

    // Recria o botão que foi removido do .ui e o armazena como membro da classe
    // para que sua conexão possa ser feita de forma segura após a inicialização.
    m_btnLockUnlockConfig = new QPushButton("Destravar Configurações");
    m_btnLockUnlockConfig->setObjectName("btnLockUnlockConfig");
    m_btnLockUnlockConfig->setMinimumHeight(40);
    layoutConfig->addWidget(m_btnLockUnlockConfig);
    layoutConfig->addStretch();

    // ===== PÁGINA 2: EXPORTAÇÃO =====
    QWidget *pageExport = new QWidget();
    QVBoxLayout *layoutExport = new QVBoxLayout(pageExport);
    layoutExport->setSpacing(10);
    layoutExport->setContentsMargins(12, 16, 12, 12);

    QPushButton *btnExportAeracao = new QPushButton("Exportar Aeração JSON");
    btnExportAeracao->setStyleSheet("background-color: #3b82f6; color: white; font-weight: bold; padding: 8px;");
    layoutExport->addWidget(btnExportAeracao);
    layoutExport->addStretch();

    // Adiciona as páginas ao layout principal
    accordionLayout->addWidget(createAccordionPage("Ações Globais", pageAcoes, true));
    accordionLayout->addWidget(createAccordionPage("Configuração dos Painéis", pageConfig));
    accordionLayout->addWidget(createAccordionPage("Exportação", pageExport));
    accordionLayout->addStretch();

    // Conexões
    connect(btnEmergencyStop, &QPushButton::clicked, this, [=]() {
        if (QMessageBox::warning(this, "Parada de Emergência", "Tem certeza que deseja PARAR TODOS os motores imediatamente?", QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes) {
            for (int i = 1; i <= 4; ++i) {
                onNewMotorState(i - 1, false); // Reutiliza o slot de atualização de estado
            }
        }
    });

    connect(btnRequestData, &QPushButton::clicked, this, &MainWindow::onRequestAllData);

    connect(btnStartAll, &QPushButton::clicked, this, [=]() {
        btnStartAll->setEnabled(false);
        btnStopAll->setEnabled(false);
        btnStartAll->setText("Processando...");
        for (int i = 1; i <= 4; ++i) {
            QPushButton* btn = findChild<QPushButton*>(QString("btnManualAction_%1").arg(i));
            QComboBox* cbOpMode = findChild<QComboBox*>(QString("cbOperationMode_%1").arg(i));
            if (btn && cbOpMode && cbOpMode->currentIndex() == 1 && !btn->property("isMotorOn").toBool()) {
                btn->click();
            }
        }
        QTimer::singleShot(3100, this, [=]() {
            btnStartAll->setText("INICIAR TODOS");
            btnStartAll->setEnabled(true);
            btnStopAll->setEnabled(true);
        });
    });

    connect(btnStopAll, &QPushButton::clicked, this, [=]() {
        btnStartAll->setEnabled(false);
        btnStopAll->setEnabled(false);
        btnStopAll->setText("Processando...");
        for (int i = 1; i <= 4; ++i) {
            QPushButton* btn = findChild<QPushButton*>(QString("btnManualAction_%1").arg(i));
            QComboBox* cbOpMode = findChild<QComboBox*>(QString("cbOperationMode_%1").arg(i));
            if (btn && cbOpMode && cbOpMode->currentIndex() == 1 && btn->property("isMotorOn").toBool()) {
                btn->click();
            }
        }
        QTimer::singleShot(3100, this, [=]() {
            btnStopAll->setText("PARAR TODOS");
            btnStartAll->setEnabled(true);
            btnStopAll->setEnabled(true);
        });
    });

    connect(btnExportAeracao, &QPushButton::clicked, this, &MainWindow::exportarAeracaoJson);
}

void MainWindow::updateSensorGrid(QString siloName) {
    QLayoutItem *item;
    while ((item = ui->sensorGridLayout->takeAt(0)) != nullptr) {
        if (item->widget()) delete item->widget();
        delete item;
    }

    int siloIndex = ui->siloList->currentRow();
    if (siloIndex < 0) return;

    // Obtém (ou cria e inicializa com zeros) o vetor de temperaturas para o silo selecionado.
    QVector<float>& currentSiloTemps = m_siloTemperatures[siloIndex];
    if (currentSiloTemps.isEmpty()) {
        currentSiloTemps.resize(65);
        currentSiloTemps.fill(0.0f);
    }

    // Mapeamento da ordem visual (coluna/linha 0 a 4) para a ordem canônica dos dados (pêndulo 0 a 4).
    // Ordem visual: Oeste, Sul, Centro, Norte, Leste.
    const int visual_to_data_map[5] = { 4, 3, 0, 1, 2 };

    double pendulumAverages[5] = {0.0};
    double totalSum = 0.0;
    int totalSensorCount = 0;

    for (int visual_p = 0; visual_p < 5; ++visual_p) {
        int p = visual_to_data_map[visual_p]; // p é o índice canônico do pêndulo (0=Centro, etc.)
        for (int s = 0; s < 13; ++s) {
            int sensorId = (p * 13) + s + 1;
            // Lê a temperatura do armazenamento central em vez de zerá-la.
            int temp_idx = (p * 13) + s;
            double temp = currentSiloTemps[temp_idx];
            
            SensorWidget* sw = new SensorWidget(1, p + 1, sensorId, this);
            sw->setTemperature(temp);
            sw->setOnline(true);
            sw->setZoom(m_zoomFactor);
            sw->setPhysicalPosition(m_sensorMapping[p][s]);
            connect(sw, &SensorWidget::positionChanged, this, &MainWindow::onSensorPositionChanged);

            int gridPos = m_sensorMapping[p][s] - 1;
            if (gridPos < 0 || gridPos >= 13) gridPos = s;

            if (m_gridHorizontal) { // Pêndulos como linhas
                ui->sensorGridLayout->addWidget(sw, visual_p, gridPos);
            } else { // Pêndulos como colunas
                ui->sensorGridLayout->addWidget(sw, gridPos, visual_p);
            }

            pendulumAverages[p] += temp; // Acumula no índice canônico
            totalSum += temp;
            totalSensorCount++;
        }
    }

    // Finaliza o cálculo das médias.
    for (int p = 0; p < 5; ++p) {
        if (totalSensorCount > 0) {
            pendulumAverages[p] /= 13.0;
        }
    }
    double overallAverage = (totalSensorCount > 0) ? totalSum / totalSensorCount : 0.0;

    // Atualiza todos os painéis de visão geral com as médias calculadas.
    for (QWidget* panel : m_overviewPanels) {
        if (!panel) continue;
        for (int visual_p = 0; visual_p < 5; ++visual_p) {
            int data_p = visual_to_data_map[visual_p];
            QLabel* lbl = panel->findChild<QLabel*>(QString("lblTempPendulum_%1").arg(visual_p + 1));
            if (lbl) {
                lbl->setText(QString("Pêndulo %1: %2 °C").arg(visual_p + 1).arg(pendulumAverages[data_p], 0, 'f', 1));
            }
        }
        QLabel* lblGeral = panel->findChild<QLabel*>("lblTempGeral");
        if (lblGeral) {
            lblGeral->setText(QString("Média Geral: %1 °C").arg(overallAverage, 0, 'f', 1));
        }
    }
    // Sincroniza as temperaturas com as outras visualizações, usando o armazenamento central.
    // Isso é um pouco redundante ao trocar de silo, mas garante consistência.
    if (m_glWidget) m_glWidget->setSensorTemperatures(currentSiloTemps);
    if (m_silo2DWidget) m_silo2DWidget->setSensorTemperatures(currentSiloTemps);
}
void MainWindow::exportarTermometriaJson() {
    QString siloName = ui->lblCurrentSilo->text();
    QJsonObject root;
    root["silo"] = siloName;
    root["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    root["tipo"] = "termometria";

    QJsonArray pendulums;
    double pendulumAverages[5] = {0.0};
    double totalSum = 0.0;
    int totalSensorCount = 0;

    // Mapeamento da ordem visual para a ordem canônica dos dados.
    const int visual_to_data_map[5] = { 4, 3, 0, 1, 2 };

    for (int visual_p_idx = 0; visual_p_idx < 5; ++visual_p_idx) {
        int data_p = visual_to_data_map[visual_p_idx];
        QJsonArray sensors;
        double pendulumSum = 0.0;
        int pendulumSensorCount = 0;

        for (int s = 1; s <= 13; ++s) { // Itera sensores de 1 a 13
            SensorWidget* sw = nullptr;
            // Encontra o widget correspondente pelo ID do sensor, que é mais robusto
            for (int i = 0; i < ui->sensorGridLayout->count(); ++i) {
                if (SensorWidget* sensor = qobject_cast<SensorWidget*>(ui->sensorGridLayout->itemAt(i)->widget())) {
                    int expectedSensorId = (data_p * 13) + s;
                    if (sensor->sensorId() == expectedSensorId) {
                        sw = sensor;
                        break;
                    }
                }
            }

            if (sw) {
                QJsonObject sensorObj;
                double temp = sw->temperature();
                sensorObj["id"] = QString("G1-N%1-S%2").arg(data_p + 1).arg(s);
                sensorObj["temperatura"] = temp;
                sensorObj["online"] = sw->isOnline();
                sensors.append(sensorObj);

                pendulumSum += temp;
                pendulumSensorCount++;
            }
        }
        QJsonObject pendulumObj;
        pendulumObj["pendulo_id_canonico"] = data_p + 1;
        pendulumObj["pendulo_id_visual"] = visual_p_idx + 1;
        pendulumObj["sensores"] = sensors;
        pendulums.append(pendulumObj);

        if (pendulumSensorCount > 0) {
            pendulumAverages[data_p] = pendulumSum / pendulumSensorCount;
        }
        totalSum += pendulumSum;
        totalSensorCount += pendulumSensorCount;
    }
    root["pendulums"] = pendulums;

    // Calcula as médias internamente, removendo a dependência dos QLabels do painel antigo.
    QJsonObject medias;
    for(int i = 0; i < 5; ++i) {
        medias[QString("pendulo_canonico_%1").arg(i+1)] = pendulumAverages[i];
    }
    medias["geral"] = (totalSensorCount > 0) ? (totalSum / totalSensorCount) : 0.0;
    root["medias"] = medias;

    QString fileName = QFileDialog::getSaveFileName(this, "Exportar Termometria",
        QString("termometria_%1.json").arg(siloName), "JSON Files (*.json)");
    if (!fileName.isEmpty()) {
        QFile file(fileName);
        if (file.open(QIODevice::WriteOnly)) {
            file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
            file.close();
            QMessageBox::information(this, "Sucesso", QString("Termometria exportada para %1").arg(fileName));
        }
    }
}

void MainWindow::exportarAeracaoJson() {
    QJsonObject root;
    root["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    root["tipo"] = "aeracao";

    QJsonArray motores;
    QStringList motorNames = {"Motor Alpha", "Motor Beta", "Motor Charlie", "Motor Delta"};

    for (int i = 1; i <= 4; ++i) {
        QJsonObject motor;
        motor["nome"] = motorNames.value(i - 1);
        motor["id"] = i;

        QComboBox* cbOpMode = findChild<QComboBox*>(QString("cbOperationMode_%1").arg(i));
        QComboBox* cbAutoMode = findChild<QComboBox*>(QString("cbAeracaoMode_%1").arg(i));
        QDoubleSpinBox* spinTemp = findChild<QDoubleSpinBox*>(QString("spinBoxMaxTemp_%1").arg(i));
        QTimeEdit* timeOn = findChild<QTimeEdit*>(QString("timeEditOn_%1").arg(i));
        QTimeEdit* timeOff = findChild<QTimeEdit*>(QString("timeEditOff_%1").arg(i));
        QPushButton* btn = findChild<QPushButton*>(QString("btnManualAction_%1").arg(i));

        motor["modo"] = cbOpMode ? (cbOpMode->currentIndex() == 0 ? "automatico" : "manual") : "unknown";

        if (cbAutoMode) {
            if (cbAutoMode->currentIndex() == 0) {
                motor["gatilho"] = "horario";
                motor["ligar"] = timeOn ? timeOn->time().toString("HH:mm") : "";
                motor["desligar"] = timeOff ? timeOff->time().toString("HH:mm") : "";
            } else {
                motor["gatilho"] = "temperatura";
                motor["tempMax"] = spinTemp ? spinTemp->value() : 35.0;
            }
        }

        motor["estado"] = btn ? (btn->property("isMotorOn").toBool() ? "ligado" : "desligado") : "desligado";

        motores.append(motor);
    }
    root["motores"] = motores;

    QString fileName = QFileDialog::getSaveFileName(this, "Exportar Aeração",
        QString("aeracao_%1.json").arg(QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss")), "JSON Files (*.json)");
    if (!fileName.isEmpty()) {
        QFile file(fileName);
        if (file.open(QIODevice::WriteOnly)) {
            file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
            file.close();
            QMessageBox::information(this, "Sucesso", QString("Aeração exportada para %1").arg(fileName));
        }
    }
}

bool MainWindow::eventFilter(QObject *obj, QEvent *event) {
    // Esta função foi mantida para compatibilidade com o override, mas a lógica
    // do acordeão foi movida para a implementação com QGroupBox.

    // Lógica de hover para os labels de status do motor.
    // Em vez de redimensionar o label (que é cortado pelo pai), usamos um QToolTip
    // com HTML para exibir a informação completa em uma janela flutuante.
    if (obj->objectName().startsWith("lblMotorSummaryLeft_")) {
        QLabel* label = qobject_cast<QLabel*>(obj);
        if (label) {
            if (event->type() == QEvent::Enter) {
                // Obtém as cores do tema atual para estilizar o tooltip.
                const auto& colors = m_themeManager->getCurrentColors();
                QString bgColor = colors.surface.name();
                QString textColor = colors.textPrimary.name();
                QString borderColor = colors.border.name();

                // Cria o conteúdo HTML para o tooltip, garantindo que ele tenha a mesma aparência do app.
                QString richText = QString(
                    "<div style='background-color:%1; color:%2; border:1px solid %3; padding:4px; border-radius:4px; font-size:10px; font-weight:bold;'>%4</div>"
                ).arg(bgColor, textColor, borderColor, label->text());

                // Exibe o tooltip próximo à posição do cursor.
                QToolTip::showText(QCursor::pos(), richText, label);
                return true; // O evento foi tratado.
            } else if (event->type() == QEvent::Leave) {
                QToolTip::hideText(); // Esconde o tooltip quando o mouse sai.
                return true; // O evento foi tratado.
            }
        }
    }

    return QMainWindow::eventFilter(obj, event);
}

void MainWindow::loadNetworkSettings()
{
    QSettings settings("VilarinoEngenharia", "GrainMonitor");
    QString apIP = settings.value("network/ap_ip", "").toString();
    QString ssid = settings.value("network/ssid", "").toString();
    QString password = settings.value("network/password", "").toString();

    // Encontra os widgets nas suas respectivas páginas
    QLineEdit* apIpEdit = findChild<QLineEdit*>("apIpAddressEdit");
    QLineEdit* ssidEdit = findChild<QLineEdit*>("wifiSsidEdit");
    QLineEdit* passwordEdit = findChild<QLineEdit*>("wifiPasswordEdit");

    if (apIpEdit) apIpEdit->setText(apIP);
    if (ssidEdit) ssidEdit->setText(ssid);
    if (passwordEdit) passwordEdit->setText(password);

    // Tenta conectar automaticamente na inicialização se o IP do AP já existir
    if (!apIP.isEmpty()) {
        qInfo() << "Configurações de rede carregadas. Tentando conectar automaticamente ao AP...";
        if (m_networkManager) {
            m_networkManager->setAccessPointAddress(apIP, 4210);
            m_networkManager->startListening(4210);
        }
    }
}

void MainWindow::onSaveAndConnectNetwork()
{
    // Esta função agora é chamada pelo botão "Conectar" na página do Ponto de Acesso.
    // Ela salva apenas o IP do AP e inicia a conexão.
    QLineEdit* apIpEdit = findChild<QLineEdit*>("apIpAddressEdit");

    if (!apIpEdit) {
        qWarning() << "Não foi possível encontrar o campo de IP do Ponto de Acesso.";
        return;
    }

    QString apIP = apIpEdit->text().trimmed();

    if (apIP.isEmpty()) {
        QMessageBox::warning(this, "Configuração Incompleta", "Por favor, insira o endereço IP do Access Point.");
        return;
    }

    QSettings settings("VilarinoEngenharia", "GrainMonitor");
    settings.setValue("network/ap_ip", apIP);

    qInfo() << "Configurações de rede salvas.";

    if (m_networkManager) {
        m_networkManager->setAccessPointAddress(apIP, 4210);
        m_networkManager->startListening(4210);
        // Envia um pacote inicial para o AP aprender nosso IP
        m_networkManager->pingAccessPoint();
    }
}

void MainWindow::onRemoteNodeStatusUpdate(int nodeId, const QString& status, const QString& details)
{
    QString prefix;
    if (nodeId == 0) prefix = "gateway";
    else if (nodeId == 1) prefix = "motor";
    else if (nodeId == 2) prefix = "meteo";
    else if (nodeId == 3) prefix = "termometria";
    else return;

    QLabel* statusLabel = m_redeStatusLabels.value(QString("status_%1_status").arg(prefix), nullptr);
    QLabel* rssiLabel = m_redeStatusLabels.value(QString("status_%1_details").arg(prefix), nullptr);
    QLabel* lastSeenLabel = m_redeStatusLabels.value(QString("status_%1_last_seen").arg(prefix), nullptr);

    if (statusLabel) {
        statusLabel->setText(status);
        statusLabel->setStyleSheet(status.toLower() == "online"
                                           ? "font-weight: bold; color: #22c55e;"  // Verde
                                           : "font-weight: bold; color: #ef4444;"); // Vermelho
    }

    if (status.toLower() == "online") {
        if (rssiLabel) {
            rssiLabel->setText(details);
        }
        if (lastSeenLabel) {
            lastSeenLabel->setText(QDateTime::currentDateTime().toString("dd/MM/yyyy hh:mm:ss"));
        }
    } else {
        if (rssiLabel) rssiLabel->setText("N/A (Offline)");
    }
}

void MainWindow::onRequestGatewayStatus()
{
    if (m_networkManager) {
        qInfo() << "Botão 'Solicitar Status Agora' clicado. Enviando requisição...";
        m_networkManager->requestGatewayStatus();
    }
}

void MainWindow::onRequestAllData()
{
    if (m_networkManager) {
        m_networkManager->requestAllData();
        appendToLog("Informações requisitadas para todos os nodos...");
    }
}

void MainWindow::onToggleGatewayBeacon()
{
    if (!m_networkManager) return;

    QPushButton* btn = findChild<QPushButton*>("toggleGatewayBeaconButton");
    if (btn) {
        bool nextState = !m_isGatewayBeaconActive;
        m_networkManager->sendToggleGatewayBeaconCommand(nextState);
        m_isGatewayBeaconActive = nextState;
        btn->setText(m_isGatewayBeaconActive ? "Desligar Beacon (Gateway)" : "Ligar Beacon (Gateway)");
    }
}

void MainWindow::resizeEvent(QResizeEvent *event) { QMainWindow::resizeEvent(event); }

void MainWindow::onStatsTabChanged(int pageIndex) {
    if (pageIndex < 0 || pageIndex > 4) return;
    
    if (m_statsCentralStack) {
        m_statsCentralStack->setCurrentIndex(pageIndex);
    }
    if (m_statsOptionsStack) {
        m_statsOptionsStack->setCurrentIndex(pageIndex);
    }
    
    if (pageIndex == 0) updateTermometriaStatsChart();
    else if (pageIndex == 1) updateAeracaoStatsChart();
    else if (pageIndex == 3) updateMeteorologiaStatsChart();
    else if (pageIndex == 4) updateStatsSummaryText();
}

void MainWindow::setupEstatisticasTab() {
    QWidget *tab = ui->estatisticasTab;
    if (!tab) return;

    // Limpa o conteúdo placeholder
    qDeleteAll(tab->findChildren<QWidget*>(QString(), Qt::FindDirectChildrenOnly));
    delete tab->layout();

    // Layout principal em Grid
    QGridLayout *mainLayout = new QGridLayout(tab);
    mainLayout->setSpacing(10);
    mainLayout->setContentsMargins(10, 10, 10, 10);

    // === 1. PAINEL ESQUERDO (Seleção) ===
    QWidget *leftSidebar = new QWidget();
    leftSidebar->setObjectName("statsLeftSidebarPanel");
    leftSidebar->setFixedWidth(250);
    QVBoxLayout *leftMainLayout = new QVBoxLayout(leftSidebar);
    leftMainLayout->setContentsMargins(0, 0, 0, 0);
    leftMainLayout->setSpacing(0);

    // Title Row with Toggle Button
    QWidget* leftTitleWidget = new QWidget();
    QHBoxLayout* leftTitleLayout = new QHBoxLayout(leftTitleWidget);
    leftTitleLayout->setContentsMargins(4, 4, 4, 4);
    QLabel *lblListTitle = new QLabel("Estatísticas");
    lblListTitle->setStyleSheet("font-weight: bold; font-size: 14px;");
    QPushButton* btnToggleLeft = new QPushButton("");
    btnToggleLeft->setObjectName("panelToggleButton");
    btnToggleLeft->setFixedSize(24, 24);
    btnToggleLeft->setToolTip("Minimizar/Maximizar Seleção");
    leftTitleLayout->addWidget(lblListTitle);
    leftTitleLayout->addStretch();
    leftTitleLayout->addWidget(btnToggleLeft);
    leftMainLayout->addWidget(leftTitleWidget);

    // Conteúdo do Painel Esquerdo
    m_statsSelectionList = new QListWidget();
    m_statsSelectionList->setObjectName("statsSelectionList");
    m_statsSelectionList->addItems({"Termometria", "Aeração", "Rede", "Meteorologia"});
    m_statsSelectionList->setCurrentRow(0);
    leftMainLayout->addWidget(m_statsSelectionList);

    QWidget* panel = createGlobalOverviewPanel();
    leftMainLayout->addWidget(panel);
    leftMainLayout->addStretch();

    // Conecta o botão de minimizar o painel esquerdo
    connect(btnToggleLeft, &QPushButton::clicked, this, [this]() {
        m_statsSelectionList->setVisible(!m_statsSelectionList->isVisible());
    });

    // === 2. CONTEÚDO CENTRAL (QStackedWidget) ===
    m_statsCentralStack = new QStackedWidget();
    m_statsCentralStack->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    // --- PÁGINA 0: Termometria Chart ---
    QWidget* termPage = new QWidget();
    QVBoxLayout* termLayout = new QVBoxLayout(termPage);
    termLayout->setContentsMargins(0, 0, 0, 0);
    m_statsTermTabs = new QTabWidget();
    m_statsTermTabs->setTabPosition(QTabWidget::South);
    
    QChartView* termChartView = new QChartView();
    termChartView->setObjectName("termStatsChartView");
    termChartView->setRenderHint(QPainter::Antialiasing);
    
    m_statsTermTabs->addTab(termChartView, "Modo 1");
    
    QChartView* termRateChartView = new QChartView();
    termRateChartView->setObjectName("termRateChartView");
    termRateChartView->setRenderHint(QPainter::Antialiasing);
    
    m_statsTermTabs->addTab(termRateChartView, "Variação Temporal (\u0394T/\u0394t)");
    m_statsTermTabs->addTab(new QWidget(), "Modo 3");
    
    termLayout->addWidget(m_statsTermTabs);
    m_statsCentralStack->addWidget(termPage);

    // --- PÁGINA 1: Aeração Chart ---
    QWidget* aerPage = new QWidget();
    QVBoxLayout* aerLayout = new QVBoxLayout(aerPage);
    aerLayout->setContentsMargins(0, 0, 0, 0);
    m_statsAerTabs = new QTabWidget();
    m_statsAerTabs->setTabPosition(QTabWidget::South);
    
    QChartView* aerChartView = new QChartView();
    aerChartView->setObjectName("aerStatsChartView");
    aerChartView->setRenderHint(QPainter::Antialiasing);
    
    m_statsAerTabs->addTab(aerChartView, "Modo 1");
    m_statsAerTabs->addTab(new QWidget(), "Modo 2");
    m_statsAerTabs->addTab(new QWidget(), "Modo 3");
    
    aerLayout->addWidget(m_statsAerTabs);
    m_statsCentralStack->addWidget(aerPage);

    // --- PÁGINA 2: Rede ---
    QWidget* redePage = new QWidget();
    QVBoxLayout* redeLayout = new QVBoxLayout(redePage);
    redeLayout->setContentsMargins(0, 0, 0, 0);
    m_statsRedeTabs = new QTabWidget();
    m_statsRedeTabs->setTabPosition(QTabWidget::South);
    
    m_statsRedeTabs->addTab(new QWidget(), "Modo 1");
    m_statsRedeTabs->addTab(new QWidget(), "Modo 2");
    m_statsRedeTabs->addTab(new QWidget(), "Modo 3");
    
    redeLayout->addWidget(m_statsRedeTabs);
    m_statsCentralStack->addWidget(redePage);

    // --- PÁGINA 3: Meteorologia ---
    QWidget* metPage = new QWidget();
    QVBoxLayout* metLayout = new QVBoxLayout(metPage);
    metLayout->setContentsMargins(0, 0, 0, 0);
    m_statsMeteoTabs = new QTabWidget();
    m_statsMeteoTabs->setTabPosition(QTabWidget::South);
    
    QChartView* metChartView = new QChartView();
    metChartView->setObjectName("meteoStatsChartView");
    metChartView->setRenderHint(QPainter::Antialiasing);
    
    m_statsMeteoTabs->addTab(metChartView, "Modo 1");
    m_statsMeteoTabs->addTab(new QWidget(), "Modo 2");
    m_statsMeteoTabs->addTab(new QWidget(), "Modo 3");
    
    metLayout->addWidget(m_statsMeteoTabs);
    m_statsCentralStack->addWidget(metPage);

    // --- PÁGINA 3: Sistema (Status & Resumo) ---
    QWidget* sysPage = new QWidget();
    QVBoxLayout* sysLayout = new QVBoxLayout(sysPage);
    sysLayout->setContentsMargins(15, 15, 15, 15);
    sysLayout->setSpacing(10);

    QGroupBox* gbStatus = new QGroupBox("Status do Armazenamento de Dados");
    QVBoxLayout* statusL = new QVBoxLayout(gbStatus);
    statusL->setSpacing(10);

    m_lblStatsSummary = new QLabel();
    m_lblStatsSummary->setStyleSheet("font-size: 13px; line-height: 1.5; color: #f1f5f9;");
    statusL->addWidget(m_lblStatsSummary);

    sysLayout->addWidget(gbStatus);
    sysLayout->addStretch();
    m_statsCentralStack->addWidget(sysPage);

    // === 3. PAINEL DIREITO (Opções) ===
    m_statsOptionsStack = new QStackedWidget();
    m_statsOptionsStack->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Maximum);

    auto createOptionsPage = [&](const QString& title, QLayout* contentLayout) -> QWidget* {
        QWidget* page = new QWidget();
        QVBoxLayout* mainL = new QVBoxLayout(page);
        mainL->setContentsMargins(0, 0, 0, 0);
        mainL->setSpacing(0);
        QGroupBox* group = new QGroupBox(title);
        group->setLayout(contentLayout);
        mainL->addWidget(group);
        mainL->addStretch();
        return page;
    };

    // --- PÁGINA OPÇÕES 0: Termometria ---
    QVBoxLayout* termOptLayout = new QVBoxLayout();
    QLabel* lblSiloSelect = new QLabel("Selecionar Silo:");
    QComboBox* comboSilo = new QComboBox();
    comboSilo->setObjectName("statsSiloCombo");
    comboSilo->addItems({"Silo Alpha", "Silo Beta", "Silo Charlie", "Silo Delta"});
    
    QLabel* lblPendulumSelect = new QLabel("Selecionar Pêndulo:");
    QComboBox* comboPendulum = new QComboBox();
    comboPendulum->setObjectName("statsPendulumCombo");
    comboPendulum->addItems({"Pêndulo Centro", "Pêndulo Norte", "Pêndulo Leste", "Pêndulo Sul", "Pêndulo Oeste"});

    QPushButton* btnResetChart = new QPushButton("Limpar e Iniciar Agora");
    btnResetChart->setObjectName("btnResetChart");
    btnResetChart->setMinimumHeight(32);
    btnResetChart->setStyleSheet("QPushButton { font-weight: bold; background-color: #ef4444; color: white; border-radius: 4px; } QPushButton:hover { background-color: #dc2626; }");

    QPushButton* btnRestoreHistory = new QPushButton("Restaurar 30 Dias");
    btnRestoreHistory->setObjectName("btnRestoreHistory");
    btnRestoreHistory->setMinimumHeight(32);
    btnRestoreHistory->setStyleSheet("QPushButton { font-weight: bold; background-color: #3b82f6; color: white; border-radius: 4px; } QPushButton:hover { background-color: #2563eb; }");

    termOptLayout->addWidget(lblSiloSelect);
    termOptLayout->addWidget(comboSilo);
    termOptLayout->addWidget(lblPendulumSelect);
    termOptLayout->addWidget(comboPendulum);
    termOptLayout->addWidget(btnResetChart);
    termOptLayout->addWidget(btnRestoreHistory);
    m_statsOptionsStack->addWidget(createOptionsPage("Filtros Termometria", termOptLayout));

    // Conecta mudanças de filtros de termometria
    connect(comboSilo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MainWindow::updateTermometriaStatsChart);
    connect(comboPendulum, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MainWindow::updateTermometriaStatsChart);

    connect(btnResetChart, &QPushButton::clicked, this, [this]() {
        m_statsStartTime = QDateTime::currentDateTime();
        updateTermometriaStatsChart();
    });

    connect(btnRestoreHistory, &QPushButton::clicked, this, [this]() {
        m_statsStartTime = QDateTime(); // invalid QDateTime
        updateTermometriaStatsChart();
    });

    // --- PÁGINA OPÇÕES 1: Aeração ---
    QVBoxLayout* aerOptLayout = new QVBoxLayout();
    QLabel* lblAerInfo = new QLabel("Histórico de Aeração\n(Últimos 30 dias)");
    lblAerInfo->setAlignment(Qt::AlignCenter);
    aerOptLayout->addWidget(lblAerInfo);
    m_statsOptionsStack->addWidget(createOptionsPage("Ações Aeração", aerOptLayout));

    // --- PÁGINA OPÇÕES 2: Rede ---
    QVBoxLayout* redeOptLayout = new QVBoxLayout();
    QLabel* lblRedeInfo = new QLabel("Estatísticas de Rede\n(Nodos e Gateway)");
    lblRedeInfo->setAlignment(Qt::AlignCenter);
    redeOptLayout->addWidget(lblRedeInfo);
    m_statsOptionsStack->addWidget(createOptionsPage("Filtros de Rede", redeOptLayout));

    // --- PÁGINA OPÇÕES 3: Meteorologia ---
    QVBoxLayout* metOptLayout = new QVBoxLayout();
    QLabel* lblMetInfo = new QLabel("Histórico de Meteorologia\n(Últimos 30 dias)");
    lblMetInfo->setAlignment(Qt::AlignCenter);
    metOptLayout->addWidget(lblMetInfo);
    m_statsOptionsStack->addWidget(createOptionsPage("Ações Meteorologia", metOptLayout));

    // --- PÁGINA OPÇÕES 3: Sistema ---
    QVBoxLayout* sysOptLayout = new QVBoxLayout();
    
    QLabel* lblIntegrityTitle = new QLabel("INTEGRIDADE DE DADOS (SQLITE)");
    lblIntegrityTitle->setStyleSheet("font-weight: bold; font-size: 10px; color: #888; letter-spacing: 1.5px; margin-bottom: 8px; margin-top: 8px;");
    sysOptLayout->addWidget(lblIntegrityTitle);

    QPushButton* btnExportHistory = new QPushButton("Exportar Dados (CSV)");
    btnExportHistory->setMinimumHeight(32);
    sysOptLayout->addWidget(btnExportHistory);

    m_statsOptionsStack->addWidget(createOptionsPage("Sistema", sysOptLayout));

    // Conexões do Sistema
    connect(btnExportHistory, &QPushButton::clicked, this, [this]() {
        QString filePath = QFileDialog::getSaveFileName(this, "Exportar Banco de Dados", "", "CSV Files (*.csv)");
        if (!filePath.isEmpty()) {
            exportDatabaseToCsv(filePath);
            QMessageBox::information(this, "Exportação", "Dados exportados com sucesso!");
        }
    });

    // Conexões de sincronização das abas (currentRowChanged)
    connect(m_statsSelectionList, &QListWidget::currentRowChanged, this, [this](int row) {
        if (row >= 0) {
            if (m_statsConfigList) {
                m_statsConfigList->blockSignals(true);
                m_statsConfigList->clearSelection();
                m_statsConfigList->setCurrentRow(-1);
                m_statsConfigList->blockSignals(false);
            }
            onStatsTabChanged(row);
        }
    });




    // Seção de Configurações Minimizável no painel direito
    QWidget *configContainer = new QWidget();
    QVBoxLayout *configLayout = new QVBoxLayout(configContainer);
    configLayout->setContentsMargins(0, 0, 0, 0);
    configLayout->setSpacing(0);

    // --- Header do Accordion "Opções" ---
    QWidget* optionsTitleWidget = new QWidget();
    QHBoxLayout* optionsTitleLayout = new QHBoxLayout(optionsTitleWidget);
    optionsTitleLayout->setContentsMargins(4, 4, 4, 4); // Margem superior reduzida para subir a estrutura
    
    QLabel *lblOptionsTitle = new QLabel("Opções");
    lblOptionsTitle->setStyleSheet("font-weight: bold; font-size: 14px;");
    
    QPushButton* btnToggleOptions = new QPushButton("");
    btnToggleOptions->setObjectName("panelToggleButton");
    btnToggleOptions->setFixedSize(24, 24);
    btnToggleOptions->setToolTip("Minimizar/Maximizar Opções");
    btnToggleOptions->setCheckable(true);
    btnToggleOptions->setChecked(true); // Inicialmente aberto

    optionsTitleLayout->addWidget(lblOptionsTitle);
    optionsTitleLayout->addStretch();
    optionsTitleLayout->addWidget(btnToggleOptions);

    configLayout->addWidget(optionsTitleWidget);

    QWidget *configContentWidget = new QWidget();
    QVBoxLayout *configContentLayout = new QVBoxLayout(configContentWidget);
    configContentLayout->setContentsMargins(0, 0, 0, 0);
    configContentLayout->setSpacing(0);

    m_statsConfigList = new QListWidget();
    m_statsConfigList->setObjectName("statsSelectionList"); // Usa o mesmo estilo global (ThemeManager)
    m_statsConfigList->addItems({"Sistema"});
    m_statsConfigList->setMinimumHeight(44);
    m_statsConfigList->setMaximumHeight(44);
    m_statsConfigList->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    // Conexões de sincronização da lista de configurações (conectada após m_statsConfigList ser criada)
    connect(m_statsConfigList, &QListWidget::currentRowChanged, this, [this](int row) {
        if (row >= 0) {
            if (m_statsSelectionList) {
                m_statsSelectionList->blockSignals(true);
                m_statsSelectionList->clearSelection();
                m_statsSelectionList->setCurrentRow(-1);
                m_statsSelectionList->blockSignals(false);
            }
            onStatsTabChanged(4); // "Sistema" agora é a quinta página
        }
    });


    configContentLayout->addWidget(m_statsConfigList);
    configLayout->addWidget(configContentWidget);
    configContainer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    connect(btnToggleOptions, &QPushButton::toggled, configContentWidget, &QWidget::setVisible);

    // Criação do contêiner da barra lateral direita
    QWidget *rightSidebar = new QWidget();
    rightSidebar->setFixedWidth(250);
    QVBoxLayout *rightSidebarLayout = new QVBoxLayout(rightSidebar);
    rightSidebarLayout->setContentsMargins(0, 0, 0, 0);
    rightSidebarLayout->setSpacing(10);
    rightSidebarLayout->addWidget(m_statsOptionsStack);
    rightSidebarLayout->addWidget(configContainer); // Posicionado logo abaixo do widget de filtros
    rightSidebarLayout->addStretch(); // Absorve o espaço excedente ao final mantendo os widgets agrupados no topo

    // Montagem do layout
    mainLayout->addWidget(leftSidebar, 0, 0);
    mainLayout->addWidget(m_statsCentralStack, 0, 1);
    mainLayout->addWidget(rightSidebar, 0, 2);

    // Inicializa os gráficos e resumos
    updateTermometriaStatsChart();
    updateAeracaoStatsChart();
    updateMeteorologiaStatsChart();
    updateStatsSummaryText();

    // Timer de atualização periódica automática (a cada 10 segundos)
    m_statsRefreshTimer = new QTimer(this);
    m_statsRefreshTimer->setInterval(10000); // 10 segundos
    connect(m_statsRefreshTimer, &QTimer::timeout, this, [this]() {
        if (ui->mainTabWidget && ui->mainTabWidget->currentWidget() == ui->estatisticasTab) {
            if (m_statsSelectionList && m_statsSelectionList->currentRow() >= 0) {
                int row = m_statsSelectionList->currentRow();
                if (row == 0) updateTermometriaStatsChart();
                else if (row == 1) updateAeracaoStatsChart();
                else if (row == 3) updateMeteorologiaStatsChart();
            } else if (m_statsConfigList && m_statsConfigList->currentRow() == 0) {
                updateStatsSummaryText();
            }
        }
    });
    m_statsRefreshTimer->start();
}

void MainWindow::updateTermometriaStatsChart() {
    if (!m_statsCentralStack) return;
    
    QComboBox* comboSilo = findChild<QComboBox*>("statsSiloCombo");
    QComboBox* comboPendulum = findChild<QComboBox*>("statsPendulumCombo");
    QChartView* chartView = findChild<QChartView*>("termStatsChartView");
    QChartView* rateChartView = findChild<QChartView*>("termRateChartView"); // O novo visualizador
    
    if (!comboSilo || !comboPendulum || !chartView) return;
    
    int siloId = comboSilo->currentIndex();
    int pendulumId = comboPendulum->currentIndex();
    
    // --- GRÁFICO 1: TEMPERATURA ABSOLUTA ---
    QChart* chart = new QChart();
    chart->setTitle(QString("Temperaturas ao Longo do Tempo - %1 (%2)")
                        .arg(comboPendulum->currentText())
                        .arg(comboSilo->currentText()));
    
    // --- GRÁFICO 2: TAXA DE VARIAÇÃO ---
    QChart* rateChart = new QChart();
    rateChart->setTitle(QString("Taxa de Variação Temporal (\u0394T/\u0394t) - %1 (%2)")
                        .arg(comboPendulum->currentText())
                        .arg(comboSilo->currentText()));
    
    const auto& colors = m_themeManager->getCurrentColors();
    chart->setBackgroundBrush(QBrush(colors.surface));
    chart->setTitleBrush(QBrush(colors.textPrimary));
    rateChart->setBackgroundBrush(QBrush(colors.surface));
    rateChart->setTitleBrush(QBrush(colors.textPrimary));
    
    QVector<QLineSeries*> seriesList;
    QVector<QLineSeries*> rateSeriesList;
    for (int i = 0; i < 13; ++i) {
        // Séries Absolutas
        QLineSeries* series = new QLineSeries();
        series->setName(QString("Sensor %1").arg(i + 1));
        seriesList.append(series);
        chart->addSeries(series);
        
        // Séries de Variação
        QLineSeries* rateSeries = new QLineSeries();
        rateSeries->setName(QString("Sensor %1").arg(i + 1));
        rateSeriesList.append(rateSeries);
        rateChart->addSeries(rateSeries);
    }
    
    QDateTime minTime, maxTime;
    float minVal = 100.0f, maxVal = -100.0f;
    float minRateVal = 100.0f, maxRateVal = -100.0f;
    
    QDateTime end = QDateTime::currentDateTime();
    QDateTime start = m_statsStartTime.isValid() ? m_statsStartTime : end.addDays(-30);
    QVector<PendulumDataPoint> history = DatabaseManager::instance().getPendulumHistory(siloId, pendulumId, start, end, 500);

    // Filtro de Ruído (Hardware Spikes)
    // Se a leitura pular instantaneamente mais que 10 °C em relação ao último valor válido, nós a invalidamos.
    float lastValidTemp[13];
    for (int i = 0; i < 13; ++i) lastValidTemp[i] = -999.0f;
    
    for (int j = 0; j < history.size(); ++j) {
        PendulumDataPoint& rec = history[j];
        for (int i = 0; i < 13; ++i) {
            float val = rec.temps[i];
            if (val > -90.0f) {
                if (lastValidTemp[i] > -90.0f && qAbs(val - lastValidTemp[i]) > 10.0f) {
                    rec.temps[i] = -999.0f; // Invalida o valor absurdo
                } else {
                    lastValidTemp[i] = val; // Atualiza a âncora de segurança
                }
            }
        }
    }

    for (int j = 0; j < history.size(); ++j) {
        const PendulumDataPoint& rec = history[j];
        qint64 msecs = rec.timestamp.toMSecsSinceEpoch();
        QDateTime dt = rec.timestamp;
        
        if (minTime.isNull() || dt < minTime) minTime = dt;
        if (maxTime.isNull() || dt > maxTime) maxTime = dt;
        
        // Povoa gráfico absoluto
        for (int i = 0; i < 13; ++i) {
            float val = rec.temps[i];
            if (val > -90.0f) {
                seriesList[i]->append(msecs, val);
                if (val < minVal) minVal = val;
                if (val > maxVal) maxVal = val;
            }
        }
        
        // Povoa gráfico de taxa de variação (a partir do 2º ponto)
        if (j > 0) {
            // Busca um ponto de referência para suavizar o cálculo (idealmente ~12 horas atrás)
            // Se não tiver 12 horas, pega o mais antigo disponível antes de 'rec'
            int refIdx = j - 1;
            for (int k = j - 1; k >= 0; --k) {
                if (rec.timestamp.toMSecsSinceEpoch() - history[k].timestamp.toMSecsSinceEpoch() >= 12 * 3600 * 1000) {
                    refIdx = k;
                    break;
                }
                refIdx = k;
            }
            
            const PendulumDataPoint& prevRec = history[refIdx];
            qint64 diffMsecs = rec.timestamp.toMSecsSinceEpoch() - prevRec.timestamp.toMSecsSinceEpoch();
            if (diffMsecs > 0) {
                double days = diffMsecs / 86400000.0;
                for (int i = 0; i < 13; ++i) {
                    float prevVal = prevRec.temps[i];
                    float val = rec.temps[i];
                    
                    if (val > -90.0f && prevVal > -90.0f) {
                        float rate = (val - prevVal) / days; // °C por dia
                        rateSeriesList[i]->append(msecs, rate);
                        if (rate < minRateVal) minRateVal = rate;
                        if (rate > maxRateVal) maxRateVal = rate;
                    }
                }
            }
        }
    }
    
    // Configuração dos Eixos (Absoluto e Taxa)
    QDateTimeAxis* axisX = new QDateTimeAxis();
    axisX->setFormat("dd/MM hh:mm");
    axisX->setTitleText("Horário");
    axisX->setLabelsColor(colors.secondary);
    axisX->setTitleBrush(QBrush(colors.textPrimary));
    
    QDateTimeAxis* rateAxisX = new QDateTimeAxis();
    rateAxisX->setFormat("dd/MM hh:mm");
    rateAxisX->setTitleText("Horário");
    rateAxisX->setLabelsColor(colors.secondary);
    rateAxisX->setTitleBrush(QBrush(colors.textPrimary));
    
    if (minTime.isNull() || maxTime.isNull() || minTime == maxTime) {
        QDateTime now = QDateTime::currentDateTime();
        QDateTime graphStart = m_statsStartTime.isValid() ? m_statsStartTime : now.addSecs(-60);
        QDateTime graphEnd = now;
        if (graphStart >= graphEnd) {
            graphStart = graphEnd.addSecs(-60);
        }
        axisX->setRange(graphStart, graphEnd);
        rateAxisX->setRange(graphStart, graphEnd);
    } else {
        QDateTime graphStart = m_statsStartTime.isValid() ? m_statsStartTime : minTime;
        QDateTime graphEnd = maxTime;
        if (graphStart >= graphEnd) {
            graphStart = graphEnd.addSecs(-60);
        }
        axisX->setRange(graphStart, graphEnd);
        rateAxisX->setRange(graphStart, graphEnd);
    }
    chart->addAxis(axisX, Qt::AlignBottom);
    rateChart->addAxis(rateAxisX, Qt::AlignBottom);
    
    QValueAxis* axisY = new QValueAxis();
    axisY->setTitleText("Temperatura (°C)");
    axisY->setLabelsColor(colors.secondary);
    axisY->setTitleBrush(QBrush(colors.textPrimary));
    
    if (minVal > 90.0f || maxVal < -90.0f) {
        axisY->setRange(0, 50);
    } else {
        axisY->setRange(qMax(0.0f, minVal - 2.0f), qMin(60.0f, maxVal + 2.0f));
    }
    chart->addAxis(axisY, Qt::AlignLeft);
    
    QValueAxis* rateAxisY = new QValueAxis();
    rateAxisY->setTitleText("Taxa de Aquecimento (°C/dia)");
    rateAxisY->setLabelsColor(colors.secondary);
    rateAxisY->setTitleBrush(QBrush(colors.textPrimary));
    
    if (minRateVal > 90.0f || maxRateVal < -90.0f) {
        rateAxisY->setRange(-2.0, 2.0);
    } else {
        float rMin = qMin(-1.0f, minRateVal - 0.5f);
        float rMax = qMax(1.5f, maxRateVal + 0.5f);
        rateAxisY->setRange(rMin, rMax);
    }
    rateChart->addAxis(rateAxisY, Qt::AlignLeft);
    
    for (int i = 0; i < 13; ++i) {
        seriesList[i]->attachAxis(axisX);
        seriesList[i]->attachAxis(axisY);
        
        rateSeriesList[i]->attachAxis(rateAxisX);
        rateSeriesList[i]->attachAxis(rateAxisY);
    }
    
    chart->legend()->setVisible(true);
    chart->legend()->setAlignment(Qt::AlignRight);
    chart->legend()->setLabelBrush(QBrush(colors.textPrimary));
    
    rateChart->legend()->setVisible(true);
    rateChart->legend()->setAlignment(Qt::AlignRight);
    rateChart->legend()->setLabelBrush(QBrush(colors.textPrimary));
    
    QChart* oldChart = chartView->chart();
    chartView->setChart(chart);
    if (oldChart) delete oldChart;
    
    if (rateChartView) {
        QChart* oldRateChart = rateChartView->chart();
        rateChartView->setChart(rateChart);
        if (oldRateChart) delete oldRateChart;
    }
}

void MainWindow::updateAeracaoStatsChart() {
    if (!m_statsCentralStack) return;
    
    QChartView* chartView = findChild<QChartView*>("aerStatsChartView");
    if (!chartView) return;
    
    QChart* chart = new QChart();
    chart->setTitle("Histórico de Acionamento dos Motores (Aeração)");
    
    const auto& colors = m_themeManager->getCurrentColors();
    chart->setBackgroundBrush(QBrush(colors.surface));
    chart->setTitleBrush(QBrush(colors.textPrimary));
    
    QStringList motorNames = {"Motor Alpha", "Motor Beta", "Motor Charlie", "Motor Delta"};
    QVector<QLineSeries*> seriesList;
    for (int i = 0; i < 4; ++i) {
        QLineSeries* series = new QLineSeries();
        series->setName(motorNames[i]);
        seriesList.append(series);
        chart->addSeries(series);
    }
    
    QDateTime minTime, maxTime;
    QDateTime end = QDateTime::currentDateTime();
    QDateTime start = end.addDays(-30);

    for (int idx = 0; idx < 4; ++idx) {
        int motorId = idx + 1;
        QVector<MotorEventDataPoint> history = DatabaseManager::instance().getMotorHistory(motorId, start, end);
        
        bool hasLast = false;
        float lastVal = 0.0f;
        for (const MotorEventDataPoint& rec : history) {
            qint64 msecs = rec.timestamp.toMSecsSinceEpoch();
            QDateTime dt = rec.timestamp;
            
            if (minTime.isNull() || dt < minTime) minTime = dt;
            if (maxTime.isNull() || dt > maxTime) maxTime = dt;
            
            float val = idx + (rec.isOn ? 0.8f : 0.0f);
            
            if (hasLast) {
                seriesList[idx]->append(msecs - 1, lastVal);
            }
            seriesList[idx]->append(msecs, val);
            
            lastVal = val;
            hasLast = true;
        }
    }
    
    QDateTimeAxis* axisX = new QDateTimeAxis();
    axisX->setFormat("dd/MM hh:mm");
    axisX->setTitleText("Horário");
    axisX->setLabelsColor(colors.secondary);
    axisX->setTitleBrush(QBrush(colors.textPrimary));
    
    if (minTime.isNull() || maxTime.isNull() || minTime == maxTime) {
        QDateTime now = QDateTime::currentDateTime();
        axisX->setRange(now.addSecs(-60), now);
    } else {
        axisX->setRange(minTime, maxTime);
    }
    chart->addAxis(axisX, Qt::AlignBottom);
    
    QValueAxis* axisY = new QValueAxis();
    axisY->setTitleText("Estado do Motor");
    axisY->setLabelsColor(colors.secondary);
    axisY->setTitleBrush(QBrush(colors.textPrimary));
    axisY->setRange(0, 4);
    axisY->setTickCount(5);
    chart->addAxis(axisY, Qt::AlignLeft);
    
    for (auto* series : seriesList) {
        series->attachAxis(axisX);
        series->attachAxis(axisY);
    }
    
    QChart* oldChart = chartView->chart();
    chartView->setChart(chart);
    if (oldChart) {
        delete oldChart;
    }
}

void MainWindow::updateMeteorologiaStatsChart() {
    if (!m_statsCentralStack) return;
    
    QChartView* chartView = findChild<QChartView*>("meteoStatsChartView");
    if (!chartView) return;
    
    QChart* chart = new QChart();
    chart->setTitle("Temperatura e Umidade Ambiente (Estação Meteorológica)");
    
    const auto& colors = m_themeManager->getCurrentColors();
    chart->setBackgroundBrush(QBrush(colors.surface));
    chart->setTitleBrush(QBrush(colors.textPrimary));
    
    QLineSeries* tempSeries = new QLineSeries();
    tempSeries->setName("Temperatura (°C)");
    chart->addSeries(tempSeries);
    
    QLineSeries* humSeries = new QLineSeries();
    humSeries->setName("Umidade (%)");
    chart->addSeries(humSeries);
    
    QDateTime minTime, maxTime;
    float minTemp = 100.0f, maxTemp = -100.0f;
    float minHum = 100.0f, maxHum = -100.0f;
    
    QDateTime end = QDateTime::currentDateTime();
    QDateTime start = end.addDays(-30);
    QVector<MeteorologyDataPoint> history = DatabaseManager::instance().getMeteorologyHistory(start, end, 500);

    for (const MeteorologyDataPoint& rec : history) {
        qint64 msecs = rec.timestamp.toMSecsSinceEpoch();
        QDateTime dt = rec.timestamp;
        
        if (minTime.isNull() || dt < minTime) minTime = dt;
        if (maxTime.isNull() || dt > maxTime) maxTime = dt;
        
        float t = rec.temperature;
        float h = rec.humidity;
        
        tempSeries->append(msecs, t);
        humSeries->append(msecs, h);
        
        if (t < minTemp) minTemp = t;
        if (t > maxTemp) maxTemp = t;
        if (h < minHum) minHum = h;
        if (h > maxHum) maxHum = h;
    }
    
    QDateTimeAxis* axisX = new QDateTimeAxis();
    axisX->setFormat("dd/MM hh:mm");
    axisX->setTitleText("Horário");
    axisX->setLabelsColor(colors.secondary);
    axisX->setTitleBrush(QBrush(colors.textPrimary));
    
    if (minTime.isNull() || maxTime.isNull() || minTime == maxTime) {
        QDateTime now = QDateTime::currentDateTime();
        axisX->setRange(now.addSecs(-60), now);
    } else {
        axisX->setRange(minTime, maxTime);
    }
    chart->addAxis(axisX, Qt::AlignBottom);
    
    QValueAxis* axisY = new QValueAxis();
    axisY->setTitleText("Valores");
    axisY->setLabelsColor(colors.secondary);
    axisY->setTitleBrush(QBrush(colors.textPrimary));
    
    float low = qMin(0.0f, qMin(minTemp, minHum) - 2.0f);
    float high = qMax(100.0f, qMax(maxTemp, maxHum) + 2.0f);
    axisY->setRange(low, high);
    chart->addAxis(axisY, Qt::AlignLeft);
    
    tempSeries->attachAxis(axisX);
    tempSeries->attachAxis(axisY);
    
    humSeries->attachAxis(axisX);
    humSeries->attachAxis(axisY);
    
    QChart* oldChart = chartView->chart();
    chartView->setChart(chart);
    if (oldChart) {
        delete oldChart;
    }
}

void MainWindow::onSensorPositionChanged(int gateway, int node, int rawSensorId, int newPosition)
{
    int p = node - 1;
    int s = (rawSensorId - 1) % 13;

    if (p >= 0 && p < 5 && s >= 0 && s < 13) {
        int oldPosition = m_sensorMapping[p][s];
        if (oldPosition != newPosition) {
            // Encontra o outro sensor no mesmo pêndulo que possui a nova posição e faz a troca (swap)
            int swap_s = -1;
            for (int other_s = 0; other_s < 13; ++other_s) {
                if (other_s != s && m_sensorMapping[p][other_s] == newPosition) {
                    swap_s = other_s;
                    break;
                }
            }

            if (swap_s != -1) {
                m_sensorMapping[p][swap_s] = oldPosition;
                // Atualiza o combo box do outro widget se ele estiver no grid layout
                int swapSensorId = (p * 13) + swap_s + 1;
                for (int i = 0; i < ui->sensorGridLayout->count(); ++i) {
                    if (SensorWidget* other_sw = qobject_cast<SensorWidget*>(ui->sensorGridLayout->itemAt(i)->widget())) {
                        if (other_sw->sensorId() == swapSensorId) {
                            other_sw->setPhysicalPosition(oldPosition);
                            break;
                        }
                    }
                }
            }

            m_sensorMapping[p][s] = newPosition;
            saveSensorMapping();

            if (swap_s != -1) {
                qInfo() << QString("Posição do sensor alterada (com troca): Pêndulo %1, Sensor Raw %2 -> Posição %3 (Trocado com Sensor Raw %4 -> Posição %5)")
                           .arg(p + 1).arg(rawSensorId).arg(newPosition).arg((p * 13) + swap_s + 1).arg(oldPosition);
            } else {
                qInfo() << QString("Posição do sensor alterada: Pêndulo %1, Sensor Raw %2 -> Posição %3")
                           .arg(p + 1).arg(rawSensorId).arg(newPosition);
            }

            // Atualiza as posições em todas as visualizações imediatamente
            updateSensorGridLayout();
            if (m_silo2DWidget) {
                m_silo2DWidget->updateSensorPositions(m_sensorMapping);
            }
            if (m_glWidget) {
                m_glWidget->updateSensorPositions(m_sensorMapping);
            }
        }
    }
}

void MainWindow::loadSensorMapping()
{
    QSettings settings("VilarinoEngenharia", "GrainMonitor");
    int size = settings.beginReadArray("sensor_mapping");
    
    bool loadFailedOrCorrupted = false;
    
    if (size == 65) {
        for (int p = 0; p < 5; ++p) {
            QSet<int> seenPositions;
            bool pendulumCorrupted = false;
            
            // Primeiro lê e valida para este pêndulo
            int tempMapping[13];
            for (int s = 0; s < 13; ++s) {
                settings.setArrayIndex(p * 13 + s);
                int pos = settings.value("position", s + 1).toInt();
                if (pos < 1 || pos > 13 || seenPositions.contains(pos)) {
                    pendulumCorrupted = true;
                    break;
                }
                seenPositions.insert(pos);
                tempMapping[s] = pos;
            }
            
            if (!pendulumCorrupted) {
                for (int s = 0; s < 13; ++s) {
                    m_sensorMapping[p][s] = tempMapping[s];
                }
            } else {
                qWarning() << QString("Mapeamento do pêndulo %1 corrompido ou duplicado no QSettings. Restaurando padrão.").arg(p + 1);
                for (int s = 0; s < 13; ++s) {
                    m_sensorMapping[p][s] = s + 1;
                }
                loadFailedOrCorrupted = true;
            }
        }
    } else {
        for (int p = 0; p < 5; ++p) {
            for (int s = 0; s < 13; ++s) {
                m_sensorMapping[p][s] = s + 1;
            }
        }
        loadFailedOrCorrupted = true;
    }
    settings.endArray();
    
    if (loadFailedOrCorrupted) {
        saveSensorMapping();
    }
}

void MainWindow::saveSensorMapping()
{
    QSettings settings("VilarinoEngenharia", "GrainMonitor");
    settings.beginWriteArray("sensor_mapping");
    for (int p = 0; p < 5; ++p) {
        for (int s = 0; s < 13; ++s) {
            settings.setArrayIndex(p * 13 + s);
            settings.setValue("position", m_sensorMapping[p][s]);
        }
    }
    settings.endArray();
}

void MainWindow::updateSensorGridLayout()
{
    // Cria uma lista de todos os widgets de sensores atuais.
    QList<SensorWidget*> widgets;
    for (int i = 0; i < ui->sensorGridLayout->count(); ++i) {
        if (SensorWidget* sw = qobject_cast<SensorWidget*>(ui->sensorGridLayout->itemAt(i)->widget())) {
            widgets.append(sw);
        }
    }

    // Desanexa todos os widgets do layout sem excluí-los.
    for (SensorWidget* sw : widgets) {
        ui->sensorGridLayout->removeWidget(sw);
    }

    // Mapeamento da ordem visual (coluna/linha 0 a 4) para a ordem canônica dos dados (pêndulo 0 a 4).
    const int visual_to_data_map[5] = { 4, 3, 0, 1, 2 };

    // Re-adiciona os widgets na orientação atual, preservando as posições mapeadas.
    for (SensorWidget* sw : widgets) {
        int sensorId = sw->sensorId();
        int data_p = (sensorId - 1) / 13;
        int s_idx = (sensorId - 1) % 13;

        // Encontra o índice visual correspondente ao pêndulo do sensor.
        int visual_p = -1;
        for (int i = 0; i < 5; ++i) {
            if (visual_to_data_map[i] == data_p) {
                visual_p = i;
                break;
            }
        }
        if (visual_p == -1) continue;

        int gridPos = m_sensorMapping[data_p][s_idx] - 1;
        if (gridPos < 0 || gridPos >= 13) gridPos = s_idx;

        if (m_gridHorizontal) { // Pêndulos como linhas
            ui->sensorGridLayout->addWidget(sw, visual_p, gridPos);
        } else { // Pêndulos como colunas
            ui->sensorGridLayout->addWidget(sw, gridPos, visual_p);
        }
    }
}

void MainWindow::onExportSensorOrder()
{
    QJsonObject root;
    root["tipo"] = "sensor_mapping";
    root["data_de_exportacao"] = QDateTime::currentDateTime().toString(Qt::ISODate);

    QJsonArray mappingArray;
    for (int p = 0; p < 5; ++p) {
        QJsonArray pendulumArray;
        for (int s = 0; s < 13; ++s) {
            pendulumArray.append(m_sensorMapping[p][s]);
        }
        mappingArray.append(pendulumArray);
    }
    root["sensor_mapping"] = mappingArray;

    QString fileName = QFileDialog::getSaveFileName(this, "Exportar Ordem dos Sensores",
        "ordem_sensores.json", "JSON Files (*.json)");
    if (!fileName.isEmpty()) {
        QFile file(fileName);
        if (file.open(QIODevice::WriteOnly)) {
            file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
            file.close();
            QMessageBox::information(this, "Sucesso", "Ordem dos sensores exportada com sucesso!");
        } else {
            QMessageBox::warning(this, "Erro", "Não foi possível salvar o arquivo. Verifique as permissões de gravação.");
        }
    }
}

void MainWindow::onImportSensorOrder()
{
    QString fileName = QFileDialog::getOpenFileName(this, "Importar Ordem dos Sensores",
        "", "JSON Files (*.json)");
    if (fileName.isEmpty()) {
        return;
    }

    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly)) {
        QMessageBox::warning(this, "Erro", "Não foi possível abrir o arquivo selecionado.");
        return;
    }

    QByteArray data = file.readAll();
    file.close();

    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        QMessageBox::critical(this, "Erro de Formato", "O arquivo não contém um JSON válido: " + parseError.errorString());
        return;
    }

    if (!doc.isObject()) {
        QMessageBox::critical(this, "Erro de Formato", "O JSON raiz deve ser um objeto.");
        return;
    }

    QJsonObject root = doc.object();
    if (!root.contains("tipo") || root["tipo"].toString() != "sensor_mapping") {
        QMessageBox::critical(this, "Erro de Compatibilidade", "Este arquivo não parece ser um mapeamento de ordem de sensores válido.");
        return;
    }

    if (!root.contains("sensor_mapping") || !root["sensor_mapping"].isArray()) {
        QMessageBox::critical(this, "Erro de Formato", "O campo 'sensor_mapping' é obrigatório e deve ser uma matriz.");
        return;
    }

    QJsonArray mappingArray = root["sensor_mapping"].toArray();
    if (mappingArray.size() != 5) {
        QMessageBox::critical(this, "Erro de Validação", "O mapeamento deve conter dados para exatamente 5 pêndulos.");
        return;
    }

    int tempMapping[5][13];

    for (int p = 0; p < 5; ++p) {
        if (!mappingArray[p].isArray()) {
            QMessageBox::critical(this, "Erro de Validação", QString("Os dados do pêndulo %1 devem ser uma matriz.").arg(p + 1));
            return;
        }

        QJsonArray pendulumArray = mappingArray[p].toArray();
        if (pendulumArray.size() != 13) {
            QMessageBox::critical(this, "Erro de Validação", QString("O pêndulo %1 deve conter exatamente 13 sensores.").arg(p + 1));
            return;
        }

        QSet<int> seenPositions;
        for (int s = 0; s < 13; ++s) {
            QJsonValue val = pendulumArray[s];
            if (!val.isDouble()) {
                QMessageBox::critical(this, "Erro de Validação", QString("O sensor %1 do pêndulo %2 possui um valor não numérico.").arg(s + 1).arg(p + 1));
                return;
            }

            int pos = val.toInt();
            if (pos < 1 || pos > 13) {
                QMessageBox::critical(this, "Erro de Validação", QString("O sensor %1 do pêndulo %2 possui posição fora do intervalo [1, 13] (%3).").arg(s + 1).arg(p + 1).arg(pos));
                return;
            }

            if (seenPositions.contains(pos)) {
                QMessageBox::critical(this, "Erro de Validação", QString("O pêndulo %1 possui posições duplicadas (posição %2 duplicada).").arg(p + 1).arg(pos));
                return;
            }

            seenPositions.insert(pos);
            tempMapping[p][s] = pos;
        }
    }

    // Se passou em todas as validações, atualiza o mapeamento na memória
    for (int p = 0; p < 5; ++p) {
        for (int s = 0; s < 13; ++s) {
            m_sensorMapping[p][s] = tempMapping[p][s];
        }
    }

    // Salva o mapeamento nos QSettings persistentes da aplicação
    saveSensorMapping();

    // Atualiza a disposição visual das células na grade
    updateSensorGridLayout();

    // Atualiza as visualizações 2D e 3D
    if (m_silo2DWidget) {
        m_silo2DWidget->updateSensorPositions(m_sensorMapping);
    }
    if (m_glWidget) {
        m_glWidget->updateSensorPositions(m_sensorMapping);
    }

    // Atualiza o estado físico/seleção visual do combobox de cada SensorWidget no layout
    for (int i = 0; i < ui->sensorGridLayout->count(); ++i) {
        if (SensorWidget* sw = qobject_cast<SensorWidget*>(ui->sensorGridLayout->itemAt(i)->widget())) {
            int sensorId = sw->sensorId();
            int p = (sensorId - 1) / 13;
            int s = (sensorId - 1) % 13;
            if (p >= 0 && p < 5 && s >= 0 && s < 13) {
                sw->setPhysicalPosition(m_sensorMapping[p][s]);
            }
        }
    }

    QMessageBox::information(this, "Sucesso", "Ordem dos sensores importada e aplicada com sucesso!");
}

void MainWindow::closeEvent(QCloseEvent *event) {
    // O banco de dados salva dados automaticamente de forma persistente. Apenas encerra.
    event->accept();
}

void MainWindow::exportDatabaseToCsv(const QString& filePath) {
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) return;
    QTextStream out(&file);
    out << "Timestamp,Sensor/Motor,Valor\n";
    out << "Os dados estão salvos no Banco SQLite. Esta função pode ser iterada no futuro para dumps complexos.\n";
    file.close();
}

void MainWindow::updateStatsSummaryText() {
    if (!m_lblStatsSummary) return;

    QString summary = QString(
        "<b>Armazenamento Seguro (Banco de Dados SQLite)</b><br/><br/>"
        "Os dados de Termometria, Meteorologia e Aeração estão sendo registrados "
        "e armazenados de forma contínua e persistente.<br/><br/>"
        "<i>Não é necessário se preocupar com perda de dados ou quedas "
        "de energia. O salvamento é efetuado em tempo real.</i>"
    );

    m_lblStatsSummary->setText(summary);
}

QString MainWindow::formatDuration(int seconds) {
    if (seconds < 0) return "0 segundos";
    int hours = seconds / 3600;
    int minutes = (seconds % 3600) / 60;
    int secs = seconds % 60;
    
    QStringList parts;
    if (hours > 0) parts.append(QString("%1 hora(s)").arg(hours));
    if (minutes > 0) parts.append(QString("%1 minuto(s)").arg(minutes));
    if (secs > 0 || parts.isEmpty()) parts.append(QString("%1 segundo(s)").arg(secs));
    
    return parts.join(", ");
}

