#include "modbusbase.h"
#include <QDebug>

ModbusBase::ModbusBase(QObject *parent) : QObject(parent),
    m_modbusMaster(nullptr),
    m_serialPort(nullptr),
    m_baudRate(9600),
    m_parity(QSerialPort::NoParity),
    m_dataBits(QSerialPort::Data8),
    m_stopBits(QSerialPort::OneStop),
    m_slaveAddr(1),
    m_deviceStatus(Status_UnConnected)
{
    // 初始化Modbus RTU主站
    m_modbusMaster = new QModbusRtuSerialMaster(this);
    // 初始化心跳定时器
    m_heartTimer = new QTimer(this);
    m_heartTimer->setInterval(HEART_INTERVAL);
    m_heartTimer->setSingleShot(false);

    // 连接Modbus错误信号
    connect(m_modbusMaster, &QModbusRtuSerialMaster::errorOccurred,
            this, &ModbusBase::onModbusError);
}

ModbusBase::~ModbusBase()
{
    disconnectDevice();
    if (m_modbusMaster) {
        m_modbusMaster->deleteLater();
    }
    if (m_heartTimer) {
        m_heartTimer->stop();
        m_heartTimer->deleteLater();
    }
}

// 设置串口参数
void ModbusBase::setSerialParam(const QString &portName, qint32 baudRate,
                                QSerialPort::Parity parity, QSerialPort::DataBits dataBits,
                                QSerialPort::StopBits stopBits)
{
    m_portName = portName;
    m_baudRate = baudRate;
    m_parity = parity;
    m_dataBits = dataBits;
    m_stopBits = stopBits;
}

// 连接设备
bool ModbusBase::connectDevice()
{
    if (m_modbusMaster->state() == QModbusDevice::ConnectedState) {
        updateDeviceStatus(Status_Connected);
        return true;
    }
    // 配置Modbus RTU串口参数
    m_modbusMaster->setConnectionParameter(QModbusDevice::SerialPortNameParameter, m_portName);
    m_modbusMaster->setConnectionParameter(QModbusDevice::SerialBaudRateParameter, m_baudRate);
    m_modbusMaster->setConnectionParameter(QModbusDevice::SerialParityParameter, m_parity);
    m_modbusMaster->setConnectionParameter(QModbusDevice::SerialDataBitsParameter, m_dataBits);
    m_modbusMaster->setConnectionParameter(QModbusDevice::SerialStopBitsParameter, m_stopBits);
    m_modbusMaster->setTimeout(1000);  // 超时1s
    m_modbusMaster->setNumberOfRetries(1);  // 重试1次

    // 打开Modbus连接
    bool isOk = m_modbusMaster->connectDevice();
    if (isOk) {
        updateDeviceStatus(Status_Connected);
        m_heartTimer->start();  // 启动心跳定时器
        qDebug() << "ModbusBase: 设备" << m_slaveAddr << "连接成功，串口号：" << m_portName;
    } else {
        updateDeviceStatus(Status_UnConnected);
        emit sigError(m_slaveAddr, "连接失败：" + m_modbusMaster->errorString());
        qDebug() << "ModbusBase: 设备" << m_slaveAddr << "连接失败：" << m_modbusMaster->errorString();
    }
    return isOk;
}

// 断开设备
void ModbusBase::disconnectDevice()
{
    if (m_modbusMaster->state() == QModbusDevice::ConnectedState) {
        m_modbusMaster->disconnectDevice();
    }
    m_heartTimer->stop();
    updateDeviceStatus(Status_UnConnected);
    qDebug() << "ModbusBase: 设备" << m_slaveAddr << "断开连接";
}

// 通用读请求（功能码03/04，对应保持寄存器/输入寄存器）
QModbusReply* ModbusBase::sendReadRequest(int startReg, int regNum, QModbusDataUnit::RegisterType regType)
{
    if (m_modbusMaster->state() != QModbusDevice::ConnectedState) {
        emit sigError(m_slaveAddr, "读请求失败：设备未连接");
        return nullptr;
    }
    QModbusDataUnit readUnit(regType, startReg, regNum);
    QModbusReply *reply = m_modbusMaster->sendReadRequest(readUnit, m_slaveAddr);
    if (!reply) {
        emit sigError(m_slaveAddr, "读请求发送失败：" + m_modbusMaster->errorString());
        return nullptr;
    }
    // 连接无参数的 finished() 信号到无参数的槽函数
    connect(reply, &QModbusReply::finished, this, &ModbusBase::onModbusReplyFinished);
    // 修正：使用默认连接（或 Qt::UniqueConnection）
    connect(reply, &QModbusReply::finished, reply, &QModbusReply::deleteLater);
    // 若需防止重复连接，改用：
    // connect(reply, &QModbusReply::finished, reply, &QModbusReply::deleteLater, Qt::UniqueConnection);
    return reply;
}

QModbusReply* ModbusBase::sendWriteRequest(int startReg, const QVector<quint16> &data, QModbusDataUnit::RegisterType regType)
{
    if (m_modbusMaster->state() != QModbusDevice::ConnectedState) {
        emit sigError(m_slaveAddr, "写请求失败：设备未连接");
        return nullptr;
    }
    QModbusDataUnit writeUnit(regType, startReg, data);
    QModbusReply *reply = m_modbusMaster->sendWriteRequest(writeUnit, m_slaveAddr);
    if (!reply) {
        emit sigError(m_slaveAddr, "写请求发送失败：" + m_modbusMaster->errorString());
        return nullptr;
    }
    connect(reply, &QModbusReply::finished, this, &ModbusBase::onModbusReplyFinished);
    // 同样修正这里的连接类型
    connect(reply, &QModbusReply::finished, reply, &QModbusReply::deleteLater);
    return reply;
}

// 串口错误处理
void ModbusBase::onSerialError(QSerialPort::SerialPortError error)
{
    if (error != QSerialPort::NoError) {
        updateDeviceStatus(Status_UnConnected);
        emit sigError(m_slaveAddr, "串口错误：" + m_serialPort->errorString());
        qDebug() << "ModbusBase: 设备" << m_slaveAddr << "串口错误：" << m_serialPort->errorString();
    }
}

// Modbus错误处理
void ModbusBase::onModbusError(QModbusDevice::Error error)
{
    if (error != QModbusDevice::NoError) {
        updateDeviceStatus(Status_Offline);
        emit sigError(m_slaveAddr, "Modbus错误：" + m_modbusMaster->errorString());
        qDebug() << "ModbusBase: 设备" << m_slaveAddr << "Modbus错误：" << m_modbusMaster->errorString();
    }
}

// modbusbase.cpp 中修改 onModbusReplyFinished 的实现（原第147行）
// modbusbase.cpp
void ModbusBase::onModbusReplyFinished()  // 无参数
{
    // 关键：通过 sender() 获取当前触发信号的 QModbusReply 对象
    QModbusReply *reply = qobject_cast<QModbusReply*>(sender());
    if (!reply) return;  // 空指针防护

    if (reply->error() != QModbusDevice::NoError) {
        updateDeviceStatus(Status_Offline);
        emit sigError(m_slaveAddr, "回复错误：" + reply->errorString());
        reply->deleteLater();
        return;
    }
    updateDeviceStatus(Status_Online);
    reply->deleteLater();
}

// 更新设备状态
void ModbusBase::updateDeviceStatus(DeviceStatus status)
{
    if (m_deviceStatus != status) {
        m_deviceStatus = status;
        emit sigDeviceStatusChanged(m_slaveAddr, status);
    }
}
