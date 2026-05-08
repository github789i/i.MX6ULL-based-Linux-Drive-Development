#include "camera.h"
#include "ui_camera.h"
#include "mainwindow.h"

/* 实现了一个基于 V4L2 (Video for Linux Two) 框架的摄像头驱动程序。它是嵌入式 Linux 系统中直接操作多媒体硬件的标准方式
 * V4L2 框架 (ioctl 核心操作):
 *  # VIDIOC_QUERYCAP: 查询设备能力（确认是否支持视频采集和内存映射）
 *  # VIDIOC_S_FMT: 设置图像格式（如 MJPEG 或 YUYV）和分辨率
 *  # VIDIOC_REQBUFS & mmap: 向内核申请缓冲区，并通过内存映射（mmap）技术将内核空间的图像数据“投影”到用户空间，避免了昂贵的内存拷贝
 *  # VIDIOC_QBUF & VIDIOC_DQBUF: 这是“生产者-消费者”模型。内核把图像填入缓冲区并入队（Q），程序从队列中取出（DQ）数据显示
 * 软件模拟(比音视频播放器复杂)
 *  因为 camera.cpp 直接调用了底层系统 API (ioctl, open("/dev/video"))
 *  与 mplayer 这种应用级模拟不同，摄像头模拟通常有两种方案：
 *  # 硬件级模拟 (推荐)：
 *      在 Ubuntu 上使用 v4l2loopback 内核模块创建一个虚拟摄像头设备（例如 /dev/video2）
 *  # 代码级模拟：
 *      修改代码，完全绕过 V4L2 接口，改用一张静态图片或本地视频文件循环读取来模拟实时画面
 */

camera::camera(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::camera)
{
    printf("camera ui run\r\n");
    ui->setupUi(this);
    this->setWindowTitle("Camera");
    this->setFixedSize(800,480);

    //每隔固定的时间显示一帧
    timer = new QTimer();
    connect(timer, SIGNAL(timeout()), this, SLOT(video_show()));

//    if(0 == camera_open()){
//        printf("open the camera successfully!\n");
//        // 由于摄像头默认30帧每秒,虽然10ms定时执行一次,但实际上1秒内最多有30次可以执行成功
//        // 其余都会在ioctl处阻塞
//        timer->start(10);
//        start = 1;
//        ui->camera_open->setText("Close");
//    }

    // 获取当前目录
    QString currentDir = QDir::currentPath();
    // 拼接photo文件夹路径
#ifdef __arm__
    QString photoDirPath = "/qt/photo";
#else
    QString photoDirPath = currentDir + "/photo";
#endif
    // 创建QDir对象
    QDir photoDir(photoDirPath);
    // 判断photo文件夹是否存在
    if (!photoDir.exists()){
        // 创建photo文件夹
        if (photoDir.mkdir(photoDirPath))
            printf("mkdir photo successfully\n");
        else
            printf("Failed to mkdir photo\n");
    }
    else
        printf("The photo folder directory already exists\r\n");
}

camera::~camera()
{
    delete ui;
}


// 通过 10ms 的定时器不断尝试从 V4L2 队列中取出图像帧，并将其加载为 QPixmap 显示在 QLabel 上
void camera::video_show(void)
{
//    QPixmap pix;

//    /* 采集图片数据 */
//    //定义结构体变量，用于获取内核队列数据
//    struct v4l2_buffer buffer;
//    buffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

//    /* 从内核中捕获好的输出队列中取出一个 */
//    if(0 == ioctl(video_fd, VIDIOC_DQBUF, &buffer)){
//        /* 显示在label控件上 */
//        //获取一帧显示
//        pix.loadFromData((unsigned char *)userbuff[buffer.index], buffer.bytesused);
//        pix = pix.scaled(ui->label->width(), ui->label->height(),
//                         Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
//        ui->label->setPixmap(pix);
//    }
//    /* 将使用后的缓冲区放回到内核的输入队列中 (VIDIOC_QBUF) */
//    if(0 > ioctl(video_fd, VIDIOC_QBUF, &buffer)){
//        perror("Failed to return to queue！");
//        start = 0;
//        timer->stop();
//        ui->camera_open->setText("Open");
//    }
    QPixmap pix;

#ifdef __arm__
    // --- 真实 ARM 硬件逻辑 ---
    struct v4l2_buffer buffer;
    buffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    if(0 == ioctl(video_fd, VIDIOC_DQBUF, &buffer)){
        pix.loadFromData((unsigned char *)userbuff[buffer.index], buffer.bytesused);
        ui->label->setPixmap(pix.scaled(ui->label->width(), ui->label->height(),
                             Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation));
    }
    ioctl(video_fd, VIDIOC_QBUF, &buffer);
#else
    // --- PC 纯软件模拟逻辑 ---
    // 模拟摄像头画面：你可以放一张 test.jpg 到项目运行目录
    static int fake_frame = 0;
    pix.load(":/images/camera_test.png"); // 或者直接 QPixmap(640,480) 填色

    if(pix.isNull()){
        // 如果找不到图片，画个动态变色的方块模拟视频流
        pix = QPixmap(ui->label->size());
        pix.fill(QColor::fromHsv(fake_frame % 360, 200, 200));
        fake_frame += 5;
    }

    ui->label->setPixmap(pix.scaled(ui->label->width(), ui->label->height(),
                         Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation));
#endif
}


//打开 & 设置相机
int camera::camera_open(void)
{
    /* 1.打开摄像头设备 */
#ifdef __arm__
    video_fd = open("/dev/video1", O_RDWR);

    if(video_fd < 0){
        perror("Failed to open the camera!\r\n");
        QMessageBox::warning(this, "Open failed", "No such file or directory!\r\n");
        return -1;
    }
    /* 3.获取摄像头的能力 (VIDIOC_QUERYCAP：是否支持视频采集、内存映射等) */
    struct v4l2_capability capability;
    // VIDIOC_QUERYCAP: 查询设备能力（确认是否支持视频采集和内存映射）
    if(0 == ioctl(video_fd, VIDIOC_QUERYCAP, &capability)){
        if((capability.capabilities & V4L2_CAP_VIDEO_CAPTURE) == 0){
            perror("This camera device does not support video capture!\r\n");
            ::close(video_fd);
            return -2;
        }
        if((capability.capabilities & V4L2_MEMORY_MMAP) == 0){
            perror("This camera device does not support mmap memory mapping!\r\n");
            ::close(video_fd);
            return -3;
        }
    }

    /* 4.枚举摄像头支持的格式           (VIDIOC_ENUM_FMT：MJPG、YUYV等)
      列举出每种格式下支持的分辨率      (VIDIOC_ENUM_FRAMESIZES) */
    struct v4l2_fmtdesc fmtdesc;
    memset(&fmtdesc, 0, sizeof(fmtdesc));
    fmtdesc.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;  //设置视频采集设备类型
    int i = 0;
    while(1){
        fmtdesc.index = i++;
        // 获取支持格式
        if(0 == ioctl(video_fd, VIDIOC_ENUM_FMT, &fmtdesc)){
            printf("Supported Formats: %s, %c%c%c%c\n", fmtdesc.description,
                                            fmtdesc.pixelformat & 0xff,
                                            fmtdesc.pixelformat >> 8 & 0xff,
                                            fmtdesc.pixelformat >> 16 & 0xff,
                                            fmtdesc.pixelformat >> 24 & 0xff);
            // 列出该格式下支持的分辨率 VIDIOC_ENUM_FRAMESIZES & 默认帧率 VIDIOC_G_PARM
            // 1.默认帧率
            struct v4l2_streamparm streamparm;
            streamparm.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            if(0 == ioctl(video_fd, VIDIOC_G_PARM, &streamparm))
                printf("The default frame rate for this format %d fps\n", streamparm.parm.capture.timeperframe.denominator);
            // 2.循环列出支持的分辨率
            struct v4l2_frmsizeenum frmsizeenum;
            frmsizeenum.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            frmsizeenum.pixel_format = fmtdesc.pixelformat;   //设置成对应的格式
            int j = 0;
            printf("The supported resolutions are:\n");
            while(1){
                frmsizeenum.index = j++;
                if(0 == ioctl(video_fd, VIDIOC_ENUM_FRAMESIZES, &frmsizeenum))
                    printf("%d x %d\n", frmsizeenum.discrete.width, frmsizeenum.discrete.height);
                else break;
            }
            printf("\n");
        }else break;
    }

    /* 5.设置摄像头类型为捕获、设置分辨率、视频采集格式 (VIDIOC_S_FMT) */
    struct v4l2_format format;
    format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;   /* 视频采集 */
    format.fmt.pix.width = video_width;          /* 宽 */
    format.fmt.pix.height = video_height;    	 /* 高 */
    format.fmt.pix.pixelformat = V4L2_PIX_FMT_MJPEG;   /* 设置输出类型：MJPG */
    // VIDIOC_S_FMT: 设置图像格式（如 MJPEG 或 YUYV）和分辨率
    if(0 > ioctl(video_fd, VIDIOC_S_FMT, &format)){
        perror("Failed to set camera parameters!");
        ::close(video_fd);
        return -4;
    }

    /* 6.向内核申请内存 (VIDIOC_REQBUFS：个数、映射方式为mmap)
         将申请到的缓存加入内核队列 (VIDIOC_QBUF)
         将内核内存映射到用户空间 (mmap) */
    struct v4l2_requestbuffers requestbuffers;
    requestbuffers.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    requestbuffers.count = 4;    //申请缓存个数
    requestbuffers.memory = V4L2_MEMORY_MMAP;     //申请为物理连续的内存空间
    // VIDIOC_REQBUFS & mmap: 向内核申请缓冲区，并通过内存映射（mmap）技术将内核空间的图像数据“投影”到用户空间，避免了昂贵的内存拷贝
    if(0 == ioctl(video_fd, VIDIOC_REQBUFS, &requestbuffers)){
        /* 申请到内存后 */
        for(unsigned int i = 0; i < requestbuffers.count; i++){
            /* 将申请到的缓存加入内核队列 (VIDIOC_QBUF)              */
            struct v4l2_buffer buffer;
            buffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            buffer.index = i;
            buffer.memory = V4L2_MEMORY_MMAP;
            // VIDIOC_QBUF & VIDIOC_DQBUF: 这是“生产者-消费者”模型。内核把图像填入缓冲区并入队（Q），程序从队列中取出（DQ）数据显示
            if(0 == ioctl(video_fd, VIDIOC_QBUF, &buffer)){
                /* 加入内核队列成功后，将内存映射到用户空间 (mmap) */
                userbuff[i] = (char *)mmap(NULL, buffer.length, PROT_READ | PROT_WRITE, MAP_SHARED, video_fd, buffer.m.offset);
                userbuff_length[i] = buffer.length;
            }
        }
    }
    else{
        perror("Failed to apply for memory!");
        ::close(video_fd);
        return -5;
    }
    int type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if(0 > ioctl(video_fd, VIDIOC_STREAMON, &type)){
        perror("Failed to open video stream!");
        return -6;
    }
    return 0;
#else
    // PC 模拟：直接假装成功
    printf("PC Simulator: Virtual camera started (Software Mode)\n");
    start = 1;
    return 0;
#endif
}

int camera::camera_close(void)
{
    /* 8.停止采集，关闭视频流 (VIDIOC_STREAMOFF)
      关闭摄像头设备 & 关闭LCD设备 */
    int type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if(0 == ioctl(video_fd, VIDIOC_STREAMOFF, &type)){
        /* 9.释放映射 */
        for(int i = 0; i < 4; i++)
            munmap(userbuff[i], userbuff_length[i]);
        ::close(video_fd);
        printf("Close the camera successfully!\n");
        return 0;
    }
    return -1;
}

void camera::on_camera_open_clicked()
{
    printf("on_camera_open_clicked\r\n");
    if(start == 0){
        // 使用v4l2打开 & 设置相机成功
        if(0 == camera_open()){
            printf("Open the camera successfully!\n");
            // 由于摄像头默认30帧每秒,虽然10ms定时执行一次,但实际上1秒内最多有30次可以执行成功
            // 其余都会在ioctl处阻塞
            timer->start(10);
            start = 1;
            ui->camera_open->setText("Close");
        }
    }else{
        if(0 == camera_close()){
            start = 0;
            timer->stop();
            ui->camera_open->setText("Open");
        }
    }
}

void camera::on_camera_photos_clicked()
{
    printf("on_camera_photos_clicked\r\n");
    if(0 == camera_close()){
        timer->stop();
    }
    this->close();
    showphoto *s = new showphoto(1);
    s->show();
}

void camera::on_camera_take_clicked()
{
//    printf("on_camera_take_clicked\r\n");
//    if(start == 1){
//        //随机产生10个数字,作为照片的名字
//        QString randomNumbers;
//        for(int i=0; i<10; i++) {
//            // QRandomGenerator: 用于生成随机文件名，防止拍照时文件覆盖
//            int randomNumber = QRandomGenerator::global()->bounded(10);
//            randomNumbers.append(QString::number(randomNumber));
//        }
//        QString str = "./photo/photo_" + randomNumbers + ".jpg";

//        /* 采集图片数据 */
//        //定义结构体变量，用于获取内核队列数据
//        struct v4l2_buffer buffer;
//        buffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

//        /* 从内核中捕获好的输出队列中取出一个 */
//        if(0 == ioctl(video_fd, VIDIOC_DQBUF, &buffer)){
//            //保存到本地
//            int fd = open(str.toStdString().data(), O_RDWR | O_CREAT, 0777);   //打开并创建一个新文件
//            write(fd, userbuff[buffer.index], buffer.bytesused);
//            printf("%s\n", str.toStdString().data());
//            ::close(fd);
//        }
//        /* 将使用后的缓冲区放回到内核的输入队列中 (VIDIOC_QBUF) */
//        if(0 > ioctl(video_fd, VIDIOC_QBUF, &buffer)){
//            perror("Failed to return to queue!");
//        }

//        //清空label,相当于拍照效果提示
//        ui->label->clear();
//    }else{
//        printf("no camera opened\r\n");
//        QMessageBox::warning(this, "Take failed", "No camera opened!\r\n");
//    }
    if(start == 1){
        //随机产生10个数字,作为照片的名字
        QString randomNumbers;
        for(int i=0; i<10; i++) {
            // QRandomGenerator: 用于生成随机文件名，防止拍照时文件覆盖
            int randomNumber = QRandomGenerator::global()->bounded(10);
            randomNumbers.append(QString::number(randomNumber));
        }
        QString str = "./photo/photo_" + randomNumbers + ".jpg";

#ifdef __arm__
        // --- ARM 逻辑：从 v4l2 缓冲区写文件 ---
        struct v4l2_buffer buffer;
        buffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        if(0 == ioctl(video_fd, VIDIOC_DQBUF, &buffer)){
            int fd = open(str.toStdString().data(), O_RDWR | O_CREAT, 0777);
            write(fd, userbuff[buffer.index], buffer.bytesused);
            ::close(fd);
            ioctl(video_fd, VIDIOC_QBUF, &buffer);
        }
#else
        // --- PC 逻辑：直接保存当前显示的图片 ---
        if(ui->label->pixmap()) {
            ui->label->pixmap()->save(str, "JPG");
        }
#endif
        printf("Saved: %s\n", str.toStdString().data());
        ui->label->clear();
    }
}

void camera::on_camera_back_clicked()
{
    printf("on_camera_back_clicked\r\n");
    if(0 == camera_close()){
        timer->stop();
    }

    this->close();
    MainWindow *m = new MainWindow();
    m->show();
}
