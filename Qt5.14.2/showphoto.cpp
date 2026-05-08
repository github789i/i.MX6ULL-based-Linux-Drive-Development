#include "showphoto.h"
#include "ui_showphoto.h"
#include "mainwindow.h"

/* 实现了一个功能丰富的交互式图片查看器，能够根据来源（相机或画板）动态加载图片，并支持手势操作（缩放、拖拽）
 * sourcePage=2,path 必须与 sketchpad 保存时的路径完全一致: dir.setPath(mockBasePath + "/saved_drawings/");*/

showphoto::showphoto(int sourcePage, QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::showphoto)
{
    printf("showphoto ui run\r\n");
    ui->setupUi(this);
    this->setWindowTitle("Photo");
    this->setFixedSize(800,480);

    srcPage = sourcePage;

    // 获取系统临时目录路径 (与之前模拟路径保持一致)
    QString mockBasePath = QDir::tempPath() + "/imx6_mock";

    // 通过 sourcePage 变量区分图片来源
    //  sourcePage == 1：指向相机相册。
    //  sourcePage == 2：指向画板作品

    // 相册
    if(sourcePage == 1){
#ifdef __arm__
        dir.setPath("/qt/photo/");
#else
         dir.setPath("./photo/"); // 设置目录路径
        // 模拟相册路径
//        dir.setPath(mockBasePath + "/photo/");
#endif
    }
    // 画图板
    else if(sourcePage == 2){
#ifdef __arm__
        dir.setPath("/qt/images/");
#else
        //dir.setPath("./images/"); // 设置目录路径
        // 必须与 sketchpad 保存时的路径完全一致
        dir.setPath(mockBasePath + "/saved_drawings/");
#endif
    }
    // 检查目录是否存在，不存在则创建（防止 fileList 为空）
    if (!dir.exists()) {
        dir.mkpath(dir.absolutePath());
    }

    dir.setFilter(QDir::Files | QDir::NoDotAndDotDot); // 过滤条件，只获取文件
    // 过滤器 (setNameFilters)：确保程序只处理 .jpg 和 .png 文件，防止加载非图片文件导致崩溃
    dir.setNameFilters(QStringList() << "*.jpg" << "*.png"); // 建议加上 png 兼容
    // 排序机制 (setSorting)：使用 QDir::Time 让最新生成的作品排列在最前面，符合用户的使用习惯
    dir.setSorting(QDir::Time);  // 去掉 Reversed，通常最新的在前面
    fileList = dir.entryList(); // 获取文件列表

    //先显示一张图片
    if(fileList.length() > 0){
        QString filePath = dir.filePath(fileList[currentIndex]); // 获取文件路径
        QPixmap pixmap(filePath); // 创建图片对象
        ui->label->setPixmap(pixmap); // 在控件上显示图片
    }

    // 将QLabel嵌入到QScrollArea中
    // QScrollArea (视口)： QLabel 的父容器
    //关键作用：当图片通过“双击”放大到 2 倍尺寸后，原本 800x480 的屏幕装不下大图，QScrollArea 会提供滚动查看的功能，类似于手机查看大图的效果
    ui->scrollArea->setWidgetResizable(true);
    ui->scrollArea->setGeometry(0, 0, 800, 480);

#ifdef __arm__
    // 模拟长按
    // 由于嵌入式 ARM 板（电阻屏或单点电容屏）可能不支持标准的多点触控缩放，代码巧妙地模拟了手机操作：
    // 长按模拟 (pressTimer)：仅在 __arm__ 环境下启用。按下 500ms 后触发 onLongPress 缩放，这是解决嵌入式设备交互单一的常用方案
    // 设置pressTimer的超时和槽函数
    pressTimer = new QTimer(this);
    pressTimer->setSingleShot(true);
    pressTimer->setInterval(500); // 长按时间阈值（500毫秒）
    connect(pressTimer, &QTimer::timeout, this, &showphoto::onLongPress);
#endif
}



showphoto::~showphoto()
{
    delete ui;
}

void showphoto::on_showphoto_next_clicked()
{
    if(fileList.length() > 0){
        currentIndex++; // 更新当前显示的文件索引
        if (currentIndex >= fileList.size()) {
            currentIndex = 0;
        }

        if(isZoomed){
            printf("Force restore view size\r\n");
#ifdef __arm__
            onLongPress();
#else
            QMouseEvent *dblClickEvent = new QMouseEvent(QEvent::MouseButtonDblClick,
                                                            ui->label->rect().center(),
                                                            Qt::LeftButton,
                                                            Qt::LeftButton,
                                                            Qt::NoModifier);

            // 发送事件到目标对象
            QCoreApplication::postEvent(ui->label, dblClickEvent);
#endif
        }

        QString filePath = dir.filePath(fileList[currentIndex]); // 获取文件路径
        QPixmap pixmap(filePath); // 创建图片对象
//        ui->label->setPixmap(pixmap); // 在控件上显示图片
        // 增加：自动缩放图片以适应 Label 尺寸，保持长宽比
        ui->label->setPixmap(pixmap.scaled(ui->label->size(),
                                            Qt::KeepAspectRatio,
                                            Qt::SmoothTransformation));
    }
}

void showphoto::on_showphoto_back_clicked()
{
    printf("on_showphoto_back_clicked\r\n");
    this->close();
    if(srcPage == 1){
        camera *v = new camera();
        v->show();
    } else if(srcPage == 2){
        sketchpad *s = new sketchpad();
        s->show();
    }
}

void showphoto::on_showphoto_front_clicked()
{
    if(fileList.length() > 0){
        currentIndex--; // 更新当前显示的文件索引
        if (currentIndex < 0) {
            currentIndex = fileList.size()-1;
        }

        if(isZoomed){
            printf("Force restore view size\r\n");
#ifdef __arm__
            onLongPress();
#else
            QMouseEvent *dblClickEvent = new QMouseEvent(QEvent::MouseButtonDblClick,
                                                            ui->label->rect().center(),
                                                            Qt::LeftButton,
                                                            Qt::LeftButton,
                                                            Qt::NoModifier);

            // 发送事件到目标对象
            QCoreApplication::postEvent(ui->label, dblClickEvent);
#endif
        }

        QString filePath = dir.filePath(fileList[currentIndex]); // 获取文件路径
        QPixmap pixmap(filePath); // 创建图片对象
        ui->label->setPixmap(pixmap); // 在控件上显示图片
    }
}
#ifdef __arm__
void showphoto::onLongPress()
#else
void showphoto::mouseDoubleClickEvent(QMouseEvent *event)
#endif
{
    if (fileList.length() > 0) {
        QString filePath = dir.filePath(fileList[currentIndex]); // 获取文件路径
        QPixmap pixmap(filePath); // 创建图片对象

        // 缩放逻辑与双击事件 (isZoomed)
        if (isZoomed) {
           // 恢复原来的大小
           ui->label->setPixmap(pixmap);
           // 坐标转换：在缩放切换时，通过 ui->label->move() 将图片重新居中，保证视觉上的平滑
           ui->label->move((ui->scrollArea->width() - ui->label->width()) / 2,
                           (ui->scrollArea->height() - ui->label->height()) / 2);
           // 状态切换: 控制缩放后是否允许随容器自动调整大小
           ui->scrollArea->setWidgetResizable(true);
           isZoomed = false;
        } else {
            // 放大显示
            QPixmap scaledPixmap = pixmap.scaled(pixmap.size() * 2, Qt::KeepAspectRatio);
            ui->label->setPixmap(scaledPixmap);
            ui->label->resize(scaledPixmap.size());
            ui->scrollArea->setWidgetResizable(false);
            isZoomed = true;
        }
    }
#ifndef __arm__
    QMainWindow::mouseDoubleClickEvent(event); // 保持默认行为
#endif
}


void showphoto::mousePressEvent(QMouseEvent *event)
{
    if (isZoomed) {
        dragging = true;
        lastMousePos = event->pos();
    }
#ifdef __arm__
    pressTimer->start();
#endif
    QMainWindow::mousePressEvent(event); // 保持默认行为
}

void showphoto::mouseMoveEvent(QMouseEvent *event)
{
    // 拖拽逻辑 (dragging)：
    // 只有在 isZoomed（放大状态）下才允许拖拽。
    // 通过计算 mouseMoveEvent 中坐标的增量 (dx, dy)，实时移动 QLabel 的位置，实现“随手而动”的效果
    if (dragging && isZoomed) {
        int dx = event->pos().x() - lastMousePos.x();
        int dy = event->pos().y() - lastMousePos.y();

        ui->label->move(ui->label->x() + dx, ui->label->y() + dy);
        lastMousePos = event->pos();
    }
#ifdef __arm__
    pressTimer->stop();
#endif
    QMainWindow::mouseMoveEvent(event); // 保持默认行为
}

void showphoto::mouseReleaseEvent(QMouseEvent *event)
{
    if (isZoomed) {
        dragging = false;
    }
#ifdef __arm__
    pressTimer->stop();
#endif
    QMainWindow::mouseReleaseEvent(event); // 保持默认行为
}
