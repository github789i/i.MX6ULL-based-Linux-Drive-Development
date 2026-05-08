#include "uart.h"
#include "ui_uart.h"
#include "mainwindow.h"

/*
 * 实现了串口调试助手的核心功能：串口配置（波特率、数据位等）、打开/关闭、数据发送与实时接收
 * 在 i.MX6ULL 嵌入式开发中，这常用于与外部传感器或其他主控板通信
 * 软件模拟(UART 是一个双向的数据流):
 *  GPIO 模拟：重定向到 /tmp 下的普通文本文件
 *  UART 模拟：最佳方案是使用 “虚拟串口对 (Virtual Serial Port Pair)”
 *      在 Linux (Ubuntu) 中，这通常通过 socat 工具实现，它能创建两个互相连通的虚拟串口（例如 ttyS10 和 ttyS11），你往一个发，另一个就能收
 *      对于串口这种流式设备，简单的文件读写（如 GPIO 那样）无法触发 readyRead() 信号。通过 socat 模拟真实的 PTY（伪终端）
 *
 * 模拟测试方法
 *  在 Ubuntu 运行修改后的程序
 *  在下拉列表中选择 /tmp/ttyV0 并点击 Open
 *  打开 Ubuntu 终端，输入以下命令向模拟串口发送数据：echo "Hello Gemini" > /tmp/ttyV1
 *  观察：你的 Qt 界面 rx_browser（接收区）会立即显示 "Hello Gemini"
 * 反向测试：在 Qt 界面发送区输入文字并点击 Send，在终端输入 cat /tmp/ttyV1 即可看到发送的内容
 */

uart::uart(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::uart)
{
    ui->setupUi(this);
    this->setWindowTitle("UART");
    this->setFixedSize(800,480);

//#if __arm__
//    system("cd /lib/modules/4.1.15");
//    system("depmod");
//    /* 驱动加载:在 ARM 环境下通过系统命令加载 USB 转串口芯片（CH341）的驱动内核模块
//     * 确保插上 USB 串口线时，系统能识别出 /dev/ttyUSB0 之类的设备 */
//    system("modprobe ch341.ko");
//#endif

// 环境检测
#ifdef IS_ARM_BOARD  // 使用我们之前在 .pro 定义的宏
    system("modprobe ch341.ko");
#else
    // [SIMULATION] 在 Ubuntu 上启动时，自动在后台创建一个虚拟串口对
    // 创建两个串口互联：/tmp/ttyV0 <-> /tmp/ttyV1
    // [SIMULATION] 使用 nohup 确保 socat 在后台稳定运行，并增加权限设置
    // 强制创建两个虚拟串口，权限设为 777 (所有人可读写)
    system("nohup socat -d -d PTY,link=/tmp/ttyV0,raw,echo=0,mode=777 PTY,link=/tmp/ttyV1,raw,echo=0,mode=777 &");
    // 给系统一点点响应时间（可选）
    QThread::msleep(100);
    qDebug() << "Simulation: Virtual Serial Ports /tmp/ttyV0 <-> /tmp/ttyV1 created.";
#endif

    /* 串口核心对象QSerialPort
     * 作用：Qt 串口通信的实体
     * 主要方法：
     *  setPortName(): 指定串口号
     *  open() / close(): 开启或关闭硬件访问权限
     *  write(): 向串口线发送字节数据
     *  readAll(): 从接收缓冲区读出所有数据
    */
    serialPort = new QSerialPort(this);
    ui->send->setEnabled(false);

    ui->rx_browser->setPlaceholderText("Received messages...");
    ui->tx_edit->setPlaceholderText("send message.....");
    ui->rx_browser->show();
    ui->tx_edit->show();

    /* 扫描系统的串口 */
    scanSerialPort();

    /* 波特率项初始化 */
    baudRateItemInit();

    /* 数据位项初始化 */
    dataBitsItemInit();

    /* 检验位项初始化 */
    parityItemInit();

    /* 停止位项初始化 */
    stopBitsItemInit();

    /* 串口有数据就显示 */
    // 异步接收机制 (readyRead 信号)
    // 最重要的绑定: 只要串口线收到数据，系统会自动触发 readyRead 信号，从而调用 serialPortReadyRead() 函数
    // 不需要写死循环去轮询，极大节省了 CPU 资源
    connect(serialPort, SIGNAL(readyRead()), this, SLOT(serialPortReadyRead()));
}

uart::~uart()
{
    delete serialPort;

    delete ui;
}

/* 扫描系统的串口 */
void uart::scanSerialPort()
{
//    /* 查找可用串口 */
//    foreach (const QSerialPortInfo &info, QSerialPortInfo::availablePorts()) {
//        ui->port->addItem(info.portName());
//    }
    // 修改串口扫描（识别模拟串口）
    ui->port->clear();
    foreach (const QSerialPortInfo &info, QSerialPortInfo::availablePorts()) {
        ui->port->addItem(info.portName());
    }

#ifdef IS_PC_SIMULATOR
    // [SIMULATION] 强制把模拟路径加入下拉列表
    ui->port->addItem("/tmp/ttyV0");
    ui->port->addItem("/tmp/ttyV1");
#endif
}

/* 波特率项初始化 */
void uart::baudRateItemInit()
{
    /* QList链表，字符串类型 */
    QList <QString> list;
    list<<"1200"<<"2400"<<"4800"<<"9600"
       <<"19200"<<"38400"<<"57600"
      <<"115200"<<"230400"<<"460800"
     <<"921600";
    for (int i = 0; i < 11; i++) {
        ui->rate->addItem(list[i]);
    }
    ui->rate->setCurrentIndex(7);
}

/* 数据位项初始化 */
void uart::dataBitsItemInit()
{
    /* QList链表，字符串类型 */
    QList <QString> list;
    list<<"5"<<"6"<<"7"<<"8";
    for (int i = 0; i < 4; i++) {
        ui->data_bit->addItem(list[i]);
    }
    ui->data_bit->setCurrentIndex(3);
}

/* 检验位项初始化 */
void uart::parityItemInit()
{
    /* QList链表，字符串类型 */
    QList <QString> list;
    list<<"None"<<"Even"<<"Odd"<<"Space"<<"Mark";
    for (int i = 0; i < 5; i++) {
        ui->check_bit->addItem(list[i]);
    }
    ui->check_bit->setCurrentIndex(0);
}


/* 停止位项初始化 */
void uart::stopBitsItemInit()
{
    /* QList链表，字符串类型 */
    QList <QString> list;
    list<<"1"<<"2";
    for (int i = 0; i < 2; i++) {
        ui->stop_bit->addItem(list[i]);
    }
    ui->stop_bit->setCurrentIndex(0);
}


// 返回主界面
void uart::on_uart_back_clicked()
{
    printf("on_uart_back_clicked\r\n");
//    serialPort->close();

//#if __arm__
//    system("rmmod ch341.ko");
//#endif
    // 修改返回逻辑（清理进程）
    serialPort->close();
#ifndef IS_ARM_BOARD
    system("pkill socat"); // 退出时杀掉模拟进程
#endif

    this->close();
    MainWindow *m = new MainWindow();
    m->show();
}

// 开启串口
void uart::on_open_clicked()
{
    printf("on_open_clicked\r\n");
    if (ui->open->text() == "Open") {
          /* 设置串口名 */
          serialPort->setPortName(ui->port->currentText());
          /* 设置波特率 */
          serialPort->setBaudRate(ui->rate->currentText().toInt());
          /* 设置数据位数 */
          switch (ui->data_bit->currentText().toInt()) {
          case 5:
              serialPort->setDataBits(QSerialPort::Data5);
              break;
          case 6:
              serialPort->setDataBits(QSerialPort::Data6);
              break;
          case 7:
              serialPort->setDataBits(QSerialPort::Data7);
              break;
          case 8:
              serialPort->setDataBits(QSerialPort::Data8);
              break;
          default: break;
          }
          /* 设置奇偶校验 */
          switch (ui->check_bit->currentIndex()) {
          case 0:
              serialPort->setParity(QSerialPort::NoParity);
              break;
          case 1:
              serialPort->setParity(QSerialPort::EvenParity);
              break;
          case 2:
              serialPort->setParity(QSerialPort::OddParity);
              break;
          case 3:
              serialPort->setParity(QSerialPort::SpaceParity);
              break;
          case 4:
              serialPort->setParity(QSerialPort::MarkParity);
              break;
          default: break;
          }
          /* 设置停止位 */
          switch (ui->stop_bit->currentText().toInt()) {
          case 1:
              serialPort->setStopBits(QSerialPort::OneStop);
              break;
          case 2:
              serialPort->setStopBits(QSerialPort::TwoStop);
              break;
          default: break;
          }
          /* 设置流控制 */
          serialPort->setFlowControl(QSerialPort::NoFlowControl);
          if (!serialPort->open(QIODevice::ReadWrite))
              QMessageBox::warning(NULL, "error",
                                 "The serial port cannot be opened! \r\n"
                                 "Maybe the serial port is already occupied!");
          else {
              ui->port->setEnabled(false);
              ui->rate->setEnabled(false);
              ui->check_bit->setEnabled(false);
              ui->stop_bit->setEnabled(false);
              ui->data_bit->setEnabled(false);
              ui->open->setText("Close");
              ui->send->setEnabled(true);
          }
      } else {
          serialPort->close();
          ui->port->setEnabled(true);
          ui->rate->setEnabled(true);
          ui->check_bit->setEnabled(true);
          ui->stop_bit->setEnabled(true);
          ui->data_bit->setEnabled(true);
          ui->open->setText("Open");
          ui->send->setEnabled(false);
      }
}

// 只要串口线收到数据，系统会自动触发 readyRead 信号，从而调用 serialPortReadyRead() 函数
void uart::serialPortReadyRead()
{
    /* 接收缓冲区中读取数据 */
    QByteArray buf = serialPort->readAll();

    ui->rx_browser->moveCursor(QTextCursor::End);
    ui->rx_browser->insertPlainText(QString(buf));
    ui->rx_browser->moveCursor(QTextCursor::End);

    qDebug() << buf << endl;
}

// 发送Send按钮
void uart::on_send_clicked()
{
    printf("on_send_clicked\r\n");
    /* 获取textEdit数据,转换成utf8格式的字节流 */
    QByteArray data = ui->tx_edit->toPlainText().toUtf8();
    serialPort->write(data);
    qDebug() << data << endl;
}

// 清除接收区按钮
void uart::on_clear_clicked()
{
    printf("on_clear_clicked\r\n");
    ui->rx_browser->clear();
}
