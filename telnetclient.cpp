#include "telnetclient.h"
#include "pwralloc/pau_broker.h"
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
    int start = msg.indexOf(EN_ARCHE_ALPHA);
    if (start == -1)
        return;
    int contentStart = start + strlen(EN_ARCHE_ALPHA);

    int end = msg.indexOf(EPI_TELEI_OMEGA, contentStart);
    if (end == -1)
        return;
    QString content = msg.mid(contentStart, end - contentStart).trimmed();

    // 格式: <节点数><桩数><拓扑类型>(节点所属充电桩十六进制){接触器状态十六进制}@不可用节点@[桩信息1][桩信息2]...
    // 示例: <8><8><1>(5CC985FBE5E1){4D00}@040612@[F4BA1][1E8AC1][5893C2]

    // 使用一个正则表达式解析所有内容
    // 分组: 1:节点数, 2:桩数, 3:拓扑类型, 4:节点所属充电桩, 5:接触器状态, 6:不可用节点, 7:桩信息列表
    QRegExp rx("<(\\d+)><(\\d+)><(\\d+)>\\(([0-9A-Fa-f]+)\\)\\{([0-9A-Fa-f]+)\\}(@[0-9]*@)?(.*)");

    if (!rx.exactMatch(content))
    {
        qWarning() << "Invalid CYCLUS format:" << content;
        return;
    }

    int nodeCount = rx.cap(1).toInt();
    int pileCount = rx.cap(2).toInt();
    int topologyType = rx.cap(3).toInt();
    QString nodeOwnersDec = rx.cap(4);
    QString contactorHex = rx.cap(5);
    QString disabledNodesStr = rx.cap(6); // 包含 @ 符号，如 @040612@ 或 @@
    QString rest = rx.cap(7);             // 桩信息列表

    // 将拓扑类型转换为字符串
    QString topologyStr;
    switch (topologyType)
    {
    case 0:
        topologyStr = "FullMatrix";
        break;
    case 1:
        topologyStr = "CakraWheel";
        break;
    case 2:
        topologyStr = "SemiHybrid";
        break;
    default:
        topologyStr = "Unknown";
        break;
    }

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
    if (SemiHybrid == topologyType)
    {
        size_t factorial(ID_TYPE n);
        totalContactors = nodeCount * 5 / 3 + factorial(nodeCount / 3);
    }
    QVector<bool> contactorStates(totalContactors, false);
    // 报文最低位对应最后一个接触器：从右侧字节的 bit0 开始，
    // 依次向左填充 contactorStates 的尾部。
    int contactorIndex = totalContactors - 1;
    for (int byteIdx = contactorBytes.size() - 1;
         byteIdx >= 0 && contactorIndex >= 0;
         --byteIdx)
    {
        const uchar byte = static_cast<uchar>(contactorBytes.at(byteIdx));
        for (int bitIdx = 0; bitIdx < 8 && contactorIndex >= 0; ++bitIdx)
        {
            contactorStates[contactorIndex--] = (byte >> bitIdx) & 1;
        }
    }

    // 3. 解析不可用节点信息
    QVector<int> disabledNodes;
    // 去掉 @ 符号，提取中间的节点编号
    if (!disabledNodesStr.isEmpty() && disabledNodesStr != "@@")
    {
        // 去掉首尾的 @ 符号
        QString nodesStr = disabledNodesStr;
        nodesStr.remove(0, 1); // 去掉开头的 @
        if (nodesStr.endsWith("@"))
        {
            nodesStr.chop(1); // 去掉结尾的 @
        }

        // 每两位解析一个节点ID
        int len = nodesStr.length();
        if (len % 2 != 0)
        {
            qWarning() << "Invalid disabled nodes string length:" << nodesStr;
        }
        else
        {
            for (int i = 0; i < len; i += 2)
            {
                bool ok;
                int nodeId = nodesStr.mid(i, 2).toInt(&ok);
                if (ok && nodeId > 0 && nodeId <= nodeCount)
                {
                    disabledNodes.append(nodeId);
                }
            }
        }
    }

    // 4. 充电桩充电信息解析
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

    // 5. 发出信号，包含所有解析出的信息
    emit topologyStateReceived(nodeCount, pileCount, topologyStr,
                               nodeOwners, contactorStates, chargingPiles,
                               disabledNodes);
}
