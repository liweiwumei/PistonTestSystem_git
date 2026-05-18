#ifndef PLCCONTROLLER_H
#define PLCCONTROLLER_H

#include <QObject>
#include <QTcpSocket>
#include <QTimer>
#include <QQueue>
#include <functional>

class PlcController : public QObject
{
    Q_OBJECT
public:
    explicit PlcController(QObject *parent = nullptr);
    ~PlcController();

    // 连接PLC
    void connectPlc(const QString &ip, quint16 port = 8080);
    void disconnectPlc();

    // 外部接口
    bool isConnected() const;
    bool isHeartbeatOk() const;

    // 指令（和你原来 mainwindow_plc 完全一样）
    void writeM(int addr, bool value, std::function<void(bool ok)> cb = nullptr);
    void writeD(int addr, qint32 value, std::function<void(bool ok)> cb = nullptr);
    void readM(int addr, std::function<void(bool ok, bool val)> cb);
    void readD(int addr, std::function<void(bool ok, quint32 uVal, qint32 sVal)> cb);

signals:
    void sigConnected();
    void sigDisconnected();
    void sigHeartbeatChanged(bool ok);

private slots:
    void onSocketConnected();
    void onSocketDisconnected();
    void onSocketError(QAbstractSocket::SocketError err);
    void onReadyRead();
    void onHeartbeatCheck();

private:
    struct CmdItem {
        QByteArray frame;
        std::function<void(const QByteArray&)> callback;
    };

    void enqueueFrame(const QByteArray &frame, std::function<void(const QByteArray&)> cb);
    void sendNextCmd();
    bool parseWriteResponse(const QByteArray &resp);
    bool parseReadMResponse(const QByteArray &resp, bool &outVal);
    bool parseReadDResponse(const QByteArray &resp, quint32 &uVal, qint32 &sVal);

private:
    QTcpSocket *m_socket;
    QTimer *m_heartbeatTimer;
    QQueue<CmdItem> m_queue;
    bool m_isBusy;
    bool m_isConnected;
    bool m_heartbeatOk;
    QString m_plcIp;
    quint16 m_plcPort;
};

#endif // PLCCONTROLLER_H
