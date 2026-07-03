#include "telnetconsole.h"
#include <QDebug>

TelnetConsole::TelnetConsole(QObject *parent)
    : QTcpServer(parent)
{
}

TelnetConsole::~TelnetConsole()
{
    // 断开所有客户端
    for (QTcpSocket *client : m_clients)
    {
        client->disconnectFromHost();
        delete client;
    }
    m_clients.clear();
}

bool TelnetConsole::start(quint16 port)
{
    if (!listen(QHostAddress::Any, port))
    {
        qWarning() << "TelnetConsole: Failed to listen on port" << port;
        return false;
    }
    qDebug() << "TelnetConsole: Listening on port" << port;
    return true;
}

void TelnetConsole::incomingConnection(qintptr socketDescriptor)
{
    QTcpSocket *socket = new QTcpSocket(this);
    if (!socket->setSocketDescriptor(socketDescriptor))
    {
        delete socket;
        return;
    }

    connect(socket, &QTcpSocket::readyRead, this, &TelnetConsole::onClientReadyRead);
    connect(socket, &QTcpSocket::disconnected, this, &TelnetConsole::onClientDisconnected);

    QMutexLocker locker(&m_mutex);
    m_clients.insert(socket);

    // 发送欢迎消息
    socket->write("\r\n=== RingTopologyPower Telnet Console ===\r\n");
    socket->write("Connected. Debug output will be shown here.\r\n\r\n");
    socket->flush();

    qDebug() << "TelnetConsole: New client connected from" << socket->peerAddress().toString();
}

void TelnetConsole::onClientReadyRead()
{
    QTcpSocket *socket = qobject_cast<QTcpSocket *>(sender());
    if (!socket)
        return;

    // 可选：接收客户端输入，目前只做单向输出，可以忽略输入
    socket->readAll(); // 丢弃输入
}

void TelnetConsole::onClientDisconnected()
{
    QTcpSocket *socket = qobject_cast<QTcpSocket *>(sender());
    if (!socket)
        return;

    QMutexLocker locker(&m_mutex);
    m_clients.remove(socket);
    socket->deleteLater();

    qDebug() << "TelnetConsole: Client disconnected";
}

void TelnetConsole::sendToAll(const QString &message)
{
    QMutexLocker locker(&m_mutex);
    for (QTcpSocket *client : m_clients)
    {
        if (client->state() == QAbstractSocket::ConnectedState)
        {
            client->write(message.toUtf8());
            client->write("\r\n");
            // client->flush(); // 不调用 flush()，避免阻塞主线程
        }
    }
}

void TelnetConsole::doSendToAll(const QString &message)
{
    sendToAll(message);
}