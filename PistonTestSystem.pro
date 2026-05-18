# =================== 项目名称 ===================
QT       += core gui serialbus serialport multimedia sql xlsx charts printsupport

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = PistonTestSystem
TEMPLATE = app


DEFINES += QT_DEPRECATED_WARNINGS

# You can also make your code fail to compile if you use deprecated APIs.
# In order to do so, uncomment the following line.
# You can also select to disable deprecated APIs only up to a certain version of Qt.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

CONFIG += c++11

# =================== 源文件 ===================
SOURCES += main.cpp \
           mainwindow.cpp \
           utils.cpp \
           modules/modbusbase.cpp \
           modules/dam2601modbus.cpp \
           modules/dam3055modbus.cpp \
           modules/dam3130modbus.cpp \
           modules/ssrdiomodbus.cpp \
           modules/waterinvmodbus.cpp \
           modules/plccontroller.cpp \
           qcustomplot.cpp \
           IgnitionPlotWidget.cpp \
           subcurve.cpp

# =================== 头文件 ===================
HEADERS += mainwindow.h \
           utils.h \
           modules/modbusbase.h \
           modules/dam2601modbus.h \
           modules/dam3055modbus.h \
           modules/dam3130modbus.h \
           modules/ssrdiomodbus.h \
           modules/waterinvmodbus.h \
           modules/plccontroller.h \
           qcustomplot.h \
           IgnitionPlotWidget.h \
           subcurve.h

# =================== UI 文件（如果你用 .ui 文件可以添加） ===================
# FORMS += mainwindow.ui

# =================== 资源文件（可选） ===================
# RESOURCES += resources.qrc

# =================== 其他配置 ===================
# 输出目录
DESTDIR = bin

# 包含路径（可选，如果模块放在 modules/ 下）
INCLUDEPATH += modules

RESOURCES += \
    image.qrc
