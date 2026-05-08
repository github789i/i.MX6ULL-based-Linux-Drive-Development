#include "adc.h"
#include "mainwindow.h"
#include "ui_adc.h"

/* ADC实验模块
    实现了一个电压实时监控系统。通过读取 Linux 的 IIO (Industrial I/O) 子系统文件来获取模拟信号转换后的数值，并将其绘制成动态折线图
    软件模拟有两种模拟方式：
        纯数值模拟（代码内）：代码中的 #else 分支已经使用了 QRandomGenerator，但这会让曲线看起来像剧烈的噪声。
        文件级模拟（系统级）：类似于 GPIO 实验，创建两个真实的文本文件，模拟 Linux 的 IIO 接口。可以通过修改文件内容，观察 Qt 界面曲线的起伏
            计算公式验证: $$adc\_value = raw\_buf.toFloat() * scale\_buf.toFloat() / 1000;$$
            按照模拟文件写入的 $2048 \times 0.8055 / 1000$，你的图表上应该会显示一条 1.65V 左右的水平直线
*/

adc::adc(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::adc)
{
    ui->setupUi(this);
    this->setWindowTitle("ADC");
    this->setFixedSize(800,480);

    adc_timer = new QTimer();

#ifdef __arm__
    // IIO 子系统接口 (硬件驱动层)
    // 直接利用 QFile 读取这些系统文件。这是 Linux 驱动暴露给应用层的最标准接口
    // in_voltage1_raw：读取到的原始数字值（例如 12 位 ADC 范围是 0-4095）
    adc_raw_file.setFileName("/sys/bus/iio/devices/iio:device0/in_voltage1_raw");
    if (!adc_raw_file.exists()){
        printf("no find adc device\r\n");
        QMessageBox::warning(this, "Open failed", "can't open adc_raw_file\r\n");
    }

    // in_voltage_scale：刻度因子。ADC 转换公式为：$电压 (V) = (原始值 \times 刻度) / 1000
    adc_scale_file.setFileName("/sys/bus/iio/devices/iio:device0/in_voltage_scale");
    if (!adc_scale_file.exists()){
        printf("no find adc device\r\n");
        QMessageBox::warning(this, "Open failed", "can't open adc_scale_file\r\n");
    }
#else
    // 文件级模拟（系统级
    // [SIMULATION] Ubuntu 模拟模式：自动化创建虚拟 IIO 设备文件
    QString mockPath = QDir::tempPath() + "/imx6_mock/iio";
    QDir().mkpath(mockPath); // 递归创建目录 /tmp/imx6_mock/iio

    adc_raw_file.setFileName(mockPath + "/in_voltage1_raw");
    adc_scale_file.setFileName(mockPath + "/in_voltage_scale");

    // 初始化原始值文件（模拟 12 位 ADC 中位值：2048）
    if (!adc_raw_file.exists()) {
        if (adc_raw_file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            adc_raw_file.write("2048");
            adc_raw_file.close();
        }
    }
    // 初始化比例文件（模拟 3.3V 逻辑：3300mV / 4096 ≈ 0.805）
    if (!adc_scale_file.exists()) {
        if (adc_scale_file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            adc_scale_file.write("0.805566");
            adc_scale_file.close();
        }
    }
    qDebug() << "ADC 模拟文件已就绪: " << mockPath;
#endif
    // 检查文件是否存在（不论是真硬件还是模拟文件）
    if (!adc_raw_file.exists() || !adc_scale_file.exists()){
        printf("no find adc device\r\n");
        QMessageBox::warning(this, "Open failed", "can't open adc_raw_file\r\n");
    }

    adc_create_chart();

    connect(adc_timer, SIGNAL(timeout()), this, SLOT(adc_timer_timeout()));

    adc_timer->start(100);
}


adc::~adc()
{
    delete adc_timer;
    delete ui;
}


void adc::getADCValue(QFile &raw_file, QFile &scale_file)
{
    /* 如果文件不存在，则返回 */
    if (!raw_file.exists() || !scale_file.exists())
        return;

    if(!raw_file.open(QIODevice::ReadOnly))
        qDebug()<<raw_file.errorString();

    if(!scale_file.open(QIODevice::ReadOnly))
        qDebug()<<scale_file.errorString();

    /* 获取原始 ADC 值 */
    QTextStream raw_in(&raw_file);
    QString raw_buf = raw_in.readLine();
    qDebug()<< raw_file.fileName() << " = " << raw_buf << endl;
    raw_file.close();

     /* 获取 ADC 尺度 */
    QTextStream scale_in(&scale_file);
    QString scale_buf = scale_in.readLine();
    qDebug()<< scale_file.fileName() << " = " << scale_buf << endl;
    scale_file.close();

    adc_value = raw_buf.toFloat() * scale_buf.toFloat() / 1000;
    printf("adc value = %f\r\n", adc_value);
}



void adc::adc_create_chart()
{
    chart = new QChart();
    mAxY = new QValueAxis();
    mAxX = new QValueAxis();
    // 平滑曲线: 会自动对点进行样条插值，使电压波形看起来更圆润、更像示波器的效果
    mADCSeries = new QSplineSeries();

    // x,y轴范围
    mAxX->setRange(0, 10);
    mAxY->setRange(0, 3.3);

    // x,Y轴分等份
    mAxX->setTickCount(11);
    mAxY->setTickCount(10);

    // 将系列添加到图表
    chart->addSeries(mADCSeries);
    chart->setTheme(QtCharts::QChart::ChartThemeLight);

    mAxX->setTitleText(QString(tr("time")));
    mAxY->setTitleText(QString(tr("Voltage")));
    chart->addAxis(mAxX, Qt::AlignBottom);
    chart->addAxis(mAxY, Qt::AlignLeft);

    mADCSeries->attachAxis(mAxY);
    mADCSeries->attachAxis(mAxX);


    //隐藏背景
    //chart->setBackgroundVisible(true);
    //设置外边界全部为0
    //chart->setContentsMargins(0, 0, 0, 0);
    //设置内边界全部为0
    //chart->setMargins(QMargins(0, 0, 0, 0));
    //设置背景区域无圆角
    //chart->setBackgroundRoundness(0);

    //突出曲线上的点
    mADCSeries->setPointsVisible(true);

    //图例
    mlegend = chart->legend();
    mADCSeries->setName("GPIO1_IO01");
    mADCSeries->setColor(QColor(255,0,0));

    //在底部显示
    mlegend->setAlignment(Qt::AlignBottom);
    mlegend->show();


    QFont font = mlegend->font();
    font.setPointSizeF(20);
    mlegend->setFont(font);

    // 将图表绑定到视图 wiget 为 QChartView
    ui->adc_chart->setChart(chart);

    const auto markers = chart->legend()->markers();
    for (QLegendMarker *marker : markers) {
        // Disconnect possible existing connection to avoid multiple connections
        QObject::disconnect(marker, &QLegendMarker::clicked, this ,&adc::handleMarkerClicked);
        QObject::connect(marker, &QLegendMarker::clicked, this, &adc::handleMarkerClicked);
    }
}


// 图例交互: 点击事件
// 点击图表下方的图例（Marker），可以切换曲线的显示或隐藏。这在多通道监控时非常有用
void adc::handleMarkerClicked()
{
    QLegendMarker* marker = qobject_cast<QLegendMarker*> (sender());
    //断言
    Q_ASSERT(marker);
    switch (marker->type())
    {
        case QLegendMarker::LegendMarkerTypeXY:
        {
        //控序列隐藏/显示
        // Toggle visibility of series
        marker->series()->setVisible(!marker->series()->isVisible());

        // Turn legend marker back to visible, since hiding series also hides the marker
        // and we don't want it to happen now.
        marker->setVisible(true);

        //修改图例
        // Dim the marker, if series is not visible
        qreal alpha = 1.0;

        if (!marker->series()->isVisible())
            alpha = 0.5;

        QColor color;
        QBrush brush = marker->labelBrush();
        color = brush.color();
        color.setAlphaF(alpha);
        brush.setColor(color);
        marker->setLabelBrush(brush);

        brush = marker->brush();
        color = brush.color();
        color.setAlphaF(alpha);
        brush.setColor(color);
        marker->setBrush(brush);

        QPen pen = marker->pen();
        color = pen.color();
        color.setAlphaF(alpha);
        pen.setColor(color);
        marker->setPen(pen);
        break;
        }
    default:
        {
        qDebug() << "Unknown marker type";
        break;
        }
    }
}

// 动态滑窗算法
// 当数据点超过 X 轴最大值（10）时，它不是增加 X 轴长度，而是将旧数据整体左移，并将新数据填在最右侧。这实现了类似心电图机的“滚动播放”效果
void adc::adc_update_chart(QSplineSeries *Series, QValueAxis *Axis, float add_value)
{
    // 如果数据序列中的值数量超过了 x 轴设置的最大值 10
    if(Series->count() > Axis->max())
    {
        // 将序列数据中的前 10 个值依次向前挪一个
        for(int i=0; i<Series->count()-1; ++i)
        {
            QPointF point = Series->at(i+1);
            Series->replace(i, point.x() -1, point.y());
        }
        // 在最后添加新的点
        Series->replace(Axis->max(), Axis->max(), add_value);
    }
    // 如果数据序列中的值没超过 x 轴设置的最大值 10 ，就正常添加点
    else
    {
        QPointF point;
        point.setX(Series->count());
        point.setY(add_value);
        Series->append(point);
    }
}

void adc::adc_timer_timeout()
{
#if __arm__
    getADCValue(adc_raw_file, adc_scale_file);
#else
//    // 纯数值模拟（代码内
//    adc_value = QRandomGenerator::global()->bounded(0,3);
//    // 优化随机数（模拟正弦波）
//    // [SIMULATION] 模拟一个 50Hz 的正弦波形
//    static float angle = 0;
//    angle += 0.2;
//    // 产生一个 0V 到 3.3V 之间的平滑电压
//    adc_value = 1.65 + 1.65 * qSin(angle);

    // 文件级模拟（系统级
    // 模拟外部传感器电平波动，并写回模拟文件
    int mock_raw = 2048 + QRandomGenerator::global()->bounded(-200, 200);
    if (adc_raw_file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        adc_raw_file.write(QByteArray::number(mock_raw));
        adc_raw_file.close();
    }
    // 依然通过读取文件来更新界面，确保 getADCValue 的逻辑得到测试
    getADCValue(adc_raw_file, adc_scale_file);
#endif
    adc_update_chart(mADCSeries, mAxX, adc_value);
}

void adc::on_adc_back_clicked()
{
    printf("on_adc_back_clicked\r\n");
    adc_timer->stop();

    this->close();
    MainWindow *m = new MainWindow();
    m->show();
}
