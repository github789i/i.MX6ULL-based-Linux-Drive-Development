#include "config.h"
#include "mainwindow.h"
#include "ui_config.h"

/* 实现了一个系统背光调节功能。通过控制 Linux 系统的 backlight 驱动节点来改变 LCD 屏幕的亮度，并同步更新 UI 上的图标
    软件模拟:
        与 GPIO 的模拟逻辑完全一致，都是通过读写 /sys 路径下的某个文本文件来改变状态。
        在 Ubuntu 上，我们同样可以将这个路径重定向到 /tmp 目录下的模拟文件*/

extern int brightness;

config::config(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::config)
{
    ui->setupUi(this);
    this->setWindowTitle("Config");
    this->setFixedSize(800,480);

//#ifdef __arm__
//    // 背光驱动节点 (Sysfs Interface): "/sys/devices/platform/backlight/backlight/backlight/brightness"
//    // Linux 内核提供的标准接口。向这个文件写入数字（如 1-7），内核驱动就会改变硬件 PWM 的占空比，从而改变屏幕亮度
//    brightness_file.setFileName("/sys/devices/platform/backlight/backlight/backlight/brightness");
//    if (!brightness_file.exists())
//        printf("no find brightness device\r\n");

//    if(!brightness_file.open(QIODevice::ReadWrite))
//        qDebug()<<brightness_file.errorString();

//    QTextStream in(&brightness_file);

//    /* 读取文件所有数据 */
//    QString buf = in.readLine();

//    /* 打印出读出的值 */
//    qDebug()<< brightness_file.fileName() << " = " << buf << endl;
//    brightness_file.close();

//    // QSlider (滑动条): 提供用户交互界面。滑动条的取值范围通常在 Qt Designer 中设置为 0-6（对应背光等级 1-7）
//    ui->brightness_slider->setValue(buf.toInt()-1);
//    brightness = buf.toInt();
//#endif

//}

#ifdef __arm__
    // 真实的 ARM 开发板路径
    brightness_file.setFileName("/sys/devices/platform/backlight/backlight/backlight/brightness");
#else
    // [SIMULATION] Ubuntu 模拟模式：使用 /tmp 路径
    QString mockPath = QDir::tempPath() + "/imx6_mock/backlight";
    QDir().mkpath(mockPath); // 创建目录

    brightness_file.setFileName(mockPath + "/brightness");

    // 如果模拟文件不存在，初始化一个默认亮度 (例如 4)
    if (!brightness_file.exists()) {
        if (brightness_file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            brightness_file.write("4");
            brightness_file.close();
        }
    }
    qDebug() << "背光模拟文件位置: " << brightness_file.fileName();
#endif

    // 统一的初始化读取逻辑
    if (brightness_file.exists()) {
        if (brightness_file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QTextStream in(&brightness_file);
            QString buf = in.readLine();    /* 读取文件所有数据 */
            brightness_file.close();

            int currentVal = buf.toInt();
            brightness = (currentVal > 0) ? currentVal : 1; // 确保不为0
            ui->brightness_slider->setValue(brightness - 1);    // QSlider (滑动条): 提供用户交互界面
        }
    } else {
        printf("no find brightness device\r\n");
    }
}


config::~config()
{
    delete ui;
}

void config::setBrightness(QFile &file, int value)
{
    /* 如果文件不存在，则返回 */
    if (!file.exists())
        return;

    if(!file.open(QIODevice::ReadWrite))
        qDebug()<<file.errorString();

    QByteArray buf[7] = {"1", "2", "3", "4", "5", "6", "7"};
    file.write(buf[value]);
    printf("set brightness = %d\r\n", buf[value].toInt());

    /* 关闭文件 */
    file.close();
}


void config::on_config_back_clicked()
{
    printf("on_config_back_clicked\r\n");
    this->close();
    MainWindow *m = new MainWindow();
    m->show();
}

// 信号槽: 在用户拖动滑块时实时触发 on_brightness_slider_valueChanged
void config::on_brightness_slider_valueChanged(int value)
{
//#ifdef __arm__
//    setBrightness(brightness_file, value);
//    brightness = value+1;
//#else
//    printf("set brightness = %d\r\n", value+1);
//    brightness = value+1;
//#endif

    // 不再区分平台，统一调用 setBrightness
    // 在 PC 上它会写 /tmp，在 ARM 上它会写 /sys
    setBrightness(brightness_file, value);
    brightness = value + 1;

    // 动态图标切换: 根据亮度等级（brightness 变量），程序会从资源文件 :/images/ 中加载不同的 SVG 图标
    QString fileName = QString(":/images/brightness_%1.svg").arg(brightness - 1);
    QPixmap pixmap(fileName);
    ui->brightness->setPixmap(pixmap);
    ui->brightness->setFixedSize(64, 64);
}

// 界面同步: 当用户从主界面进入设置界面时，确保滑块的位置和图标与当前的全局变量 brightness 保持一致
void config::showEvent(QShowEvent *event) {
    QMainWindow::showEvent(event);

    QString fileName = QString(":/images/brightness_%1.svg").arg(brightness - 1);
    QPixmap pixmap(fileName);
    ui->brightness->setPixmap(pixmap);
    ui->brightness->setFixedSize(64, 64);
    ui->brightness_slider->setValue(brightness - 1);
}
