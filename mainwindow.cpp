
#include "mainwindow.h"
#include "utils.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QMessageBox>
#include <QCheckBox>
#include <QDebug>
#include <QDialog>
#include <QFormLayout>
#include <QTimer>
#include <QTransform>
#include <QPixmap>
#include <QFrame>
#include <QStackedLayout>
#include <QPainter>
//#include <QCustomPlot>
#include <QFile>
#include <QFileDialog>
#include <QSettings>
#include <QDateTime>
#include "ignitionplotwidget.h"
#include "subcurve.h"

// 补充 ModuleStatus 结构体定义（如果头文件未定义）

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , heartbeatTimer(nullptr)
       , stackedWidget(nullptr)
       , mainPage(nullptr)
       , fireParamsPage(nullptr)
       , testPage(nullptr)
       , coolingParamsPage(nullptr)
       , recordPage(nullptr)
{
    // 1. 创建模块（安全初始化）
    moduleStatusDialog = nullptr;
    dam2601 = new DAM2601MModbus(this);
    dam3055 = new DAM3055MModbus(this);
    for(int i=0;i<3;i++) dam3130[i] = new DAM3130Modbus(this);
    ssrdiomodbus = new SSRDioModbus(this);
    waterinvmodbus = new WaterINVModbus(this);

    // 2. 主堆栈窗口
    stackedWidget = new QStackedWidget(this);
    setCentralWidget(stackedWidget);

    // 3. 初始化所有界面（先初始化主界面，再初始化模块状态对话框）
    setupMainPage();
    setupModuleStatusDialog();  // 改为初始化对话框
    setupFireParamsPage();
    setupTestPage();
    setupCoolingParamsPage();
    setupRecordPage();
    setupDeviceTestDialog();  // 新增：必须调用此函数创建对话框

    // 4. 加入堆栈（仅加入普通页面，对话框不加入堆栈）
    stackedWidget->addWidget(mainPage);
    //stackedWidget->addWidget(fireParamsPage);
    stackedWidget->addWidget(testPage);
    stackedWidget->addWidget(coolingParamsPage);
    stackedWidget->addWidget(recordPage);
    stackedWidget->setCurrentWidget(mainPage);

    // 5. 信号槽绑定
    setupConnections();

    // 6. 初始化模块状态列表（仅初始化一次）
    initModuleStatusList();

    // 7. 心跳定时器（安全启动：先绑定再启动）
    heartbeatTimer = new QTimer(this);
    heartbeatTimer->setInterval(1000);
    connect(heartbeatTimer, &QTimer::timeout, this, &MainWindow::checkHeartbeat, Qt::UniqueConnection);
    heartbeatTimer->start();

    m_plc = new PlcController(this);
    m_plcHomeOk = false;
    m_motorPositionDialog = nullptr;

    connect(m_plc, &PlcController::sigConnected, this, [=]() {
        btnMotorOrigin->setEnabled(true);  // 启用按钮
    });

    // ==============================================
    // 你 PLC 真实信号：disconnected → 断开连接
    // ==============================================
    connect(m_plc, &PlcController::sigDisconnected, this, [=]() {
        btnMotorOrigin->setEnabled(false); // 禁用按钮
        QMessageBox::warning(this, "通讯异常", "PLC 已断开连接！");
    });




       // PLC 状态接入心跳
       connect(m_plc, &PlcController::sigHeartbeatChanged, this, [this](bool ok){
           if (modulesStatus.size() > 7) {
               modulesStatus[7].connected = ok;
               modulesStatus[7].statusLabel->setStyleSheet(
                   ok ? "background:green;border-radius:8px;"
                      : "background:red;border-radius:8px;");
           }
       });

    this->resize(1200, 700);
}

// 初始化模块状态列表
void MainWindow::initModuleStatusList()
{
    modulesStatus.clear();
    QStringList names = {
        "继电器模块",
        "电流信号输出模块",
        "热电偶采集模块2",
        "冷却水泵",
        "电流信号采集模块",
        "热电偶采集模块1",
        "热电偶采集模块3",
        "PLC"
    };

    for(int i=0;i<names.size();i++){
        ModuleStatus s;
        s.name = names[i];
        s.connected = false;
        s.statusLabel = nullptr;
        s.failCount = 0;
        modulesStatus.append(s);
    }

    // 绑定状态标签（从对话框中查找）
    for(int i=0;i<modulesStatus.size();i++){
        modulesStatus[i].statusLabel = qobject_cast<QLabel*>(
            moduleStatusDialog->findChild<QLabel*>(QString("status_%1").arg(i))
        );
    }
}

// ===================== 模块状态对话框（模态） =====================
void MainWindow::setupModuleStatusDialog()
{
    // 创建模态对话框（替代原有的Widget）
    moduleStatusDialog = new QDialog(this);
    moduleStatusDialog->setWindowTitle("模块通讯状态");
    moduleStatusDialog->setFixedSize(800, 500);
    moduleStatusDialog->setModal(true);  // 模态对话框

    // 对话框主布局
    QVBoxLayout *mainLayout = new QVBoxLayout(moduleStatusDialog);
    mainLayout->setContentsMargins(15, 15, 15, 15);
    mainLayout->setSpacing(0);

    // 1. 左上角标题“模块通讯”
    QLabel *titleLabel = new QLabel("模块通讯", moduleStatusDialog);
    titleLabel->setStyleSheet(R"(
        QLabel {
            border:1px solid black;
            border-bottom: none;
            border-right: none;
            padding: 10px 30px;
            font-size:18px;
            font-weight:bold;
            background-color:white;
        }
    )");
    titleLabel->setAlignment(Qt::AlignCenter);
    mainLayout->addWidget(titleLabel, 0, Qt::AlignLeft);

    // 2. 4列网格布局（严格对应比例）
    QGridLayout *gridLayout = new QGridLayout();
    gridLayout->setSpacing(15);
    gridLayout->setContentsMargins(15, 0, 15, 15);

    // 列宽比例
    gridLayout->setColumnStretch(0, 3);
    gridLayout->setColumnStretch(1, 1);
    gridLayout->setColumnStretch(2, 3);
    gridLayout->setColumnStretch(3, 1);

    // 行高统一
    for (int row = 0; row < 4; row++) {
        gridLayout->setRowStretch(row, 1);
    }

    // 3. 模块列表（顺序固定）
    QStringList moduleNames = {
        "继电器模块",
        "电流信号输出模块",
        "热电偶采集模块2",
        "冷却水泵",
        "电流信号采集模块",
        "热电偶采集模块1",
        "热电偶采集模块3",
        "PLC"
    };

    // 4. 生成4列控件（模块名 + 状态灯）
    for (int i = 0; i < moduleNames.size(); i++)
    {
        int row = i / 2;
        bool isRightSide = (i % 2 == 1);

        // 模块名称Label
        QLabel *nameLabel = new QLabel(moduleNames[i], moduleStatusDialog);
        nameLabel->setStyleSheet(R"(
            QLabel {
                font-family: "Microsoft YaHei";
                border:1px solid black;
                padding: 8px 12px;
                font-size:24px;
                font-weight:bold;
                background-color:white;
            }
        )");
        nameLabel->setAlignment(Qt::AlignCenter);

        // 状态Label（初始红色，设置对象名用于查找）
        QLabel *statusLabel = new QLabel(moduleStatusDialog);
        statusLabel->setObjectName(QString("status_%1").arg(i));  // 设置唯一标识
        statusLabel->setStyleSheet(R"(
            QLabel {
                border:1px solid black;
                background-color:red;
                border-radius: 8px;
            }
        )");
        statusLabel->setMinimumSize(40, 40);  // 保证状态灯可见

        // 按位置添加到网格
        if (!isRightSide) {
            gridLayout->addWidget(nameLabel, row, 0);
            gridLayout->addWidget(statusLabel, row, 1);
        } else {
            gridLayout->addWidget(nameLabel, row, 2);
            gridLayout->addWidget(statusLabel, row, 3);
        }
    }

    mainLayout->addLayout(gridLayout);
}

// ===================== 主界面 =====================
void MainWindow::setupMainPage()
{
    mainPage = new QWidget(this);
    QVBoxLayout *mainLayout = new QVBoxLayout(mainPage);
    mainLayout->setContentsMargins(30, 20, 30, 20);
    mainLayout->setSpacing(16);

    // 顶部标题 + LOGO
    QLabel *titleLabel = new QLabel("发动机受热件热负荷模拟试验系统", mainPage);
    titleLabel->setAlignment(Qt::AlignCenter);
    titleLabel->setStyleSheet("font-size:24px; font-weight:bold;");

    QLabel *logoLabel = new QLabel("江滨活塞\nLOGO", mainPage);
    logoLabel->setAlignment(Qt::AlignRight);

    QHBoxLayout *titleLayout = new QHBoxLayout();
    titleLayout->addStretch();
    titleLayout->addWidget(titleLabel);
    titleLayout->addStretch();
    titleLayout->addWidget(logoLabel);
    mainLayout->addLayout(titleLayout);

    mainLayout->addStretch(1);

    // 中间区域：左侧按钮 + 右侧2x2功能块
    QHBoxLayout *centerLayout = new QHBoxLayout();
    centerLayout->setSpacing(40);

    QWidget *leftContainer = new QWidget(this);
    //leftContainer->setMaximumWidth(200);
    QVBoxLayout *leftBtnLayout = new QVBoxLayout(leftContainer);
    leftBtnLayout->setSpacing(12);
    leftBtnLayout->setContentsMargins(0,0,0,0);

    QString btnStyle = R"(
        QPushButton {
            font-family: "Microsoft YaHei";
            font-size: 20px;
            font-weight: bold;
            padding: 8px 12px;
        }
       QPushButton[connected="true"] {
                  background-color: green;
                  color: white;
              }
    )";

    QString funcBtnStyle = R"(
            QPushButton {
                font-family: "Microsoft YaHei";
                font-size: 40px;
                font-weight: bold;
                padding: 8px 12px;
                border: 1px solid black;
                background-color: white;
                min-height: 220px;
                min-width: 240px;
            }
            QPushButton:hover {
                background-color: #f0f0f0;
            }
            QPushButton:pressed {
                background-color: #e0e0e0;
            }
        )";

    QString labelStyle = R"(
        QLabel {
            font-family: "Microsoft YaHei";
            font-size: 40px;
            font-weight: bold;
            padding: 8px 12px;
        }
    )";

    btnOneKeyConnect = new QPushButton("一键通讯", mainPage);
    btnOneKeyConnect->setStyleSheet(btnStyle);
    btnOneKeyConnect->setProperty("connected", false);
    btnDisconnect = new QPushButton("切断通讯", mainPage);
    btnDisconnect->setStyleSheet(btnStyle);
    btnDisconnect->setEnabled(false);
    btnMotorOrigin = new QPushButton("电机回零点位置", mainPage);
    btnMotorOrigin->setStyleSheet(btnStyle);
    btnMotorOrigin->setEnabled(false);


    btnOpenGas = new QPushButton("开启/关闭天然气", mainPage);
    btnOpenGas->setStyleSheet(btnStyle);
    btnOpenGas->setEnabled(false);
    btnOpenOxygen = new QPushButton("开启/关闭氧气", mainPage);
    btnOpenOxygen->setStyleSheet(btnStyle);
    btnOpenOxygen->setEnabled(false);
    btnClearWarning = new QPushButton("报警清除", mainPage);
    btnClearWarning->setStyleSheet(btnStyle);
    btnFireParams = new QPushButton("点火参数设置", mainPage);
    btnFireParams->setStyleSheet(btnStyle);
    btnFireParams->setEnabled(true);
    btnCoolingParams = new QPushButton("冷却水恒温参数设置", mainPage);
    btnCoolingParams->setStyleSheet(btnStyle);
    btnCoolingParams->setEnabled(false);
    btnCheckRecord = new QPushButton("试验记录查询", mainPage);
    btnCheckRecord->setStyleSheet(btnStyle);
    btnHelp = new QPushButton("帮助", mainPage);
    btnHelp->setStyleSheet(btnStyle);
    btnExit = new QPushButton("退出试验系统", mainPage);
    btnExit->setStyleSheet(btnStyle);

    leftBtnLayout->addWidget(btnOneKeyConnect);
    leftBtnLayout->addWidget(btnDisconnect);
    leftBtnLayout->addWidget(btnMotorOrigin);
    leftBtnLayout->addWidget(btnOpenGas);
    leftBtnLayout->addWidget(btnOpenOxygen);
    leftBtnLayout->addWidget(btnClearWarning);
    leftBtnLayout->addWidget(btnFireParams);
    leftBtnLayout->addWidget(btnCoolingParams);
    leftBtnLayout->addWidget(btnCheckRecord);
    leftBtnLayout->addWidget(btnHelp);

    // 右侧 2×2 功能块
    QWidget *rightContainer = new QWidget(this);
    QGridLayout *gridLayout = new QGridLayout(rightContainer);
    gridLayout->setSpacing(30);
    gridLayout->setContentsMargins(0,0,0,0);

//    auto createBlock = [=](const QString &text) {
//        QFrame *frame = new QFrame(mainPage);
//        frame->setFrameShape(QFrame::Box);
//        frame->setMinimumSize(240, 130);
//        QVBoxLayout *ly = new QVBoxLayout(frame);
//        QLabel *label = new QLabel(text, frame);
//        label->setAlignment(Qt::AlignCenter);
//        label->setStyleSheet(labelStyle);
//        ly->addWidget(label);
//        return frame;
//    };

    // 创建功能按钮
       btnDeviceStatusTest = new QPushButton("设备状态测试", mainPage);
       btnDeviceStatusTest->setStyleSheet(funcBtnStyle);
       btnMotorPosition = new QPushButton("电机位置设置", mainPage);
       btnMotorPosition->setStyleSheet(funcBtnStyle);
       btnHeatTransfer = new QPushButton("传热特性试验", mainPage);
       btnHeatTransfer->setStyleSheet(funcBtnStyle);
       btnThermalFatigue = new QPushButton("热疲劳试验", mainPage);
       btnThermalFatigue->setStyleSheet(funcBtnStyle);

       //btnDeviceStatusTest->setEnabled(false);
       //btnMotorPosition->setEnabled(false);
       btnHeatTransfer->setEnabled(false);
       btnThermalFatigue->setEnabled(false);

    gridLayout->addWidget(btnDeviceStatusTest, 0, 0);
    gridLayout->addWidget(btnMotorPosition, 0, 1);
    gridLayout->addWidget(btnHeatTransfer, 1, 0);
    gridLayout->addWidget(btnThermalFatigue, 1, 1);

    // 调整左右比例
    centerLayout->addWidget(leftContainer, 1);
    centerLayout->addWidget(rightContainer, 3);
    mainLayout->addLayout(centerLayout);

    mainLayout->addStretch(1);

    // 底部：大学研制 + 退出按钮
    QHBoxLayout *bottomLayout = new QHBoxLayout();
    QLabel *labelSchool = new QLabel("大学研制", mainPage);

    bottomLayout->addWidget(labelSchool);
    bottomLayout->addStretch();
    bottomLayout->addWidget(btnExit);
    mainLayout->addLayout(bottomLayout);

    mainPage->setLayout(mainLayout);
}

// ===================== 点火参数界面 =====================

// 全局点火参数
FireBurnerParam g_burner1;
FireBurnerParam g_burner2;
int g_keepTime = 0; // 最大开度保持时间

// ===================== 点火参数界面（模态对话框） =====================
void MainWindow::setupFireParamsPage()
{
    m_fireParamsDialog = new QDialog(this);
    m_fireParamsDialog->setWindowTitle("点火参数设置");
    m_fireParamsDialog->setModal(true);
    // 不再固定死窗口大小，支持自由缩放
    m_fireParamsDialog->setMinimumSize(1800, 900); // 只设最小尺寸

    QVBoxLayout* mainLayout = new QVBoxLayout(m_fireParamsDialog);
    mainLayout->setContentsMargins(10,10,10,10);
    mainLayout->setSpacing(8);

    // ===================== 上层主布局：水平排列 =====================
    QHBoxLayout* upperLayout = new QHBoxLayout;
    upperLayout->setSpacing(10);

    QHBoxLayout* left1_2_max_Layout = new QHBoxLayout;
    left1_2_max_Layout->setSpacing(10);
    // =====================================================
    // 【最左侧】燃烧器1
    // =====================================================
    QGroupBox* gbBurner1 = new QGroupBox;
    QVBoxLayout* layoutBurner1 = new QVBoxLayout(gbBurner1);
    layoutBurner1->setSpacing(10);

    QHBoxLayout* layBtn1 = new QHBoxLayout;
    layBtn1->addWidget(new QLabel("燃烧器1"));
    QPushButton* btn1On = new QPushButton("开");
    QPushButton* btn1Off = new QPushButton("关");
    layBtn1->addWidget(btn1On);
    layBtn1->addWidget(btn1Off);
    layoutBurner1->addLayout(layBtn1);

    QGroupBox* gbGas1 = new QGroupBox("天然气流量控制阀1控制参数");
    QFormLayout* formGas1 = new QFormLayout(gbGas1);
    formGas1->setRowWrapPolicy(QFormLayout::DontWrapRows);
    editGas1Target = new QLineEdit("0");
    editGas1Up = new QLineEdit("0");
    editGas1Down = new QLineEdit("0");
    formGas1->addRow("目标流量开度", editGas1Target);
    formGas1->addRow("渐开时间", editGas1Up);
    formGas1->addRow("渐关时间", editGas1Down);
    layoutBurner1->addWidget(gbGas1);

    QGroupBox* gbOxy1 = new QGroupBox("氧气流量控制阀1控制参数");
    QFormLayout* formOxy1 = new QFormLayout(gbOxy1);
    formOxy1->setRowWrapPolicy(QFormLayout::DontWrapRows);
    editOxy1Target = new QLineEdit("0");
    editOxy1Up = new QLineEdit("0");
    editOxy1Down = new QLineEdit("0");
    editOxy1Delay = new QLineEdit("0");
    formOxy1->addRow("目标流量开度", editOxy1Target);
    formOxy1->addRow("渐开时间", editOxy1Up);
    formOxy1->addRow("渐关时间", editOxy1Down);
    formOxy1->addRow("氧气开启时间", editOxy1Delay);
    layoutBurner1->addWidget(gbOxy1);

    QGroupBox* gbIgn1 = new QGroupBox("高能点火器1控制参数");
    QFormLayout* formIgn1 = new QFormLayout(gbIgn1);
    formIgn1->setRowWrapPolicy(QFormLayout::DontWrapRows);
    editIgn1T1 = new QLineEdit("0");
    editIgn1Dur = new QLineEdit("0");
    formIgn1->addRow("点火开始时间T1", editIgn1T1);
    formIgn1->addRow("点火持续时间", editIgn1Dur);
    layoutBurner1->addWidget(gbIgn1);

    auto setBurner1Enabled = [=](bool en) {
        editGas1Target->setEnabled(en);
        editGas1Up->setEnabled(en);
        editGas1Down->setEnabled(en);
        editOxy1Target->setEnabled(en);
        editOxy1Up->setEnabled(en);
        editOxy1Down->setEnabled(en);
        editOxy1Delay->setEnabled(en);
        editIgn1T1->setEnabled(en);
        editIgn1Dur->setEnabled(en);
    };
    connect(btn1On, &QPushButton::clicked, this, [=]() { g_burner1.enabled = true; setBurner1Enabled(true); });
    connect(btn1Off, &QPushButton::clicked, this, [=]() { g_burner1.enabled = false; setBurner1Enabled(false); });
    setBurner1Enabled(false);

    left1_2_max_Layout->addWidget(gbBurner1);

    // =====================================================
    // 【左二】燃烧器2
    // =====================================================
    QGroupBox* gbBurner2 = new QGroupBox;
    QVBoxLayout* layoutBurner2 = new QVBoxLayout(gbBurner2);
    layoutBurner2->setSpacing(10);

    QHBoxLayout* layBtn2 = new QHBoxLayout;
    layBtn2->addWidget(new QLabel("燃烧器2"));
    QPushButton* btn2On = new QPushButton("开");
    QPushButton* btn2Off = new QPushButton("关");
    layBtn2->addWidget(btn2On);
    layBtn2->addWidget(btn2Off);
    layoutBurner2->addLayout(layBtn2);

    QGroupBox* gbGas2 = new QGroupBox("天然气流量控制阀2控制参数");
    QFormLayout* formGas2 = new QFormLayout(gbGas2);
    formGas2->setRowWrapPolicy(QFormLayout::DontWrapRows);
    editGas2Target = new QLineEdit("0");
    editGas2Up = new QLineEdit("0");
    editGas2Down = new QLineEdit("0");
    formGas2->addRow("目标流量开度", editGas2Target);
    formGas2->addRow("渐开时间", editGas2Up);
    formGas2->addRow("渐关时间", editGas2Down);
    layoutBurner2->addWidget(gbGas2);

    QGroupBox* gbOxy2 = new QGroupBox("氧气流量控制阀2控制参数");
    QFormLayout* formOxy2 = new QFormLayout(gbOxy2);
    formOxy2->setRowWrapPolicy(QFormLayout::DontWrapRows);
    editOxy2Target = new QLineEdit("0");
    editOxy2Up = new QLineEdit("0");
    editOxy2Down = new QLineEdit("0");
    editOxy2Delay = new QLineEdit("0");
    formOxy2->addRow("目标流量开度", editOxy2Target);
    formOxy2->addRow("渐开时间", editOxy2Up);
    formOxy2->addRow("渐关时间", editOxy2Down);
    formOxy2->addRow("氧气开启时间", editOxy2Delay);
    layoutBurner2->addWidget(gbOxy2);

    QGroupBox* gbIgn2 = new QGroupBox("高能点火器2控制参数");
    QFormLayout* formIgn2 = new QFormLayout(gbIgn2);
    formIgn2->setRowWrapPolicy(QFormLayout::DontWrapRows);
    editIgn2T1 = new QLineEdit("0");
    editIgn2Dur = new QLineEdit("0");
    formIgn2->addRow("点火开始时间T1", editIgn2T1);
    formIgn2->addRow("点火持续时间", editIgn2Dur);
    layoutBurner2->addWidget(gbIgn2);

    auto setBurner2Enabled = [=](bool en) {
        editGas2Target->setEnabled(en);
        editGas2Up->setEnabled(en);
        editGas2Down->setEnabled(en);
        editOxy2Target->setEnabled(en);
        editOxy2Up->setEnabled(en);
        editOxy2Down->setEnabled(en);
        editOxy2Delay->setEnabled(en);
        editIgn2T1->setEnabled(en);
        editIgn2Dur->setEnabled(en);
    };
    connect(btn2On, &QPushButton::clicked, this, [=]() { g_burner2.enabled = true; setBurner2Enabled(true); });
    connect(btn2Off, &QPushButton::clicked, this, [=]() { g_burner2.enabled = false; setBurner2Enabled(false); });
    setBurner2Enabled(false);

    left1_2_max_Layout->addWidget(gbBurner2);

    // 左侧容器：燃烧器1 + 燃烧器2 + 公共参数
    QWidget* leftContainer = new QWidget;
    QVBoxLayout* burn_up_layCommon = new QVBoxLayout(leftContainer);

    QHBoxLayout* layCommon = new QHBoxLayout;
    layCommon->addWidget(new QLabel("最大开度加热持续时间"));
    editKeepTime = new QLineEdit("0");
    layCommon->addWidget(editKeepTime);

    layCommon->addSpacing(20);
    layCommon->addWidget(new QLabel("工位1和工位3顶部压缩空气冷却"));
    QPushButton* btnCoolOn = new QPushButton("开");
    QPushButton* btnCoolOff = new QPushButton("关");
    layCommon->addWidget(btnCoolOn);
    layCommon->addWidget(btnCoolOff);

    burn_up_layCommon->addLayout(left1_2_max_Layout);
    burn_up_layCommon->addLayout(layCommon);
    leftContainer->setMaximumWidth(400);
    upperLayout->addWidget(leftContainer);

    // =====================================================
    // 点火时序图 【删除了所有 setFixedSize】
    // =====================================================
    QWidget* plotTimeWidget = new QWidget;
    QVBoxLayout* plotTimeLayout = new QVBoxLayout(plotTimeWidget);
    plotTimeLayout->setSpacing(10);

    QWidget* plotIgn1 = new QWidget;
    plotIgn1->setStyleSheet("border: 1px solid purple; background-color: white;");
    plotTimeLayout->addWidget(plotIgn1);


    IgnitionPlotWidget* plotIgn2 = new IgnitionPlotWidget(this);
    plotIgn2->setStyleSheet("border: 1px solid purple;");
    plotTimeLayout->addWidget(plotIgn2);
    plotTimeWidget->setMaximumWidth(700);
    upperLayout->addWidget(plotTimeWidget);


    // =====================================================
    // 【中间右】活塞1/3 温度显示（粉色框）
    // =====================================================
    QWidget* tempDisplayWidget = new QWidget;
    QVBoxLayout* tempDisplayLayout = new QVBoxLayout(tempDisplayWidget);
    tempDisplayLayout->setSpacing(10);

    // =====================================================
    // 活塞1 温度显示（紧挨不拉伸，框大小自适应，行高可调）
    // =====================================================
//    QGroupBox* gbPiston1 = new QGroupBox("活塞1");
//    QVBoxLayout* layoutPiston1 = new QVBoxLayout(gbPiston1);
    //layoutPiston1->setSpacing(6);      // 🔥 行与行之间的高度间隙（自己改数字）
    //layoutPiston1->setAlignment(Qt::AlignTop);

    QList<QColor> lineColors = {
        QColor(220,80,80),   // 0
        QColor(80,220,80),   // 1
        QColor(80,80,220),   // 2
        QColor(220,220,80),  // 3
        QColor(220,80,220),  // 4
        QColor(80,220,220),  // 5
        QColor(255,180,0),   // 6
        QColor(180,255,0),   // 7
        QColor(180,0,255),   // 8
        QColor(255,255,255), // 9
        QColor(255,120,0),   // 10
        QColor(0,255,180)    // 11
    };

    QGroupBox* gbPiston1 = new QGroupBox("活塞1");
    QVBoxLayout* layoutPiston1 = new QVBoxLayout(gbPiston1);

    QList<QLabel*> labPiston1;
    for (int i = 0; i < 12; i++) {
        QHBoxLayout* rowLayout = new QHBoxLayout;
        rowLayout->setSpacing(2);
        rowLayout->setContentsMargins(0,0,0,0);

        // 序号标签 —— 自动使用对应曲线颜色
        QLabel* labNum = new QLabel(QString::number(i+1));
        QColor color = lineColors[i]; // 取对应颜色
        labNum->setStyleSheet(QString(
            "border:1px solid black;"
            "background-color: %1;"
            "color: black;"
        ).arg(color.name()));
        labNum->setAlignment(Qt::AlignCenter);
        labNum->setMinimumSize(20, 22); // 固定大小更好看

        // 温度显示标签
        QLabel* labVal = new QLabel("0.0");
        labVal->setStyleSheet("background-color: #FFFFFF; border:1px solid black;");
        labVal->setAlignment(Qt::AlignCenter);
        labVal->setMinimumHeight(22);
        labPiston1.append(labVal);

        rowLayout->addWidget(labNum, 1);
        rowLayout->addWidget(labVal, 3);

        layoutPiston1->addLayout(rowLayout);
    }
    tempDisplayLayout->addWidget(gbPiston1);

    // =====================================================
    // 活塞3 温度显示（颜色完全一样）
    // =====================================================
    QGroupBox* gbPiston3 = new QGroupBox("活塞3");
    QVBoxLayout* layoutPiston3 = new QVBoxLayout(gbPiston3);

    QList<QLabel*> labPiston3;
    for (int i = 0; i < 12; i++) {
        QHBoxLayout* rowLayout = new QHBoxLayout;
        rowLayout->setSpacing(2);
        rowLayout->setContentsMargins(0,0,0,0);

        // 序号标签 —— 颜色和曲线 1:1 对应
        QLabel* labNum = new QLabel(QString::number(i+1));
        QColor color = lineColors[i];
        labNum->setStyleSheet(QString(
            "border:1px solid black;"
            "background-color: %1;"
            "color: black;"
        ).arg(color.name()));
        labNum->setAlignment(Qt::AlignCenter);
        labNum->setMinimumSize(20, 22);

        QLabel* labVal = new QLabel("0.0");
        labVal->setStyleSheet("background-color: #FFE6E6; border:1px solid black;");
        labVal->setAlignment(Qt::AlignCenter);
        labVal->setMinimumHeight(22);
        labPiston3.append(labVal);

        rowLayout->addWidget(labNum, 1);
        rowLayout->addWidget(labVal, 3);

        layoutPiston3->addLayout(rowLayout);
    }
    tempDisplayLayout->addWidget(gbPiston3);

    upperLayout->addWidget(tempDisplayWidget);

    // =====================================================
    // 实时温度曲线 【删除了所有 setFixedSize】
    // =====================================================
    QWidget* curveWidget = new QWidget;
    QVBoxLayout* curveLayout = new QVBoxLayout(curveWidget);
    curveLayout->setSpacing(10);

    SubCurve* curvePiston1 = new SubCurve(this);
    curvePiston1->setStyleSheet("border: 1px solid purple; background-color: white;");
    curveLayout->addWidget(curvePiston1);

    // 工位3 活塞温度曲线 —— 替换成 SubCurve
    SubCurve* curvePiston3 = new SubCurve(this);
    curvePiston3->setStyleSheet("border: 1px solid purple; background-color: white;");
    curveLayout->addWidget(curvePiston3);

    upperLayout->addWidget(curveWidget);

    curvePiston1->initTemperatureCurve();
    curvePiston1->setYAxisRange(0, 150);    // 温度范围 0~150℃
    curvePiston1->setXRangeSeconds(60);    // X轴显示60秒
   // curvePiston1->setKeepDataSeconds(300); // 保留5分钟数据
    //curvePiston1->setComputedMode(true);    // 第13条 = 最大值
    curvePiston1->resetTemperatureGraphs(); // 创建13条曲线

    // 工位3
    curvePiston3->initTemperatureCurve();
    curvePiston3->setYAxisRange(0, 150);
    curvePiston3->setXRangeSeconds(60);
    //curvePiston3->setKeepDataSeconds(300);
    //curvePiston3->setComputedMode(true);
    curvePiston3->resetTemperatureGraphs();

    // ======================= ✅ 核心：设置宽度比例 =======================
    upperLayout->setStretch(0, 3);  // 燃烧器1+2 占比 3
    upperLayout->setStretch(1, 4);  // 时序图      占比 2
    upperLayout->setStretch(2, 1);  // 温度显示    占比 2
    upperLayout->setStretch(3, 8);  // 温度曲线    占比 3

    mainLayout->addLayout(upperLayout);

    // =====================================================
    // 底部按钮
    // =====================================================
    QHBoxLayout* btnLayout = new QHBoxLayout;
    QPushButton* btnDraw = new QPushButton("点击生成点火时序图");
    QPushButton* btnStart = new QPushButton("按时序图开启点火");
    QPushButton* btnStop = new QPushButton("关闭点火");
    QPushButton* btnSave = new QPushButton("另存当前点火参数");
    QPushButton* btnLoad = new QPushButton("打开已有点火参数文件");
    QPushButton* btnDef = new QPushButton("将当前点火参数设为默认值");
    QPushButton* btnApply = new QPushButton("应用当前点火参数");
    btnLayout->addWidget(btnDraw);
    btnLayout->addWidget(btnStart);
    btnLayout->addWidget(btnStop);
    btnLayout->addWidget(btnSave);
    btnLayout->addWidget(btnLoad);
    btnLayout->addWidget(btnDef);
    btnLayout->addWidget(btnApply);
    mainLayout->addLayout(btnLayout);

    // ================== 功能实现 ==================
    auto readParam = [&]() {
        g_burner1.gasTarget = editGas1Target->text().toInt();
        g_burner1.gasRampUp = editGas1Up->text().toInt();
        g_burner1.gasRampDown = editGas1Down->text().toInt();
        g_burner1.oxyTarget = editOxy1Target->text().toInt();
        g_burner1.oxyRampUp = editOxy1Up->text().toInt();
        g_burner1.oxyRampDown = editOxy1Down->text().toInt();
        g_burner1.oxyDelay = editOxy1Delay->text().toInt();
        g_burner1.igniteT1 = editIgn1T1->text().toInt();
        g_burner1.igniteDur = editIgn1Dur->text().toInt();

        g_burner2.gasTarget = editGas2Target->text().toInt();
        g_burner2.gasRampUp = editGas2Up->text().toInt();
        g_burner2.gasRampDown = editGas2Down->text().toInt();
        g_burner2.oxyTarget = editOxy2Target->text().toInt();
        g_burner2.oxyRampUp = editOxy2Up->text().toInt();
        g_burner2.oxyRampDown = editOxy2Down->text().toInt();
        g_burner2.oxyDelay = editOxy2Delay->text().toInt();
        g_burner2.igniteT1 = editIgn2T1->text().toInt();
        g_burner2.igniteDur = editIgn2Dur->text().toInt();

        g_keepTime = editKeepTime->text().toInt();
    };

    connect(btnDraw, &QPushButton::clicked, this, [=]() {
          readParam();
//        QPainter p1(plotIgn1);
//        p1.fillRect(plotIgn1->rect(), Qt::white);
//        p1.setPen(QPen(Qt::yellow, 2));
//        int w = plotIgn1->width();
//        int h = plotIgn1->height();
//        p1.drawLine(0, h, w * 0.3, 30);
//        p1.drawLine(w * 0.3, 30, w * 0.7, 30);
//        p1.drawLine(w * 0.7, 30, w, h);

//        QPainter p2(plotIgn2);
//        p2.fillRect(plotIgn2->rect(), Qt::white);
//        p2.setPen(QPen(Qt::yellow, 2));
//        p2.drawLine(0, h, w * 0.3, 30);
//        p2.drawLine(w * 0.3, 30, w * 0.7, 30);
//        p2.drawLine(w * 0.7, 30, w, h);
        plotIgn2->drawIgnitionPlot(); // 触发绘图
    });

    // 先确保你的头文件包含了 subcurve.h
    // #include "subcurve.h"

    QTimer* timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, [=]() {
        // 用于存储 12 路温度
        if (!curvePiston1 || !curvePiston3)
                return;
        QVector<float> tempsPiston1;
        QVector<float> tempsPiston3;

        // 读取 12 个热电偶温度 + 更新标签显示
        for (int i = 0; i < 12; i++) {
            // --------------------- 活塞1 ---------------------
//            float t1 = dam3130[0] ? dam3130[0]->getAIValue(i + 1) : 25.0f;
//            labPiston1[i]->setText(QString::number(t1, 'f', 1));
//            tempsPiston1.append(t1);
            float t1 = 88.6+i*5;
            labPiston1[i]->setText(QString::number(t1, 'f', 1));
            tempsPiston1.append(t1);

//            // --------------------- 活塞3 ---------------------
//            float t3 = dam3130[2] ? dam3130[2]->getAIValue(i + 1) : 25.0f;
//            labPiston3[i]->setText(QString::number(t3, 'f', 1));
//            tempsPiston3.append(t3);

            float t3 = 88.6+i*5;
            labPiston1[i]->setText(QString::number(t1, 'f', 1));
            tempsPiston3.append(t3);

        }

        //labPiston1[12]->setText(QString::number(77, 'f', 1));
        //tempsPiston1.append(77.7);

//            // --------------------- 活塞3 ---------------------
//            float t3 = dam3130[2] ? dam3130[2]->getAIValue(i + 1) : 25.0f;
//            labPiston3[i]->setText(QString::number(t3, 'f', 1));
//            tempsPiston3.append(t3);

        //float t3 = 88.6+i*5;
        //labPiston1[i]->setText(QString::number(t1, 'f', 1));
        //tempsPiston3.append(77.7);

        // ===================== 核心：调用 SubCurve 绘图 =====================
        // curvePiston1 和 curvePiston3 都是 SubCurve 控件
        //if (curvePiston1) {
            // 自动计算第13条曲线（内部取最大值/平均值）
            curvePiston1->updateTemperatureData(tempsPiston1);
        //}
       // if (curvePiston3) {
            curvePiston3->updateTemperatureData(tempsPiston3);
       // }
    });

    QTimer::singleShot(300, this, [=]() {
        timer->start(200);
    });

    connect(btnStart, &QPushButton::clicked, this, [=]() {
        readParam();
        if (dam2601 && ssrdiomodbus) {
            ssrdiomodbus->writeDOStatus(12, true);
            dam2601->writeAO(1, g_burner1.gasTarget);
        }
    });
    connect(btnStop, &QPushButton::clicked, this, [=]() {
        if (dam2601) dam2601->writeAO(1, 0);
        if (ssrdiomodbus) ssrdiomodbus->writeDOStatus(12, false);
    });
}
// ===================== 测试/继电器界面 =====================
void MainWindow::setupTestPage()
{
    testPage = new QWidget(this);
    QVBoxLayout *vLayout = new QVBoxLayout(testPage);
    vLayout->setContentsMargins(30,30,30,30);

    QGroupBox *ssrBox = new QGroupBox("继电器控制", testPage);
    QGridLayout *ssrLayout = new QGridLayout(ssrBox);

    QStringList ssrNames = {
        "工位2水冷","工位1水冷","工位4水冷","工位3水冷",
        "总回水","工位2吹气","工位4吹气","工位1吹气",
        "工位3吹气","工位2&4顶部吹气","工位1&3顶部吹气",
        "工位1&3点火","工位2&4点火","风扇开关"
    };

    for(int i=0;i<ssrNames.size();i++)
    {
        QPushButton *btn = new QPushButton(ssrNames[i],ssrBox);
        btn->setCheckable(true);
        ssrLayout->addWidget(btn,i/4,i%4);

        connect(btn, &QPushButton::toggled, this, [=](bool checked){
            if(ssrdiomodbus) {
                ssrdiomodbus->writeDOStatus(i+1, checked);
            }
        });
    }

    vLayout->addWidget(ssrBox);
    testPage->setLayout(vLayout);
}

// ===================== 冷却水参数 =====================
void MainWindow::setupCoolingParamsPage()
{
    coolingParamsPage = new QWidget(this);
    QVBoxLayout *vLayout = new QVBoxLayout(coolingParamsPage);
    vLayout->setContentsMargins(30,30,30,30);

    QLabel *lblTemp = new QLabel("恒温水箱温度(℃):", coolingParamsPage);
    QLineEdit *editTemp = new QLineEdit(coolingParamsPage);
    QCheckBox *chkPump = new QCheckBox("水泵开关", coolingParamsPage);
    QCheckBox *chkFan = new QCheckBox("风扇开关", coolingParamsPage);
    QPushButton *btnSave = new QPushButton("确定", coolingParamsPage);

    vLayout->addWidget(lblTemp);
    vLayout->addWidget(editTemp);
    vLayout->addWidget(chkPump);
    vLayout->addWidget(chkFan);
    vLayout->addWidget(btnSave);

    coolingParamsPage->setLayout(vLayout);
}

// ===================== 试验记录 =====================
void MainWindow::setupRecordPage()
{
    recordPage = new QWidget(this);
    QVBoxLayout *vLayout = new QVBoxLayout(recordPage);
    vLayout->setContentsMargins(30,30,30,30);

    QHBoxLayout *filterLayout = new QHBoxLayout();
    filterLayout->addWidget(new QPushButton("热疲劳实验",recordPage));
    filterLayout->addWidget(new QPushButton("传热特性测试",recordPage));
    filterLayout->addWidget(new QPushButton("铝合金",recordPage));
    filterLayout->addWidget(new QPushButton("钢铁",recordPage));
    filterLayout->addWidget(new QPushButton("活塞",recordPage));

    QTableWidget *table = new QTableWidget(recordPage);
    table->setColumnCount(1);
    table->setHorizontalHeaderLabels({"试验文件夹"});

    vLayout->addLayout(filterLayout);
    vLayout->addWidget(table);
    recordPage->setLayout(vLayout);
}

// ===================== 信号槽 =====================
void MainWindow::setupConnections()
{
    connect(btnOneKeyConnect, &QPushButton::clicked, this, &MainWindow::onOneKeyConnect, Qt::UniqueConnection);
    connect(btnDisconnect, &QPushButton::clicked, this, &MainWindow::onDisconnect, Qt::UniqueConnection);
    connect(btnMotorOrigin, &QPushButton::clicked, this, &MainWindow::onMotorOrigin, Qt::UniqueConnection);
    connect(btnOpenGas, &QPushButton::clicked, this, &MainWindow::onOpenGas, Qt::UniqueConnection);
    connect(btnOpenOxygen, &QPushButton::clicked, this, &MainWindow::onOpenOxygen, Qt::UniqueConnection);
    connect(btnFireParams, &QPushButton::clicked, this, &MainWindow::onFireParams, Qt::UniqueConnection);
    connect(btnCoolingParams, &QPushButton::clicked, this, &MainWindow::onCoolingParams, Qt::UniqueConnection);
    connect(btnHelp, &QPushButton::clicked, this, &MainWindow::onHelp, Qt::UniqueConnection);
    connect(btnExit, &QPushButton::clicked, this, &MainWindow::onExit, Qt::UniqueConnection);

    connect(btnDeviceStatusTest, &QPushButton::clicked, this, &MainWindow::onDeviceStatusTest, Qt::UniqueConnection);
    connect(btnMotorPosition, &QPushButton::clicked, this, &MainWindow::onMotorPosition, Qt::UniqueConnection);
    connect(btnHeatTransfer, &QPushButton::clicked, this, &MainWindow::onHeatTransfer, Qt::UniqueConnection);
    connect(btnThermalFatigue, &QPushButton::clicked, this, &MainWindow::onThermalFatigue, Qt::UniqueConnection);

    // ==============================================
    // 你 PLC 真实信号：connected → 连接成功
    // ==============================================

    // ==============================================
    // 心跳断开（你自带的信号）
    // ==============================================
//    connect(m_plc, &PlcController::connectionLost, this, [=]() {
//        btnMotorOrigin->setEnabled(false);
//        QMessageBox::critical(this, "通讯错误", "PLC 心跳丢失，已断开！");
//    });

    // 设备状态测试按钮（从右侧2x2块中添加点击事件，示例）
    // 如需绑定需先获取右侧功能块按钮，这里简化处理
}

// ===================== 心跳检测 =====================
void MainWindow::checkHeartbeat()
{
    struct ModuleCheck {
        ModbusBase* mod;
        int index;
        QString name;
    };

    QVector<ModuleCheck> checks = {
        {ssrdiomodbus, 0, "继电器模块"},
        {dam2601,      1, "电流信号输出模块"},
        {dam3055,      4, "电流信号采集模块"},
        {waterinvmodbus,3,"冷却水泵"},
        {dam3130[0],   5, "热电偶采集模块1"},
        {dam3130[1],   2, "热电偶采集模块2"},
        {dam3130[2],   6, "热电偶采集模块3"},
        {nullptr,      7, "PLC"}
    };

    bool hasDisconnected = false;
    bool allConnected = btnOneKeyConnect->property("connected").toBool();

    for(const ModuleCheck &c : checks)
    {
        if(c.index >= modulesStatus.size()) continue; // 防止越界
        ModuleStatus &status = modulesStatus[c.index];
        if(!status.statusLabel) continue;

        // PLC模块单独处理（灰色）
        if(c.index == 7){
            status.statusLabel->setStyleSheet("border:1px solid black; background-color:gray;border-radius:10px;");
            continue;
        }

        if(!c.mod) continue; // 空模块跳过

        // 检查模块连接状态
        bool isModuleConnected = (c.mod->getDeviceStatus() == Status_Connected ||
                                  c.mod->getDeviceStatus() == Status_Online);

        if(!isModuleConnected)
        {
            status.statusLabel->setStyleSheet("border:1px solid black; background-color:red;border-radius:10px;");
            status.connected = false;
            status.failCount++;
            if(allConnected){
                hasDisconnected = true;
            }
        }
        else
        {
            status.statusLabel->setStyleSheet("border:1px solid black; background-color:green;border-radius:10px;");
            status.connected = true;
            status.failCount = 0;
        }
    }

    // 检测到断开，弹出模态对话框
    if(hasDisconnected){
        QMessageBox::warning(this, "警告", "部分模块通讯断开，请检查！");
        if(moduleStatusDialog) {
            moduleStatusDialog->exec(); // 弹出模态对话框
        }
    }
}

// ===================== 一键连接 =====================
void MainWindow::onOneKeyConnect()
{
    // 设置串口参数（根据实际硬件调整）
    dam2601->setSerialParam("COM16",9600);
    dam3055->setSerialParam("COM17",9600);
    dam3130[0]->setSerialParam("COM18",9600);
    dam3130[1]->setSerialParam("COM19",9600);
    dam3130[2]->setSerialParam("COM20",9600);
    ssrdiomodbus->setSerialParam("COM15",9600);
    waterinvmodbus->setSerialParam("COM21",9600);
    m_plc->connectPlc("192.168.3.250", 8080);

    // 连接所有模块
    bool ok = true;
    ok &= dam2601->connectDevice();
    ok &= dam3055->connectDevice();
    ok &= dam3130[0]->connectDevice();
    ok &= dam3130[1]->connectDevice();
    ok &= dam3130[2]->connectDevice();
    ok &= ssrdiomodbus->connectDevice();
    ok &= waterinvmodbus->connectDevice();

    if(ok)
    {
        //QMessageBox::information(this,"成功","所有模块连接成功");
        btnOneKeyConnect->setProperty("connected", true);
        btnOneKeyConnect->style()->unpolish(btnOneKeyConnect);
        btnOneKeyConnect->style()->polish(btnOneKeyConnect); // 更新按钮样式
        btnOneKeyConnect->setEnabled(false);
        btnDisconnect->setEnabled(true);
        btnOpenGas->setEnabled(true);
        btnOpenOxygen->setEnabled(true);
        btnFireParams->setEnabled(true);
        btnCoolingParams->setEnabled(true);
        btnDeviceStatusTest->setEnabled(true);;
        btnMotorPosition->setEnabled(true);;
        btnHeatTransfer->setEnabled(true);;
        btnThermalFatigue->setEnabled(true);;


    }
    else
    {
        // 连接失败，弹出状态对话框
        if(moduleStatusDialog) {
            moduleStatusDialog->exec();
        }
    }
}

void MainWindow::onMotorOrigin()
{
    // 先禁用，防止重复点击
    if (!m_plc->isConnected())
        return;

    btnMotorOrigin->setEnabled(false);

    // 固定按钮大小和字体，绝不变形
    QSize fixedSize = btnMotorOrigin->size();
    btnMotorOrigin->setFixedSize(fixedSize);
    QFont btnFont = btnMotorOrigin->font();
    btnMotorOrigin->setFont(btnFont);

    // 闪烁定时器
    QTimer *flashTimer = new QTimer(this);
    flashTimer->setInterval(500);
    bool isGreen = false;

    connect(flashTimer, &QTimer::timeout, this, [=]() mutable {
        isGreen = !isGreen;
        if (isGreen) {
            btnMotorOrigin->setStyleSheet("background-color: green; color: white;");
        } else {
            btnMotorOrigin->setStyleSheet("");
        }
    });
    flashTimer->start();

    // ======================
    // 开始执行回零逻辑
    // ======================
    QTimer::singleShot(200, this, [this, flashTimer]() {
        m_plc->writeM(0, 1, nullptr);

        QTimer::singleShot(200, this, [this, flashTimer]() {
            m_plc->writeM(1, true, [this, flashTimer](bool ok) {
                if (!ok) return;

                // ======================
                // 轮询读取 M51
                // ======================
                QTimer *pollTimer = new QTimer(this);
                int timeoutCount = 0;

                // 【修复】这里必须捕获 pollTimer + flashTimer + this
                connect(pollTimer, &QTimer::timeout, this, [this, flashTimer, pollTimer, &timeoutCount]() mutable {
                    timeoutCount++;

                    // 超时保护
                    if (timeoutCount > 500) {
                        pollTimer->stop();
                        pollTimer->deleteLater();
                        flashTimer->stop();
                        flashTimer->deleteLater();
                        btnMotorOrigin->setStyleSheet("background-color: red; color: white;");
                        btnMotorOrigin->setEnabled(true);
                        qDebug() << "M51轮询超时，未置位";
                        return;
                    }

                    // 读取 M51
                    m_plc->readM(51, [this, flashTimer, pollTimer](bool ok, bool val) {
                        if (!ok) return;

                        qDebug() << "M51 当前值：" << val;

                        if (val) {
                            // 回零完成
                            m_plcHomeOk = true;
                            btnMotorPosition->setEnabled(true);
                            m_plc->writeM(1, false, nullptr);
                            m_plc->writeM(51, false, nullptr);

                            // 停止所有定时器
                            flashTimer->stop();
                            flashTimer->deleteLater();
                            pollTimer->stop();
                            pollTimer->deleteLater();

                            // 最终状态
                            btnMotorOrigin->setStyleSheet("background-color: green; color: white;");
                            btnMotorOrigin->setEnabled(false);
                        }
                    });
                });

                pollTimer->start(200); // 开始轮询
            });
        });
    });
}
// ===================== 断开连接 =====================
void MainWindow::onDisconnect()
{
    dam2601->disconnectDevice();
    dam3055->disconnectDevice();
    for(int i=0;i<3;i++) dam3130[i]->disconnectDevice();
    ssrdiomodbus->disconnectDevice();
    waterinvmodbus->disconnectDevice();

    btnOneKeyConnect->setProperty("connected", false);
    btnOneKeyConnect->style()->unpolish(btnOneKeyConnect);
    btnOneKeyConnect->style()->polish(btnOneKeyConnect);

    // 重置状态灯为红色
    for(auto &status : modulesStatus) {
        if(status.statusLabel) {
            status.statusLabel->setStyleSheet("border:1px solid black; background-color:red;border-radius:8px;");
        }
        status.connected = false;
        status.failCount = 0;
    }

    btnDisconnect->setEnabled(false);
    btnOpenGas->setEnabled(false);
    btnOpenOxygen->setEnabled(false);
    btnFireParams->setEnabled(false);
    btnCoolingParams->setEnabled(false);
    btnDeviceStatusTest->setEnabled(false);;
    btnMotorPosition->setEnabled(false);;
    btnHeatTransfer->setEnabled(false);;
    btnThermalFatigue->setEnabled(false);;
    btnOneKeyConnect->setProperty("connected", false);

    stackedWidget->setCurrentWidget(mainPage);
}

// ===================== 空实现函数 =====================
void MainWindow::onOpenGas()
{
    bool isOpen = ssrdiomodbus->getDIStatus(CH_GAS_STATION1);

    if (!isOpen)
    {
        ssrdiomodbus->writeDOStatus(CH_GAS_STATION1, true);

        // 再次读取确认是否打开成功
        bool checkOpen = ssrdiomodbus->getDIStatus(CH_GAS_STATION1);;
        if (checkOpen) {

            btnOpenGas->setStyleSheet("background-color: #90EE90; font-size:20px; font-weight:bold;");
            //QMessageBox::information(this, "成功", "氧气阀门已打开！");
        }
    }
    else
    {
        ssrdiomodbus->writeDOStatus(CH_GAS_STATION1, false);

        // 再次读取确认是否关闭成功
        bool checkClose = !ssrdiomodbus->getDIStatus(CH_GAS_STATION1);;
        if (checkClose) {
            // 关闭成功 → 恢复原来颜色
            btnOpenGas->setStyleSheet("background-color: ; font-size:20px; font-weight:bold;");
            //QMessageBox::information(this, "成功", "氧气阀门已关闭！");
        }
    }
}
void MainWindow::onOpenOxygen()
{
    // ================================
    bool isOpen = ssrdiomodbus->getDIStatus(CH_OXYGEN_STATION1);

    if (!isOpen)
    {
        ssrdiomodbus->writeDOStatus(CH_OXYGEN_STATION1, true);

        // 再次读取确认是否打开成功
        bool checkOpen = ssrdiomodbus->getDIStatus(CH_OXYGEN_STATION1);;
        if (checkOpen) {

            btnOpenGas->setStyleSheet("background-color: #90EE90; font-size:20px; font-weight:bold;");
            //QMessageBox::information(this, "成功", "氧气阀门已打开！");
        }
    }
    else
    {
        ssrdiomodbus->writeDOStatus(CH_OXYGEN_STATION1, false);

        // 再次读取确认是否关闭成功
        bool checkClose = !ssrdiomodbus->getDIStatus(CH_OXYGEN_STATION1);;
        if (checkClose) {
            // 关闭成功 → 恢复原来颜色
            btnOpenGas->setStyleSheet("background-color: ; font-size:20px; font-weight:bold;");
            //QMessageBox::information(this, "成功", "氧气阀门已关闭！");
        }
    }
}
void MainWindow::onDeviceStatus(){ stackedWidget->setCurrentWidget(testPage);}
void MainWindow::onFireParams()
{
    if(!m_fireParamsDialog) setupFireParamsPage();
        m_fireParamsDialog->exec();
}
void MainWindow::onCoolingParams(){ stackedWidget->setCurrentWidget(coolingParamsPage);}
void MainWindow::onHelp(){}
void MainWindow::onThermalFatigue(){}
void MainWindow::onHeatTransfer(){}//onMotorPosition
void MainWindow::onMotorPosition()
{
//    if (!m_plcHomeOk) {
//        addLog("请先执行电机回零！", 1);
//        return;
//    }

    // ======================
    // 主窗口
    // ======================
    QDialog *motorPosDialog = new QDialog(this);
    motorPosDialog->setWindowTitle("电机位置控制");
    motorPosDialog->setFixedSize(900, 550);
    motorPosDialog->setStyleSheet("background-color: #f0f0f0;");

    // ======================
    // 动画定时器
    // ======================
    QTimer *arrowRotateTimer = new QTimer(this);
    QTimer *pistonUpdateTimer = new QTimer(this);
    int currentRotateAngle = 90;

    // ======================
    // 主布局：左右结构
    // ======================
    QHBoxLayout *mainLayout = new QHBoxLayout(motorPosDialog);
    mainLayout->setContentsMargins(15, 15, 15, 15);
    mainLayout->setSpacing(15);

    // ======================
    // 左侧：活塞运动电机
    // ======================
    QWidget *leftWidget = new QWidget();
    QVBoxLayout *leftLayout = new QVBoxLayout(leftWidget);
    leftLayout->setSpacing(10);
    leftLayout->setContentsMargins(10, 10, 10, 10);

    // 速度模式
    QGroupBox *speedGroup = new QGroupBox("速度模式");
    QFormLayout *speedLayout = new QFormLayout(speedGroup);
    QLineEdit *editRunSpeed = new QLineEdit();
    speedLayout->addRow("运行速度", editRunSpeed);
    speedLayout->addRow("激光传感器测距", new QPushButton("激光传感器测距"));

    QHBoxLayout *posRecLayout = new QHBoxLayout();
    posRecLayout->addWidget(new QPushButton("记录位置1"));
    posRecLayout->addWidget(new QPushButton("记录位置2"));
    posRecLayout->addWidget(new QPushButton("记录位置3"));
    speedLayout->addRow(posRecLayout);

    QHBoxLayout *runStopLayout = new QHBoxLayout();
    runStopLayout->addWidget(new QPushButton("开始运动"));
    runStopLayout->addWidget(new QPushButton("停止运动"));
    speedLayout->addRow(runStopLayout);
    leftLayout->addWidget(speedGroup);

    // 位置模式
    QGroupBox *posGroup = new QGroupBox("位置模式");
    QHBoxLayout *posModeLayout = new QHBoxLayout(posGroup);
    posModeLayout->addWidget(new QPushButton("运动至位置1"));
    posModeLayout->addWidget(new QPushButton("运动至位置2"));
    posModeLayout->addWidget(new QPushButton("运动至位置3"));
    leftLayout->addWidget(posGroup);

    // ======================
    // ✅ 活塞示意图（你要的滑动功能）
    // ======================
    QWidget *pistonWidget = new QWidget();
    pistonWidget->setFixedHeight(150);
    pistonWidget->setStyleSheet("background: white; border: 1px solid #ccc;");

    // 背景：轨道（双横线 + 固定竖线）
    QLabel *bgTrack = new QLabel(pistonWidget);
    bgTrack->setPixmap(QPixmap(":/bg_track.png").scaled(800, 100, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    bgTrack->setGeometry(20, 25, 800, 100);
    bgTrack->setStyleSheet("border: none; outline: none; background: transparent;");

    // 前景：黄色框（可移动）
    QLabel *yellowBox = new QLabel(bgTrack);
    yellowBox->setPixmap(QPixmap(":/fg_yellow_box.png").scaled(120, 100, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    yellowBox->setStyleSheet("border: none; outline: none; background: transparent;");

    // 红色双向箭头
    QLabel *redArrow = new QLabel(bgTrack);
    redArrow->setText("↔");
    redArrow->setStyleSheet("color: red; font-size: 24px;");

    // 距离显示
    QLabel *distLabel = new QLabel(bgTrack);
    distLabel->setStyleSheet("color: red; font-size:14px; font-weight:bold;");

    // 放到顶层
    yellowBox->raise();
    redArrow->raise();
    distLabel->raise();

    leftLayout->addWidget(pistonWidget);

    // ======================
    // 右侧：点火杆旋转电机（最终无错版）
    // ======================
    QWidget *rightWidget = new QWidget();
    QVBoxLayout *rightLayout = new QVBoxLayout(rightWidget);
    rightLayout->setSpacing(15);
    rightLayout->setContentsMargins(10, 10, 10, 10);

    QHBoxLayout *fireBtnLayout = new QHBoxLayout();
    fireBtnLayout->addWidget(new QPushButton("旋转至点火工位"));
    fireBtnLayout->addWidget(new QPushButton("旋转至非点火工位"));
    rightLayout->addLayout(fireBtnLayout);

    // 旋转显示画布（自己绘图，不使用文字箭头）
    QLabel *rotateDisplay = new QLabel();
    rotateDisplay->setFixedSize(220, 220);

    // 旋转角度
    //int currentRotateAngle = 0;

    // 旋转动画定时器
    connect(arrowRotateTimer, &QTimer::timeout, this, [=]() mutable {
        // 创建画布
        QPixmap bgPix(":/rotate_bg.png"); // 请替换成你的实际图片路径
            // 方式B：如果你的图是在Qt资源文件里，用上面的方式；如果是本地路径，用下面的方式
            // QPixmap bgPix("C:/xxx/burner_bg.png");

            // 缩放背景图到label大小（保持比例，居中显示）
            QPixmap scaledBg = bgPix.scaled(220, 220, Qt::KeepAspectRatio, Qt::SmoothTransformation);
            QPixmap pix = scaledBg.copy(); // 复制一份用来画箭头，不破坏原图

            QPainter painter(&pix);
            painter.setRenderHint(QPainter::Antialiasing);

        // 画红色双向箭头
        painter.setPen(QPen(Qt::red, 3));
        painter.setBrush(Qt::red);

        int cx = pix.width() / 2;
         int cy = pix.height() / 2+10;
        int arrowLength = 80;

        painter.save();
        painter.translate(cx, cy);
        painter.rotate(currentRotateAngle);

        // 竖线
        painter.drawLine(0, -arrowLength, 0, arrowLength);

        // 上箭头
        painter.drawLine(0, -arrowLength, -7, -arrowLength + 15);
        painter.drawLine(0, -arrowLength, 7, -arrowLength + 15);

        // 下箭头
        painter.drawLine(0, arrowLength, -7, arrowLength - 15);
        painter.drawLine(0, arrowLength, 7, arrowLength - 15);

        painter.restore();
        painter.end();

        rotateDisplay->setPixmap(pix);

        // 旋转速度
        currentRotateAngle += 2;
        if (currentRotateAngle > 90) {
            currentRotateAngle = 0;
        }
    });

    rightLayout->addWidget(rotateDisplay, 0, Qt::AlignCenter);
    // ======================
    // 总布局组装
    // ======================
    mainLayout->addWidget(leftWidget, 5);
    mainLayout->addWidget(rightWidget, 3);



    // ======================
    // ✅ 活塞滑动逻辑（完全按你要求）
    // ======================
    const int FIXED_LINE1 = 80;    // 左侧固定竖线
    const int FIXED_LINE2 = 240;    // 初始位置竖线
    const int BOX_WIDTH = 120;
    const int RIGHT_LIMIT = 720;    // 轨道最右侧

    // 初始位置：黄色框紧贴第二条竖线
    yellowBox->move(FIXED_LINE2, (bgTrack->height() - yellowBox->height())/2-13);
    float dist = 60;

    connect(pistonUpdateTimer, &QTimer::timeout, this, [=]() mutable {
        //if (!dam3055 || !dam3055[0]) return;

        // 真实距离
        //float dist = dam3055[0]->getAIValue(8);
        dist = dist-0.5;
        // 计算位置
        int x = FIXED_LINE2 + (dist - 50) * 4.0f;

        // 左限位：不能碰到第一条竖线
        if (x < FIXED_LINE1 + 10)
            x = FIXED_LINE1 + 10;

        // 右限位：黄色框右边不出轨道
        if (x + BOX_WIDTH > RIGHT_LIMIT)
            x = RIGHT_LIMIT - BOX_WIDTH;
        qDebug()<<x;

        // 移动黄色框
        yellowBox->move(x, yellowBox->y());

        // 箭头在两条竖线中间
        int arrowX = (FIXED_LINE2 + x) / 2 - 15;
        redArrow->setGeometry(arrowX, 20, 30, 30);

        // 距离显示
        distLabel->setText(QString::number(dist, 'f', 1) + " mm");
        distLabel->setGeometry(arrowX - 20, 5, 60, 20);
    });

    // 旋转动画
    connect(arrowRotateTimer, &QTimer::timeout, this, [=]() mutable {
        currentRotateAngle += 4;
        if (currentRotateAngle >= 180) currentRotateAngle = 0;
        //rotateArrow->setStyleSheet(QString("font-size:120px; color:red; qtransform: rotate(%1deg);").arg(currentRotateAngle));
    });

    // 关闭时停止定时器
    connect(motorPosDialog, &QDialog::finished, this, [=]() {
        arrowRotateTimer->stop();
        pistonUpdateTimer->stop();
    });

    arrowRotateTimer->start(40);
    pistonUpdateTimer->start(200);

    QList<QPushButton*> btns = motorPosDialog->findChildren<QPushButton*>();
       for (QPushButton* btn : btns) {
           btn->setStyleSheet(R"(
               QPushButton {
                   min-height: 40px;
                   font-size:14px;
                   border:1px solid black;
                   background:white;
               }
               QPushButton:pressed {
                   background-color: #d0d0d0;
                   padding-left: 2px;
                   padding-top: 2px;
               }
               QPushButton:checked {
                   background:#90EE90;
               }
           )");
       }

    // 显示窗口
    motorPosDialog->exec();
}

void MainWindow::onMotorControl(){}
void MainWindow::onExit(){ close();}

// ===================== 析构函数（防止内存泄漏） =====================
//MainWindow::~MainWindow()
//{
//    if(moduleStatusDialog) {
//        delete moduleStatusDialog;
//    }
//}


void MainWindow::setupDeviceTestDialog()
{
    m_deviceTestDialog = new QDialog(this);
    m_deviceTestDialog->setWindowTitle("设备状态测试");
    m_deviceTestDialog->setFixedSize(1400, 900);
    m_deviceTestDialog->setModal(true); // 模态对话框，挡住主界面

    // 主布局：三列布局（部件功能区 | 采集区1 | 采集区2）
    QHBoxLayout *mainLayout = new QHBoxLayout(m_deviceTestDialog);
    mainLayout->setSpacing(20);
    mainLayout->setContentsMargins(15,15,15,15);

    // ===================== 左侧：部件功能测试区 =====================
    QWidget *leftWidget = new QWidget();
    QVBoxLayout *leftLayout = new QVBoxLayout(leftWidget);
    leftLayout->setSpacing(15);

    // --- 继电器开关功能区 ---
    QGroupBox *relayBox = new QGroupBox("继电器开关功能");
    QGridLayout *relayLayout = new QGridLayout(relayBox);
    relayLayout->setSpacing(10);
    QStringList relayNames = {
        "工位2水冷", "工位1水冷", "工位4水冷",
        "工位3水冷", "总回水", "",
        "工位2下空冷", "工位4下空冷", "工位1下空冷",
        "工位3下空冷", "工位24上空冷", "工位13上空冷",
        "甲烷1", "氧气1", "",
        "高能点火器1", "高能点火器2", ""
    };
    for(int i=0; i<relayNames.size(); i++){
        if(relayNames[i].isEmpty()) continue;
        QPushButton *btn = new QPushButton(relayNames[i], relayBox);
        btn->setCheckable(true);
        btn->setStyleSheet("QPushButton { min-height: 40px; font-size:14px; border:1px solid black; background:white; } QPushButton:checked { background:#90EE90; }");
        // 绑定继电器操作（这里的索引对应你定义的通道宏）
        connect(btn, &QPushButton::toggled, this, [=](bool checked){
            // 索引0~16对应你14个SSR通道（根据你的宏调整）
            if(i < 14 && ssrdiomodbus){
                ssrdiomodbus->writeDOStatus(i+1, checked);
            }
        });
        relayLayout->addWidget(btn, i/3, i%3);
        m_relayBtns.append(btn);
    }
    leftLayout->addWidget(relayBox);

    // --- 流量调节功能区 ---
    QGroupBox *flowBox = new QGroupBox("流量调节功能");
    QVBoxLayout *flowLayout = new QVBoxLayout(flowBox);
    flowLayout->setSpacing(10);
    QStringList flowNames = {
        "天然气质量流量控制阀1流量调节",
        "天然气质量流量控制阀2流量调节",
        "氧气质量流量控制阀1流量调节",
        "氧气质量流量控制阀2流量调节",
        "冷却泵流量调节"
    };
    for(int i=0; i<flowNames.size(); i++){
        QLabel *title = new QLabel(flowNames[i]);
        QSlider *slider = new QSlider(Qt::Horizontal);
        slider->setRange(0, 100);
        slider->setValue(0);
        slider->setStyleSheet("QSlider::handle:horizontal { background:#5c87b2; border:1px solid #5c87b2; width:18px; margin:-2px 0; border-radius:9px; }");
        flowLayout->addWidget(title);
        flowLayout->addWidget(slider);
        m_flowSliders.append(slider);

        // 绑定滑块事件
        if(i < 4){ // 前4个对应2601模块输出
            connect(slider, &QSlider::valueChanged, this, [=](int value){
                if(dam2601){
                    dam2601->writeAO(i+1, value/5);
                }
            });
        } else { // 冷却泵对应waterinvmodbus
            connect(slider, &QSlider::valueChanged, this, [=](int value){
                if(waterinvmodbus){
                    waterinvmodbus->writeAO(0,value/5);
                }
            });
        }
    }
    leftLayout->addWidget(flowBox);
    leftWidget->setLayout(leftLayout);
    mainLayout->addWidget(leftWidget, 1);

    // ===================== 中间：信号采集功能区（热电偶+流量） =====================
    QWidget *midWidget = new QWidget();
    QVBoxLayout *midLayout = new QVBoxLayout(midWidget);
    midLayout->setSpacing(15);

    // --- 热电偶温度采集 ---
    QGroupBox *thermoBox = new QGroupBox("热电偶温度采集");
    QGridLayout *thermoLayout = new QGridLayout(thermoBox);
    thermoLayout->setSpacing(10);
    // 表头
    QStringList headers = {"", "活塞1", "活塞2", "活塞3", "活塞4"};
    for(int i=0; i<headers.size(); i++){
        QLabel *lab = new QLabel(headers[i]);
        lab->setStyleSheet("font-weight:bold; border:1px solid black; padding:5px;");
        lab->setAlignment(Qt::AlignCenter);
        thermoLayout->addWidget(lab, 0, i);
    }
    // 12行×4列 = 48个热电偶数据
    for(int row=1; row<=12; row++){
        QLabel *rowLab = new QLabel(QString::number(row));
        rowLab->setStyleSheet("border:1px solid black; padding:5px;");
        rowLab->setAlignment(Qt::AlignCenter);
        thermoLayout->addWidget(rowLab, row, 0);
        for(int col=1; col<=4; col++){
            QLabel *valLab = new QLabel("0.00");
            valLab->setStyleSheet("border:1px solid black; padding:5px;");
            valLab->setAlignment(Qt::AlignCenter);
            thermoLayout->addWidget(valLab, row, col);
            m_thermoLabels.append(valLab);
        }
    }
    midLayout->addWidget(thermoBox);

    // --- 天然气与氧气流量采集 ---
    QGroupBox *flowDataBox = new QGroupBox("天然气与氧气流量采集");
    QGridLayout *flowDataLayout = new QGridLayout(flowDataBox);
    flowDataLayout->setSpacing(10);
    QStringList flowItems = {"天然气流量1", "天然气流量2", "氧气流量1", "氧气流量2"};
    QStringList flowCols = {"流量监测值", "电流采集值"};
    // 表头
    flowDataLayout->addWidget(new QLabel(""), 0, 0);
    for(int i=0; i<flowCols.size(); i++){
        QLabel *lab = new QLabel(flowCols[i]);
        lab->setStyleSheet("font-weight:bold; border:1px solid black; padding:5px;");
        lab->setAlignment(Qt::AlignCenter);
        flowDataLayout->addWidget(lab, 0, i+1);
    }
    // 数据行
    for(int row=0; row<flowItems.size(); row++){
        QLabel *itemLab = new QLabel(flowItems[row]);
        itemLab->setStyleSheet("border:1px solid black; padding:5px;");
        flowDataLayout->addWidget(itemLab, row+1, 0);
        for(int col=0; col<2; col++){
            QLabel *valLab = new QLabel("0.00");
            valLab->setStyleSheet("border:1px solid black; padding:5px;");
            valLab->setAlignment(Qt::AlignCenter);
            flowDataLayout->addWidget(valLab, row+1, col+1);
            m_flowLabels.append(valLab);
        }
    }
    midLayout->addWidget(flowDataBox);
    midWidget->setLayout(midLayout);
    mainLayout->addWidget(midWidget, 1);

    // ===================== 右侧：信号采集功能区（压力+温度） =====================
    QWidget *rightWidget = new QWidget();
    QVBoxLayout *rightLayout = new QVBoxLayout(rightWidget);
    rightLayout->setSpacing(15);

    // --- 压力采集 ---
    QGroupBox *pressureBox = new QGroupBox("压力采集");
    QGridLayout *pressureLayout = new QGridLayout(pressureBox);
    pressureLayout->setSpacing(10);
    QStringList pressureItems = {
        "天然气进气压力", "氧气进气压力", "天然气稳压腔压力", "氧气稳压腔压力",
        "冷却水进水压力", "冷却水回水压力", "冷却水管路压力", "空气管路压力"
    };
    QStringList pressureCols = {"压力监测值", "电流采集值"};
    // 表头
    pressureLayout->addWidget(new QLabel(""), 0, 0);
    for(int i=0; i<pressureCols.size(); i++){
        QLabel *lab = new QLabel(pressureCols[i]);
        lab->setStyleSheet("font-weight:bold; border:1px solid black; padding:5px;");
        lab->setAlignment(Qt::AlignCenter);
        pressureLayout->addWidget(lab, 0, i+1);
    }
    // 数据行
    for(int row=0; row<pressureItems.size(); row++){
        QLabel *itemLab = new QLabel(pressureItems[row]);
        itemLab->setStyleSheet("border:1px solid black; padding:5px;");
        pressureLayout->addWidget(itemLab, row+1, 0);
        for(int col=0; col<2; col++){
            QLabel *valLab = new QLabel("0.00");
            valLab->setStyleSheet("border:1px solid black; padding:5px;");
            valLab->setAlignment(Qt::AlignCenter);
            pressureLayout->addWidget(valLab, row+1, col+1);
            m_pressureLabels.append(valLab);
        }
    }
    rightLayout->addWidget(pressureBox);

    // --- 温度采集 ---
    QGroupBox *tempBox = new QGroupBox("温度采集");
    QGridLayout *tempLayout = new QGridLayout(tempBox);
    tempLayout->setSpacing(10);
    QStringList tempItems = {
        "天然气稳压腔温度", "氧气稳压腔温度", "冷却水进水温度", "冷却水管路温度"
    };
    QStringList tempCols = {"温度监测值", "电流采集值"};
    // 表头
    tempLayout->addWidget(new QLabel(""), 0, 0);
    for(int i=0; i<tempCols.size(); i++){
        QLabel *lab = new QLabel(tempCols[i]);
        lab->setStyleSheet("font-weight:bold; border:1px solid black; padding:5px;");
        lab->setAlignment(Qt::AlignCenter);
        tempLayout->addWidget(lab, 0, i+1);
    }
    // 数据行
    for(int row=0; row<tempItems.size(); row++){
        QLabel *itemLab = new QLabel(tempItems[row]);
        itemLab->setStyleSheet("border:1px solid black; padding:5px;");
        tempLayout->addWidget(itemLab, row+1, 0);
        for(int col=0; col<2; col++){
            QLabel *valLab = new QLabel("0.00");
            valLab->setStyleSheet("border:1px solid black; padding:5px;");
            valLab->setAlignment(Qt::AlignCenter);
            tempLayout->addWidget(valLab, row+1, col+1);
            m_tempLabels.append(valLab);
        }
    }
    rightLayout->addWidget(tempBox);
    rightWidget->setLayout(rightLayout);
    mainLayout->addWidget(rightWidget, 1);

    m_deviceTestDialog->setLayout(mainLayout);

    // 初始化数据刷新定时器（1秒刷新一次）
    m_deviceTestTimer = new QTimer(this);
    connect(m_deviceTestTimer, &QTimer::timeout, this, &MainWindow::updateDeviceTestData);
}

void MainWindow::updateDeviceTestData()
{
    // 1. 更新热电偶数据（3130模块，48个通道）
    // 刷新热电偶温度（完全按你的通道规则）
    if(dam3130[0] && dam3130[1] && dam3130[2] && m_thermoLabels.size() >= 48)
    {

        dam3130[0]->readAllAI();
        dam3130[1]->readAllAI();
        dam3130[2]->readAllAI();

        float tempValue[48] = {0.0f};

        // ==========================
        // 活塞1：1~12号测点 → dam3130[0] 通道 1~12
        // ==========================
        for(int i=0; i<12; i++){
            tempValue[i] = dam3130[0]->getAIValue(i+1);
        }

        // ==========================
        // 活塞2：1~12号测点 → dam3130[1] 通道 1~12
        // ==========================
        for(int i=0; i<12; i++){
            tempValue[12 + i] = dam3130[1]->getAIValue(i+1);
        }

        // ==========================
        // 活塞3：1~12号测点 → dam3130[2] 通道 1~12
        // ==========================
        for(int i=0; i<12; i++){
            tempValue[24 + i] = dam3130[2]->getAIValue(i+1);
        }

        // ==========================
        // 活塞4：1~12号测点（特殊映射）
        // ==========================
        // 活塞4 1~4  → dam3130[0] 16,15,14,13
        tempValue[36 + 0] = dam3130[0]->getAIValue(16);
        tempValue[36 + 1] = dam3130[0]->getAIValue(15);
        tempValue[36 + 2] = dam3130[0]->getAIValue(14);
        tempValue[36 + 3] = dam3130[0]->getAIValue(13);

        // 活塞4 5~8  → dam3130[1] 16,15,14,13
        tempValue[36 + 4] = dam3130[1]->getAIValue(16);
        tempValue[36 + 5] = dam3130[1]->getAIValue(15);
        tempValue[36 + 6] = dam3130[1]->getAIValue(14);
        tempValue[36 + 7] = dam3130[1]->getAIValue(13);

        // 活塞4 9~12 → dam3130[2] 16,15,14,13
        tempValue[36 + 8]  = dam3130[2]->getAIValue(16);
        tempValue[36 + 9]  = dam3130[2]->getAIValue(15);
        tempValue[36 + 10] = dam3130[2]->getAIValue(14);
        tempValue[36 + 11] = dam3130[2]->getAIValue(13);

        // 更新到界面48个标签
        for(int row = 0; row < 12; row++)
        {
           // 第1列：1~12 → 索引 0,1,2...11
           m_thermoLabels[row + 0] ->setText(QString::number(tempValue[row + 0], 'f', 1));
           // 第2列：13~24 → 索引12,13...23
           m_thermoLabels[row + 12]->setText(QString::number(tempValue[row + 12], 'f', 1));
           // 第3列：25~36 → 索引24,25...35
           m_thermoLabels[row + 24]->setText(QString::number(tempValue[row + 24], 'f', 1));
           // 第4列：37~48 → 索引36,37...47
           m_thermoLabels[row + 36]->setText(QString::number(tempValue[row + 36], 'f', 1));
         }
    }

    // 2. 更新流量数据（2601模块，4路AI）
    if(dam2601){
        dam2601->readAI_ALL();
        for(int i=0; i<4; i++){
            float flow = dam2601->getAIValue(i+1);
            float current = dam2601->getAIRawValue(i+1); // 假设电流采集在第5-8通道
            m_flowLabels[i*2]->setText(QString::number(flow, 'f', 2));
            m_flowLabels[i*2+1]->setText(QString::number(current, 'f', 2));
        }
    }

    // 3. 更新压力数据（3055模块，8路AI）
    if(dam3055){
        for(int i=0; i<8; i++){
            //float pressure = dam3055->getAIValue(i+1);
            //float current = dam3055->getAIValue(i+9); // 假设电流采集在第9-16通道
            //m_pressureLabels[i*2]->setText(QString::number(pressure, 'f', 2));
            //m_pressureLabels[i*2+1]->setText(QString::number(current, 'f', 2));
        }
    }

    // 4. 更新温度数据（3055模块，4路AI）
    if(dam3055){
        for(int i=0; i<4; i++){
            //float temp = dam3055->getAIValue(i+17); // 假设温度采集在第17-20通道
           // float current = dam3055->getAIValue(i+21);
            // m_tempLabels[i*2]->setText(QString::number(temp, 'f', 2));
            //m_tempLabels[i*2+1]->setText(QString::number(current, 'f', 2));
        }
    }
}

void MainWindow::onDeviceStatusTest()
{
    // 安全检查：必须已经连接所有模块
//    if(!btnOneKeyConnect->property("connected").toBool()){
//        QMessageBox::warning(this, "警告", "请先执行一键通讯，连接所有设备！");
//        return;
//    }
    // 1. 空指针安全检查（核心修复）
      if(!m_deviceTestDialog){
          QMessageBox::critical(this, "错误", "设备测试对话框未初始化！");
          return;
      }
      if(!m_deviceTestTimer){
          QMessageBox::critical(this, "错误", "数据刷新定时器未初始化！");
          return;
      }
    // 启动数据刷新定时器
    m_deviceTestTimer->start(1000);

    // 模态弹出对话框
    if(m_deviceTestDialog){
        m_deviceTestDialog->exec();
    }

    // 关闭对话框后停止定时器
    m_deviceTestTimer->stop();
}
