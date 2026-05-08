#include "gpio.h"
#include "ui_gpio.h"
#include "mainwindow.h"
#include <linux/input.h>
#include <QDir>

// 非常典型的 Linux Sysfs 硬件交互 示例:
// 在 Linux 中，“一切皆文件”，控制 LED 亮灭本质上就是向 /sys/class/leds/xxx/brightness 文件写入 "1" 或 "0"
// 在 i.MX6ULL 这种嵌入式 Linux 设备上，驱动程序已经写好了。
// 作为应用层（Qt）开发者，你不需要写底层驱动，只需要像读写文本文件一样操作对应的路径(虚拟接口)，就能控制硬件
// 寻找该虚拟接口的标准步骤
//      1.核心命令：查看 class 目录: Linux 所有的硬件设备都按类别分好了组。要找 LED，直接去 /sys/class/leds 目录下“点名”,会看到系统中所有被内核识别的 LED 设备
//      2.进入设备目录查看控制文件:选定你想控制的设备（例如 red_led），进入其子目录:cd /sys/class/leds/red_led/ and ls
//          会看到以下关键文件：
//              # brightness：这是最重要的。读取它能知道当前亮度（0或1），写入它能控制开关。
//              # max_brightness：告诉你亮度的最大上限（通常是 1 或 255）。
//              # trigger：内核触发器（如前面提到的 heartbeat 或 none）
//      3.如何确定这就是我要找的那个“灯”
//          用 “点亮测试法”观察开发板（需要 root 权限）:
//              # 1. 解除触发器（防止内核干扰:）echo none > /sys/class/leds/red_led/trigger
//              # 2. 强制点亮: echo 1 > /sys/class/leds/red_led/brightnes
//              # 3. 强制熄灭: echo 0 > /sys/class/leds/red_led/brightness
//              # 权限不足（Permission Denied），通过修改 udev 规则来让你的 Qt 程序无需 sudo 就能直接操作这些硬件文件
//      4.深度原理
//          如果在 /sys/class/leds/ 下什么都找不到，那说明内核没加载驱动或者**设备树（Device Tree）**没配置好。
//          在 i.MX6ULL 中，这个路径是由内核根据 .dts 设备树文件生成的。可以查阅源码中的设备树文件（通常在 arch/arm/boot/dts/ 下）


// 构造函数: 在对象实例化时被系统自动调用，仅且调用一次
gpio::gpio(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::gpio)
{
    ui->setupUi(this);
    this->setWindowTitle("GPIOS Simulator");
    this->setFixedSize(800,480);

// 当使用 ARM 交叉编译器
#ifdef __arm__
    // 开发板模式：使用真实路径
    // 解除内核触发器,如果不执行这一步，内核可能会根据系统状态随时改变硬件电平，导致你的 Qt 程序虽然显示“点亮”，但物理上的灯却在闪烁或不亮
    system("echo none > /sys/class/leds/red_led/trigger");
    system("echo none > /sys/class/leds/beep/trigger");

    // 设置文件路径
    // "/sys/class/leds/red_led/brightness" 是Linux Sysfs 子系统提供的一个虚拟接口,它是硬件的“分身”：
    //      往这个文件写 "1"，物理上的 LED 红色引脚就会输出高电平，灯亮。
    //      往这个文件写 "0"，物理上的 LED 引脚输出低电平，灯灭
    // setFileName
    //      只是告诉 QFile 对象：“你以后负责管这个文件”。此时并没有真正的打开文件，只是建立了联系
    led_file.setFileName("/sys/class/leds/red_led/brightness");
    beep_file.setFileName("/sys/class/leds/beep/brightness");
#else
    // [SIMULATION] Ubuntu 模拟模式：使用 /tmp 路径
    // 软件模拟: GPIO 模拟：重定向到 /tmp 下的普通文本文件 /tmp/imx6_mock/led_brightness

    // QDir::tempPath() 是 Qt 提供的一个跨平台函数：
    //      在 Linux/Ubuntu 上：它通常返回 /tmp。/tmp 实际上是挂载在 内存 里的,重启即消失
    //      在 Windows 上：它通常返回 C:\Users\用户名\AppData\Local\Temp
    QString mockPath = QDir::tempPath() + "/imx6_mock"; // /tmp/imx6_mock
    QDir().mkpath(mockPath); // 创建模拟文件夹

    led_file.setFileName(mockPath + "/led_brightness");
    beep_file.setFileName(mockPath + "/beep_brightness");

    // 初始化模拟文件，否则 file.exists() 会返回 false
    if (!led_file.exists()) {
        led_file.open(QIODevice::WriteOnly);
        led_file.write("0");
        led_file.close();
    }
    if (!beep_file.exists()) {
        beep_file.open(QIODevice::WriteOnly);
        beep_file.write("0");
        beep_file.close();
    }
    qDebug() << "模拟文件位置: " << mockPath;
#endif

    if (!led_file.exists()) printf("no find led device\r\n");
    if (!beep_file.exists()) printf("no find beep device\r\n");
}


// 析构函数: 在对象结束其生命周期时系统自动执行析构函数
gpio::~gpio()
{
    delete ui;
}


void gpio::setGpioState(QFile &file, bool value)
{
    if (!file.exists()) return;

    // 1. 以读写模式打开文件
    if(!file.open(QIODevice::ReadWrite | QIODevice::Text)) {
        qDebug() << "Open Error:" << file.errorString();
        return;
    }

    // 2. 先读取当前状态
    QTextStream stream(&file);
    QString old_state = stream.readAll().trimmed();
    bool is_high = (old_state == "1");

    // 3. 只有当目标状态与当前状态不同时，才执行写入（保护 Flash/防止抖动）
    if (value != is_high) {
        // 重要：写入前必须清空文件内容或重置指针
        file.resize(0);
        stream << (value ? "1" : "0");
        stream.flush(); // 确保数据写入内核
        qDebug() << file.fileName() << " changed to: " << (value ? "1" : "0");
    }

    // 4. 可选：验证新状态
    file.seek(0); // 将指针移回开头准备读取
    QString new_state = stream.readAll().trimmed();
    qDebug() << "Confirmed new state:" << new_state << endl;

    // 5. 关闭文件
    file.close();
}


bool gpio::getGpioState(QFile &file)
{
    if (!file.exists()) return false;

    // 仅以只读方式打开，提高安全性
    if(!file.open(QIODevice::ReadOnly)) {
        qDebug() << "Read Error:" << file.errorString();
        return false;
    }

    QTextStream in(&file);
    // readAll().trimmed() 可以过滤掉 "1\n" 里的换行符，变成 "1"
    QString content = in.readAll().trimmed();

    file.close();

    qDebug() << file.fileName() << " current value: " << content;
    return (content == "1");
}

// Back button: back to mainwindow
void gpio::on_gpio_back_clicked()
{
    printf("on_gpio_back_clicked\r\n");
    setGpioState(led_file, 0);
    setGpioState(beep_file, 0);

    this->close();
    MainWindow *m = new MainWindow();
    m->show();
}

void gpio::on_beep_clicked(bool checked)
{
    printf("on_beep_clicked:%d\r\n", checked);

//#ifdef __arm__
//    setGpioState(beep_file, checked);
//#endif

    // [SIMULATION] 去掉这里的 #ifdef __arm__，让 PC 也能执行写入逻辑
    setGpioState(beep_file, checked);

}

void gpio::on_led_clicked(bool checked)
{
    // UI按钮点击变色:
    //  # 是通过切换背景图片实现
    //  # 最常用的方式：QSS 样式表（类似 CSS
    //  # 代码逻辑控制（信号与槽）
    printf("on_led_clicked:%d\r\n", checked);
//#ifdef __arm__
//    setGpioState(led_file, checked);
//#endif
    // [SIMULATION] 同理，去掉限制
    setGpioState(led_file, checked);
}


void gpio::keyPressEvent(QKeyEvent *event)
{
//    printf("keyPressEvent\r\n");

//#ifdef __arm__
//    if(event->key() == Qt::Key_VolumeDown) {
//        printf("Key_Enter Press\r\n");
//        ui->key->setChecked(true);
//    }
//#else
//    if(event->key() == Qt::Key_Down) {
//        printf("Key_Enter Press\r\n");
//        ui->key->setChecked(true);
//    }
//#endif

//    QWidget::keyPressEvent(event);

    // [SIMULATION] 增加对普通键盘 Enter 或 空格键的监听，方便 PC 测试
    if(event->key() == Qt::Key_VolumeDown || event->key() == Qt::Key_Space) {
        printf("Key Press Detected\r\n");
        // ui->key: 这通常是一个 QCheckBox 或者 QPushButton (设置了 checkable 属性)。
        // setChecked(true): 强制将界面上的按钮设为“选中/按下”状态。
        // 效果：当你的手指按下板子上的物理按键不松开，界面上的按钮也会同步变绿或下沉，实现**“虚实结合”**
        ui->key->setChecked(true);
    }
    // 把这个事件“还给”了父类,如界面上还有其他需要处理按键的组件（比如输入框）
    QWidget::keyPressEvent(event);

}

void gpio::keyReleaseEvent(QKeyEvent *event) {
//#ifdef __arm__
//    if(event->key() == Qt::Key_VolumeDown) {
//        printf("Key_Enter Release\r\n");
//        ui->key->setChecked(false);
//    }
//#else
//    if(event->key() == Qt::Key_Down) {
//        printf("Key_Enter Press\r\n");
//        ui->key->setChecked(false);
//    }
//#endif
//    QWidget::keyReleaseEvent(event);
    if(event->key() == Qt::Key_VolumeDown || event->key() == Qt::Key_Space) {
        printf("Key Release Detected\r\n");
        ui->key->setChecked(false);
    }
    QWidget::keyReleaseEvent(event);
}
