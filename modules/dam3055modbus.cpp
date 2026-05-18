#include "dam3055modbus.h"
#include <QDebug>
#include <QModbusDataUnit>

DAM3055MModbus::DAM3055MModbus(QObject *parent) : ModbusBase(parent)
{
    // 初始化16路AI量程为默认值（4~20mA）
    for (int i = 0; i < 16; i++) {
        m_aiRange[i] = RANGE_3055_4_20mA;
    }
}

// 心跳包：读模块地址（寄存器40133，功能码03，保持寄存器）
void DAM3055MModbus::sendHeartBeat()
{
    // Qt Modbus为0起始地址，手册1起始地址需-1
    sendReadRequest(REG_3055_MODULE_ADDR - 1, 1, QModbusDataUnit::HoldingRegisters);
}

// 读AI采集值（16路差分输入，通道ch：0-15，功能码04，输入寄存器）
void DAM3055MModbus::readAI(int ch)
{
    if (ch < 0 || ch > 15) {
        emit sigError(m_slaveAddr, "DAM-3055M AI通道错误：仅支持0-15路");
        return;
    }
    // AI通道对应寄存器：REG_3055_AI_BASE-1 + ch（0起始转换）
    sendReadRequest(REG_3055_AI_BASE - 1 + ch, 1, QModbusDataUnit::InputRegisters);
}

// 写AO（DAM-3055M无AO输出功能，直接返回错误）
void DAM3055MModbus::writeAO(int ch, quint16 value)
{
    Q_UNUSED(ch);
    Q_UNUSED(value);
    emit sigError(m_slaveAddr, "DAM-3055M 无AO输出功能，不支持写AO操作");
}

// 设置AI通道量程（0-15通道，量程代码参考头文件定义）
void DAM3055MModbus::setAIRange(int ch, quint16 rangeCode)
{
    if (ch < 0 || ch > 15) {
        emit sigError(m_slaveAddr, "DAM-3055M AI通道错误：仅支持0-15路");
        return;
    }
    // 校验量程代码合法性
    QList<quint16> validRanges = {RANGE_3055_0_5V, RANGE_3055_1_5V,
                                 RANGE_3055_0_20mA, RANGE_3055_4_20mA};
    if (!validRanges.contains(rangeCode)) {
        emit sigError(m_slaveAddr, "DAM-3055M 量程代码错误：不支持该量程");
        return;
    }
    // 量程寄存器地址：REG_3055_AI_RANGE_BASE-1 + ch
    sendWriteRequest(REG_3055_AI_RANGE_BASE - 1 + ch,
                     QVector<quint16>() << rangeCode,
                     QModbusDataUnit::HoldingRegisters);
    m_aiRange[ch] = rangeCode;  // 更新缓存量程
    qDebug() << "DAM3055MModbus: 设备" << m_slaveAddr << "AI" << ch << "量程设置为：" << QString("0x%1").arg(rangeCode, 4, 16, QChar('0'));
}

// 设置看门狗超时时间（0=禁用，5-65535ms）
void DAM3055MModbus::setWatchdogTimeout(quint16 timeoutMs)
{
    if (timeoutMs != 0 && timeoutMs < 5) {
        emit sigError(m_slaveAddr, "看门狗超时时间错误：需设置为0或5-65535ms");
        return;
    }
    sendWriteRequest(REG_3055_WATCHDOG - 1,
                     QVector<quint16>() << timeoutMs,
                     QModbusDataUnit::HoldingRegisters);
    qDebug() << "DAM3055MModbus: 设备" << m_slaveAddr << "看门狗超时时间设置为：" << timeoutMs << "ms";
}

// 使能/禁用数据换算功能（0=关闭，1=上下限换算使能）
void DAM3055MModbus::enableConvert(bool enable)
{
    quint16 enableCode = enable ? 0x0001 : 0x0000;
    sendWriteRequest(REG_3055_CONVERT_ENABLE - 1,
                     QVector<quint16>() << enableCode,
                     QModbusDataUnit::HoldingRegisters);
    qDebug() << "DAM3055MModbus: 设备" << m_slaveAddr << "数据换算功能" << (enable ? "启用" : "禁用");
}

// 设置传输数据类型（参考DATA_TYPE_xxx定义）
void DAM3055MModbus::setDataType(quint16 dataType)
{
    QList<quint16> validTypes = {DATA_TYPE_UINT, DATA_TYPE_INT,
                                 DATA_TYPE_ULONG, DATA_TYPE_LONG, DATA_TYPE_FLOAT};
    if (!validTypes.contains(dataType)) {
        emit sigError(m_slaveAddr, "DAM-3055M 数据类型错误：不支持该数据类型");
        return;
    }
    sendWriteRequest(REG_3055_DATA_TYPE - 1,
                     QVector<quint16>() << dataType,
                     QModbusDataUnit::HoldingRegisters);
    qDebug() << "DAM3055MModbus: 设备" << m_slaveAddr << "传输数据类型设置为：" << QString("0x%1").arg(dataType, 4, 16, QChar('0'));
}

// 心跳定时器超时：发送心跳包
void DAM3055MModbus::onHeartTimerTimeout()
{
    sendHeartBeat();
}

// 解析DAM-3055M的Modbus回复（区分心跳、AI数据）
void DAM3055MModbus::onModbusReplyFinished()
{
    QModbusReply *reply = qobject_cast<QModbusReply*>(sender());
    if (!reply) return;

    if (reply->error() != QModbusDevice::NoError) {
        updateDeviceStatus(Status_Offline);
        emit sigError(m_slaveAddr, "DAM-3055M回复错误：" + reply->errorString());
        reply->deleteLater();
        return;
    }

    // 更新设备在线状态
    updateDeviceStatus(Status_Online);
    QModbusDataUnit unit = reply->result();
    int startReg = unit.startAddress();  // Qt Modbus 0起始地址

    // 解析心跳回复（读模块地址：REG_3055_MODULE_ADDR-1 = 40132）
    if (startReg == REG_3055_MODULE_ADDR - 1) {
        parseHeartBeatReply(reply);
    }
    // 解析AI采集值回复（AI基地址-1=40000，0-15路：40000-40015）
    else if (startReg >= REG_3055_AI_BASE - 1 && startReg <= REG_3055_AI_BASE - 1 + 15) {
        int ch = startReg - (REG_3055_AI_BASE - 1);
        parseAIReply(reply, ch);
    }

    reply->deleteLater();
}

// 解析心跳回复（验证模块地址）
void DAM3055MModbus::parseHeartBeatReply(QModbusReply *reply)
{
    QModbusDataUnit unit = reply->result();
    quint16 addr = unit.value(0) & 0x00FF;  // 手册：Bit15-Bit8=0，Bit7-Bit0为地址
    emit sigDataReceived(m_slaveAddr, -1, "HeartBeat_ModuleAddr", addr);
    qDebug() << "DAM3055MModbus: 设备" << m_slaveAddr << "心跳成功，模块地址：" << addr;
}

// 解析AI采集值（根据当前量程转换为工程值）
void DAM3055MModbus::parseAIReply(QModbusReply *reply, int ch)
{
    QModbusDataUnit unit = reply->result();
    quint16 rawValue = unit.value(0);  // 原始采集值（0-4095，12位分辨率）
    float engValue = 0.0f;             // 工程值（电压/电流）
    QString unitStr = "";              // 单位（V/mA）

    // 根据当前量程转换工程值（参考手册表7）
    switch (m_aiRange[ch]) {
    case RANGE_3055_0_5V:
        engValue = (rawValue / 4095.0f) * 5.0f;  // 0~5V：0对应0V，4095对应5V
        unitStr = "V";
        break;
    case RANGE_3055_1_5V:
        engValue = 1.0f + (rawValue / 4095.0f) * 4.0f;  // 1~5V：819对应1V，4095对应5V
        unitStr = "V";
        break;
    case RANGE_3055_0_20mA:
        engValue = (rawValue / 4095.0f) * 20.0f;  // 0~20mA：0对应0mA，4095对应20mA
        unitStr = "mA";
        break;
    case RANGE_3055_4_20mA:
        engValue = 4.0f + (rawValue / 4095.0f) * 16.0f;  // 4~20mA：819对应4mA，4095对应20mA
        unitStr = "mA";
        break;
    default:
        engValue = rawValue;
        unitStr = "Raw";
        break;
    }

    // 发送原始值和工程值给主线程
    emit sigDataReceived(m_slaveAddr, ch, "AI_Raw", rawValue);
    emit sigDataReceived(m_slaveAddr, ch, "AI_Eng", QString::asprintf("%.2f%s", engValue, unitStr.toUtf8().data()));
    qDebug() << "DAM3055MModbus: 设备" << m_slaveAddr << "AI" << ch << "原始值：" << rawValue
             << "工程值：" << QString::asprintf("%.2f%s", engValue, unitStr.toUtf8().data());
}
