#ifndef TELNETCLIENT_H
#define TELNETCLIENT_H

#include <QObject>
#include <QTcpSocket>
#include <QTimer>

class TelnetClient : public QObject
{
    Q_OBJECT
public:
    explicit TelnetClient(const QString &host, quint16 port, QObject *parent = nullptr);
    ~TelnetClient();

    void start(); // 开始连接（支持重连）
    void stop();  // 停止重连并断开

signals:
    void connected();                        // 连接成功信号
    void disconnected();                     // 断开连接信号
    void rawLogReceived(const QString &log); // 原始日志（逐行，供窗口显示）
    void topologyStateReceived(int nodeCount, int pileCount,
                               const QVector<int> &nodeOwners,
                               const QVector<bool> &contactorStates,
                               const QMap<int, QPair<int, int>> &chargingPiles); // 桩id -> (requiredPower, priority)

private slots:
    void onConnected();
    void onDisconnected();
    void onReadyRead();
    void onReconnectTimeout();

private:
    void connectToHost();
    void parseMessage(const QString &msg); // 解析 $CYCLUS$ ... $SULCYC$

    QString m_host;
    quint16 m_port;
    QTcpSocket *m_socket;
    QTimer *m_reconnectTimer;
    bool m_connecting;
    QByteArray m_buffer; // 粘包处理
};

#endif // TELNETCLIENT_H