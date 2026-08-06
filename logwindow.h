#ifndef LOGWINDOW_H
#define LOGWINDOW_H

#include <QDialog>
#include <QTextEdit>
#include <QSize>
#include <QKeyEvent>
#include <QPushButton>
#include <QFile>
#include <QTextStream>
#include <QDateTime>

class LogWindow : public QDialog
{
    Q_OBJECT
public:
    explicit LogWindow(QWidget *parent = nullptr);
    ~LogWindow();

    // 设置拓扑信息用于文件名
    void setTopologyInfo(const QString &topologyType, int nodeCount, int pileCount);
    void appendLog(const QString &text);

protected:
    void keyPressEvent(QKeyEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private slots:
    void restoreToInitialSize();
    void togglePause();

private:
    QString ansiToHtml(const QString &text);
    void displayPendingLogs();
    void updateButtonPosition();
    void openLogFile();                    // 打开日志文件
    void writeToFile(const QString &text); // 写入文件

    QTextEdit *m_textEdit;
    QPushButton *m_pauseBtn;
    QSize m_initialSize;
    bool m_paused;
    QStringList m_pendingLogs;

    // 日志文件相关
    QFile *m_logFile;
    QTextStream *m_logStream;
    QString m_topologyType;
    int m_nodeCount;
    int m_pileCount;
    bool m_fileOpened;
};

#endif // LOGWINDOW_H