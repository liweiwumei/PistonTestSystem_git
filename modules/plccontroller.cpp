#include "plccontroller.h"
#include <QDebug>
#include <QByteArray>

PlcController::PlcController(QObject *parent)
    : QObject(parent)
    , m_isBusy(false)
    , m_isConnected(false)
    , m_heartbeatOk(false)
{
    m_socket = new QTcpSocket(this);
    m_heartbeatTimer = new QTimer(this);
    m_heartbeatTimer->setInterval(1000);

    connect(m_socket, &QTcpSocket::connected, this, &PlcController::onSocketConnected);
    connect(m_socket, &QTcpSocket::disconnected, this, &PlcController::onSocketDisconnected);
    connect(m_socket, SIGNAL(error(QAbstractSocket::SocketError)),
               this, SLOT(onSocketError(QAbstractSocket::SocketError)));
    connect(m_socket, &QTcpSocket::readyRead, this, &PlcController::onReadyRead);
    connect(m_heartbeatTimer, &QTimer::timeout, this, &PlcController::onHeartbeatCheck);
}

PlcController::~PlcController()
{
    disconnectPlc();
}

void PlcController::connectPlc(const QString &ip, quint16 port)
{
    m_plcIp = ip;
    m_plcPort = port;
    m_socket->connectToHost(ip, port);
}

void PlcController::disconnectPlc()
{
    m_heartbeatTimer->stop();
    m_socket->close();
}

bool PlcController::isConnected() const
{
    return m_isConnected;
}

bool PlcController::isHeartbeatOk() const
{
    return m_heartbeatOk;
}

// ==============================
// 队列发送
// ==============================
void PlcController::enqueueFrame(const QByteArray &frame, std::function<void(const QByteArray&)> cb)
{
    m_queue.enqueue({frame, cb});
    sendNextCmd();
}

void PlcController::sendNextCmd()
{
    if (m_isBusy || m_queue.isEmpty()) return;
    auto item = m_queue.head();
    m_isBusy = true;
    m_socket->write(item.frame);
    m_socket->flush();
}

// ==============================
// 解析
// ==============================
bool PlcController::parseWriteResponse(const QByteArray &resp)
{
    return resp.toHex() == "d00000ffff030002000000";
}

bool PlcController::parseReadMResponse(const QByteArray &resp, bool &outVal)
{
    if (resp.size() < 11) return false;
    outVal = ((uchar)resp.constEnd()[-1] == 0x10);
    return true;
}

bool PlcController::parseReadDResponse(const QByteArray &resp, quint32 &uVal, qint32 &sVal)
{
    if (resp.size() < 16) return false;
    const uchar *d = (const uchar*)resp.constData() + 15;
    uVal = (quint32)d[3] << 24 | (quint32)d[2] << 16 | (quint32)d[1] << 8 | d[0];
    sVal = static_cast<qint32>(uVal);
    return true;
}

// ==============================
// 读写接口
// ==============================
void PlcController::writeM(int addr, bool value, std::function<void(bool ok)> cb)
{
    QByteArray cmd = QByteArray::fromHex("500000FFFF03000D0010000114010001000090010010");
    cmd[15] = addr & 0xFF;
    cmd[16] = (addr >> 8) & 0xFF;
    cmd[17] = (addr >> 16) & 0xFF;
    cmd[21] = value ? 0x10 : 0x00;
    enqueueFrame(cmd, [this, cb](const QByteArray &resp){
        bool ok = parseWriteResponse(resp);
        if (cb) cb(ok);
    });
}

void PlcController::writeD(int addr, qint32 value, std::function<void(bool ok)> cb)
{
    QByteArray cmd = QByteArray::fromHex("500000FFFF03001000100001140000000000A80200F1D8FFFF");
    cmd[15] = addr & 0xFF;
    cmd[16] = (addr >> 8) & 0xFF;
    cmd[17] = (addr >> 16) & 0xFF;

    quint32 uVal = *(quint32*)&value;
    quint16 lo = uVal % 65536;
    quint16 hi = uVal / 65536;

    cmd[21] = lo & 0xFF;
    cmd[22] = (lo >> 8) & 0xFF;
    cmd[23] = hi & 0xFF;
    cmd[24] = (hi >> 8) & 0xFF;

    enqueueFrame(cmd, [this, cb](const QByteArray &resp){
        bool ok = parseWriteResponse(resp);
        if (cb) cb(ok);
    });
}

void PlcController::readM(int addr, std::function<void(bool ok, bool val)> cb)
{
    QByteArray cmd = QByteArray::fromHex("500000FFFF03000C00100001040100330000900100");
    cmd[15] = addr & 0xFF;
    cmd[16] = (addr >> 8) & 0xFF;
    cmd[17] = (addr >> 16) & 0xFF;
    enqueueFrame(cmd, [this, cb](const QByteArray &resp){
        bool val = false;
        bool ok = parseReadMResponse(resp, val);
        cb(ok, val);
    });
}

void PlcController::readD(int addr, std::function<void(bool ok, quint32 uVal, qint32 sVal)> cb)
{
    QByteArray cmd = QByteArray::fromHex("54003412000000FFFF03000C00100001040000640000A80300");
    cmd[19] = addr & 0xFF;
    cmd[20] = (addr >> 8) & 0xFF;
    cmd[21] = (addr >> 16) & 0xFF;
    enqueueFrame(cmd, [this, cb](const QByteArray &resp){
        quint32 uVal = 0;
        qint32 sVal = 0;
        bool ok = parseReadDResponse(resp, uVal, sVal);
        cb(ok, uVal, sVal);
    });
}

// ==============================
// 套接字 & 心跳
// ==============================
void PlcController::onSocketConnected()
{
    m_isConnected = true;
    m_heartbeatOk = true;
    m_heartbeatTimer->start();
    emit sigConnected();
}

void PlcController::onSocketDisconnected()
{
    m_isConnected = false;
    m_heartbeatOk = false;
    m_heartbeatTimer->stop();
    emit sigDisconnected();
}

void PlcController::onSocketError(QAbstractSocket::SocketError err)
{
    Q_UNUSED(err);
    qDebug() << "PLC error:" << m_socket->errorString();
}

void PlcController::onReadyRead()
{
    QByteArray resp = m_socket->readAll();
    if (!m_queue.isEmpty()) {
        auto item = m_queue.dequeue();
        if (item.callback) item.callback(resp);
    }
    m_isBusy = false;
    sendNextCmd();
}

void PlcController::onHeartbeatCheck()
{
    if (!m_isConnected) {
        m_heartbeatOk = false;
        emit sigHeartbeatChanged(false);
        return;
    }
    readM(0, [this](bool ok, bool){
        m_heartbeatOk = ok;
        emit sigHeartbeatChanged(ok);
    });
}
