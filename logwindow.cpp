#include "logwindow.h"
#include <QVBoxLayout>
#include <QTextCursor>
#include <QRegularExpression>
#include <QColor>
#include <QResizeEvent>

LogWindow::LogWindow(QWidget *parent)
    : QDialog(parent), m_initialSize(800, 400), m_paused(false)
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
}

void LogWindow::appendLog(const QString &text)
{
    QString html = ansiToHtml(text);
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
    // 原 ansiToHtml 实现保持不变（此处省略，请复制原函数体）
    // 为防止意外，以下粘贴原完整函数（与用户提供的完全一致）
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