#include <QApplication>
#include <QMessageBox>
#include <QFile>
#include <QTextStream>
#include <QDateTime>
#include "mainwindow.h"
#include <QTimer>
#include <QPointer>

// Custom log handler to provide more context in log files.
// Usamos um QPointer para acessar a MainWindow de forma segura, especialmente se o log
// for chamado de outras threads.
static QPointer<MainWindow> g_mainWindow = nullptr;
static void customLogHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg)
{
    QString log_message;
    QTextStream stream(&log_message);

    stream << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz ");

    switch (type) {
        case QtDebugMsg:    stream << "[DEBUG]   "; break;
        case QtInfoMsg:     stream << "[INFO]    "; break;
        case QtWarningMsg:  stream << "[WARNING] "; break;
        case QtCriticalMsg: stream << "[CRITICAL]"; break;
        case QtFatalMsg:    stream << "[FATAL]   "; break;
    }

    stream << " " << msg;
    if (context.file) {
        stream << " (" << context.file << ":" << context.line << ", " << context.function << ")";
    }

    // Write to file
    /*
    QFile file("debug_qt_log.txt");
    if (file.open(QIODevice::Append | QIODevice::Text)) {
        QTextStream out(&file); // Em Qt6, QTextStream com QIODevice::Text usa UTF-8 por padrão.
        out << log_message << Qt::endl;
        file.close();
    }
    */

    // Also print to standard error output for live debugging
    QTextStream(stderr) << log_message << Qt::endl;

    // Envia a mensagem para a UI de forma segura (thread-safe)
    // A conexão é feita com AutoConnection. Se o log for gerado na thread principal
    // (antes do loop de eventos), a chamada será direta. Se for de outra thread,
    // será enfileirada, o que é seguro após o início do loop de eventos.
    if (g_mainWindow) {
        // Usar AutoConnection resolve o problema de deadlock na inicialização.
        QMetaObject::invokeMethod(g_mainWindow, "appendToLog", Qt::AutoConnection, Q_ARG(QString, log_message));
    }

    if (type == QtFatalMsg) {
        QMessageBox::critical(nullptr, "Erro Fatal",
                              "Ocorreu um erro fatal e a aplicação precisa ser encerrada.\n\n"
                              "Detalhes: " + msg + "\n\n"
                              "Consulte o arquivo debug_qt_log.txt para mais informações.");
        abort(); // A fatal error is unrecoverable. Aborting is appropriate.
    }
}

int main(int argc, char *argv[]) {
    qInstallMessageHandler(customLogHandler);
    qInfo() << "Application starting...";

    // High-DPI scaling is always enabled in Qt6; the attribute is deprecated and unnecessary.
    QApplication app(argc, argv);
    qInfo() << "QApplication instance created.";

    MainWindow w;
    qInfo() << "MainWindow instance created.";
    g_mainWindow = &w; // Registra a instância da MainWindow para o logger global
    qInfo() << "Global main window pointer assigned.";

    // Define a aba "Rede" como a aba inicial a ser exibida
    w.setInitialTabByName("Rede");
    qInfo() << "Initial tab set to 'Rede'.";
    w.show();
    qInfo() << "Main window shown. Entering event loop.";

    // Adia a inicialização da rede e a aplicação do stylesheet global para depois que o
    // loop de eventos iniciar. Isso evita qualquer tipo de deadlock ou chamada de UI
    // em momento inadequado durante a inicialização, garantindo que a janela principal
    // seja exibida primeiro.
    qInfo() << "Scheduling post-initialization tasks (network, stylesheet)...";
    QTimer::singleShot(0, &w, [&w]() {
        w.startNetworkInitialization();
        w.performPostStartupUIUpdates();
    });

    int result = app.exec();
    qInfo() << "Application finished with exit code" << result;
    return result;
}