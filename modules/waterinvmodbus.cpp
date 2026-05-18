#include "waterinvmodbus.h"
#include <QDebug>
#include <QModbusDataUnit>

WaterINVModbus::WaterINVModbus(QObject *parent) : ModbusBase(parent)
{
    m_targetPressure = 3.0f;      // 出厂默认目标压力3.0Bar
    m_setFrequency = 50.0f;       // 出厂默认频率50.0Hz
    m_runStatus = STATUS_STANDBY; // 初始状态：待机
    m_lastFaultCode = 0;          // 初始无故障
    m_motorPowerCode = 3;         // 出厂默认电机功率2.2kW（代码3）
}

// 心跳包：读运行状态（3000H）+ 反馈压力（100DH），双重验证在线
void WaterINVModbus::sendHeartBeat()
{
    // 读运行状态（保持寄存器，功能码03）
    sendReadRequest(REG_PC20_RUN_STATUS, 1, QModbusDataUnit::HoldingRegisters);
    // 读反馈压力（保持寄存器，功能码03）
    sendReadRequest(REG_PC20_FEEDBACK_PRESS, 1, QModbusDataUnit::HoldingRegisters);
}

// 读AI（映射为读反馈压力，ch无效，仅兼容基类接口）
void WaterINVModbus::readAI(int ch)
{
    Q_UNUSED(ch);
    readPressureInfo();
}

// 写AO（映射为设定目标压力，ch无效，value=压力×10（0.1Bar精度））
void WaterINVModbus::writeAO(int ch, quint16 value)
{
    Q_UNUSED(ch);
    float press = value / 10.0f;
    setTargetPressure(press);
}

// 变频器控制（启停/复位等命令）
void WaterINVModbus::controlInverter(quint16 cmd)
{
    // 校验命令合法性
    QList<quint16> validCmds = {CMD_FORWARD_RUN, CMD_REVERSE_RUN, CMD_FORWARD_JOG,
                                 CMD_REVERSE_JOG, CMD_FREE_STOP, CMD_DECEL_STOP, CMD_FAULT_RESET};
    if (!validCmds.contains(cmd)) {
        emit sigError(m_slaveAddr, "Water-INV变频器控制命令错误：不支持该命令");
        return;
    }
    // 发送控制命令（写保持寄存器，功能码06）
    QModbusReply *reply = sendWriteRequest(REG_PC20_CONTROL_CMD, QVector<quint16>() << cmd, QModbusDataUnit::HoldingRegisters);
    // 关联命令与回复（用于解析时区分命令类型）
    reply->setProperty("controlCmd", cmd);
}

// 设定目标压力（单位：Bar，0.1Bar精度）
void WaterINVModbus::setTargetPressure(float press)
{
    // 压力范围校验（参考P0.00：1.0~P0.21，默认P0.21=9.0Bar）
    if (press < 1.0f || press > 9.0f) {
        emit sigError(m_slaveAddr, "Water-INV目标压力错误：仅支持1.0~9.0Bar");
        return;
    }
    m_targetPressure = press;
    quint16 regValue = static_cast<quint16>(press * 10); // 转换为0.1Bar精度
    // 写目标压力寄存器（功能码06）
    sendWriteRequest(REG_PC20_TARGET_PRESS, QVector<quint16>() << regValue, QModbusDataUnit::HoldingRegisters);
    qDebug() << "WaterINVModbus: 设备" << m_slaveAddr << "目标压力设定为：" << press << "Bar";
}

// 设定输出频率（单位：Hz，0.01Hz精度）
void WaterINVModbus::setOutputFrequency(float freq)
{
    // 频率范围校验（参考P1.06：下限频率~50.00Hz）
    if (freq < 0.0f || freq > 50.0f) {
        emit sigError(m_slaveAddr, "Water-INV输出频率错误：仅支持0.00~50.00Hz");
        return;
    }
    m_setFrequency = freq;
    quint16 regValue = static_cast<quint16>(freq * 100); // 转换为0.01Hz精度
    // 写通讯设定值寄存器（功能码06，对应频率设定）
    sendWriteRequest(REG_PC20_COMM_SET_VAL, QVector<quint16>() << regValue, QModbusDataUnit::HoldingRegisters);
    qDebug() << "WaterINVModbus: 设备" << m_slaveAddr << "输出频率设定为：" << freq << "Hz";
}

// 故障复位
void WaterINVModbus::resetFault()
{
    controlInverter(CMD_FAULT_RESET);
    qDebug() << "WaterINVModbus: 设备" << m_slaveAddr << "发送故障复位命令";
}

// 设置电机功率（0=0.75kW，1=1.1kW，2=1.5kW，3=2.2kW）
void WaterINVModbus::setMotorPower(quint16 powerCode)
{
    if (powerCode > 3) {
        emit sigError(m_slaveAddr, "Water-INV电机功率代码错误：仅支持0-3（对应0.75~2.2kW）");
        return;
    }
    sendWriteRequest(REG_PC20_MOTOR_POWER, QVector<quint16>() << powerCode, QModbusDataUnit::HoldingRegisters);
    m_motorPowerCode = powerCode;
    QString powerStr = powerCode == 0 ? "0.75kW" :
                       powerCode == 1 ? "1.1kW" :
                       powerCode == 2 ? "1.5kW" : "2.2kW";
    qDebug() << "WaterINVModbus: 设备" << m_slaveAddr << "电机功率设置为：" << powerStr;
}

// 设置PID功能模式（0=关闭，1=休眠模式1，2=休眠模式2，3=休眠模式3）
void WaterINVModbus::setPIDMode(quint16 mode)
{
    if (mode > 3) {
        emit sigError(m_slaveAddr, "Water-INVPID模式错误：仅支持0-3（0=关闭，1-3=休眠模式）");
        return;
    }
    sendWriteRequest(REG_PC20_PID_MODE, QVector<quint16>() << mode, QModbusDataUnit::HoldingRegisters);
    QString modeStr = mode == 0 ? "关闭" :
                      mode == 1 ? "休眠模式1（休眠优先）" :
                      mode == 2 ? "休眠模式2（恒压优先）" : "休眠模式3（均衡模式）";
    qDebug() << "WaterINVModbus: 设备" << m_slaveAddr << "PID模式设置为：" << modeStr;
}

// 设置高压报警值（0.1Bar精度）
void WaterINVModbus::setHighPressureAlarm(float press)
{
    if (press < m_targetPressure || press > 10.0f) {
        emit sigError(m_slaveAddr, QString("Water-INV高压报警值错误：需大于目标压力（%1Bar）且≤10.0Bar").arg(m_targetPressure));
        return;
    }
    quint16 regValue = static_cast<quint16>(press * 10);
    sendWriteRequest(REG_PC20_HIGH_PRESS_ALM, QVector<quint16>() << regValue, QModbusDataUnit::HoldingRegisters);
    qDebug() << "WaterINVModbus: 设备" << m_slaveAddr << "高压报警值设置为：" << press << "Bar";
}

// 设置低压报警值（0.1Bar精度）
void WaterINVModbus::setLowPressureAlarm(float press)
{
    if (press < 0.0f || press > m_targetPressure) {
        emit sigError(m_slaveAddr, QString("Water-INV低压报警值错误：需≥0.0Bar且小于目标压力（%1Bar）").arg(m_targetPressure));
        return;
    }
    quint16 regValue = static_cast<quint16>(press * 10);
    sendWriteRequest(REG_PC20_LOW_PRESS_ALM, QVector<quint16>() << regValue, QModbusDataUnit::HoldingRegisters);
    qDebug() << "WaterINVModbus: 设备" << m_slaveAddr << "低压报警值设置为：" << press << "Bar";
}

// 使能上电自动启动（0=关闭，1=开启）
void WaterINVModbus::enableAutoStart(bool enable)
{
    quint16 regValue = enable ? 0x0001 : 0x0000;
    sendWriteRequest(REG_PC20_AUTO_START, QVector<quint16>() << regValue, QModbusDataUnit::HoldingRegisters);
    qDebug() << "WaterINVModbus: 设备" << m_slaveAddr << "上电自动启动" << (enable ? "开启" : "关闭");
}

// 设置缺水保护模式（0=关闭，1=频率+电流，2=频率+压力，3=三者结合）
void WaterINVModbus::setWaterLackProtection(quint16 mode)
{
    if (mode > 3) {
        emit sigError(m_slaveAddr, "Water-INV缺水保护模式错误：仅支持0-3");
        return;
    }
    sendWriteRequest(REG_PC20_WATER_LACK_PROT, QVector<quint16>() << mode, QModbusDataUnit::HoldingRegisters);
    QString modeStr = mode == 0 ? "关闭" :
                      mode == 1 ? "频率+电流判断" :
                      mode == 2 ? "频率+压力判断" : "频率+电流+压力判断";
    qDebug() << "WaterINVModbus: 设备" << m_slaveAddr << "缺水保护模式设置为：" << modeStr;
}

// 读取运行状态
void WaterINVModbus::readRunStatus()
{
    sendReadRequest(REG_PC20_RUN_STATUS, 1, QModbusDataUnit::HoldingRegisters);
}

// 读取故障代码
void WaterINVModbus::readFaultCode()
{
    sendReadRequest(REG_PC20_FAULT_CODE, 1, QModbusDataUnit::HoldingRegisters);
}

// 读取压力信息（设定/反馈/显示）
void WaterINVModbus::readPressureInfo()
{
    // 批量读取3个压力寄存器（设定+反馈+显示）
    sendReadRequest(REG_PC20_SET_PRESS, 3, QModbusDataUnit::HoldingRegisters);
}

// 读取电气参数（电压/电流/频率）
void WaterINVModbus::readElectricalParams()
{
    // 批量读取运行频率、母线电压、输出电压、输出电流
    sendReadRequest(REG_PC20_RUN_FREQ, 4, QModbusDataUnit::HoldingRegisters);
}

// 读取运行时间/上电时间
void WaterINVModbus::readRunningHours()
{
    sendReadRequest(REG_PC20_POWER_ON_HOUR, 2, QModbusDataUnit::HoldingRegisters);
}

// 心跳定时器超时：发送心跳包
void WaterINVModbus::onHeartTimerTimeout()
{
    sendHeartBeat();
}

// 解析变频器回复数据
void WaterINVModbus::onModbusReplyFinished()
{
    QModbusReply *reply = qobject_cast<QModbusReply*>(sender());
    if (!reply) return;

    if (reply->error() != QModbusDevice::NoError) {
        updateDeviceStatus(Status_Offline);
        emit sigError(m_slaveAddr, "Water-INV（PC20）变频器回复错误：" + reply->errorString());
        reply->deleteLater();
        return;
    }

    // 更新设备在线状态
    updateDeviceStatus(Status_Online);
    QModbusDataUnit unit = reply->result();
    int startReg = unit.startAddress();
    QModbusDataUnit::RegisterType regType = unit.registerType();

    // 解析控制命令回复（控制命令寄存器：2000H）
    if (startReg == REG_PC20_CONTROL_CMD && regType == QModbusDataUnit::HoldingRegisters) {
        quint16 cmd = reply->property("controlCmd").toUInt();
        parseControlReply(reply, cmd);
    }
    // 解析运行状态回复（3000H）
    else if (startReg == REG_PC20_RUN_STATUS && regType == QModbusDataUnit::HoldingRegisters) {
        parseRunStatusReply(reply);
    }
    // 解析压力信息回复（100CH~100DH，批量读取3个寄存器）
    else if (startReg == REG_PC20_SET_PRESS && regType == QModbusDataUnit::HoldingRegisters && unit.valueCount() >= 3) {
        parsePressureReply(reply);
    }
    // 解析电气参数回复（1001H~1005H，批量读取4个寄存器）
    else if (startReg == REG_PC20_RUN_FREQ && regType == QModbusDataUnit::HoldingRegisters && unit.valueCount() >= 4) {
        parseElectricalReply(reply);
    }
    // 解析故障代码回复（1008H）
    else if (startReg == REG_PC20_FAULT_CODE && regType == QModbusDataUnit::HoldingRegisters) {
        parseFaultReply(reply);
    }
    // 解析反馈压力回复（心跳用，100DH）
    else if (startReg == REG_PC20_FEEDBACK_PRESS && regType == QModbusDataUnit::HoldingRegisters) {
        float feedbackPress = unit.value(0) / 10.0f;
        emit sigDataReceived(m_slaveAddr, -1, "HeartBeat_FeedbackPress", QString::asprintf("%.1fBar", feedbackPress));
    }

    reply->deleteLater();
}

// 解析控制命令回复
void WaterINVModbus::parseControlReply(QModbusReply *reply, quint16 cmd)
{
    QString cmdStr = "";
    switch (cmd) {
    case CMD_FORWARD_RUN: cmdStr = "正转运行"; break;
    case CMD_REVERSE_RUN: cmdStr = "反转运行"; break;
    case CMD_FORWARD_JOG: cmdStr = "正转点动"; break;
    case CMD_REVERSE_JOG: cmdStr = "反转点动"; break;
    case CMD_FREE_STOP: cmdStr = "自由停机"; break;
    case CMD_DECEL_STOP: cmdStr = "减速停机"; break;
    case CMD_FAULT_RESET: cmdStr = "故障复位"; break;
    default: cmdStr = "未知命令"; break;
    }
    emit sigDataReceived(m_slaveAddr, -1, "Control_Result", QString("命令执行成功：%1").arg(cmdStr));
    qDebug() << "WaterINVModbus: 设备" << m_slaveAddr << cmdStr << "命令执行成功";
}

// 解析运行状态回复
void WaterINVModbus::parseRunStatusReply(QModbusReply *reply)
{
    QModbusDataUnit unit = reply->result();
    m_runStatus = unit.value(0);
    QString statusStr = "";
    switch (m_runStatus) {
    case STATUS_FORWARD_RUN: statusStr = "正转运行"; break;
    case STATUS_REVERSE_RUN: statusStr = "反转运行"; break;
    case STATUS_STANDBY: statusStr = "待机"; break;
    case STATUS_FAULT: statusStr = "故障"; break;
    default: statusStr = QString("未知状态（代码：%1）").arg(m_runStatus); break;
    }
    emit sigDataReceived(m_slaveAddr, -1, "Run_Status", statusStr);
    qDebug() << "WaterINVModbus: 设备" << m_slaveAddr << "运行状态：" << statusStr;
    // 若为故障状态，自动读取故障代码
    if (m_runStatus == STATUS_FAULT) {
        readFaultCode();
    }
}

// 解析压力信息回复
void WaterINVModbus::parsePressureReply(QModbusReply *reply)
{
    QModbusDataUnit unit = reply->result();
    float setPress = unit.value(0) / 10.0f;    // 设定压力（100CH）
    float feedbackPress = unit.value(1) / 10.0f;// 反馈压力（100DH）
    float displayPress = unit.value(2) / 10.0f;  // 显示压力（100BH）
    m_targetPressure = setPress;

    emit sigDataReceived(m_slaveAddr, -1, "Set_Pressure", QString::asprintf("%.1fBar", setPress));
    emit sigDataReceived(m_slaveAddr, -1, "Feedback_Pressure", QString::asprintf("%.1fBar", feedbackPress));
    emit sigDataReceived(m_slaveAddr, -1, "Display_Pressure", QString::asprintf("%.1fBar", displayPress));

    qDebug() << "WaterINVModbus: 设备" << m_slaveAddr << "压力信息 - 设定：" << setPress << "Bar，反馈：" << feedbackPress << "Bar，显示：" << displayPress << "Bar";
}

// 解析电气参数回复
void WaterINVModbus::parseElectricalReply(QModbusReply *reply)
{
    QModbusDataUnit unit = reply->result();
    float runFreq = unit.value(0) / 100.0f;      // 运行频率（1001H，0.01Hz）
    float busVoltage = unit.value(1) / 10.0f;    // 母线电压（1003H，0.1V）
    float outputVoltage = unit.value(2);          // 输出电压（1004H，1V）
    float outputCurrent = unit.value(3) / 100.0f; // 输出电流（1005H，0.01A）

    emit sigDataReceived(m_slaveAddr, -1, "Run_Frequency", QString::asprintf("%.2fHz", runFreq));
    emit sigDataReceived(m_slaveAddr, -1, "Bus_Voltage", QString::asprintf("%.1fV", busVoltage));
    emit sigDataReceived(m_slaveAddr, -1, "Output_Voltage", QString::asprintf("%dV", static_cast<int>(outputVoltage)));
    emit sigDataReceived(m_slaveAddr, -1, "Output_Current", QString::asprintf("%.2fA", outputCurrent));

    qDebug() << "WaterINVModbus: 设备" << m_slaveAddr << "电气参数 - 运行频率：" << runFreq << "Hz，母线电压：" << busVoltage << "V，输出电压：" << outputVoltage << "V，输出电流：" << outputCurrent << "A";
}

// 解析故障代码回复
void WaterINVModbus::parseFaultReply(QModbusReply *reply)
{
    QModbusDataUnit unit = reply->result();
    m_lastFaultCode = unit.value(0);
    QString faultStr = "";
    // 故障代码映射（参考手册第五章故障代码表）
    switch (m_lastFaultCode) {
    case 0x0000: faultStr = "无故障"; break;
    case 0x0001: faultStr = "E001：逆变单元故障"; break;
    case 0x0002: faultStr = "E002：加速过电流"; break;
    case 0x0003: faultStr = "E003：减速过电流"; break;
    case 0x0004: faultStr = "E004：恒速过电流"; break;
    case 0x0005: faultStr = "E005：加速过电压"; break;
    case 0x0006: faultStr = "E006：减速过电压"; break;
    case 0x0009: faultStr = "E009：母线欠压"; break;
    case 0x000A: faultStr = "E010：变频器过载"; break;
    case 0x000B: faultStr = "E011：电机过载"; break;
    case 0x0015: faultStr = "E021：EEPROM读写故障"; break;
    case 0x0018: faultStr = "E024：PID反馈断线故障"; break;
    case 0x001B: faultStr = "E027：缺水报警"; break;
    case 0x001C: faultStr = "E028：高水压报警"; break;
    case 0x001D: faultStr = "E029：低水压报警"; break;
    default: faultStr = QString("未知故障（代码：0x%1）").arg(m_lastFaultCode, 4, 16, QChar('0')); break;
    }
    emit sigDataReceived(m_slaveAddr, -1, "Fault_Code", faultStr);
    emit sigError(m_slaveAddr, faultStr); // 触发错误信号，弹窗提醒
    qDebug() << "WaterINVModbus: 设备" << m_slaveAddr << "故障信息：" << faultStr;
}