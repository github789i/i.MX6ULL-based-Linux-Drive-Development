#include "mainwindow.h"
#include "ui_mainwindow.h"

extern int scrollBarValue;

/*
 * 嵌入式设备的**“主桌面（Launcher）”**
 * 它负责展示功能图标、处理触摸屏的滑动翻页，以及通过“长按”进入各个子功能模块
*/

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    this->setWindowTitle("I.MX6ULL Embedded System Simulator");
    // 设置了固定大小
    // this->setFixedSize(1130,480);

#ifdef IS_ARM_BOARD
    // 实机运行：强制固定尺寸适配 7 寸屏
    this->setFixedSize(1130, 480);
#else
    // PC 模拟：允许调整大小，或者设置一个 Ubuntu 屏幕放得下的尺寸
    this->resize(1150, 480);
    // 额外修复：如果 Ubuntu 缩放导致模糊，可以在 main.cpp 加 DPI 支持（见上条建议）
    qDebug() << "PC Simulation Mode Active";
#endif

    // 界面滚动容器
    ui->scrollArea->setWidget(ui->widget);
    ui->scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    ui->scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    // ui->scrollArea->setGeometry(0, 0, 800, 480);
    // 将 setGeometry 改为跟随窗口宽度
    ui->scrollArea->setGeometry(0, 0, this->width(), this->height());

    ui->widget->setMinimumSize(width(), height());

    // 长按检测机制,为了防止误触，项目设计为长按图标 0.4 秒才进入功能
    pressTimer = new QTimer();
    pressTimer->setInterval(400);
    pressTimer->setSingleShot(true);
    // 长按逻辑的“绑定”: connect 绑定, 计时器溢出，自动调用 handleLongPress()
    connect(pressTimer, &QTimer::timeout, this, &MainWindow::handleLongPress);

    // 初始化 13 个功能按钮，并放入 scrollArea
    connectButton(ui->camera);
    connectButton(ui->sketchpad);
    connectButton(ui->adc);
    connectButton(ui->ble);     // 蓝牙低功耗 (Bluetooth Low Energy)
    connectButton(ui->can);
    connectButton(ui->gpio);
    connectButton(ui->uart);
    connectButton(ui->ap3216c);     // 环境光 & 接近传感器 (I2C 接口)
    connectButton(ui->icm20608);    // 六轴加速度计 & 陀螺仪 (SPI 接口)
    connectButton(ui->music_player);
    connectButton(ui->video);
    connectButton(ui->config);
    connectButton(ui->recording);
}

// 析构函数
MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::mousePressEvent(QMouseEvent *event){
    lastPos = event->pos();
}

// 触摸滑动模拟:左右拖动
void MainWindow::mouseMoveEvent(QMouseEvent *event){
    int dx = event->pos().x() - lastPos.x();
    ui->scrollArea->horizontalScrollBar()->setValue(ui->scrollArea->horizontalScrollBar()->value() - dx / 2);
    lastPos = event->pos();
}

// 页面跳转逻辑: 若按下超过 400ms，执行 handleLongPress，识别 currentButton，进入对应子窗口
// 路由中枢。它根据当前按下的按钮对象，决定销毁主界面并跳转到哪个硬件实验模块（如 adc、gpio、music_player 等）
void MainWindow::handleLongPress() {
    printf("%s btn was long pressed\r\n",
           currentButton->text().toStdString().c_str());

    if (currentButton == ui->camera) {
        this->close();              // 销毁主界面
        camera *c = new camera();   // 并跳转到camera实验模块   o
        c->show();
    } else if (currentButton == ui->sketchpad){
        this->close();
        sketchpad *c = new sketchpad();   // 并跳转到sketchpad实验模块  o
        c->show();
    } else if (currentButton == ui->ap3216c){
        this->close();
        ap3216c *c = new ap3216c();   // 并跳转到ap3216c实验模块,环境光 & 接近传感器 (I2C 接口) o
        c->show();
    } else if (currentButton == ui->icm20608){
        this->close();
        icm20608 *c = new icm20608();   // 并跳转到icm20608实验模块,六轴加速度计 & 陀螺仪 (SPI 接口) o
        c->show();
    } else if (currentButton == ui->uart){
        this->close();
        uart *c = new uart();   // 并跳转到UART实验模块 o
        c->show();
    } else if (currentButton == ui->music_player){
        this->close();
        music_player *c = new music_player();   // 并跳转到music_player实验模块 o
        c->show();
    } else if (currentButton == ui->gpio){
        this->close();
        printf("entering gpio module...");
        gpio *c = new gpio();   // 并跳转到gpio实验模块 o
        c->show();
    } else if (currentButton == ui->can){
        this->close();
        can *c = new can();   // 并跳转到CAN实验模块 o
        c->show();
    } else if (currentButton == ui->ble){
        this->close();
        ble *c = new ble();   // 并跳转到BLE实验模块    x
        c->show();
    } else if (currentButton == ui->adc){
        this->close();
        adc *c = new adc();   // 并跳转到ADC实验模块    o
        c->show();
    } else if (currentButton == ui->video){
        this->close();
        video *c = new video();   // 并跳转到Video实验模块  o
        c->show();
    } else if (currentButton == ui->config){
        this->close();
        config *c = new config();   // 并跳转到config实验模块 o
        c->show();
    } else if (currentButton == ui->recording){
        this->close();
        recording *c = new recording();   // 并跳转到recoding实验模块   x
        c->show();
    }
}

// 按下图标：connectButton 捕获按下信号，启动 pressTimer
void MainWindow::connectButton(QPushButton *button) {
    // 最关键的逻辑绑定: connect 绑定
    connect(button, &QPushButton::pressed, this, [this, button]() {
        currentButton = button;
        pressTimer->start();
    });

    connect(button, &QPushButton::released, this, [this, button]() {
        Q_UNUSED(button);
        if (pressTimer->isActive()) {
            pressTimer->stop();
        }
        currentButton = nullptr;
    });
}


// 状态保存：关闭主界面时，closeEvent 会把当前的滚动位置存入全局变量 scrollBarValue，以便下次回来时桌面还在原来的位置
void MainWindow::closeEvent(QCloseEvent *event) {
    scrollBarValue = ui->scrollArea->horizontalScrollBar()->value();
    QMainWindow::closeEvent(event);
}


void MainWindow::showEvent(QShowEvent *event) {
    QMainWindow::showEvent(event);

    ui->scrollArea->hide();

    // 使用 QTimer::singleShot 延迟设置滚动条的值
    QTimer::singleShot(0, this, [this]() {
        ui->scrollArea->horizontalScrollBar()->setValue(scrollBarValue);
        ui->scrollArea->show();
    });
}
