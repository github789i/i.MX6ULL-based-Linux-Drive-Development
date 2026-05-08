#include "ap3216c.h"
#include "ui_ap3216c.h"
#include "mainwindow.h"

/* ap3216c实验模块,环境光 & 接近传感器 (I2C 接口)
    实现了一个三合一传感器的监控界面，
    通过 I2C 总线读取环境光（ALS）、接近程度（PS）和红外强度（IR），
    并使用 Qt Charts 模块将其可视化*/

ap3216c::ap3216c(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::ap3216c)
{
    ui->setupUi(this);
    this->setWindowTitle("Ap3216c");
    this->setFixedSize(800,480);

    ap3216c_timer = new QTimer();
#if __arm__
    system("cd /lib/modules/4.1.15");
    system("depmod");
    // 硬件驱动加载,在 ARM 环境下安装内核模块。AP3216C 是 I2C 设备，内核驱动会将其识别为一个字符设备
    system("modprobe ap3216c.ko");
#endif

    connect(ap3216c_timer, SIGNAL(timeout()), this, SLOT(ap3216c_timer_timeout()));

#if __arm__
    // filename: /dev/ap3216c
    fd = open(filename, O_RDWR);
    if(fd < 0) {
        printf("can't open file %s\r\n", filename);
        return;
    }
#endif

    ap3216c_create_chart();

    // 定时刷新：ap3216c_timer 设置为 200ms 触发一次，实现数据的“准实时”动态更新
    ap3216c_timer->start(500);
}


ap3216c::~ap3216c()
{
    delete ap3216c_timer;
    delete ui;
}

// 图表可视化
void ap3216c::ap3216c_create_chart()
{
    // Qt Charts 的核心，负责管理坐标轴和数据系列
    chart = new QChart();
    mAxY = new QValueAxis();

    // y轴范围
    mAxY->setRange(0, 1024);

    // Y轴分等份
    mAxY->setTickCount(9);
    mAxY->setLabelFormat("%d");

    chart->addAxis(mAxY, Qt::AlignLeft);

    // 柱状图: ALSBar、PSBar 和 IRBar 分别代表三根柱子
    ALSBar = new QBarSet("ALS");
    PSBar = new QBarSet("PS");
    IRBar = new QBarSet("IR");

    *ALSBar << 0;
    *PSBar << 0;
    *IRBar << 0;

    mBarseries = new QBarSeries();
    mBarseries->append(ALSBar);
    mBarseries->append(PSBar);
    mBarseries->append(IRBar);

    mBarseries->attachAxis(mAxY);

    QFont font = chart->legend()->font();
    font.setPointSizeF(20);
    chart->legend()->setFont(font);


    chart->addSeries(mBarseries);
    chart->setAnimationOptions(QChart::SeriesAnimations);
    chart->legend()->setAlignment(Qt::AlignBottom);

    // 将图表绑定到视图 wiget 为 QChartView
    ui->ap3216c_chart->setChart(chart);

}

// 数据读取逻辑
void ap3216c::get_ap3216c_data()
{
    int ret = 0;
    // 典型的 Linux C 应用层读取驱动数据的方式
    // 驱动程序通常会将 16 位的传感器原始数据封装在 databuf 数组
    ret = read(fd, databuf, sizeof(databuf));
    if(ret == 0) { 			/* 数据读取成功 */
        IR =  databuf[0]; 	/* IR：Infrared Sensor（红外传感器） */
        ALS = databuf[1]; 	/* ALS：Ambient Light Sensor（环境光传感器），数值越大环境越亮 */
        PS =  databuf[2]; 	/* PS：Proximity Sensor（接近传感器），数值越大物体离传感器越近 */
    }

    printf("IR=%d, ALS=%d, PS=%d\r\n", IR, ALS, PS);
}


void ap3216c::ap3216c_timer_timeout()
{
//#if __arm__
//    get_ap3216c_data();
//    // mBarseries->detachAxis(mAxY);
//    mBarseries->attachAxis(mAxY);
//    ALSBar->insert(0,ALS);
//    PSBar->insert(0,PS);
//    IRBar->insert(0,IR);
//    ui->ap3216c_chart->update();
//#else
//    // 代码中已经预留了模拟逻辑:传感器通常模拟的是其数值变化
//    ALS = QRandomGenerator::global()->bounded(1024);
//    PS = QRandomGenerator::global()->bounded(1024);
//    IR = QRandomGenerator::global()->bounded(1024);

//    // mBarseries->detachAxis(mAxY);
//    mBarseries->attachAxis(mAxY);

//    ALSBar->insert(0,ALS);
//    PSBar->insert(0,PS);
//    IRBar->insert(0,IR);

//    ui->ap3216c_chart->update();
//#endif

    // 模拟得更真实（例如模拟手靠近传感器导致 PS 值升高的过程）
#if __arm__
    get_ap3216c_data();
#else
    // [SIMULATION] 模拟数值逻辑更新
    static int direction = 1;
    static int mock_val = 100;

    // 模拟 PS (接近传感器) 的往复变化
    mock_val += (5 * direction);
    if (mock_val > 800 || mock_val < 50) direction *= -1;

    ALS = 500 + QRandomGenerator::global()->bounded(50); // 模拟稳定的室内光线
    PS = mock_val;                                       // 模拟物体靠近又离开
    IR = 200;                                            // 静态红外背景
#endif

    // 更新图表（通用逻辑）
    // 先移除旧数据，确保柱子高度实时变化
    ALSBar->remove(0, 1);
    PSBar->remove(0, 1);
    IRBar->remove(0, 1);

    ALSBar->insert(0, ALS);
    PSBar->insert(0, PS);
    IRBar->insert(0, IR);

    // 自动调整 Y 轴上限（可选优化）
    if (PS > mAxY->max()) mAxY->setRange(0, PS + 100);
    mBarseries->attachAxis(mAxY);

    ui->ap3216c_chart->update();
}

void ap3216c::on_ap3216c_back_clicked()
{
    printf("on_ap3216c_back_clicked\r\n");
    ap3216c_timer->stop();

#if __arm__
    system("rmmod ap3216c.ko");
#endif

    this->close();
    MainWindow *m = new MainWindow();
    m->show();
}
