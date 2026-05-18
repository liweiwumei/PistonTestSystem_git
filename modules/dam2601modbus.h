#ifndef DAM2601MMODBUS_H
#define DAM2601MMODBUS_H

#include "modbusbase.h"
#include <QModbusReply>
#include <QTimer>
#include <QMutex>

// DAM-2601M 寄存器定义（参考手册）
#define REG_MODULE_ADDR    48213    // 模块地址寄存器（读写）
#define REG_AI_BASE        40001    // AI输入基地址（功能码04，8路：40001-40008）
#define REG_AO_BASE        41025    // AO输出基地址（功能码06，8路：41025-41031）
#define REG_AI_RANGE_BASE  40066    // AI量程基地址（40066-40073）
#define REG_AO_RANGE_BASE  41058    // AO量程基地址（41058-41065）

// 量程代码（参考手册表5）
#define RANGE_0_10V        0x000E
#define RANGE_0_20mA       0x000B

class DAM2601MModbus : public ModbusBase
{
    Q_OBJECT
public:
    explicit DAM2601MModbus(QObject *parent = nullptr);


//    bool connectDevice() override;
//    void disconnectDevice() override;
//    bool isConnected() override;

    // 扩展DAM-2601M专属接口
    void setAIRange(int ch, quint16 rangeCode);  // 设置AI量程
    void setAORange(int ch, quint16 rangeCode);  // 设置AO量程

    void readAI_ALL();
    float m_flowValues[8] = {0.0f};
    float m_flow_raw_Values[8] = {0.0f};
    float getAIValue(int ch);
    float getAIRawValue(int ch);
    void writeAO(int ch, quint16 value) override;

protected:
    // 实现基类纯虚接口：心跳包（读模块地址）
    void sendHeartBeat() override;
    // 实现基类纯虚接口：读AI、写AO
    void readAI(int ch) override;


private slots:
    // 实现基类纯虚槽：心跳定时器超时
    void onHeartTimerTimeout() override;
    // 解析DAM-2601M的Modbus回复（重写通用处理，增加协议解析）
    void onModbusReplyFinished() override;  // 补充参数

    // 解析AI采集值
    void parseAIReply(QModbusReply *reply, int ch);
    // 解析模块地址（心跳回复）
    void parseHeartBeatReply(QModbusReply *reply);
};

#endif // DAM2601MMODBUS_H
