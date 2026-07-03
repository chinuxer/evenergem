#ifndef LOGWINDOW_H
#define LOGWINDOW_H

#include <QDialog>
#include <QTextEdit>
#include <QSize>
#include <QKeyEvent>
#include <QPushButton>

class LogWindow : public QDialog
{
    Q_OBJECT
public:
    explicit LogWindow(QWidget *parent = nullptr);
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

    QTextEdit *m_textEdit;
    QPushButton *m_pauseBtn;
    QSize m_initialSize;
    bool m_paused;
    QStringList m_pendingLogs;
};

#endif // LOGWINDOW_H