#include "logwindow.h"
#include <QVBoxLayout>
#include <QTextCursor>
#include <QRegularExpression>
#include <QColor>
#include <QResizeEvent>
#include <QDir>
#include <QCoreApplication>
#include <QDebug>
LogWindow::LogWindow(QWidget *parent)
    : QDialog(parent), m_initialSize(800, 400), m_paused(false), m_logFile(nullptr), m_logStream(nullptr), m_nodeCount(0), m_pileCount(0), m_fileOpened(false)
{
    setWindowTitle("远程 Telnet 日志");
    resize(m_initialSize);
    setWindowFlags(windowFlags() | Qt::WindowMaximizeButtonHint);

    m_textEdit = new QTextEdit(this);
    m_textEdit->setReadOnly(true);
    m_textEdit->setStyleSheet("QTextEdit { background-color: #1e1e1e; color: #d4d4d4; }");

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_textEdit);
    setLayout(layout);

    // 创建亚克力风格按钮，置于文本框右下角
    m_pauseBtn = new QPushButton("暂停", m_textEdit);
    m_pauseBtn->setCursor(Qt::PointingHandCursor);
    m_pauseBtn->setStyleSheet(
        "QPushButton {"
        "    background-color: rgba(30, 35, 50, 180);"
        "    color: #a3ccf5;"
        "    border: 1px solid rgba(100, 150, 200, 100);"
        "    border-radius: 5px;"
        "    padding: 4px 10px;"
        "    font-size: 10pt;"
        "    font-weight: bold;"
        "}"
        "QPushButton:hover {"
        "    background-color: rgba(60, 70, 90, 220);"
        "    border: 1px solid #2a82da;"
        "}"
        "QPushButton:pressed {"
        "    background-color: rgba(20, 25, 40, 200);"
        "}");
    m_pauseBtn->setFixedSize(60, 26);
    updateButtonPosition();

    connect(m_pauseBtn, &QPushButton::clicked, this, &LogWindow::togglePause);

    // 延迟创建日志文件，等待 setTopologyInfo() 被调用
    // 不在这里创建文件，避免产生临时 unknown_n0_p0.log 文件
}

LogWindow::~LogWindow()
{
    // 关闭日志文件
    if (m_logStream)
    {
        m_logStream->flush();
        delete m_logStream;
        m_logStream = nullptr;
    }
    if (m_logFile)
    {
        if (m_logFile->isOpen())
        {
            m_logFile->close();
        }
        delete m_logFile;
        m_logFile = nullptr;
    }
}

void LogWindow::setTopologyInfo(const QString &topologyType, int nodeCount, int pileCount)
{
    m_topologyType = topologyType;
    m_nodeCount = nodeCount;
    m_pileCount = pileCount;

    // 如果文件还没打开，则创建
    if (!m_fileOpened)
    {
        openLogFile();
    }
    else
    {
        // 如果已打开，关闭旧文件，重新打开以使用新名称（通常不会走到这个分支）
        if (m_logStream)
        {
            m_logStream->flush();
            delete m_logStream;
            m_logStream = nullptr;
        }
        if (m_logFile)
        {
            if (m_logFile->isOpen())
            {
                m_logFile->close();
            }
            delete m_logFile;
            m_logFile = nullptr;
        }
        m_fileOpened = false;
        openLogFile();
    }
}

void LogWindow::openLogFile()
{
    // 如果拓扑信息尚未设置，不创建文件
    if (m_topologyType.isEmpty() || m_nodeCount == 0 || m_pileCount == 0)
    {
        qWarning() << "LogWindow: Topology info not set, deferring log file creation";
        return;
    }

    // 使用当前程序运行目录
    QString logDirPath = QCoreApplication::applicationDirPath();
    logDirPath += "/evenergem_logs";

    // 创建目录
    QDir logDir(logDirPath);
    if (!logDir.exists())
    {
        if (!logDir.mkpath("."))
        {
            qWarning() << "Failed to create log directory:" << logDirPath;
            return;
        }
    }

    // 构建文件名
    // 格式: YYYYMMDD_HHMMSS_topology_nodes_piles.log
    QString timestamp = QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss");
    QString topoStr = m_topologyType.isEmpty() ? "unknown" : m_topologyType.toLower();
    QString fileName = QString("%1_%2_n%3_p%4.log")
                           .arg(timestamp)
                           .arg(topoStr)
                           .arg(m_nodeCount)
                           .arg(m_pileCount);

    QString filePath = logDirPath + "/" + fileName;

    // 创建并打开文件
    m_logFile = new QFile(filePath, this);
    if (!m_logFile->open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Append))
    {
        qWarning() << "Failed to open log file:" << filePath;
        delete m_logFile;
        m_logFile = nullptr;
        return;
    }

    m_logStream = new QTextStream(m_logFile);
    m_logStream->setCodec("UTF-8");
    m_fileOpened = true;

    // 写入文件头
    QString header = QString(
                         "========================================\n"
                         "evenergem Telnet Log\n"
                         "Started: %1\n"
                         "Topology: %2\n"
                         "Nodes: %3, Piles: %4\n"
                         "========================================\n\n")
                         .arg(QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss"))
                         .arg(m_topologyType.isEmpty() ? "Unknown" : m_topologyType)
                         .arg(m_nodeCount)
                         .arg(m_pileCount);

    *m_logStream << header;
    m_logStream->flush();

    // ========== 在日志窗口显示文件保存位置和定制信息 ==========
    QTextCursor cursor = m_textEdit->textCursor();
    cursor.movePosition(QTextCursor::End);
    m_textEdit->setTextCursor(cursor);

    // 日志文件位置信息（绿色）
    QString infoMsg = QString("日志文件同步保存至: %1").arg(filePath);
    m_textEdit->insertHtml(QString("<span style=\"color:#6b8e23;\">%1</span>\n").arg(infoMsg));

    // ========== 定制软件信息（蓝色/青色） ==========
    QString customInfo =
        "\n========================================\n"
        "  evenergem v1.0.6 - EV Charger pwralloc kits\n"
        "  Copyright (c) 2026 Infy Power Technology Co., Ltd.\n"
        "  Project: A2605\n"
        "========================================";

    // 将换行符替换为 <br>，并转义 HTML 特殊字符
    QString htmlCustomInfo = customInfo.toHtmlEscaped();
    htmlCustomInfo.replace("\n", "<br>");
    m_textEdit->insertHtml(QString("<span style=\"color:#00ced1; font-family: monospace;\">%1</span>").arg(htmlCustomInfo));
    m_textEdit->insertPlainText("\n");

    cursor.movePosition(QTextCursor::End);
    m_textEdit->setTextCursor(cursor);
}

void LogWindow::writeToFile(const QString &text)
{
    if (!m_fileOpened || !m_logStream)
    {
        return;
    }

    // 写入原始文本（去掉HTML标签，保留纯文本）
    QString plainText = text;
    // 简单去除HTML标签（仅处理常见的span标签）
    plainText.remove(QRegularExpression("<[^>]*>"));
    // 去除多余的空格和换行
    plainText = plainText.trimmed();

    if (!plainText.isEmpty())
    {
        // 添加时间戳
        QString timestamp = QDateTime::currentDateTime().toString("HH:mm:ss");
        *m_logStream << "[" << timestamp << "] " << plainText << "\n";
        m_logStream->flush();
    }
}

void LogWindow::appendLog(const QString &text)
{
    QString html = ansiToHtml(text);

    // 写入文件（写入原始ANSI文本或纯文本）
    writeToFile(text);

    if (m_paused)
    {
        m_pendingLogs.append(html);
    }
    else
    {
        if (!m_pendingLogs.isEmpty())
        {
            displayPendingLogs();
        }
        QTextCursor cursor = m_textEdit->textCursor();
        cursor.movePosition(QTextCursor::End);
        m_textEdit->setTextCursor(cursor);
        m_textEdit->insertHtml(html);
        m_textEdit->insertPlainText("\n");
        cursor.movePosition(QTextCursor::End);
        m_textEdit->setTextCursor(cursor);
    }
}

void LogWindow::displayPendingLogs()
{
    for (const QString &html : m_pendingLogs)
    {
        QTextCursor cursor = m_textEdit->textCursor();
        cursor.movePosition(QTextCursor::End);
        m_textEdit->setTextCursor(cursor);
        m_textEdit->insertHtml(html);
        m_textEdit->insertPlainText("\n");
    }
    m_pendingLogs.clear();
    // 滚动到底部
    QTextCursor cursor = m_textEdit->textCursor();
    cursor.movePosition(QTextCursor::End);
    m_textEdit->setTextCursor(cursor);
}

void LogWindow::togglePause()
{
    m_paused = !m_paused;
    if (m_paused)
    {
        m_pauseBtn->setText("继续");
    }
    else
    {
        m_pauseBtn->setText("暂停");
        displayPendingLogs();
    }
}

void LogWindow::restoreToInitialSize()
{
    if (isMaximized())
        showNormal();
    resize(m_initialSize);
}

void LogWindow::keyPressEvent(QKeyEvent *event)
{
    if (event->modifiers() == Qt::ControlModifier && event->key() == Qt::Key_R)
    {
        restoreToInitialSize();
        event->accept();
    }
    else if (event->modifiers() == Qt::ControlModifier && event->key() == Qt::Key_P)
    {
        togglePause();
        event->accept();
    }
    else
    {
        QDialog::keyPressEvent(event);
    }
}

void LogWindow::resizeEvent(QResizeEvent *event)
{
    QDialog::resizeEvent(event);
    updateButtonPosition();
}

void LogWindow::updateButtonPosition()
{
    if (m_pauseBtn && m_textEdit)
    {
        int x = m_textEdit->width() - m_pauseBtn->width() - 12;
        int y = m_textEdit->height() - m_pauseBtn->height() - 12;
        m_pauseBtn->move(x, y);
    }
}

QString LogWindow::ansiToHtml(const QString &text)
{
    QString result;
    QRegularExpression ansiRegex("\x1b\\[([0-9;]*)m");
    int lastPos = 0;
    int pos = 0;
    QString currentColor = "#d4d4d4";
    bool bold = false;

    auto applyStyle = [&]()
    {
        QString style;
        if (bold)
            style += "font-weight:bold;";
        style += QString("color:%1;").arg(currentColor);
        return style;
    };

    while ((pos = text.indexOf(ansiRegex, lastPos)) != -1)
    {
        QString plain = text.mid(lastPos, pos - lastPos);
        if (!plain.isEmpty())
        {
            result += QString("<span style=\"%1\">%2</span>")
                          .arg(applyStyle())
                          .arg(plain.toHtmlEscaped());
        }

        QString code = ansiRegex.match(text, pos).captured(1);
        QStringList codes = code.split(';');
        for (const QString &c : codes)
        {
            if (c.isEmpty() || c == "0")
            {
                currentColor = "#d4d4d4";
                bold = false;
            }
            else if (c == "1")
                bold = true;
            else if (c == "22")
                bold = false;
            else if (c == "30")
                currentColor = "#000000";
            else if (c == "31")
                currentColor = "#cd5c5c";
            else if (c == "32")
                currentColor = "#6b8e23";
            else if (c == "33")
                currentColor = "#ffd700";
            else if (c == "34")
                currentColor = "#4682b4";
            else if (c == "35")
                currentColor = "#c71585";
            else if (c == "36")
                currentColor = "#00ced1";
            else if (c == "37")
                currentColor = "#f5f5f5";
            else if (c == "90")
                currentColor = "#808080";
            else if (c == "91")
                currentColor = "#ff6347";
            else if (c == "92")
                currentColor = "#7cfc00";
            else if (c == "93")
                currentColor = "#ffd700";
            else if (c == "94")
                currentColor = "#87cefa";
            else if (c == "95")
                currentColor = "#ff69b4";
            else if (c == "96")
                currentColor = "#40e0d0";
            else if (c == "97")
                currentColor = "#ffffff";
        }
        lastPos = pos + ansiRegex.match(text, pos).capturedLength();
    }
    QString remaining = text.mid(lastPos);
    if (!remaining.isEmpty())
    {
        result += QString("<span style=\"%1\">%2</span>")
                      .arg(applyStyle())
                      .arg(remaining.toHtmlEscaped());
    }
    return result;
}
