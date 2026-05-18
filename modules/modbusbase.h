#ifndef MODBUSBASE_H
#define MODBUSBASE_H

#include <QObject>
#include <QModbusRtuSerialMaster>
#include <QSerialPort>
#include <QTimer>
#include <QByteArray>
#include <QVariant>

// Modbus设备状态
enum DeviceStatus {
    Status_UnConnected,  // 未连接
    Status_Connected,    // 已连接
    Status_Online,       // 在线（心跳正常）
    Status_Offline,      // 离线（心跳超时）
    Status_Fault
};

class ModbusBase : public QObject
{
    Q_OBJECT
public:
    explicit ModbusBase(QObject *parent = nullptr);
    // 新增：获取从站地址的接口（推荐用方法封装，而非直接暴露变量）
        quint8 slaveAddress() const { return m_slaveAddr; }
        // 新增：设置从站地址的接口
        void setSlaveAddress(quint8 addr) { m_slaveAddr = addr; }
    virtual ~ModbusBase();

    // 初始化串口参数
    void setSerialParam(const QString &portName, qint32 baudRate = 9600,
                        QSerialPort::Parity parity = QSerialPort::NoParity,
                        QSerialPort::DataBits dataBits = QSerialPort::Data8,
                        QSerialPort::StopBits stopBits = QSerialPort::OneStop);
    // 连接/断开Modbus设备
    bool connectDevice();
    void disconnectDevice();
    // 获取当前设备状态
    DeviceStatus getDeviceStatus() const { return m_deviceStatus; }
    // 设置设备Modbus地址
    void setSlaveAddress(int addr) { m_slaveAddr = addr; }
    int getSlaveAddress() const { return m_slaveAddr; }

protected:
    QModbusRtuSerialMaster *m_modbusMaster;  // Modbus RTU主站
    QSerialPort *m_serialPort;               // 串口
    QString m_portName;                      // 串口号（如COM3）
    qint32 m_baudRate;                       // 波特率
    QSerialPort::Parity m_parity;            // 校验位
    QSerialPort::DataBits m_dataBits;        // 数据位
    QSerialPort::StopBits m_stopBits;        // 停止位
    int m_slaveAddr;                         // 从站地址（1-255）
    DeviceStatus m_deviceStatus;             // 设备状态
    QTimer *m_heartTimer;                    // 心跳定时器（1s）
    const int HEART_INTERVAL = 1000;         // 心跳间隔1s

    // 通用Modbus发送请求（封装功能码03/04/06/10）
    QModbusReply* sendReadRequest(int startReg, int regNum, QModbusDataUnit::RegisterType regType);
    QModbusReply* sendWriteRequest(int startReg, const QVector<quint16> &data, QModbusDataUnit::RegisterType regType);

    // 纯虚接口：子类实现具体的心跳包逻辑（必须实现）
    virtual void sendHeartBeat() = 0;
    // 纯虚接口：子类实现具体的设备协议逻辑（如读AI、写AO）
    virtual void readAI(int ch) = 0;
    virtual void writeAO(int ch, quint16 value) = 0;

private slots:
    // 串口错误处理
    void onSerialError(QSerialPort::SerialPortError error);
    // Modbus错误处理
    void onModbusError(QModbusDevice::Error error);
    // 心跳定时器超时
    virtual void onHeartTimerTimeout() = 0;

protected slots:
    // 通用Modbus回复处理
    //virtual void onModbusReplyFinished(QModbusReply *reply);
      virtual void onModbusReplyFinished();  // 无参数
    // 更新设备状态
    void updateDeviceStatus(DeviceStatus status);

signals:
    // 设备状态变化信号（跨线程给主线程）
    void sigDeviceStatusChanged(int slaveAddr, DeviceStatus status);
    // 数据接收信号（跨线程给主线程，如AI采集值、AO设置值）
    void sigDataReceived(int slaveAddr, int ch, QString dataType, QVariant value);
    // 错误信息信号
    void sigError(int slaveAddr, QString errorInfo);
};

#endif // MODBUSBASE_H
