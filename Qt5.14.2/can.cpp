#include "can.h"
#include "ui_can.h"
#include "mainwindow.h"

/* 实现了 CAN 总线通信工具
 * CAN（Controller Area Network）在汽车电子和工业控制中极度常用，它是一种基于“帧”的差分总线
 *
 * 软件模拟
 *  在 Linux (Ubuntu) 中，内核提供了一个叫 vcan (Virtual CAN) 的驱动。基于 Linux 内核网络协议栈 (vcan 驱动)
 *  它不需要任何外部硬件，就能在系统内部模拟出一个完整的 CAN 总线。你可以像操作真实 can0 一样操作 vcan0
 *  如何修改代码实现模拟: 驱动模拟、屏蔽硬件指令、增加虚拟接口识别
 *
 *
 *  第一步：在 Ubuntu 中创建虚拟 CAN 接口
 *      在运行 Qt 程序前，你需要在终端输入以下3条命令（或者写在代码的模拟分支里）
 *          sudo modprobe vcan  # 加载虚拟CAN内核模块
 *          sudo ip link add dev vcan0 type vcan    # 添加一个名为 vcan0 的虚拟网卡
 *          sudo ip link set up vcan0   # 激活该网卡
 *          ifconfig vcan0  # 验证是否成功（应该能看到 vcan0 处于 UP 状态）
 *  第二步(第一次运行)：修改 can.cpp 实现适配
 *      修改连接逻辑 and 修改发送逻辑
 *      模拟测试方法
 *          安装 can-utils 工具包来观察数据: sudo apt-get install can-utils
 *  第三步：启动模拟程序：在 Ubuntu 运行 Qt 程序
 *      选择接口：在插件选择 socketcan，在接口选择 vcan0。
 *      连接：点击 Connect
 *      监听流量：在终端输入命令：candump vcan0
 *      发送测试：在 Qt 界面输入 123 aa bb cc 点击 Send
 *      结果：你会看到终端立刻刷出：vcan0  123   [3]  AA BB CC
 *      接收测试：在终端模拟另一个设备发送：cansend vcan0 555#112233
 *      你的 Qt 界面 rx_browser 会立即显示收到 ID 为 555 的数据
 */

can::can(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::can)
{
    ui->setupUi(this);
    this->setWindowTitle("CAN");
    this->setFixedSize(800,480);

    ui->rx_browser->setPlaceholderText("Time ID Length Data");
    ui->tx_edit->setText("123 aa 77 66 55");
    ui->rx_browser->show();
    ui->tx_edit->show();

    ui->send->setEnabled(false);
    ui->label_message->setText(tr("not connected!"));

    /* QCanBusDevice (CAN 设备实体)
     * 作用：这是 Qt SerialBus 模块提供的核心类，代表了一个具体的 CAN 接口（如 can0）。
     * 功能：负责建立连接、断开连接、读写数据帧（QCanBusFrame） */
    //canDevice = new QCanBusDevice(this);

#ifdef __arm__
    system("ifconfig can0 down");
    // 硬件配置 (System Commands): 由于 Qt 本身在设置比特率方面对某些驱动支持不稳，代码直接调用 Linux 系统的 ip 命令来配置物理硬件
    // 自动重启延迟为 100 毫秒
    system("ip link set up can0 type can bitrate 1000000 restart-ms 100");
#endif

    pluginItemInit();
    bandRateItemInit();
}


can::~can()
{
    canDevice->disconnectDevice();
    delete canDevice;
    delete ui;
}

/* 从系统中读取可用的插件，并显示到comboBox[0] */
void can::pluginItemInit()
{
    ui->plugin->addItems(QCanBus::instance()->plugins());
    for (int i = 0; i < QCanBus::instance()->plugins().count(); i++) {
        // SocketCAN 插件
        // 作用：在 Linux 系统中，CAN 设备被映射为网络接口。socketcan 是 Qt 与 Linux 内核 CAN 驱动通信的桥梁
        if (QCanBus::instance()->plugins().at(i) == "socketcan")
            ui->plugin->setCurrentIndex(i);
    }
}


/* 初始化一些常用的比特率，can的比特率不是随便设置的，有相应的计算公式 */
void can::bandRateItemInit()
{
    const QList<int> rates = {
        10000, 20000, 50000, 125000, 156250,
        250000, 500000, 700000 , 800000, 1000000
    };

    for (int rate : rates)
        ui->rate->addItem(QString::number(rate), rate);

    /* 默认初始化以1000000比特率 */
    ui->rate->setCurrentIndex(7);
}


static QString frameFlags(const QCanBusFrame &frame)
{
    /* 格式化接收到的消息 */
    QString result = QLatin1String(" --- ");

    if (frame.hasBitrateSwitch())
        result[1] = QLatin1Char('B');
    if (frame.hasErrorStateIndicator())
        result[2] = QLatin1Char('E');
    if (frame.hasLocalEcho())
        result[3] = QLatin1Char('L');

    return result;
}


/* 接收消息 */
// 将底层的 QCanBusFrame 对象重新“翻译”回可读文本,比如
// 1773322621.6831   ---      123   [4]  DE AD BE EF
// 1773322621.6831 (时间戳):基于 Unix 时间符（秒.微秒）的时间记录
// --- (标志位/Flags): 三个位分别代表 B (比特率切换)、E (错误状态)、L (本地回显)。由于你这是正常发送且不是 CAN-FD，所以显示为 ---
// 123 (帧 ID)：这是 CAN 报文的“身份标识”。在总线上，ID 越小优先级越高。对应代码中 cansend vcan0 123#... 的前三位
// [4] (数据长度/DLC)：代表后面跟着 4 个字节 的数据
// DE AD BE EF (数据载荷/Payload)：这是你发送的实际内容（16 进制）。对应代码中 QByteArray::fromHex() 转换后的结果
void can::receivedFrames()
{
    if (!canDevice)
        return;

    /* 读取帧 */
    while (canDevice->framesAvailable()) {
        const QCanBusFrame frame = canDevice->readFrame();
        // 帧内容: ID、长度和 16 进制数据
        QString view;
        if (frame.frameType() == QCanBusFrame::ErrorFrame)
            view = canDevice->interpretErrorFrame(frame);
        else
            view = frame.toString();

        // 时间戳: 系统收到该帧的精确时间
        const QString time = QString::fromLatin1("%1.%2  ")
                .arg(frame.timeStamp()
                     .seconds(), 10, 10, QLatin1Char(' '))
                .arg(frame.timeStamp()
                     .microSeconds() / 100, 4, 10, QLatin1Char('0'));

        // 状态位: 是否有错误或比特率切换
        const QString flags = frameFlags(frame);
        /* 接收消息框追加接收到的消息 */
        ui->rx_browser->moveCursor(QTextCursor::End);
        ui->rx_browser->insertPlainText(time + flags + view + "\n");
        ui->rx_browser->moveCursor(QTextCursor::End);
        qDebug() << time + flags + view << endl;

    }
}

void can::on_can_back_clicked()
{
    printf("on_can_back_clicked\r\n");
    QString cmd1 = tr("ifconfig %1 down")
            .arg(ui->port->currentText());

    this->close();
    MainWindow *m = new MainWindow();
    m->show();
}

void can::on_clear_clicked()
{
    printf("on_clear_clicked\r\n");
    ui->rx_browser->clear();
}

void can::on_connect_clicked()
{
    printf("on_connect_clicked\r\n");
#ifdef IS_ARM_BOARD
    // 真实的硬件初始化
    /* Qt中的QCanBusDevice::BitRateKey不能设置比特率 */
    // 只有在 ARM 板子上才执行真实的 ifconfig/ip 命令
    QString cmd1 = tr("ifconfig %1 down").arg(ui->port->currentText());
    QString cmd2 =tr("ip link set up %1 type can bitrate %2 restart-ms 100").arg(ui->port->currentText()).arg(ui->rate->currentText());
    /* 使用系统指令以设置的比特率初始化CAN */
    system(cmd1.toStdString().c_str());
    system(cmd2.toStdString().c_str());
#else
    // [SIMULATION] 在 PC 上，假设 vcan0 已经手动创建好
    qDebug() << "Simulation Mode: Using Virtual CAN (vcan0)";
    // 在 PC 上，我们假设你已经按上面的步骤手动启动了 vcan0
    qDebug() << "Connecting to local virtual interface:" << ui->port->currentText();
#endif
    if (ui->connect->text() == "Connect") {
        // 创建设备时，确保插件选的是 "socketcan"
        QString errorString;
        canDevice = QCanBus::instance()->createDevice(ui->plugin->currentText(), ui->port->currentText(), &errorString);
//        // 以下代码在所有平台通用，无需改动
//        QString errorString;
//        /* 以设置的插件名与接口实例化 canDevice */
//        canDevice = QCanBus::instance()->
//                createDevice(ui->plugin->currentText(),
//                ui->port->currentText(),
//                &errorString);

        if (!canDevice) {
            ui->label_message->setText(
                        tr("Error creating device '%1', reason: '%2'")
                        .arg(ui->plugin->currentText())
                    .arg(errorString));
            return;
        }

        /* 连接CAN */
        if (!canDevice->connectDevice()) {
            ui->label_message->setText(tr("Connection error: %1")
                              .arg(canDevice->errorString()));
            delete canDevice;
            canDevice = nullptr;

            return;
        }

        /* 处理接收到的消息 */
        connect(canDevice, SIGNAL(framesReceived()),
                this, SLOT(receivedFrames()));
//        connect(canDevice,
//                SIGNAL(errorOccurred(QCanBusDevice::CanBusError)),
//                this,
//                SLOT(canDeviceErrors(QCanBusDevice::CanBusError)));
        // 替换旧的 SIGNAL/SLOT 写法:connect 改成函数指针形式
        connect(canDevice, &QCanBusDevice::errorOccurred,
                this, &can::canDeviceErrors);

        /* 将连接信息插入到label */
        ui->label_message->setText(
                    tr("plugin: %1, connected %2, band rate: %3 kBit/s")
                    .arg(ui->plugin->currentText())
                .arg(ui->port->currentText())
                .arg(ui->rate->currentText().toInt() / 1000));
        ui->connect->setText("Disconnect");

        /* 使能/失能 */
        ui->send->setEnabled(true);
        ui->plugin->setEnabled(false);
        ui->port->setEnabled(false);
        ui->rate->setEnabled(false);
    } else {
        if (!canDevice)
            return;

        /* 断开连接 */
        canDevice->disconnectDevice();
        delete canDevice;
        canDevice = nullptr;
        ui->connect->setText("Connect");
        ui->send->setEnabled(false);
        ui->label_message->setText(tr("not connected!"));
        ui->plugin->setEnabled(true);
        ui->port->setEnabled(true);
        ui->rate->setEnabled(true);
    }
}
//#ifdef __arm__
//    if (ui->connect->text() == "Connect") {
//      /* Qt中的QCanBusDevice::BitRateKey不能设置比特率 */
//      QString cmd1 = tr("ifconfig %1 down")
//              .arg(ui->port->currentText());
//      QString cmd2 =
//              tr("ip link set up %1 type can bitrate %2 restart-ms 100")
//              .arg(ui->port->currentText())
//              .arg(ui->rate->currentText());
//      /* 使用系统指令以设置的比特率初始化CAN */
//      system(cmd1.toStdString().c_str());
//      system(cmd2.toStdString().c_str());

//      // 以下代码在所有平台通用，无需改动
//      QString errorString;
//      /* 以设置的插件名与接口实例化 canDevice */
//      canDevice = QCanBus::instance()->
//              createDevice(ui->plugin->currentText(),
//              ui->port->currentText(),
//              &errorString);

//      if (!canDevice) {
//          ui->label_message->setText(
//                      tr("Error creating device '%1', reason: '%2'")
//                      .arg(ui->plugin->currentText())
//                  .arg(errorString));
//          return;
//      }

//      /* 连接CAN */
//      if (!canDevice->connectDevice()) {
//          ui->label_message->setText(tr("Connection error: %1")
//                            .arg(canDevice->errorString()));
//          delete canDevice;
//          canDevice = nullptr;

//          return;
//      }

//      /* 处理接收到的消息 */
//      connect(canDevice, SIGNAL(framesReceived()),
//              this, SLOT(receivedFrames()));
//      connect(canDevice,
//              SIGNAL(errorOccurred(QCanBusDevice::CanBusError)),
//              this,
//              SLOT(canDeviceErrors(QCanBusDevice::CanBusError)));

//      /* 将连接信息插入到label */
//      ui->label_message->setText(
//                  tr("plugin: %1, connected %2, band rate: %3 kBit/s")
//                  .arg(ui->plugin->currentText())
//              .arg(ui->port->currentText())
//              .arg(ui->rate->currentText().toInt() / 1000));
//      ui->connect->setText("Disconnect");

//      /* 使能/失能 */
//      ui->send->setEnabled(true);
//      ui->plugin->setEnabled(false);
//      ui->port->setEnabled(false);
//      ui->rate->setEnabled(false);
//  } else {
//      if (!canDevice)
//          return;

//      /* 断开连接 */
//      canDevice->disconnectDevice();
//      delete canDevice;
//      canDevice = nullptr;
//      ui->connect->setText("Connect");
//      ui->send->setEnabled(false);
//      ui->label_message->setText(tr("not connected!"));
//      ui->plugin->setEnabled(true);
//      ui->port->setEnabled(true);
//      ui->rate->setEnabled(true);
//  }
//#endif
//}

void can::on_send_clicked()
{
    printf("on_send_clicked\r\n");
    if (!canDevice){
        printf("no find canDevice\r\n");
       return;
    }

#ifndef IS_ARM_BOARD
    // [SIMULATION] 移除 PC 上对 __arm__ 的依赖限制，让发送代码生效
    #define __arm__
#endif
    // 硬件/软件模拟是同一套发送逻辑

    /* 读取QLineEdit的文件 */
    // 输入 123 aa bb cc: ID data
    QString str = ui->tx_edit->text();
    QByteArray data = 0;
    QString strTemp = nullptr;

    /* 以空格分隔lineEdit的内容，并存储到字符串链表中 */
    QStringList strlist = str.split(' ');
    for (int i = 1; i < strlist.count(); i++) {
       strTemp = strTemp + strlist[i];
    }
    /* 将字符串的内容转为QByteArray类型 */
    // 提取数据：代码取剩余部分 aa bb cc，通过 QByteArray::fromHex() 去掉空格并转为原始字节（Byte）
    data = QByteArray::fromHex(strTemp.toLatin1());

    bool ok;
    /* 以16进制读取要发送的帧内容里第一个数据，并作为帧ID */
    // 提取 ID：代码取空格前的第一部分 123，通过 toInt(&ok, 16) 转为 16 进制整数
    int framId = strlist[0].toInt(&ok, 16);

    // 封装帧: QCanBusFrame (数据帧): 包含 ID（标识符）和 Data（最多 8 字节，代码中通过 Hex 转换得到）
    QCanBusFrame frame = QCanBusFrame(framId, data);
    /* 发送: 写入帧, 交给 SocketCAN 驱动 */
    canDevice->writeFrame(frame);

#ifndef IS_ARM_BOARD
    #undef __arm__
#endif
}

void can::on_plugin_currentIndexChanged(int index)
{
    Q_UNUSED(index);
    QList<QCanBusDeviceInfo> interfaces;
    ui->port->clear();
    /* 当我们改变插件时，我们同时需要将可用接口，从插件类型中读取出来 */
    interfaces = QCanBus::instance()
            ->availableDevices(ui->plugin->currentText());
    for (const QCanBusDeviceInfo &info : qAsConst(interfaces)) {
        ui->port->addItem(info.name());
    }
}

void can::canDeviceErrors(QCanBusDevice::CanBusError error) const
{
    /* 错误处理 */
    switch (error) {
    case QCanBusDevice::ReadError:
    case QCanBusDevice::WriteError:
    case QCanBusDevice::ConnectionError:
    case QCanBusDevice::ConfigurationError:
    case QCanBusDevice::UnknownError:
        ui->label_message->setText(canDevice->errorString());
        break;
    default:
        break;
    }
}
