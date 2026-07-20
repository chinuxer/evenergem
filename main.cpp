#include "mainwindow.h"
#include "telnetconsole.h"
#include <QApplication>
#include <QDebug>
#include <QMessageBox>
#include <QDialog>
#include <QVBoxLayout>
#include <QTextEdit>
#include <QDialogButtonBox>
#include <QFile>
#include <QTextStream>
#include <QPushButton>
#include <QMetaObject>
#include <QObject>
#include <QString>
#include <QtGlobal>
#include <cstdarg>
#include <cstdio>
#include "topologyselectdialog.h"
// 全局指针，用于消息处理器访问
static TelnetConsole *g_telnetConsole = nullptr;

extern "C" void pau_log_printf(const char *fmt, ...)
{
    char buffer[4096];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);

    fprintf(stderr, "%s", buffer);
    fflush(stderr);

    if (g_telnetConsole)
    {
        QString msg = QString::fromLocal8Bit(buffer);
        // if (!msg.endsWith("\n"))
        //{
        //     msg.append("\r\n");
        // }
        QMetaObject::invokeMethod(g_telnetConsole, "doSendToAll",
                                  Qt::QueuedConnection,
                                  Q_ARG(QString, msg));
    }
}

// 自定义消息处理器
// ANSI 颜色代码
#define ANSI_COLOR_RESET "\033[0m"
#define ANSI_COLOR_DEBUG "\033[36m"    // 青色
#define ANSI_COLOR_INFO "\033[32m"     // 绿色
#define ANSI_COLOR_WARNING "\033[33m"  // 黄色
#define ANSI_COLOR_CRITICAL "\033[31m" // 红色
#define ANSI_COLOR_FATAL "\033[31;1m"  // 红色加粗

void messageHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg)
{
    // 构建带颜色的消息（用于 telnet）
    QString coloredMsg;
    QString plainMsg; // 用于控制台输出（不带颜色，保持可读）

    switch (type)
    {
    case QtDebugMsg:
        coloredMsg = QString("%1[Debug] %2%3").arg(ANSI_COLOR_DEBUG).arg(msg).arg(ANSI_COLOR_RESET);
        plainMsg = QString("[Debug] %1").arg(msg);
        break;
    case QtInfoMsg:
        coloredMsg = QString("%1[Info] %2%3").arg(ANSI_COLOR_INFO).arg(msg).arg(ANSI_COLOR_RESET);
        plainMsg = QString("[Info] %1").arg(msg);
        break;
    case QtWarningMsg:
        coloredMsg = QString("%1[Warning] %2%3").arg(ANSI_COLOR_WARNING).arg(msg).arg(ANSI_COLOR_RESET);
        plainMsg = QString("[Warning] %1").arg(msg);
        break;
    case QtCriticalMsg:
        coloredMsg = QString("%1[Critical] %2%3").arg(ANSI_COLOR_CRITICAL).arg(msg).arg(ANSI_COLOR_RESET);
        plainMsg = QString("[Critical] %1").arg(msg);
        break;
    case QtFatalMsg:
        coloredMsg = QString("%1[Fatal] %2%3").arg(ANSI_COLOR_FATAL).arg(msg).arg(ANSI_COLOR_RESET);
        plainMsg = QString("[Fatal] %1").arg(msg);
        break;
    }

    // 输出到控制台/日志文件（不带颜色，避免乱码）
    fprintf(stderr, "%s\n", plainMsg.toLocal8Bit().constData());
    fflush(stderr);

    // 异步发送带颜色的消息到 Telnet 客户端
    if (g_telnetConsole)
    {
        QMetaObject::invokeMethod(g_telnetConsole, "doSendToAll",
                                  Qt::QueuedConnection,
                                  Q_ARG(QString, coloredMsg));
    }

    if (type == QtFatalMsg)
    {
        abort();
    }
}

// 显示免责声明对话框，返回 true 表示用户接受，false 表示拒绝
static bool showDisclaimer()
{
    // 从资源文件中读取免责文本
    QFile file(":/disclaimer.txt"); // 冒号+斜杠表示资源系统
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        QMessageBox::critical(nullptr, "启动失败",
                              "无法加载免责声明资源，程序将退出。");
        return false;
    }

    QTextStream stream(&file);
    stream.setCodec("UTF-8");
    QString disclaimerText = stream.readAll();
    file.close();

    // 创建对话框
    QDialog dialog;
    dialog.setWindowTitle("免责声明");
    dialog.setMinimumSize(650, 550);

    QVBoxLayout *layout = new QVBoxLayout(&dialog);

    QTextEdit *textEdit = new QTextEdit();
    textEdit->setReadOnly(true);
    textEdit->setPlainText(disclaimerText);
    textEdit->setStyleSheet(
        "QTextEdit {"
        "    background-color: #15232e;"
        "    color: #b7daeb;"
        "    font-family: 'Microsoft YaHei', 'SimHei';"
        "    font-size: 11pt;"
        "}");
    layout->addWidget(textEdit);

    QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Yes | QDialogButtonBox::No);
    buttonBox->button(QDialogButtonBox::Yes)->setText("接受");
    buttonBox->button(QDialogButtonBox::No)->setText("拒绝");
    layout->addWidget(buttonBox);

    QObject::connect(buttonBox, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    QObject::connect(buttonBox, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    return dialog.exec() == QDialog::Accepted;
}
int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    a.setStyleSheet(
        "QDialog, QMessageBox {"
        "    background-color: #1e2a3a;" // 深蓝色背景
        "    color: #f0f0f0;"            // 浅灰色文字
        "}"
        "QDialog QLabel, QMessageBox QLabel {"
        "    color: #f0f0f0;"
        "}"
        "QDialog QPushButton, QMessageBox QPushButton {"
        "    background-color: #2c3e50;"
        "    color: white;"
        "    border: 1px solid #5a6e7a;"
        "    padding: 5px 10px;"
        "    min-width: 70px;"
        "}"
        "QDialog QPushButton:hover, QMessageBox QPushButton:hover {"
        "    background-color: #3a5a6e;"
        "}"
        "QDialog QPushButton:pressed, QMessageBox QPushButton:pressed {"
        "    background-color: #1a2a3a;"
        "}"
        "QTextEdit {"
        "    background-color: #15232e;" // 文本编辑区深色背景
        "    color: #e0e0e0;"
        "    border: 1px solid #2c3e50;"
        "}");
    // 显示免责声明，用户拒绝则直接退出
    // if (!showDisclaimer())
    //{
    //    return 0;
    //}
    // 显示拓扑选择对话框
    TopologySelectDialog selectDlg;
    if (selectDlg.exec() != QDialog::Accepted)
    {
        return 0; // 用户取消则退出
    }
    TOPOTYPE chosen = selectDlg.selectedTopology();
    // 创建 Telnet 控制台
    TelnetConsole telnetConsole;
    g_telnetConsole = &telnetConsole;
    telnetConsole.start(24601);

    // 安装消息处理器
    qInstallMessageHandler(messageHandler);

    // 平台特定设置
#ifdef Q_OS_LINUX
    qputenv("QT_QPA_PLATFORM", "xcb");
    printf("Running on Linux platform\r\n");
#elif defined(Q_OS_WIN)
    printf("Running on Windows platform\r\n");
#endif

    a.setApplicationName("evenergem");
    a.setApplicationVersion("1.0");
    a.setOrganizationName("chinuxer");

    MainWindow w(chosen);
    w.show();

    int ret = a.exec();

    // 清理：恢复默认消息处理器
    qInstallMessageHandler(nullptr);
    g_telnetConsole = nullptr;

    return ret;
}
