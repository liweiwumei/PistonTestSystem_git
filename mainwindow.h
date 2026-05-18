#ifndef MAINWINDOW_H
#define MAINWINDOW_H
#include <QMainWindow>
#include <QPushButton>
#include <QLabel>
#include <QLineEdit>
#include <QTableWidget>
#include <QStackedWidget>
#include <QGroupBox>
#include <QCheckBox>
#include <QTimer>
#include <QVector>
#include "modules/dam2601modbus.h"
#include "modules/dam3055modbus.h"
#include "modules/dam3130modbus.h"
#include "modules/ssrdiomodbus.h"
#include "modules/waterinvmodbus.h"
#include "plccontroller.h"
#include "qcustomplot.h"

struct ModuleStatus
{
    QString name;
    bool connected;
    QLabel *statusLabel;
    int failCount;
};

struct FireBurnerParam
{
    bool enabled = false;
    int gasTarget = 0;       // 天然气目标开度
    int gasRampUp = 0;      // 渐开时间 ms
    int gasRampDown = 0;    // 渐关时间 ms
    int oxyTarget = 0;       // 氧气目标开度
    int oxyRampUp = 0;      // 渐开时间
    int oxyRampDown = 0;    // 渐关时间
    int oxyDelay = 0;       // 氧气延迟开启时间
    int igniteT1 = 0;       // 点火开始延迟
    int igniteDur = 0;      // 点火持续时间
};
Q_DECLARE_METATYPE(FireBurnerParam)

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override = default;

private:
    QDialog* moduleStatusDialog; // ✅ 在这里添加
     void setupModuleStatusDialog();
     void initModuleStatusList();
    QStackedWidget *stackedWidget;


     QPushButton *btnOneKeyConnect;

     QPushButton *btnDisconnect;

     QPushButton *btnMotorOrigin;

     QPushButton *btnOpenGas;

     QPushButton *btnOpenOxygen;

     QPushButton *btnClearWarning;

     QPushButton *btnFireParams;

     QPushButton *btnCoolingParams;


     QPushButton *btnCheckRecord;

     QPushButton *btnHelp;

     QPushButton *btnExit;

     QPushButton *btnDeviceStatusTest;
     QPushButton *btnMotorPosition;
     QPushButton *btnHeatTransfer;
     QPushButton *btnThermalFatigue;



    QWidget *mainPage;
    QWidget *moduleStatusPage;
    QWidget *fireParamsPage;
    QWidget *testPage;
    QWidget *coolingParamsPage;
    QWidget *recordPage;

    DAM2601MModbus *dam2601;
    DAM3055MModbus *dam3055;
    DAM3130Modbus *dam3130[3];
    SSRDioModbus *ssrdiomodbus;
    WaterINVModbus *waterinvmodbus;

    QVector<ModuleStatus> modulesStatus;
    QTimer *heartbeatTimer;

    void setupMainPage();
    QDialog* setupModuleStatusPage();
    void setupFireParamsPage();
    void setupTestPage();
    void setupCoolingParamsPage();
    void setupRecordPage();
    void setupConnections();

    /************这个是设备状态测试界面的***start***************/
    // 设备状态测试对话框
       QDialog* m_deviceTestDialog = nullptr;
       // 存储所有UI控件指针，方便后续更新
       QVector<QLabel*> m_thermoLabels;   // 热电偶温度标签
       QVector<QLabel*> m_pressureLabels;  // 压力采集标签
       QVector<QLabel*> m_tempLabels;      // 温度采集标签
       QVector<QLabel*> m_flowLabels;      // 流量采集标签
       QVector<QSlider*> m_flowSliders;    // 流量控制滑块
       QVector<QPushButton*> m_relayBtns;  // 继电器按钮

       // 界面初始化函数
       void setupDeviceTestDialog();
       // 数据更新函数（定时调用）
       void updateDeviceTestData();
       // 定时器，用于刷新数据
       QTimer* m_deviceTestTimer = nullptr;
    /************这个是设备状态测试界面的***end***************/


   /************这个是电机设置界面***start***************/
   // 设备状态测试对话框
       QDialog *motorPosDialog;

       // 动态显示
       QLabel *labelPiston;       // 活塞图标
       QLabel *labelArrow;        // 旋转箭头
       QTimer *arrowRotateTimer;  // 箭头旋转定时器
       //arrowRotateTimer
       QTimer *pistonUpdateTimer; // 活塞位置刷新定时器
       int currentRotateAngle;    // 旋转角度

       // 距离显示
       QLabel *labelLeftDistValue;
       QLabel *labelRightDistValue;

       // PLC 控制按钮
       QPushButton *btnMotorJogPlus;
       QPushButton *btnMotorJogMinus;
       QPushButton *btnMotorPosSet;
       QPushButton *btnMotorStop;
       QPushButton *btnMotorClose;

       // 参数输入
       QLineEdit *editPosSpeed;
       QLineEdit *editPosTarget;
   /************这个是电机设置界面***end***************/

/************这个是点火参数界面***start***************/
       QDialog* m_fireParamsDialog = nullptr;
       QTimer* m_fireCurveTimer = nullptr;       // 温度曲线刷新定时器
       QCustomPlot* m_plot1 = nullptr;           // 工位1温度曲线
       QCustomPlot* m_plot3 = nullptr;           // 工位3温度曲线
       QList<QLabel*> m_labTemp1;
       QList<QLabel*> m_labTemp3;

       QLineEdit *editGas1Target;
       QLineEdit *editGas1Up;
       QLineEdit *editGas1Down;
       QLineEdit *editOxy1Target;
       QLineEdit *editOxy1Up;
       QLineEdit *editOxy1Down;
       QLineEdit *editOxy1Delay;
       QLineEdit *editIgn1T1;
       QLineEdit *editIgn1Dur;

       QLineEdit *editGas2Target;
       QLineEdit *editGas2Up;
       QLineEdit *editGas2Down;
       QLineEdit *editOxy2Target;
       QLineEdit *editOxy2Up;
       QLineEdit *editOxy2Down;
       QLineEdit *editOxy2Delay;
       QLineEdit *editIgn2T1;
       QLineEdit *editIgn2Dur;

       QLineEdit *editKeepTime;
/************这个是点火参数界面***end***************/


    PlcController *m_plc;
    bool m_plcHomeOk;
    QDialog *m_motorPositionDialog;

private slots:
    void onOneKeyConnect();
    void onDisconnect();
    void onMotorOrigin();
    void onOpenGas();
    void onOpenOxygen();
    void onFireParams();
    void onCoolingParams();
    void onDeviceStatus();
    void onMotorControl();
    void onHelp();
    void onExit();
    void checkHeartbeat();

    void onDeviceStatusTest();
    void onMotorPosition();
    void onHeatTransfer();
    void onThermalFatigue();
};
#endif // MAINWINDOW_H
