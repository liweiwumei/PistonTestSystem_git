#ifndef SSRDIOMODBUS_H
#define SSRDIOMODBUS_H

#include "modbusbase.h"
#include <QModbusReply>

// SSR数字量I/O模块核心寄存器定义（参考手册表2.1~2.4）
#define REG_SSR_DIO_COIL_BASE       0x0000    // 线圈寄存器基地址（DO控制：0000H-002FH，对应PLC地址00001-00048，功能码05/0F）
#define REG_SSR_DIO_DISCRETE_BASE   0x0000    // 离散输入寄存器基地址（DI状态：0000H-002FH，对应PLC地址10001-10048，功能码02）
#define REG_SSR_DIO_INPUT_BASE      0x0000    // 输入寄存器基地址（DI状态：0000H-0034H，对应PLC地址30001-30053，功能码04）
#define REG_SSR_DIO_HOLD_CTRL_BASE  0x0000    // 保持寄存器-通道控制基地址（40001-40048，功能码06/10）
#define REG_SSR_DIO_HOLD_COMM_DETECT 0x0030   // 通讯检测时间（40049，功能码06）
#define REG_SSR_DIO_HOLD_UPLOAD_CTRL 0x0031   // 输入状态主动上传控制（40050，功能码06）
#define REG_SSR_DIO_HOLD_ADDR       0x0032    // 模块地址（40051，功能码06）
#define REG_SSR_DIO_HOLD_BAUDRATE   0x0033    // 波特率设置（40052，功能码06）
#define REG_SSR_DIO_HOLD_BATCH_CTRL 0x0034    // 批量控制（40053，功能码06：0=全关，1=全开）
#define REG_SSR_DIO_HOLD_BIT_CTRL1  0x0035    // 按位控制通道1~16（40054，功能码06）
#define REG_SSR_DIO_HOLD_BIT_CTRL2  0x0036    // 按位控制通道17~32（40055，功能码06）
#define REG_SSR_DIO_HOLD_BIT_CTRL3  0x0037    // 按位控制通道33~48（40056，功能码06）
#define REG_SSR_DIO_HOLD_PARITY     0x003D    // 奇偶校验设置（40062，功能码06）
#define REG_SSR_DIO_HOLD_MODE_BASE  0x0096    // 通道控制模式基地址（40151-40198，功能码06）

// 波特率配置代码（参考手册表2.4）
#define SSR_BAUDRATE_4800           0x0000
#define SSR_BAUDRATE_9600           0x0001    // 出厂默认
#define SSR_BAUDRATE_14400          0x0002
#define SSR_BAUDRATE_19200          0x0003
#define SSR_BAUDRATE_38400          0x0004
#define SSR_BAUDRATE_56000          0x0005
#define SSR_BAUDRATE_57600          0x0006
#define SSR_BAUDRATE_115200         0x0007

// 奇偶校验配置代码（参考手册表2.4）
#define SSR_PARITY_NONE             0x0000    // 出厂默认
#define SSR_PARITY_ODD              0x0001    // 奇校验
#define SSR_PARITY_EVEN             0x0002    // 偶校验

// 通道控制模式代码（参考手册表2.4）
#define SSR_CTRL_MODE_NORMAL        0x0000    // 普通模式（出厂默认）
#define SSR_CTRL_MODE_LINKAGE       0x0001    // 联动模式（输入控制输出）
#define SSR_CTRL_MODE_TOGGLE        0x0002    // 点动模式（翻转）
#define SSR_CTRL_MODE_CYCLE         0x0003    // 开关循环模式
#define SSR_CTRL_MODE_AUTO_RESET    0x0004    // 自动复位模式

// 最大通道数（参考手册1.3节）
#define SSR_MAX_DI_DO_CHANNEL       48        // 最大48路DI/DO

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
#define CH_OXYGEN_STATION1         12  // 工位1和3的点火（天然气通道）
#define CH_GAS_STATION1            13  // 工位2和4的点火
#define CH_IGNITION_STATION1_3     14  // 工位1和3的点火（天然气通道）
#define CH_IGNITION_STATION2_4     15  // 工位2和4的点火
#define CH_WATER_COOL_FAN          26  // 给水降温的风扇开关

class SSRDioModbus : public ModbusBase
{
    Q_OBJECT
public:
    explicit SSRDioModbus(QObject *parent = nullptr);

    // 数字量输入（DI）相关接口
    void readDIStatus(int ch);             // 读取单个DI通道状态（功能码02/04）
    void readAllDIStatus();                // 读取所有DI通道状态（功能码02/04）
    void enableDIUpload(bool enable, quint16 interval = 100); // 使能DI主动上传（interval单位：0.01s）
    bool getDIStatus(int ch);

    // 数字量输出（DO）相关接口
    void writeDOStatus(int ch, bool on);   // 控制单个DO通道（功能码05/06）
    void writeBatchDOStatus(bool on);       // 批量控制所有DO通道（全开/全关，功能码06）
    void writeMultiDOStatus(const QVector<int> &chs, bool on); // 控制多个DO通道（功能码0F/10）
    void setDOControlMode(int ch, quint16 mode); // 设置DO通道控制模式（功能码06）
    void setDOBitCtrl(const QVector<int> &chs, bool on); // 按位控制DO通道（1~16/17~32/33~48，功能码06）

    // 模块参数配置接口
    void setCommDetectTime(quint16 time);  // 设置通讯检测时间（单位0.1s，功能码06）
    void setBaudrate(quint16 baudCode);    // 设置波特率（功能码06）
    void setParity(quint16 parityCode);    // 设置奇偶校验（功能码06）

//    void openOxygenValve();    // 打开氧气阀门（自动开启总回水）
//    void closeOxygenValve();   // 关闭氧气阀门
//    void openGasValve();       // 打开天然气阀门
//    void closeGasValve();      // 关闭天然气阀门
//    void openWaterCoolFan();   // 打开水冷却风扇
//    void closeWaterCoolFan();  // 关闭水冷却风扇
    //bool readDOStatus(int channel);

protected:
    // 实现基类纯虚接口：心跳包（读模块地址+通讯状态，功能码03）
    void sendHeartBeat() override;
    // 实现基类纯虚接口：读AI（此处映射为读DI状态）
    void readAI(int ch) override;
    // 实现基类纯虚接口：写AO（此处映射为写DO状态）
    void writeAO(int ch, quint16 value) override;

private slots:
    // 实现基类纯虚槽：心跳定时器超时
    void onHeartTimerTimeout() override;
    // 重写回复解析：适配DI/DO状态、模块参数等数据
    void onModbusReplyFinished() override;

    // 解析DI状态回复（功能码02/04）
    void parseDIReply(QModbusReply *reply, int ch = -1);
    // 解析DO控制回复（功能码05/06/0F/10）
    void parseDOReply(QModbusReply *reply, int ch = -1);
    // 解析心跳回复（模块地址+通讯检测时间）
    void parseHeartBeatReply(QModbusReply *reply);

private:
    quint16 m_doControlMode[SSR_MAX_DI_DO_CHANNEL]; // 缓存各DO通道控制模式（默认普通模式）
    quint16 m_commDetectTime;                  // 通讯检测时间（默认0=不检测）
    quint16 m_uploadInterval;                  // DI主动上传间隔（默认0=不主动上传）
    bool m_diStatus[16] = {false};  // 加这一行
};

#endif // SSRDIOMODBUS_H
