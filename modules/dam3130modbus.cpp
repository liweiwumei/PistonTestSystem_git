#include "dam3130modbus.h"  // 注意头文件名同步修改
#include <QDebug>
#include <QModbusDataUnit>
#include <QtEndian>

DAM3130Modbus::DAM3130Modbus(QObject *parent) : ModbusBase(parent)
{
    // 初始化默认参数
    m_thermoType = THERMO_K_TYPE;
    m_convertMode = CONVERT_LINEAR;
    m_channelEnable = 0xFFFF;  // 全通道使能

    // 初始化热电偶类型-温度范围映射（参考手册表6）
    m_thermoRangeMap.insert(THERMO_K_TYPE, QPair<float, float>(-40.0f, 1300.0f));
    m_thermoRangeMap.insert(THERMO_J_TYPE, QPair<float, float>(0.0f, 1200.0f));
    m_thermoRangeMap.insert(THERMO_T_TYPE, QPair<float, float>(-200.0f, 400.0f));
    m_thermoRangeMap.insert(THERMO_E_TYPE, QPair<float, float>(0.0f, 1000.0f));
    m_thermoRangeMap.insert(THERMO_R_TYPE, QPair<float, float>(0.0f, 1700.0f));
    m_thermoRangeMap.insert(THERMO_S_TYPE, QPair<float, float>(0.0f, 1768.0f));
    m_thermoRangeMap.insert(THERMO_B_TYPE, QPair<float, float>(250.0f, 1800.0f));
    m_thermoRangeMap.insert(THERMO_N_TYPE, QPair<float, float>(0.0f, 1300.0f));
    m_thermoRangeMap.insert(THERMO_WRe5_WRe20, QPair<float, float>(0.0f, 2500.0f));
    m_thermoRangeMap.insert(THERMO_WRe5_WRe26, QPair<float, float>(0.0f, 2310.0f));
    m_thermoRangeMap.insert(THERMO_WRe3_WRe25, QPair<float, float>(0.0f, 2315.0f));
}

// 心跳包：读模块地址（40133）+ 环境温度（40400），双重验证在线状态
void DAM3130Modbus::sendHeartBeat()
{
    // 先读模块地址（保持寄存器，功能码03）
    sendReadRequest(REG_3130_MODULE_ADDR - 1, 1, QModbusDataUnit::HoldingRegisters);
    // 再读环境温度（保持寄存器，功能码03）
    sendReadRequest(REG_3130_ENV_TEMP - 1, 1, QModbusDataUnit::HoldingRegisters);
}

// 读热电偶温度（0-15通道，功能码03，保持寄存器）
void DAM3130Modbus::readAI(int ch)
{
    if (ch < 0 || ch > 15) {
        emit sigError(m_slaveAddr, "DAM-3130 通道错误：仅支持0-15路");
        return;
    }
    // 检查通道是否使能
    if (!(m_channelEnable & (1 << ch))) {
        emit sigError(m_slaveAddr, QString("DAM-3130 通道%1未使能，请先启用通道").arg(ch));
        return;
    }
    // 温度寄存器地址：REG_3130_TEMP_BASE-1 + ch（0起始转换）
    sendReadRequest(REG_3130_TEMP_BASE - 1 + ch, 1, QModbusDataUnit::HoldingRegisters);
}

// 写AO（DAM-3130无AO输出，返回错误）
void DAM3130Modbus::writeAO(int ch, quint16 value)
{
    Q_UNUSED(ch);
    Q_UNUSED(value);
    emit sigError(m_slaveAddr, "DAM-3130 无AO输出功能，不支持写AO操作");
}

// 设置热电偶类型（所有通道共用，参考手册表3）
void DAM3130Modbus::setThermoType(quint16 thermoCode)
{
    if (!m_thermoRangeMap.contains(thermoCode)) {
        emit sigError(m_slaveAddr, "DAM-3130 热电偶类型错误：不支持该类型");
        return;
    }
    sendWriteRequest(REG_3130_THERMO_TYPE - 1, QVector<quint16>() << thermoCode, QModbusDataUnit::HoldingRegisters);
    m_thermoType = thermoCode;
    // 输出热电偶类型描述
    QString thermoName = "";
    if (thermoCode == THERMO_K_TYPE) thermoName = "K型（-40~1300℃）";
    else if (thermoCode == THERMO_J_TYPE) thermoName = "J型（0~1200℃）";
    else if (thermoCode == THERMO_T_TYPE) thermoName = "T型（-200~400℃）";
    else thermoName = "自定义类型";
    qDebug() << "DAM3130Modbus: 设备" << m_slaveAddr << "热电偶类型设置为：" << thermoName;
}

// 设置码值转换方式（0=线性映射，1=温度直传）
void DAM3130Modbus::setConvertMode(quint16 mode)
{
    if (mode != CONVERT_LINEAR && mode != CONVERT_TEMP_DIRECT) {
        emit sigError(m_slaveAddr, "DAM-3130 转换方式错误：仅支持0（线性映射）或1（温度直传）");
        return;
    }
    sendWriteRequest(REG_3130_CONVERT_MODE - 1, QVector<quint16>() << mode, QModbusDataUnit::HoldingRegisters);
    m_convertMode = mode;
    qDebug() << "DAM3130Modbus: 设备" << m_slaveAddr << "码值转换方式设置为：" << (mode == CONVERT_LINEAR ? "线性映射" : "温度直传");
}

// 设置通道使能（Bit0-15对应0-15通道，1=使能，0=禁用）
void DAM3130Modbus::setChannelEnable(quint16 enableMask)
{
    sendWriteRequest(REG_3130_CHANNEL_ENABLE - 1, QVector<quint16>() << enableMask, QModbusDataUnit::HoldingRegisters);
    m_channelEnable = enableMask;
    qDebug() << "DAM3130Modbus: 设备" << m_slaveAddr << "通道使能掩码设置为：0x" << QString("%1").arg(enableMask, 4, 16, QChar('0'));
}

// 设置校准温度（-12.7~12.8℃，有符号字节）
void DAM3130Modbus::setCalibTemperature(int8_t calibValue)
{
    // 校准值范围：-127~128（对应-12.7~12.8℃）
    if (calibValue < -127 || calibValue > 128) {
        emit sigError(m_slaveAddr, "DAM-3130 校准温度错误：仅支持-12.7~12.8℃");
        return;
    }
    quint16 regValue = (0x00 << 8) | (static_cast<quint8>(calibValue) & 0xFF);  // Bit15-8=0
    sendWriteRequest(REG_3130_CALIB_TEMP - 1, QVector<quint16>() << regValue, QModbusDataUnit::HoldingRegisters);
    qDebug() << "DAM3130Modbus: 设备" << m_slaveAddr << "校准温度设置为：" << calibValue / 10.0f << "℃";
}

// 设置安全通信时间（单位0.1s，0=禁用，5~65535=有效）
void DAM3130Modbus::setSafeCommTime(quint16 time)
{
    if (time != 0 && time < 5) {
        emit sigError(m_slaveAddr, "DAM-3130 安全通信时间错误：需设置为0或5~65535（0.1s单位）");
        return;
    }
    sendWriteRequest(REG_3130_SAFE_COMM_TIME - 1, QVector<quint16>() << time, QModbusDataUnit::HoldingRegisters);
    qDebug() << "DAM3130Modbus: 设备" << m_slaveAddr << "安全通信时间设置为：" << time * 0.1f << "s";
}

// 读取16路断偶状态（功能码01，离散输入寄存器）
void DAM3130Modbus::readOpenCircuitStatus()
{
    sendReadRequest(REG_3130_OPEN_CIRCUIT - 1, 16, QModbusDataUnit::DiscreteInputs);
}

// 读取环境温度（冷端补偿温度，功能码03，保持寄存器）
void DAM3130Modbus::readEnvTemperature()
{
    sendReadRequest(REG_3130_ENV_TEMP - 1, 1, QModbusDataUnit::HoldingRegisters);
}

// 读取高精度环境温度（2个寄存器，小端模式，功能码03）
void DAM3130Modbus::readHighPrecisionEnvTemp()
{
    sendReadRequest(REG_3130_HIGH_ENV_TEMP - 1, 2, QModbusDataUnit::HoldingRegisters);
}

// 重启模块（写入1到重启寄存器）
void DAM3130Modbus::rebootModule()
{
    sendWriteRequest(REG_3130_REBOOT - 1, QVector<quint16>() << 0x0001, QModbusDataUnit::HoldingRegisters);
    qDebug() << "DAM3130Modbus: 设备" << m_slaveAddr << "发送重启指令";
}

// 心跳定时器超时：发送心跳包
void DAM3130Modbus::onHeartTimerTimeout()
{
    sendHeartBeat();
}

// 解析DAM-3130的Modbus回复（区分温度、断偶、环境温度、心跳）
void DAM3130Modbus::onModbusReplyFinished()
{
    QModbusReply *reply = qobject_cast<QModbusReply*>(sender());
    if (!reply) return;

    if (reply->error() != QModbusDevice::NoError) {
        updateDeviceStatus(Status_Offline);
        emit sigError(m_slaveAddr, "DAM-3130回复错误：" + reply->errorString());
        reply->deleteLater();
        return;
    }

    // 更新设备在线状态
    updateDeviceStatus(Status_Online);
    QModbusDataUnit unit = reply->result();
    int startReg = unit.startAddress();  // Qt Modbus 0起始地址
    QModbusDataUnit::RegisterType regType = unit.registerType();

    int count = unit.valueCount();
    if(unit.registerType() == QModbusDataUnit::InputRegisters && count == 16)
        {
        qDebug()<<"unit.registerType() == QModbusDataUnit::InputRegisters && count == 16";
            for(int i=0; i<16; i++){
                uint16_t raw = unit.value(i);
                float temp = (float)(1340*raw)/63356 - 40;  // 手册标准换算
                m_tempValues[i] = temp;         // 保存16路温度
            }
            reply->deleteLater();
            return;
        }

    // 解析心跳回复（模块地址：REG_3130_MODULE_ADDR-1=40132）
    if (startReg == REG_3130_MODULE_ADDR - 1 && regType == QModbusDataUnit::HoldingRegisters) {
        parseHeartBeatReply(reply);
    }
    // 解析温度采集值（温度基地址-1=40000，0-15路：40000-40015）
    else if (startReg >= REG_3130_TEMP_BASE - 1 && startReg <= REG_3130_TEMP_BASE - 1 + 15 && regType == QModbusDataUnit::HoldingRegisters) {
        int ch = startReg - (REG_3130_TEMP_BASE - 1);
        parseTempReply(reply,ch);
    }
    // 解析断偶状态（断偶基地址-1=0，0-15路：0-15）
    else if (startReg == REG_3130_OPEN_CIRCUIT - 1 && regType == QModbusDataUnit::DiscreteInputs) {
        parseOpenCircuitReply(reply);
    }
    // 解析环境温度（REG_3130_ENV_TEMP-1=40399）
    else if (startReg == REG_3130_ENV_TEMP - 1 && regType == QModbusDataUnit::HoldingRegisters) {
        parseEnvTempReply(reply, false);
    }
    // 解析高精度环境温度（REG_3130_HIGH_ENV_TEMP-1=45000，2个寄存器）
    else if (startReg == REG_3130_HIGH_ENV_TEMP - 1 && regType == QModbusDataUnit::HoldingRegisters && unit.valueCount() == 2) {
        parseEnvTempReply(reply, true);
    }

    reply->deleteLater();
}

// 解析温度采集值（支持线性映射/温度直传）
void DAM3130Modbus::parseTempReply(QModbusReply *reply, int ch)
{
    QModbusDataUnit unit = reply->result();
    quint16 rawValue = unit.value(0);
    float tempValue = 0.0f;
    QString tempStr = "";

    // 根据转换方式计算温度
    if (m_convertMode == CONVERT_LINEAR) {
        // 线性映射：温度 = 原始值/65535*(最高温-最低温) + 最低温
        if (m_thermoRangeMap.contains(m_thermoType)) {
            float minTemp = m_thermoRangeMap[m_thermoType].first;
            float maxTemp = m_thermoRangeMap[m_thermoType].second;
            tempValue = (rawValue / 65535.0f) * (maxTemp - minTemp) + minTemp;
        } else {
            tempValue = rawValue;
            tempStr = QString("未知类型原始值：%1").arg(rawValue);
        }
    } else if (m_convertMode == CONVERT_TEMP_DIRECT) {
        // 温度直传：温度 = 原始值 / 10.0f
        tempValue = rawValue / 10.0f;
    }

    // 格式化输出（保留2位小数）
    if (tempStr.isEmpty()) {
        tempStr = QString::asprintf("%.2f℃", tempValue);
    }

    // 发送温度数据给主线程
    emit sigDataReceived(m_slaveAddr, ch, "Thermo_Temp", rawValue);  // 原始值
    emit sigDataReceived(m_slaveAddr, ch, "Thermo_Temp_Eng", tempStr);  // 工程值
    qDebug() << "DAM3130Modbus: 设备" << m_slaveAddr << "通道" << ch << "温度：" << tempStr << "（原始值：" << rawValue << "）";
}

// 解析16路断偶状态（0=正常，1=断偶）
void DAM3130Modbus::parseOpenCircuitReply(QModbusReply *reply)
{
    QModbusDataUnit unit = reply->result();
    QString statusLog = QString("DAM-3130 设备%1 断偶状态：").arg(m_slaveAddr);
    for (int ch = 0; ch < 16; ch++) {
        quint16 status = unit.value(ch);
        statusLog += QString("通道%1：%2  ").arg(ch).arg(status == 0 ? "正常" : "断偶");
        // 发送单通道断偶状态给主线程
        emit sigDataReceived(m_slaveAddr, ch, "Open_Circuit_Status", status == 0 ? "正常" : "断偶");
    }
    qDebug() << statusLog;
    //ui->textEdit_Log->append(statusLog);  // 输出到日志（需确保ui可访问，或通过信号传递）
    emit sigLogReceived(m_slaveAddr, statusLog);
}

// 解析环境温度（普通精度/高精度）
void DAM3130Modbus::parseEnvTempReply(QModbusReply *reply, bool isHighPrecision)
{
    QModbusDataUnit unit = reply->result();
    float envTemp = 0.0f;
    QString tempType = isHighPrecision ? "高精度环境温度" : "环境温度（冷端补偿）";

    if (isHighPrecision) {
        // 高精度模式：2个寄存器，小端模式，拼接为32位浮点数（IEEE-754）
        quint16 lowWord = unit.value(0);
        quint16 highWord = unit.value(1);
        quint32 floatValue = (static_cast<quint32>(highWord) << 16) | lowWord;
        envTemp = *reinterpret_cast<float*>(&floatValue);
    } else {
        // 普通模式：环境温度 = (码值 - 400) / 10.0f（参考手册表5）
        quint16 rawValue = unit.value(0);
        envTemp = (rawValue - 400) / 10.0f;
    }

    QString tempStr = QString::asprintf("%.2f℃", envTemp);
    emit sigDataReceived(m_slaveAddr, -1, tempType, tempStr);
    qDebug() << "DAM3130Modbus: 设备" << m_slaveAddr << tempType << "：" << tempStr;
}

// 解析心跳回复（模块地址+环境温度验证）
void DAM3130Modbus::parseHeartBeatReply(QModbusReply *reply)
{
    QModbusDataUnit unit = reply->result();
    quint16 addr = unit.value(0) & 0x00FF;  // Bit15-8=0，Bit7-8为地址
    emit sigDataReceived(m_slaveAddr, -1, "HeartBeat_ModuleAddr", addr);
    qDebug() << "DAM3130Modbus: 设备" << m_slaveAddr << "心跳成功，模块地址：" << addr;
}

float DAM3130Modbus::getAIValue(int ch)
{
    if (ch < 1 || ch > 16)
        return 0.0f;
    //readAI_after(ch);
    return m_tempValues[ch - 1];
}

float DAM3130Modbus::readAI_after(int ch)
{
    if (ch < 1 || ch > 16)
        return 0.0f;

    int startReg = ch - 1;
    sendReadRequest(startReg, 1, QModbusDataUnit::InputRegisters);
    return m_tempValues[ch - 1];
}

void DAM3130Modbus::readAllAI()
{
    // 读 16 个通道（MODBUS 功能码 03）
    sendReadRequest(0, 16, QModbusDataUnit::InputRegisters);
}
