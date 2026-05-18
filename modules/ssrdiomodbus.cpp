#include "ssrdiomodbus.h"
#include <QDebug>
#include <QModbusDataUnit>

SSRDioModbus::SSRDioModbus(QObject *parent) : ModbusBase(parent)
{
    // 初始化DO通道控制模式为普通模式
    for (int i = 0; i < SSR_MAX_DI_DO_CHANNEL; i++) {
        m_doControlMode[i] = SSR_CTRL_MODE_NORMAL;
    }
    m_commDetectTime = 0;          // 默认不检测通讯状态
    m_uploadInterval = 0;          // 默认不主动上传DI状态
}

// 心跳包：读模块地址（40051）+ 通讯检测时间（40049），双重验证
void SSRDioModbus::sendHeartBeat()
{
    // 读模块地址（保持寄存器，功能码03）
    sendReadRequest(REG_SSR_DIO_HOLD_ADDR, 1, QModbusDataUnit::HoldingRegisters);
    // 读通讯检测时间（保持寄存器，功能码03）
    sendReadRequest(REG_SSR_DIO_HOLD_COMM_DETECT, 1, QModbusDataUnit::HoldingRegisters);
}

// 读AI（映射为读DI状态，ch=DI通道号：1~48）
void SSRDioModbus::readAI(int ch)
{
    readDIStatus(ch);
}

// 写AO（映射为写DO状态，ch=DO通道号：1~48，value=1=开，0=关）
void SSRDioModbus::writeAO(int ch, quint16 value)
{
    writeDOStatus(ch, value != 0);
}

// 读取单个DI通道状态（功能码02：离散输入，默认优先）
void SSRDioModbus::readDIStatus(int ch)
{
    if (ch < 1 || ch > SSR_MAX_DI_DO_CHANNEL) {
        emit sigError(m_slaveAddr, QString("DI通道错误：仅支持1~%1路").arg(SSR_MAX_DI_DO_CHANNEL));
        return;
    }
    // 离散输入寄存器地址：ch-1（0起始）
    sendReadRequest(REG_SSR_DIO_DISCRETE_BASE + (ch - 1), 1, QModbusDataUnit::DiscreteInputs);
}

// 读取所有DI通道状态（功能码02，读取48个离散输入寄存器）
void SSRDioModbus::readAllDIStatus()
{
    sendReadRequest(REG_SSR_DIO_DISCRETE_BASE, SSR_MAX_DI_DO_CHANNEL, QModbusDataUnit::DiscreteInputs);
}

// 使能DI主动上传（0=关闭，>1=间隔时间：(N-1)*0.01s，功能码06）
void SSRDioModbus::enableDIUpload(bool enable, quint16 interval)
{
    quint16 regValue = 0;
    if (enable) {
        regValue = interval + 1; // 间隔时间=(N-1)*0.01s
        if (regValue > 65535) regValue = 65535;
        m_uploadInterval = interval;
    } else {
        regValue = 0;
        m_uploadInterval = 0;
    }
    sendWriteRequest(REG_SSR_DIO_HOLD_UPLOAD_CTRL, QVector<quint16>() << regValue, QModbusDataUnit::HoldingRegisters);
    qDebug() << "SSRDioModbus: 设备" << m_slaveAddr << "DI主动上传" << (enable ? "启用" : "关闭") << "，间隔：" << (enable ? QString("%1ms").arg(interval * 10) : "无");
}

// 控制单个DO通道（功能码06：保持寄存器控制，支持模式扩展；或功能码05：线圈控制）
void SSRDioModbus::writeDOStatus(int ch, bool on)
{
    if (ch < 1 || ch > SSR_MAX_DI_DO_CHANNEL) {
        emit sigError(m_slaveAddr, QString("DO通道错误：仅支持1~%1路").arg(SSR_MAX_DI_DO_CHANNEL));
        return;
    }
    quint16 value = on ? 0x0001 : 0x0000;
    // 优先使用保持寄存器控制（支持模式功能）
    sendWriteRequest(REG_SSR_DIO_HOLD_CTRL_BASE + (ch - 1), QVector<quint16>() << value, QModbusDataUnit::HoldingRegisters);
    qDebug() << "SSRDioModbus: 设备" << m_slaveAddr << "DO" << ch << "通道" << (on ? "开启" : "关闭");
}

// 批量控制所有DO通道（全开/全关，功能码06）
void SSRDioModbus::writeBatchDOStatus(bool on)
{
    quint16 value = on ? 0x0001 : 0x0000;
    sendWriteRequest(REG_SSR_DIO_HOLD_BATCH_CTRL, QVector<quint16>() << value, QModbusDataUnit::HoldingRegisters);
    qDebug() << "SSRDioModbus: 设备" << m_slaveAddr << "所有DO通道" << (on ? "全开" : "全关");
}

// 控制多个DO通道（功能码0F：线圈批量写入）
void SSRDioModbus::writeMultiDOStatus(const QVector<int> &chs, bool on)
{
    // 校验通道合法性
    for (int ch : chs) {
        if (ch < 1 || ch > SSR_MAX_DI_DO_CHANNEL) {
            emit sigError(m_slaveAddr, QString("DO通道错误：%1路超出范围（1~%2）").arg(ch).arg(SSR_MAX_DI_DO_CHANNEL));
            return;
        }
    }
    // 构造线圈数据（0=关，1=开）
    QVector<quint16> coilData((SSR_MAX_DI_DO_CHANNEL + 15) / 16, 0); // 按16位分组
    for (int ch : chs) {
        int index = (ch - 1) / 16;
        int bit = (ch - 1) % 16;
        if (on) {
            coilData[index] |= (1 << bit);
        } else {
            coilData[index] &= ~(1 << bit);
        }
    }
    // 发送批量写入请求（功能码0F，线圈寄存器）
    sendWriteRequest(REG_SSR_DIO_COIL_BASE, coilData, QModbusDataUnit::Coils);
    qDebug() << "SSRDioModbus: 设备" << m_slaveAddr << "DO通道" << chs << (on ? "开启" : "关闭");
}

// 设置DO通道控制模式（功能码06）
void SSRDioModbus::setDOControlMode(int ch, quint16 mode)
{
    if (ch < 1 || ch > SSR_MAX_DI_DO_CHANNEL) {
        emit sigError(m_slaveAddr, QString("DO通道错误：仅支持1~%1路").arg(SSR_MAX_DI_DO_CHANNEL));
        return;
    }
    // 校验模式合法性
    QList<quint16> validModes = {SSR_CTRL_MODE_NORMAL, SSR_CTRL_MODE_LINKAGE, SSR_CTRL_MODE_TOGGLE, SSR_CTRL_MODE_CYCLE, SSR_CTRL_MODE_AUTO_RESET};
    if (!validModes.contains(mode)) {
        emit sigError(m_slaveAddr, "DO控制模式错误：仅支持0-4（普通/联动/点动/循环/自动复位）");
        return;
    }
    sendWriteRequest(REG_SSR_DIO_HOLD_MODE_BASE + (ch - 1), QVector<quint16>() << mode, QModbusDataUnit::HoldingRegisters);
    m_doControlMode[ch - 1] = mode;
    QString modeName = mode == SSR_CTRL_MODE_NORMAL ? "普通模式" :
                       mode == SSR_CTRL_MODE_LINKAGE ? "联动模式" :
                       mode == SSR_CTRL_MODE_TOGGLE ? "点动模式" :
                       mode == SSR_CTRL_MODE_CYCLE ? "循环模式" : "自动复位模式";
    qDebug() << "SSRDioModbus: 设备" << m_slaveAddr << "DO" << ch << "通道模式设置为：" << modeName;
}

// 按位控制DO通道（1~16/17~32/33~48，功能码06）
void SSRDioModbus::setDOBitCtrl(const QVector<int> &chs, bool on)
{
    // 分组处理（1~16、17~32、33~48）
    QVector<int> group1, group2, group3;
    for (int ch : chs) {
        if (ch >= 1 && ch <= 16) group1.append(ch);
        else if (ch >= 17 && ch <= 32) group2.append(ch);
        else if (ch >= 33 && ch <= 48) group3.append(ch);
        else {
            emit sigError(m_slaveAddr, QString("DO通道错误：%1路超出范围（1~%2）").arg(ch).arg(SSR_MAX_DI_DO_CHANNEL));
            return;
        }
    }
    // 处理1~16通道（寄存器40054）
    if (!group1.isEmpty()) {
        quint16 value = 0;
        for (int ch : group1) {
            int bit = ch - 1;
            value |= (1 << bit);
        }
        if (!on) value = ~value & 0xFFFF;
        sendWriteRequest(REG_SSR_DIO_HOLD_BIT_CTRL1, QVector<quint16>() << value, QModbusDataUnit::HoldingRegisters);
    }
    // 处理17~32通道（寄存器40055）
    if (!group2.isEmpty()) {
        quint16 value = 0;
        for (int ch : group2) {
            int bit = ch - 17;
            value |= (1 << bit);
        }
        if (!on) value = ~value & 0xFFFF;
        sendWriteRequest(REG_SSR_DIO_HOLD_BIT_CTRL2, QVector<quint16>() << value, QModbusDataUnit::HoldingRegisters);
    }
    // 处理33~48通道（寄存器40056）
    if (!group3.isEmpty()) {
        quint16 value = 0;
        for (int ch : group3) {
            int bit = ch - 33;
            value |= (1 << bit);
        }
        if (!on) value = ~value & 0xFFFF;
        sendWriteRequest(REG_SSR_DIO_HOLD_BIT_CTRL3, QVector<quint16>() << value, QModbusDataUnit::HoldingRegisters);
    }
    qDebug() << "SSRDioModbus: 设备" << m_slaveAddr << "按位控制DO通道" << chs << (on ? "开启" : "关闭");
}

// 设置通讯检测时间（单位0.1s，0=不检测，功能码06）
void SSRDioModbus::setCommDetectTime(quint16 time)
{
    if (time > 65535) {
        emit sigError(m_slaveAddr, "通讯检测时间错误：仅支持0~65535（0.1s单位）");
        return;
    }
    sendWriteRequest(REG_SSR_DIO_HOLD_COMM_DETECT, QVector<quint16>() << time, QModbusDataUnit::HoldingRegisters);
    m_commDetectTime = time;
    qDebug() << "SSRDioModbus: 设备" << m_slaveAddr << "通讯检测时间设置为：" << time * 0.1 << "s";
}

// 设置波特率（功能码06）
void SSRDioModbus::setBaudrate(quint16 baudCode)
{
    QList<quint16> validBauds = {SSR_BAUDRATE_4800, SSR_BAUDRATE_9600, SSR_BAUDRATE_14400, SSR_BAUDRATE_19200,
                                 SSR_BAUDRATE_38400, SSR_BAUDRATE_56000, SSR_BAUDRATE_57600, SSR_BAUDRATE_115200};
    if (!validBauds.contains(baudCode)) {
        emit sigError(m_slaveAddr, "波特率代码错误：仅支持0-7（对应4800~115200bps）");
        return;
    }
    sendWriteRequest(REG_SSR_DIO_HOLD_BAUDRATE, QVector<quint16>() << baudCode, QModbusDataUnit::HoldingRegisters);
    // 更新基类波特率参数
    m_baudRate = baudCode == SSR_BAUDRATE_4800 ? 4800 :
                 baudCode == SSR_BAUDRATE_9600 ? 9600 :
                 baudCode == SSR_BAUDRATE_14400 ? 14400 :
                 baudCode == SSR_BAUDRATE_19200 ? 19200 :
                 baudCode == SSR_BAUDRATE_38400 ? 38400 :
                 baudCode == SSR_BAUDRATE_56000 ? 56000 :
                 baudCode == SSR_BAUDRATE_57600 ? 57600 : 115200;
    qDebug() << "SSRDioModbus: 设备" << m_slaveAddr << "波特率设置为：" << m_baudRate << "bps";
}

// 设置奇偶校验（功能码06）
void SSRDioModbus::setParity(quint16 parityCode)
{
    QList<quint16> validParities = {SSR_PARITY_NONE, SSR_PARITY_ODD, SSR_PARITY_EVEN};
    if (!validParities.contains(parityCode)) {
        emit sigError(m_slaveAddr, "校验位错误：仅支持0-2（无校验/奇校验/偶校验）");
        return;
    }
    sendWriteRequest(REG_SSR_DIO_HOLD_PARITY, QVector<quint16>() << parityCode, QModbusDataUnit::HoldingRegisters);
    // 更新基类校验位参数
    m_parity = parityCode == SSR_PARITY_NONE ? QSerialPort::NoParity :
               parityCode == SSR_PARITY_ODD ? QSerialPort::OddParity : QSerialPort::EvenParity;
    QString parityName = parityCode == SSR_PARITY_NONE ? "无校验" :
                         parityCode == SSR_PARITY_ODD ? "奇校验" : "偶校验";
    qDebug() << "SSRDioModbus: 设备" << m_slaveAddr << "校验位设置为：" << parityName;
}

// 心跳定时器超时：发送心跳包
void SSRDioModbus::onHeartTimerTimeout()
{
    sendHeartBeat();
}

// 解析回复数据（区分DI/DO/心跳/参数配置）
void SSRDioModbus::onModbusReplyFinished()
{
    QModbusReply *reply = qobject_cast<QModbusReply*>(sender());
    if (!reply) return;

    if (reply->error() != QModbusDevice::NoError) {
        updateDeviceStatus(Status_Offline);
        emit sigError(m_slaveAddr, "SSR数字量I/O模块回复错误：" + reply->errorString());
        reply->deleteLater();
        return;
    }

    // 更新设备在线状态
    updateDeviceStatus(Status_Online);
    QModbusDataUnit unit = reply->result();
    int startReg = unit.startAddress();
    QModbusDataUnit::RegisterType regType = unit.registerType();

    // 解析心跳回复（模块地址：REG_SSR_DIO_HOLD_ADDR=0x0032）
    if (startReg == REG_SSR_DIO_HOLD_ADDR && regType == QModbusDataUnit::HoldingRegisters) {
        parseHeartBeatReply(reply);
    }
    // 解析DI状态回复（离散输入寄存器：功能码02）
    else if (regType == QModbusDataUnit::DiscreteInputs) {
        if (unit.valueCount() == 1) {
            int ch = startReg + 1; // 0起始转1起始通道号
            parseDIReply(reply, ch);
        } else {
            parseDIReply(reply); // 读取所有DI
        }
    }
    // 解析DI状态回复（输入寄存器：功能码04）
    else if (regType == QModbusDataUnit::InputRegisters && startReg >= REG_SSR_DIO_INPUT_BASE) {
        parseDIReply(reply);
    }
    // 解析DO控制回复（保持寄存器：功能码06/10）
    else if (regType == QModbusDataUnit::HoldingRegisters && startReg >= REG_SSR_DIO_HOLD_CTRL_BASE && startReg <= REG_SSR_DIO_HOLD_CTRL_BASE + SSR_MAX_DI_DO_CHANNEL - 1) {
        int ch = startReg - REG_SSR_DIO_HOLD_CTRL_BASE + 1;
        parseDOReply(reply, ch);
    }
    // 解析批量DO控制回复（功能码06）
    else if (startReg == REG_SSR_DIO_HOLD_BATCH_CTRL && regType == QModbusDataUnit::HoldingRegisters) {
        parseDOReply(reply);
    }

    reply->deleteLater();
}

// 解析DI状态回复
void SSRDioModbus::parseDIReply(QModbusReply *reply, int ch)
{
    QModbusDataUnit unit = reply->result();
    if (ch != -1) {
        // 单个DI通道状态
        bool status = unit.value(0) != 0;
        if (ch >= 1 && ch <= 14)
            m_diStatus[ch - 1] = status;
        emit sigDataReceived(m_slaveAddr, ch, "DI_Status", status ? "已触发" : "未触发");
        qDebug() << "SSRDioModbus: 设备" << m_slaveAddr << "DI" << ch << "通道状态：" << (status ? "已触发" : "未触发");
    } else {
        // 所有DI通道状态（按寄存器分组解析）
        QString statusLog = QString("设备%1 所有DI通道状态：").arg(m_slaveAddr);
        for (int i = 0; i < unit.valueCount(); i++) {
            int16_t startReg = 0; // 比如Modbus起始寄存器地址，值按需设置
            int realCh = startReg + i + 1; // 实际通道号（1起始）
            bool status = unit.value(i) != 0;
            statusLog += QString("DI%1:%2  ").arg(realCh).arg(status ? "触发" : "未触发");
            emit sigDataReceived(m_slaveAddr, realCh, "DI_Status", status ? "已触发" : "未触发");
        }
        qDebug() << statusLog;
    }
}

// 解析DO控制回复
void SSRDioModbus::parseDOReply(QModbusReply *reply, int ch)
{
    QModbusDataUnit unit = reply->result();
    if (ch != -1) {
        // 单个DO通道控制结果
        bool status = unit.value(0) != 0;
        emit sigDataReceived(m_slaveAddr, ch, "DO_Status", status ? "开启" : "关闭");
        qDebug() << "SSRDioModbus: 设备" << m_slaveAddr << "DO" << ch << "通道控制成功：" << (status ? "开启" : "关闭");
    } else {
        // 批量DO控制结果
        bool status = unit.value(0) != 0;
        emit sigDataReceived(m_slaveAddr, -1, "DO_Batch_Status", status ? "所有通道全开" : "所有通道全关");
        qDebug() << "SSRDioModbus: 设备" << m_slaveAddr << "批量DO控制成功：" << (status ? "所有通道全开" : "所有通道全关");
    }
}

// 解析心跳回复（模块地址+通讯检测时间）
void SSRDioModbus::parseHeartBeatReply(QModbusReply *reply)
{
    QModbusDataUnit unit = reply->result();
    quint16 addr = unit.value(0);
    emit sigDataReceived(m_slaveAddr, -1, "HeartBeat_ModuleAddr", addr);
    qDebug() << "SSRDioModbus: 设备" << m_slaveAddr << "心跳成功，模块地址：" << addr;
}
#define CH_WATER_COOL_STATION2     1   // 工位2的水冷开关
#define CH_WATER_COOL_STATION1     2   // 工位1的水冷开关（氧气通道）
#define CH_WATER_COOL_STATION4     3   // 工位4的水冷开关
#define CH_WATER_COOL_STATION3     4   // 工位3的水冷开关
#define CH_TOTAL_RETURN_WATER      5   // 总回水（必须打开，工位1/2/3/4的水冷才起作用）
#define CH_AIR_COOL_STATION2       6   // 工位2的吹空气冷却开关
#define CH_AIR_COOL_STATION4       7   // 工位4的吹空气冷却开关
#define CH_AIR_COOL_STATION1       8   // 工位1的吹空气冷却开关
#define CH_AIR_COOL_STATION3       9   // 工位3的吹空气冷却开关
#define CH_TOP_AIR_STATION2_4      10  // 工位2和4的顶部吹空气开关
#define CH_TOP_AIR_STATION1_3      11  // 工位1和3的顶部吹空气开关
#define CH_IGNITION_STATION1_3     12  // 工位1和3的点火（天然气通道）
#define CH_IGNITION_STATION2_4     13  // 工位2和4的点火
#define CH_WATER_COOL_FAN          14  // 给水降温的风扇开关

//void SSRDioModbus::openOxygenValve()
//{
//    // 必须先开总回水，工位水冷才会生效
//    writeDOStatus(CH_TOTAL_RETURN_WATER, true);
//    // 打开氧气阀门（工位1水冷）
//    writeDOStatus(CH_OXYGEN_VALVE, true);
//    qDebug() << "SSRDioModbus: 氧气阀门（工位1水冷）已打开，总回水已同步开启";
//}

//// 关闭氧气阀门
//void SSRDioModbus::closeOxygenValve()
//{
//    writeDOStatus(CH_OXYGEN_VALVE, false);
//    qDebug() << "SSRDioModbus: 氧气阀门（工位1水冷）已关闭";
//}

//// 打开天然气阀门（工位1&3点火）
//void SSRDioModbus::openGasValve()
//{
//    writeDOStatus(CH_GAS_VALVE, true);
//    qDebug() << "SSRDioModbus: 天然气阀门（工位1&3点火）已打开";
//}

//// 关闭天然气阀门
//void SSRDioModbus::closeGasValve()
//{
//    writeDOStatus(CH_GAS_VALVE, false);
//    qDebug() << "SSRDioModbus: 天然气阀门（工位1&3点火）已关闭";
//}

//// 打开水冷却风扇（给水降温）
//void SSRDioModbus::openWaterCoolFan()
//{
//    writeDOStatus(CH_WATER_COOL_FAN, true);
//    qDebug() << "SSRDioModbus: 水冷却风扇已打开";
//}

//// 关闭水冷却风扇
//void SSRDioModbus::closeWaterCoolFan()
//{
//    writeDOStatus(CH_WATER_COOL_FAN, false);
//    qDebug() << "SSRDioModbus: 水冷却风扇已关闭";
//}

bool SSRDioModbus::getDIStatus(int ch)
{
    if (ch < 1 || ch > 14)
        return false;

    // 先发起读取请求
    readDIStatus(ch);

    // 返回缓存的最新状态
    return m_diStatus[ch - 1];
}
