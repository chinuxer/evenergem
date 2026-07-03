#ifndef TELNETCONSOLE_H
#define TELNETCONSOLE_H

#include <QTcpServer>
#include <QTcpSocket>
#include <QSet>
#include <QMutex>

class TelnetConsole : public QTcpServer
{
    Q_OBJECT
public:
    explicit TelnetConsole(QObject *parent = nullptr);
    ~TelnetConsole();

    // 启动服务，监听 port
    bool start(quint16 port = 24601);

    // 向所有已连接的客户端发送文本
    void sendToAll(const QString &message);

protected:
    void incomingConnection(qintptr socketDescriptor) override;

private slots:
    void onClientReadyRead();
    void onClientDisconnected();
    void doSendToAll(const QString &message);

private:
    QSet<QTcpSocket *> m_clients;
    QMutex m_mutex;
};

#endif // TELNETCONSOLE_H