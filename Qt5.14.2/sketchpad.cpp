#include "sketchpad.h"
#include "ui_sketchpad.h"
#include "mainwindow.h"
#include <QMessageBox>
#include <QDebug>

/* 实现了一个功能完备的电子画板
 * 利用 Qt 的绘图系统，让用户能够通过触摸（或鼠标）进行绘图、擦除、更改颜色、调节粗细以及保存作品
 * 软件模拟
 *  画板模块本身就是纯软件实现的逻辑，因此在 Ubuntu 上不需要特殊的“硬件模拟”
 *  它与 GPIO 或 UART 不同，不依赖 Linux 内核的特定驱动节点（如 /dev/xxx）。
 *  它依赖的是 Qt 的事件系统（Event System）。
 *  在 Ubuntu 上，你的鼠标操作会被自动映射为 QMouseEvent，这与 ARM 开发板上的触摸屏事件在 Qt 层面是完全一致的
 *  需要优化开发体验或文件系统适配
 */

sketchpad::sketchpad(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::sketchpad)
{
    ui->setupUi(this);
    this->setWindowTitle("Sketchpad");
    this->setFixedSize(800,480);
    // QImage (画布载体)
    // 作用：draw_image 是所有绘画操作发生的“离屏缓冲区”。
    // 原理：我们不在窗口上直接画线，而是先画在 QImage 上，然后再通过 paintEvent 将整个图像刷新到屏幕上。
    // 保证窗口被遮挡或最小化后，绘图内容不会丢失
    draw_image = QImage(800, 480, QImage::Format_RGB32);
    draw_image.fill(Qt::white);

}


sketchpad::~sketchpad()
{
    delete ui;
}

// 记录落笔的第一点
void sketchpad::mousePressEvent(QMouseEvent *event){
    if((drawingEnabled || erasingEnabled) && (event->buttons() & Qt::LeftButton)){
        printf("mousePressEvent\r\n");
        lastPoint = event->pos();
    }
}

// 只要鼠标左键按下并移动，就不断绘制微小的线段并调用 update() 触发界面重绘
void sketchpad::mouseMoveEvent(QMouseEvent *event){
    if((drawingEnabled || erasingEnabled) && (event->buttons() & Qt::LeftButton)){
        printf("mouseMoveEvent\r\n");
        // QPainter (画笔工具): Qt 的绘图引擎
        QPainter painter(&draw_image);
        // 参数设置：Qt::RoundCap 和 Qt::RoundJoin 使线条的转角和端点变得圆润，不会出现断层或锯齿
        if(drawingEnabled){
            painter.setPen(QPen(draw_brush,draw_width,Qt::SolidLine,Qt::RoundCap,Qt::RoundJoin));
        } else if(erasingEnabled){
            painter.setPen(QPen(Qt::white,draw_width+5,Qt::SolidLine,Qt::RoundCap,Qt::RoundJoin));
        }
        // 核心方法: 连接鼠标上一个位置和当前位置，形成平滑的线条
        painter.drawLine(lastPoint, event->pos());
        lastPoint = event->pos();
        update();
    }
}

// 利用滚轮动态修改 draw_width（笔触粗细）
void sketchpad::wheelEvent(QWheelEvent *event){
    Q_UNUSED(event);
    if((drawingEnabled || erasingEnabled)){
        printf("wheelEvent\r\n");
        int numDegrees = event->angleDelta().y() / 8;
        int numSteps = numDegrees / 15;
        draw_width = draw_width + numSteps;
        draw_width = draw_width>10?10:draw_width;
        draw_width = draw_width<1?1:draw_width;
    }
}

void sketchpad::paintEvent(QPaintEvent *event){
    Q_UNUSED(event);
    QPainter painter(this);
    painter.drawImage(0, 0, draw_image);
}

// 图像持久化 (保存功能)
// 将 QImage 对象编码为 .jpg 格式并写入文件系统
bool sketchpad::saveImage()
{

//#ifdef __arm__
//    QString saveDir = "/qt/images";
//#else
//    QString saveDir = QDir::currentPath() + "/images"; // 设置保存路径
//#endif

//    if (!save_dir.exists(saveDir)) {
//        save_dir.mkpath(saveDir);
//        printf("Create a file path\r\n");
//    } else {
//        printf("The file path already exists\r\n");
//    }

//    QString savePath = saveDir + "/draw_image_" +
//            QString::number(QRandomGenerator::global()->bounded(0,9999)) + ".jpg";

//    if(draw_image.save(savePath)){
//        return true;
//    }
//    else{
//        return false;
//    }
    QString saveDir;
#ifdef __arm__
    saveDir = "/qt/images";
#else
    // 使用你统一的模拟路径，方便管理
    saveDir = QDir::tempPath() + "/imx6_mock/saved_drawings";
#endif

    if (!save_dir.exists(saveDir)) {
        save_dir.mkpath(saveDir);
    }

    // 生成带时间戳的文件名，防止 QRandomGenerator 碰撞
    QString timeStamp = QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss");
    QString savePath = saveDir + "/draw_" + timeStamp + ".jpg";

    return draw_image.save(savePath);
}


void sketchpad::on_sketchpad_back_clicked()
{
    printf("on_sketchpad_back_clicked\r\n");

    this->close();
    MainWindow *m = new MainWindow();
    m->show();
}


void sketchpad::on_brush_clicked(bool checked)
{
    printf("on_brush_clicked %d\r\n", checked);
    if(checked){
        drawingEnabled = true;
        erasingEnabled = false;
        ui->eraser->setChecked(false);
    }
    else
        drawingEnabled = false;
}

void sketchpad::on_eraser_clicked(bool checked)
{
    printf("on_eraser_clicked %d\r\n", checked);
    if(checked){
        erasingEnabled = true;
        drawingEnabled = false;
        ui->brush->setChecked(false);
    }
    else
        erasingEnabled = false;
}

void sketchpad::on_save_clicked()
{
    printf("on_save_clicked \r\n");

    if(isNotEmpty(draw_image)){
        if(saveImage()){
            QMessageBox::information(this, "Saved successfully", "Image saved successfully!");
        }
        else{
            QMessageBox::warning(this, "Save failed", "Image saving failed!");
        }
    } else {
        QMessageBox::warning(this, "Save failed", "Image is empty!");
    }
}

void sketchpad::on_images_clicked()
{
    printf("on_images_clicked\r\n");
    this->close();

    showphoto *s = new showphoto(2);
    s->show();
}

void sketchpad::on_colors_clicked()
{
    QColor color = QColorDialog::getColor(Qt::white, this, "Choose Color");

    if (color.isValid()) {
        qDebug() << color.name() << endl;
        draw_brush.setColor(color);
    }
}

bool sketchpad::isNotEmpty(const QImage &image)
{
    // 获取图像的尺寸
    int width = image.width();
    int height = image.height();

    // 遍历图像的每个像素
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            // 获取当前像素的颜色
            QColor color = image.pixelColor(x, y);

            // 检查颜色是否为白色
            if (color != QColor(Qt::white)) {
                return true;  // 如果找到不为白色的像素，则图像不为空
            }
        }
    }

    return false;  // 如果所有像素都为白色，则图像为空
}


void sketchpad::on_clear_clicked()
{
    printf("on_clear_clicked\r\n");
    draw_image.fill(Qt::white);
    update();
}
