#include "telnetclient.h"
#include <QDebug>
#include <QThread>
#include <QMetaMethod>

TelnetClient::TelnetClient(const QString &host, quint16 port, QObject *parent)
    : QObject(parent), m_host(host), m_port(port), m_socket(nullptr), m_reconnectTimer(nullptr), m_connecting(false)
{
}

TelnetClient::~TelnetClient()
{
    stop();
}

void TelnetClient::start()
{
    if (m_socket)
        return;
    m_socket = new QTcpSocket(this);
    connect(m_socket, &QTcpSocket::connected, this, &TelnetClient::onConnected);
    connect(m_socket, &QTcpSocket::disconnected, this, &TelnetClient::onDisconnected);
    connect(m_socket, &QTcpSocket::readyRead, this, &TelnetClient::onReadyRead);
    connectToHost();
}

void TelnetClient::stop()
{
    if (m_reconnectTimer)
    {
        m_reconnectTimer->stop();
        delete m_reconnectTimer;
        m_reconnectTimer = nullptr;
    }
    if (m_socket)
    {
        m_socket->disconnectFromHost();
        m_socket->deleteLater();
        m_socket = nullptr;
    }
    m_connecting = false;
}

void TelnetClient::connectToHost()
{
    if (m_connecting)
        return;
    m_connecting = true;
    m_socket->connectToHost(m_host, m_port);
}

void TelnetClient::onConnected()
{
    m_connecting = false;
    if (m_reconnectTimer)
    {
        m_reconnectTimer->stop();
        delete m_reconnectTimer;
        m_reconnectTimer = nullptr;
    }
    qDebug() << "TelnetClient: connected to" << m_host << m_port;
    emit connected();
}

void TelnetClient::onDisconnected()
{
    qDebug() << "TelnetClient: disconnected, will reconnect after 5s";
    emit disconnected();
    if (m_socket)
        m_socket->deleteLater();
    m_socket = nullptr;
    m_connecting = false;

    // 启动重连定时器
    if (!m_reconnectTimer)
    {
        m_reconnectTimer = new QTimer(this);
        connect(m_reconnectTimer, &QTimer::timeout, this, &TelnetClient::onReconnectTimeout);
    }
    m_reconnectTimer->start(5000);
}

void TelnetClient::onReadyRead()
{
    m_buffer.append(m_socket->readAll());

    // 按行处理（假设每条消息以换行结束，特殊消息可能跨行，但命令格式紧凑）
    int pos;
    while ((pos = m_buffer.indexOf('\n')) != -1)
    {
        QByteArray line = m_buffer.left(pos).trimmed();
        m_buffer.remove(0, pos + 1);
        if (!line.isEmpty())
        {
            QString lineStr = QString::fromUtf8(line);
            emit rawLogReceived(lineStr);
            parseMessage(lineStr);
        }
    }
}

void TelnetClient::onReconnectTimeout()
{
    if (m_reconnectTimer)
        m_reconnectTimer->stop();
    if (!m_socket)
    {
        m_socket = new QTcpSocket(this);
        connect(m_socket, &QTcpSocket::connected, this, &TelnetClient::onConnected);
        connect(m_socket, &QTcpSocket::disconnected, this, &TelnetClient::onDisconnected);
        connect(m_socket, &QTcpSocket::readyRead, this, &TelnetClient::onReadyRead);
    }
    connectToHost();
}

// 辅助函数：十六进制字符串转十进制字符串（大整数）
static QString hexToDecimalString(const QString &hex)
{
    QByteArray bytes = QByteArray::fromHex(hex.toLatin1());
    if (bytes.isEmpty())
        return QString();
    // 将字节数组视为大端无符号整数，转换为十进制字符串
    // 简单实现：逐字节乘以256累加，用QString模拟手工除法
    QString decimal = "0";
    for (int i = 0; i < bytes.size(); ++i)
    {
        unsigned char byte = static_cast<unsigned char>(bytes[i]);
        // decimal = decimal * 256 + byte
        QString newDecimal;
        int carry = byte;
        for (int j = decimal.size() - 1; j >= 0 || carry; --j)
        {
            int digit = (j >= 0 ? decimal[j].digitValue() : 0) * 256 + carry;
            newDecimal.prepend(QChar('0' + digit % 10));
            carry = digit / 10;
        }
        decimal = newDecimal;
    }
    return decimal;
}

// 将十进制字符串按两位分割，转换为整数列表
static QVector<int> splitDecimalToInts(const QString &decimalStr, int expectedCount)
{
    QVector<int> result;
    int len = decimalStr.length();
    if (len % 2 != 0)
    {
        qWarning() << "Invalid length for node owners decimal string" << decimalStr;
        return result;
    }
    for (int i = 0; i < len; i += 2)
    {
        bool ok;
        int val = decimalStr.mid(i, 2).toInt(&ok);
        if (!ok)
            val = 0;
        result.append(val);
    }
    if (result.size() != expectedCount)
    {
        qWarning() << "Node owners count mismatch, expected" << expectedCount << "got" << result.size();
    }
    return result;
}

void TelnetClient::parseMessage(const QString &msg)
{
    // 寻找 $CYCLUS$ ... $SULCYC$
    int start = msg.indexOf("$CYCLUS$");
    if (start == -1)
        return;
    int contentStart = start + strlen("$CYCLUS$");

    int end = msg.indexOf("$SULCYC$", contentStart);
    if (end == -1)
        return;
   QString content = msg.mid(contentStart, end - contentStart).trimmed();

    // 格式: <节点数><桩数>(节点所属充电桩十六进制){接触器状态十六进制}[桩信息1][桩信息2]...
    // 示例: <8><8>(5CC985FBE5E1){4D00}[F4BA1][1E8AC1][5893C2]
    QRegExp rx("<(\\d+)><(\\d+)>\\(([0-9A-Fa-f]+)\\)\\{([0-9A-Fa-f]+)\\}(.*)");
    //  QRegExp rx("^<(\\d+)><(\\d+)>\\(([0-9]+)\\)\\{([0-9A-Fa-f]+)\\}(.*)$");
    if (!rx.exactMatch(content))
    {
        qWarning() << "Invalid CYCLUS format:" << content;
        return;
    }
    int nodeCount = rx.cap(1).toInt();
    int pileCount = rx.cap(2).toInt();
    QString nodeOwnersDec = rx.cap(3);
    QString contactorHex = rx.cap(4);
    QString rest = rx.cap(5); // 剩余部分包含多个 [hex]

    // 1. 节点所属充电桩解析

    QVector<int> nodeOwners = splitDecimalToInts(nodeOwnersDec, nodeCount);
    if (nodeOwners.size() != nodeCount)
    {
        qWarning() << "Node owners parse failed, size" << nodeOwners.size();
        return;
    }

    // 2. 接触器状态解析
    QByteArray contactorBytes = QByteArray::fromHex(contactorHex.toLatin1());
    int totalContactors = 2 * nodeCount; // 环形 + 对角
    QVector<bool> contactorStates(totalContactors, false);
    for (int i = 0; i < contactorBytes.size() * 8 && i < totalContactors; ++i)
    {
        int byteIdx = i / 8;
        int bitIdx = 7 - (i % 8); // 高位在前（根据示例推测）
        if (byteIdx < contactorBytes.size())
        {
            contactorStates[i] = (contactorBytes[byteIdx] >> bitIdx) & 1;
        }
    }

    // 3. 充电桩充电信息解析
    QMap<int, QPair<int, int>> chargingPiles; // id -> (requiredPower, priority)
    QRegExp rxPile("\\[([0-9A-Fa-f]+)\\]");
    int pos = 0;
    while ((pos = rxPile.indexIn(rest, pos)) != -1)
    {
        QString hexData = rxPile.cap(1);
        QString decimalStr = hexToDecimalString(hexData);
        // 解析十进制串: 桩id(可变长度) + 5位功率 + 1位优先级
        if (decimalStr.length() < 7)
        {
            qWarning() << "Invalid pile info hex->dec:" << hexData << "->" << decimalStr;
            pos += rxPile.matchedLength();
            continue;
        }
        // 从左边扫描id直到遇到数字? 实际上全部是数字，id长度可能是1或2
        int idLen = 1;
        if (decimalStr.length() > 7 && decimalStr.mid(0, 2).toInt() <= pileCount)
            idLen = 2;
        int pileId = decimalStr.left(idLen).toInt();
        QString powerStr = decimalStr.mid(idLen, 5);
        int requiredPower = powerStr.toInt();
        int priority = decimalStr.mid(idLen + 5, 1).toInt();
        chargingPiles[pileId] = qMakePair(requiredPower, priority);
        pos += rxPile.matchedLength();
    }

    emit topologyStateReceived(nodeCount, pileCount, nodeOwners, contactorStates, chargingPiles);
}
