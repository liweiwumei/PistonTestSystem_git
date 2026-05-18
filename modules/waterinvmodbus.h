#ifndef WATERINVMODBUS_H
#define WATERINVMODBUS_H

#include "modbusbase.h"
#include <QModbusReply>

// PC20供水变频器（Water-INV模块）核心Modbus寄存器定义（参考手册第六章）
#define REG_PC20_COMM_SET_VAL    0x1000    // 通讯设定值（-10000~10000，对应-100.00%~100.00%最大频率）
#define REG_PC20_CONTROL_CMD    0x2000    // 控制命令（W）
#define REG_PC20_RUN_STATUS     0x3000    // 运行状态（R）
#define REG_PC20_FAULT_CODE     0x1008    // 故障代码（R）
#define REG_PC20_RUN_FREQ       0x1001    // 运行频率（R，0.01Hz）
#define REG_PC20_SET_FREQ       0x1002    // 设定频率（R，0.01Hz）
#define REG_PC20_BUS_VOLTAGE    0x1003    // 母线电压（R，0.1V）
#define REG_PC20_OUTPUT_VOLTAGE 0x1004    // 输出电压（R，1V）
#define REG_PC20_OUTPUT_CURRENT 0x1005    // 输出电流（R，0.01A）
#define REG_PC20_INVERTER_TEMP  0x1006    // 变频器温度（R，℃）
#define REG_PC20_DI_STATE       0x1007    // DI输入状态（R）
#define REG_PC20_AI1_VOLTAGE    0x1009    // AI1电压（R）
#define REG_PC20_DISPLAY_PRESS  0x100B    // 压力显示（R，0.1Bar）
#define REG_PC20_SET_PRESS      0x100C    // 设定压力（R，0.1Bar）
#define REG_PC20_FEEDBACK_PRESS 0x100D    // 反馈压力（R，0.1Bar）
#define REG_PC20_POWER_ON_HOUR  0x100E    // 上电时间（R，1H）
#define REG_PC20_RUN_HOUR       0x100F    // 运行时间（R，1H）
#define REG_PC20_TARGET_PRESS   0x0000    // 设定目标压力（W，0.1Bar）
#define REG_PC20_FAULT_DETAIL   0x8000    // 故障详细信息（R）
#define REG_PC20_COMM_FAULT     0x8001    // 通讯故障（R）

// 控制命令代码（参考手册表6.1）
#define CMD_FORWARD_RUN         0x0001    // 正转运行
#define CMD_REVERSE_RUN         0x0002    // 反转运行
#define CMD_FORWARD_JOG         0x0003    // 正转点动
#define CMD_REVERSE_JOG         0x0004    // 反转点动
#define CMD_FREE_STOP           0x0005    // 自由停机
#define CMD_DECEL_STOP          0x0006    // 减速停机
#define CMD_FAULT_RESET         0x0007    // 故障复位

// 运行状态代码（参考手册表6.1）
#define STATUS_FORWARD_RUN      0x0001    // 正转运行
#define STATUS_REVERSE_RUN      0x0002    // 反转运行
#define STATUS_STANDBY          0x0003    // 待机
#define STATUS_FAULT            0x0004    // 故障

// 核心功能参数寄存器（参考手册第四章，通过Modbus写保持寄存器配置）
#define REG_PC20_PRESS_SET      0x0000    // P0.00 压力设定（1.0~P0.21，0.1Bar）
#define REG_PC20_WAKE_PRESS_DEV 0x0001    // P0.01 唤醒压力偏差（0.0~P0.00，0.1Bar）
#define REG_PC20_RUN_DIR        0x0002    // P0.02 运行方向选择（0=一致，1=相反）
#define REG_PC20_SENSOR_RANGE   0x0003    // P0.03 传感器量程（1.0~200.0，0.1Bar）
#define REG_PC20_SENSOR_TYPE    0x0004    // P0.04 传感器反馈类型（0=4-20mA/24V，3=0.5-4.5V，4=0-5V）
#define REG_PC20_PID_MODE       0x0008    // P0.08 PID功能选择（0=关闭，1=休眠模式1，2=休眠模式2，3=休眠模式3）
#define REG_PC20_PID_SLEEP_DELAY 0x0009   // P0.09 PID休眠延时（0.0~100.0s）
#define REG_PC20_AUTO_START     0x000E    // P0.14 上电自动启动（0=关闭，1=开启）
#define REG_PC20_HIGH_PRESS_ALM 0x0015    // P0.21 高压报警设定（P0.00~P0.08，0.1Bar）
#define REG_PC20_LOW_PRESS_ALM  0x0017    // P0.23 低压报警设定（0.0~P0.00，0.1Bar）
#define REG_PC20_WATER_LACK_PROT 0x0019   // P0.25 缺水保护功能（0=关闭，1=频率+电流，2=频率+压力，3=三者结合）
#define REG_PC20_MAX_OUTPUT_FREQ 0x0105   // P1.05 最大输出频率（50.00~500.00Hz）
#define REG_PC20_UPPER_FREQ     0x0106    // P1.06 上限频率（下限~最大频率）
#define REG_PC20_LOWER_FREQ     0x0107    // P1.07 下限频率（0.00~上限频率）
#define REG_PC20_MOTOR_POWER    0x010C    // P1.12 电机功率选择（0=0.75kW，1=1.1kW，2=1.5kW，3=2.2kW）
#define REG_PC20_MOTOR_RATED_POW 0x010D    // P1.13 电机额定功率（0.1~2.2kW）
#define REG_PC20_MOTOR_RATED_FREQ 0x010E  // P1.14 电机额定频率（0~最大频率）
#define REG_PC20_MOTOR_RATED_VOLT 0x010F  // P1.15 电机额定电压（0~240V）
#define REG_PC20_MOTOR_RATED_CURR 0x0110  // P1.16 电机额定电流（1.00~10.00A）
#define REG_PC20_LOCAL_ADDR     0x0123    // P1.35 本机地址（1~6，0=广播）
#define REG_PC20_BAUDRATE        0x0124    // P1.36 波特率（1=9600bps）
#define REG_PC20_DATA_FORMAT    0x0125    // P1.37 数据格式（0=无校验8N1）
#define REG_PC20_APP_MACRO      0x002F    // P0.47 应用宏选择（0~15）

class WaterINVModbus : public ModbusBase
{
    Q_OBJECT
public:
    explicit WaterINVModbus(QObject *parent = nullptr);

    // 核心控制接口
    void controlInverter(quint16 cmd);    // 变频器启停/复位控制（命令码见宏定义）
    void setTargetPressure(float press);  // 设定目标压力（单位：Bar，0.1Bar精度）
    void setOutputFrequency(float freq);  // 设定输出频率（单位：Hz，0.01Hz精度）
    void resetFault();                    // 故障复位（等价于CMD_FAULT_RESET）

    // 参数配置接口（掉电保存，部分需停机修改）
    void setMotorPower(quint16 powerCode); // 设置电机功率（0=0.75kW~3=2.2kW）
    void setPIDMode(quint16 mode);        // 设置PID功能模式（0=关闭，1-3=休眠模式）
    void setHighPressureAlarm(float press); // 设置高压报警值（0.1Bar精度）
    void setLowPressureAlarm(float press);  // 设置低压报警值（0.1Bar精度）
    void enableAutoStart(bool enable);     // 使能上电自动启动（0=关闭，1=开启）
    void setWaterLackProtection(quint16 mode); // 设置缺水保护模式（0-3）

    // 状态读取接口
    void readRunStatus();                 // 读取运行状态（运行/待机/故障）
    void readFaultCode();                 // 读取故障代码
    void readPressureInfo();              // 读取压力信息（设定/反馈/显示）
    void readElectricalParams();          // 读取电气参数（电压/电流/频率）
    void readRunningHours();              // 读取运行时间/上电时间
     void writeAO(int ch, quint16 value) override;

protected:
    // 实现基类纯虚接口：心跳包（读运行状态+反馈压力，双重验证）
    void sendHeartBeat() override;
    // 实现基类纯虚接口：读AI（映射为读反馈压力）
    void readAI(int ch) override;
    // 实现基类纯虚接口：写AO（映射为设定目标压力）


private slots:
    // 实现基类纯虚槽：心跳定时器超时
    void onHeartTimerTimeout() override;
    // 重写回复解析：适配变频器状态、压力、电气参数等数据
    void onModbusReplyFinished() override;

    // 解析控制命令回复（启停/复位结果）
    void parseControlReply(QModbusReply *reply, quint16 cmd);
    // 解析运行状态回复
    void parseRunStatusReply(QModbusReply *reply);
    // 解析压力信息回复
    void parsePressureReply(QModbusReply *reply);
    // 解析电气参数回复
    void parseElectricalReply(QModbusReply *reply);
    // 解析故障代码回复
    void parseFaultReply(QModbusReply *reply);

private:
    float m_targetPressure;       // 目标压力（单位：Bar）
    float m_setFrequency;         // 设定频率（单位：Hz）
    quint16 m_runStatus;          // 当前运行状态（参考STATUS_xxx宏）
    quint16 m_lastFaultCode;      // 上次故障代码（0=无故障）
    quint16 m_motorPowerCode;     // 电机功率代码（默认3=2.2kW）
};

#endif // WATERINVMODBUS_H
