#include "dam2601modbus.h"
#include <QDebug>
#include <QModbusDataUnit>

DAM2601MModbus::DAM2601MModbus(QObject *parent) : ModbusBase(parent)
{
//    // 重连回复解析信号（覆盖基类的通用处理）
//    disconnect(m_modbusMaster, &QModbusRtuSerialMaster::replyFinished,
//               this, &ModbusBase::onModbusReplyFinished);
//    connect(m_modbusMaster, &QModbusRtuSerialMaster::replyFinished,
//            this, &DAM2601MModbus::onModbusReplyFinished);

    // 正确代码：断开父类的通用回复处理连接（适配带参数的 finished 信号）
//      disconnect(m_modbusMaster, &QModbusClient::finished,
//                 this, &ModbusBase::onModbusReplyFinished);

//      // 连接到子类的自定义回复处理（注意信号带 QModbusReply* 参数）
//      connect(m_modbusMaster, &QModbusClient::finished,
//              this, &DAM2601MModbus::onModbusReplyFinished);
}

// 心跳包：读模块地址（寄存器48213，功能码03，保持寄存器）
void DAM2601MModbus::sendHeartBeat()
{
    sendReadRequest(REG_MODULE_ADDR - 1, 1, QModbusDataUnit::HoldingRegisters);
    // 注：Qt Modbus寄存器地址为**0起始**，手册中是1起始，需-1
}

// 读AI采集值（功能码04，输入寄存器，通道ch：REG_AI_BASE+ch-1）
void DAM2601MModbus::readAI(int ch)
{
    if (ch < 0 || ch > 7) {
        emit sigError(m_slaveAddr, "AI通道错误：仅支持0-7路");
        return;
    }
    sendReadRequest(REG_AI_BASE - 1 + ch, 1, QModbusDataUnit::InputRegisters);
}//

void DAM2601MModbus::readAI_ALL()
{
   // if (ch < 0 || ch > 7) {
   //     emit sigError(m_slaveAddr, "AI通道错误：仅支持0-7路");
   //     return;
   // }
    sendReadRequest(0, 8, QModbusDataUnit::InputRegisters);
}

// 写AO输出值（功能码06，保持寄存器，通道ch：REG_AO_BASE+ch-1，值0-0x0FFF）
void DAM2601MModbus::writeAO(int ch, quint16 value)
{
    if (ch < 0 || ch > 7) {
        emit sigError(m_slaveAddr, "AO通道错误：仅支持0-7路");
        return;
    }
    if (value > 0x0FFF) {
        emit sigError(m_slaveAddr, "AO值错误：仅支持0-0x0FFF");
        return;
    }
    sendWriteRequest(REG_AO_BASE - 1 + ch, QVector<quint16>() << value, QModbusDataUnit::HoldingRegisters);
}

// 设置AI量程（通道ch：REG_AI_RANGE_BASE+ch-1，功能码06）
void DAM2601MModbus::setAIRange(int ch, quint16 rangeCode)
{
    if (ch < 0 || ch > 7) {
        emit sigError(m_slaveAddr, "AI通道错误：仅支持0-7路");
        return;
    }
    sendWriteRequest(REG_AI_RANGE_BASE - 1 + ch, QVector<quint16>() << rangeCode, QModbusDataUnit::HoldingRegisters);
}

// 设置AO量程（通道ch：REG_AO_RANGE_BASE+ch-1，功能码06）
void DAM2601MModbus::setAORange(int ch, quint16 rangeCode)
{
    if (ch < 0 || ch > 7) {
        emit sigError(m_slaveAddr, "AO通道错误：仅支持0-7路");
        return;
    }
    sendWriteRequest(REG_AO_RANGE_BASE - 1 + ch, QVector<quint16>() << rangeCode, QModbusDataUnit::HoldingRegisters);
}

// 心跳定时器超时：发送心跳包
void DAM2601MModbus::onHeartTimerTimeout()
{
    sendHeartBeat();
}

float DAM2601MModbus::getAIValue(int ch)
{
    if (ch < 1 || ch > 16)
        return 0.0f;
    //readAI_after(ch);
    return m_flowValues[ch - 1];
}
float DAM2601MModbus::getAIRawValue(int ch)
{
    if (ch < 1 || ch > 16)
        return 0.0f;
    //readAI_after(ch);
    return m_flow_raw_Values[ch - 1];
}

// 解析DAM-2601M的Modbus回复（核心：区分心跳/AI/AO回复并解析）
void DAM2601MModbus::onModbusReplyFinished()
{
    QModbusReply *reply = qobject_cast<QModbusReply*>(sender());
    if (!reply) return;

    if (reply->error() != QModbusDevice::NoError) {
        updateDeviceStatus(Status_Offline);
        emit sigError(m_slaveAddr, "DAM-2601M回复错误：" + reply->errorString());
        reply->deleteLater();
        return;
    }

    // 更新为在线
    updateDeviceStatus(Status_Online);
    QModbusDataUnit unit = reply->result();
    int startReg = unit.startAddress();  // Qt Modbus 0起始地址

    int count = unit.valueCount();
    if(unit.registerType() == QModbusDataUnit::InputRegisters && count == 8)
    {
        //qDebug()<<"unit.registerType() == QModbusDataUnit::InputRegisters && count == 16";
            for(int i=0; i<8; i++){
                uint16_t raw = unit.value(i);
                //float temp = (float)(1340*raw)/63356 - 40;  // 手册标准换算
                float temp = (float)(10-raw)/16*100;
                m_flow_raw_Values[i] = raw;
                m_flowValues[i] = temp;         // 保存16路温度
            }
            reply->deleteLater();
            return;
     }

    // 解析心跳回复（读模块地址：REG_MODULE_ADDR-1 = 48212）
    if (startReg == REG_MODULE_ADDR - 1) {
        parseHeartBeatReply(reply);
    }
    // 解析AI采集值回复（AI基地址-1=40000，0-7路：40000-40007）
    else if (startReg >= REG_AI_BASE - 1 && startReg <= REG_AI_BASE - 1 + 7) {
        int ch = startReg - (REG_AI_BASE - 1);
        parseAIReply(reply, ch);
    }
    // AO写回复无需解析，仅反馈成功即可
    else if (startReg >= REG_AO_BASE - 1 && startReg <= REG_AO_BASE - 1 + 7) {
        int ch = startReg - (REG_AO_BASE - 1);
        emit sigDataReceived(m_slaveAddr, ch, "AO_Write", unit.value(0));
    }

    reply->deleteLater();
}

// 解析心跳回复（模块地址）
void DAM2601MModbus::parseHeartBeatReply(QModbusReply *reply)
{
    QModbusDataUnit unit = reply->result();
    quint16 addr = unit.value(0) & 0x00FF;  // 手册：Bit15-Bit8=0，Bit7-Bit0为地址
    emit sigDataReceived(m_slaveAddr, -1, "HeartBeat_ModuleAddr", addr);
    qDebug() << "DAM2601MModbus: 设备" << m_slaveAddr << "心跳成功，模块地址：" << addr;
}

// 解析AI采集值（转换为工程值：0-10V/0-20mA，0-4095对应满量程）
void DAM2601MModbus::parseAIReply(QModbusReply *reply, int ch)
{
    QModbusDataUnit unit = reply->result();
    quint16 rawValue = unit.value(0);  // 原始采集值（0-4095）
    // 转换为工程值（默认0-10V，若为0-20mA可在主线程根据量程再转换）
    float voltValue = (rawValue / 4095.0) * 10.0;
    // 发送原始值和工程值给主线程
    emit sigDataReceived(m_slaveAddr, ch, "AI_Raw", rawValue);
    emit sigDataReceived(m_slaveAddr, ch, "AI_Volt", QString::asprintf("%.2fV", voltValue));
    qDebug() << "DAM2601MModbus: 设备" << m_slaveAddr << "AI" << ch << "原始值：" << rawValue << "工程值：" << voltValue << "V";
}
