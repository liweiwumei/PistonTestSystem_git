#ifndef DAM3130MODBUS_H
#define DAM3130MODBUS_H

#include "modbusbase.h"
#include <QModbusReply>

// DAM-3130 核心寄存器定义（参考手册表4/表5）
#define REG_3130_TEMP_BASE      40001    // 温度采集基地址（16路：40001-40016，功能码03）
#define REG_3130_OPEN_CIRCUIT   00001    // 断偶状态基地址（16路：00001-00016，功能码01）
#define REG_3130_MODULE_ADDR    40133    // 模块地址寄存器（读写）
#define REG_3130_BAUDRATE       40134    // 波特率寄存器（读写）
#define REG_3130_PARITY         40135    // 奇偶校验寄存器（读写）
#define REG_3130_CONVERT_MODE   40136    // 码值转换方式（读写：0=线性映射，1=温度直传）
#define REG_3130_THERMO_TYPE    40201    // 热电偶类型寄存器（读写，所有通道共用）
#define REG_3130_CHANNEL_ENABLE 40221    // 通道使能寄存器（读写：Bit0-15对应0-15通道）
#define REG_3130_CALIB_TEMP     40288    // 校准温度寄存器（读写）
#define REG_3130_ENV_TEMP       40400    // 环境温度寄存器（读写：冷端补偿温度）
#define REG_3130_REBOOT         40519    // 模块重启寄存器（读写：1=重启）
#define REG_3130_SAFE_COMM_TIME 40577    // 安全通信时间（读写：单位0.1s，0=禁用）
#define REG_3130_HIGH_ENV_TEMP  45001    // 高精度环境温度（只读，2个寄存器，小端模式）

// 热电偶类型代码（参考手册表3）
#define THERMO_K_TYPE           0x0070   // K型（-40~1300℃）
#define THERMO_J_TYPE           0x0010   // J型（0~1200℃）
#define THERMO_T_TYPE           0x0012   // T型（-200~400℃）
#define THERMO_E_TYPE           0x0013   // E型（0~1000℃）
#define THERMO_R_TYPE           0x0014   // R型（0~1700℃）
#define THERMO_S_TYPE           0x0015   // S型（0~1768℃）
#define THERMO_B_TYPE           0x0071   // B型（250~1800℃）
#define THERMO_N_TYPE           0x0017   // N型（0~1300℃）
#define THERMO_WRe5_WRe20       0x001B   // WRe5-WRe20（0~2500℃）
#define THERMO_WRe5_WRe26       0x0019   // WRe5-WRe26（0~2310℃）
#define THERMO_WRe3_WRe25       0x001A   // WRe3-WRe25（0~2315℃）

// 码值转换方式
#define CONVERT_LINEAR          0x0000   // 线性映射（默认）
#define CONVERT_TEMP_DIRECT     0x0001   // 温度直传（值=温度×10）

// 奇偶校验代码（参考手册表5）
#define PARITY_NONE             0x0000   // 无校验（默认）
#define PARITY_EVEN             0x0001   // 偶校验
#define PARITY_ODD              0x0002   // 奇校验

class DAM3130Modbus : public ModbusBase
{
    Q_OBJECT
public:
    explicit DAM3130Modbus(QObject *parent = nullptr);

    // DAM-3130 专属接口
    void setThermoType(quint16 thermoCode);       // 设置热电偶类型（所有通道共用）
    void setConvertMode(quint16 mode);            // 设置码值转换方式
    void setChannelEnable(quint16 enableMask);    // 设置通道使能（Bit0-15对应0-15通道）
    void setCalibTemperature(int8_t calibValue);   // 设置校准温度（-12.7~12.8℃）
    void setSafeCommTime(quint16 time);           // 设置安全通信时间（0.1s单位，0=禁用）
    void readOpenCircuitStatus();                 // 读取16路断偶状态（0=正常，1=断偶）
    void readEnvTemperature();                    // 读取环境温度（冷端补偿温度）
    void readHighPrecisionEnvTemp();              // 读取高精度环境温度（V6.03+支持）
    void rebootModule();                          // 重启模块

    float getAIValue(int ch);
    void readAllAI();
    float m_tempValues[16] = {0.0f};

protected:
    // 实现基类纯虚接口：心跳包（读模块地址+环境温度，双重验证）
    void sendHeartBeat() override;
    // 实现基类纯虚接口：读AI（此处为读热电偶温度，0-15通道）
    void readAI(int ch) override;
    // 实现基类纯虚接口：写AO（DAM-3130无AO输出，返回错误）
    void writeAO(int ch, quint16 value) override;
    float readAI_after(int ch);



private slots:
    // 实现基类纯虚槽：心跳定时器超时
    void onHeartTimerTimeout() override;
    // 重写回复解析：适配温度、断偶状态、环境温度等数据
    void onModbusReplyFinished() override;

    // 解析温度采集值（支持线性映射/温度直传两种模式）
    void parseTempReply(QModbusReply *reply, int ch);
    //void parseTempReply(QModbusReply *reply);
    // 解析断偶状态（16路通道）
    void parseOpenCircuitReply(QModbusReply *reply);
    // 解析环境温度（含高精度模式）
    void parseEnvTempReply(QModbusReply *reply, bool isHighPrecision = false);
    // 解析心跳回复（模块地址+环境温度双重验证）
    void parseHeartBeatReply(QModbusReply *reply);

private:
    quint16 m_thermoType;       // 当前热电偶类型（默认K型）
    quint16 m_convertMode;      // 当前码值转换方式（默认线性映射）
    quint16 m_channelEnable;    // 通道使能掩码（默认0xFFFF=全使能）
    // 热电偶类型-温度范围映射（用于线性映射计算）
    QMap<quint16, QPair<float, float>> m_thermoRangeMap;



signals:
    // 新增日志信号：参数1=从站地址，参数2=日志内容
    void sigLogReceived(int slaveAddr, const QString &log);
};

#endif // DAM3130MODBUS_H
