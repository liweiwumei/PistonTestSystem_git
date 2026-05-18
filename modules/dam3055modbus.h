#ifndef DAM3055MMODBUS_H
#define DAM3055MMODBUS_H

#include "modbusbase.h"
#include <QModbusReply>

// DAM-3055M 核心寄存器定义（参考手册表6）
#define REG_3055_AI_BASE        40001    // AI输入基地址（功能码04，16路：40001-40016）
#define REG_3055_MODULE_ADDR    40133    // 模块地址寄存器（读写）
#define REG_3055_BAUDRATE       40134    // 波特率寄存器（读写）
#define REG_3055_AI_RANGE_BASE  40137    // AI量程基地址（40137-40152：对应0-15通道）
#define REG_3055_CONVERT_ENABLE 45101    // 换算使能寄存器（读写）
#define REG_3055_DATA_TYPE      45102    // 数据类型寄存器（读写）
#define REG_3055_WATCHDOG       40515    // 看门狗定时寄存器（读写，单位ms）

// DAM-3055M 量程代码（参考手册表4）
#define RANGE_3055_0_5V         0x000D   // 0~5V 电压量程
#define RANGE_3055_1_5V         0x0082   // 1~5V 电压量程
#define RANGE_3055_0_20mA       0x000B   // 0~20mA 电流量程
#define RANGE_3055_4_20mA       0x000C   // 4~20mA 电流量程（默认量程）

// 数据类型代码（参考手册表5）
#define DATA_TYPE_UINT          0x0000   // 无符号整形
#define DATA_TYPE_INT           0x0001   // 有符号整形
#define DATA_TYPE_ULONG         0x0002   // 无符号长整形
#define DATA_TYPE_LONG          0x0003   // 有符号长整形
#define DATA_TYPE_FLOAT         0x0004   // 浮点数（IEEE-754）

#define CH_AIR_PRESS     1   // 工位2的水冷开关
#define CH_WATER_PRESS     2   // 工位1的水冷开关（氧气通道）
#define CH_WATER_TEM     3   // 工位4的水冷开关
#define CH_GAS_PRESS     4   // 工位3的水冷开关
#define CH_GAS_TEM      5   // 总回水（必须打开，工位1/2/3/4的水冷才起作用）
#define CH_OXYGEN_PRESS       6   // 工位2的吹空气冷却开关
#define CH_OXYGEN_TEM       7   // 工位4的吹空气冷却开关
#define CH_MOTOR_DISTANCE       7   // 工位4的吹空气冷却开关


class DAM3055MModbus : public ModbusBase
{
    Q_OBJECT
public:
    explicit DAM3055MModbus(QObject *parent = nullptr);

    // DAM-3055M 专属接口
    void setAIRange(int ch, quint16 rangeCode);  // 设置AI通道量程（0-15通道）
    void setWatchdogTimeout(quint16 timeoutMs);  // 设置看门狗超时时间（0=禁用，5-65535ms）
    void enableConvert(bool enable);             // 使能/禁用数据换算功能
    void setDataType(quint16 dataType);          // 设置传输数据类型
    QStringList pipe_meaning = {
        "press",
        "press",
        "tem",
        "press",
        "tem",
        "press",
        "tem",
        "distance"
    };

protected:
    // 实现基类纯虚接口：心跳包（读模块地址，寄存器40133）
    void sendHeartBeat() override;
    // 实现基类纯虚接口：读AI（16路差分输入，0-15通道）
    void readAI(int ch) override;
    // 实现基类纯虚接口：写AO（DAM-3055M无AO输出，留空并报错）
    void writeAO(int ch, quint16 value) override;

private slots:
    // 实现基类纯虚槽：心跳定时器超时
    void onHeartTimerTimeout() override;
    // 重写回复解析：适配DAM-3055M的AI数据、心跳数据
    void onModbusReplyFinished() override;

    // 解析AI采集值（支持不同量程转换为工程值）
    void parseAIReply(QModbusReply *reply, int ch);
    // 解析心跳回复（模块地址验证）
    void parseHeartBeatReply(QModbusReply *reply);

private:
    quint16 m_aiRange[16];  // 缓存16路AI通道当前量程（默认4~20mA）
};

#endif // DAM3055MMODBUS_H
