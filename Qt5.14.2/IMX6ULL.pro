# 模块引入 (QT += ...): 决定了你的程序可以使用哪些 Qt 库
#    core gui: 基础核心库和图形界面库。
#    svg: 支持矢量图形，通常用于图标显示。
#    charts: 关键组件！用于绘制传感器数据（如 ADC 采样、陀螺仪曲线）的波形图。
#    serialport serialbus:
#        serialport: 对应 UART 串口通信。
#        serialbus: 对应 CAN 总线通信。这是工业开发板的核心模块
QT       += core gui svg
QT       += charts
QT       += serialport serialbus


# 跨版本兼容: 如果 Qt 版本大于 4（现在基本是 Qt 5 或 6），则引入 widgets 模块。这是因为从 Qt 5 开始，界面组件被独立到了这个模块中
greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

# 允许使用 C++11 标准（如 Lambda 表达式）
CONFIG += c++11

# The following define makes your compiler emit warnings if you use
# any Qt feature that has been marked deprecated (the exact warnings
# depend on your compiler). Please consult the documentation of the
# deprecated API in order to know how to port your code away from it.
DEFINES += QT_DEPRECATED_WARNINGS

# You can also make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
# You can also select to disable deprecated APIs only up to a certain version of Qt.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

# 文件管理
#    SOURCES: 所有的 .cpp 实现文件。
#    HEADERS: 所有的 .h 头文件。
#    FORMS: 所有的 .ui 界面文件。
#       注意：这里的每一个 .ui 文件在编译时都会自动生成一个 ui_xxxx.h。例如 gpio.ui 会生成 ui_gpio.h，也就是你在 gpio.cpp 里 #include 的那个
SOURCES += \
    LayoutSquare.cpp \
    WidgetADI.cpp \
    adc.cpp \
    ap3216c.cpp \
    ble.cpp \
    camera.cpp \
    can.cpp \
    config.cpp \
    gpio.cpp \
    icm20608.cpp \
    main.cpp \
    mainwindow.cpp \
    music_player.cpp \
    qfi_ADI.cpp \
    recording.cpp \
    showphoto.cpp \
    sketchpad.cpp \
    uart.cpp \
    video.cpp

HEADERS += \
    LayoutSquare.h \
    WidgetADI.h \
    adc.h \
    ap3216c.h \
    ble.h \
    camera.h \
    can.h \
    config.h \
    gpio.h \
    icm20608.h \
    mainwindow.h \
    music_player.h \
    qfi_ADI.h \
    recording.h \
    showphoto.h \
    sketchpad.h \
    uart.h \
    video.h

FORMS += \
    WidgetADI.ui \
    adc.ui \
    ap3216c.ui \
    ble.ui \
    camera.ui \
    can.ui \
    config.ui \
    gpio.ui \
    icm20608.ui \
    mainwindow.ui \
    music_player.ui \
    recording.ui \
    showphoto.ui \
    sketchpad.ui \
    uart.ui \
    video.ui

# Default rules for deployment.
# 部署路径: 当你执行 make install 时，编译好的可执行文件会被安装到开发板的 /opt/项目名/bin 目录下。这是 Linux 软件安装的常用规范路径。
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target


# 资源打包: 将图片（如按钮图标）和字体文件“压缩”并编译到二进制程序内部。
# 意义：在嵌入式开发板上，不需要在文件系统里到处找图片路径，程序跑起来后直接从内存中读取图标，避免了因路径问题导致的“图标裂掉”
RESOURCES += \
    fonts.qrc \
    images.qrc

# 条件编译逻辑
# 自动检测架构：如果是 arm 架构则定义 ARM_BOARD，否则定义 PC_SIMULATOR
    # 电脑调试：QMake 检测到 x86 环境，开启模拟宏，mainwindow.cpp UI 适配 1150x480。
    # 板子运行：QMake 检测到 ARM 环境，开启实机宏，程序去读取 /sys/class/ 下的真实驱动。
contains(QT_ARCH, arm) {
    DEFINES += IS_ARM_BOARD
    message("Build for I.MX6ULL (ARM)")
} else {
    DEFINES += IS_PC_SIMULATOR
    message("Build for Ubuntu (x86_64) - Simulation Mode")
}
