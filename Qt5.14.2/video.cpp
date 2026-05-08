#include "video.h"
#include "mainwindow.h"
#include "ui_video.h"

/* 实现了一个基于 MPlayer 开源播放器的嵌入式视频播放器。它巧妙地利用了 Linux 进程间通信和窗口嵌套技术
 * 软件模拟
 *  # 进程模拟(first running)：你可以确保 Ubuntu 上安装了 mplayer（sudo apt install mplayer），这样 QProcess 依然可以运行。
 *  # 输出后端模拟(first running)：在 Ubuntu 上通常不能使用 fbdev2（因为 Ubuntu 使用 X11），必须去掉这个参数或改为 xv 或 x11
 *  # 文件路径模拟：将 /video 路径重定向到你本地的视频文件夹*/

video::video(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::video)
{
    ui->setupUi(this);
    this->setWindowTitle("Video");
    this->setFixedSize(800,480);

    // 进程控制器: 该模块的核心
    // mplayerProcess 并不是在 Qt 内部解码视频，而是通过 QProcess 在后台启动了一个独立的 Linux 进程（mplayer）
    //  # 从模式 (-slave)：通过这个参数，Qt 可以像写文件一样向 MPlayer 发送指令（如 pause\n, quit\n, volume 80 1\n）。
    //  # 静默模式 (-quiet)：防止 MPlayer 乱发日志，干扰 Qt 对标准输出的解析
    mplayerProcess = new QProcess(this);

    // 初始化默认视频路径
    defaultVideoPath = "/video";
    currentFilePath = "";
    currentIndex = -1;

    // 获取所有视频文件
    loadvideoFiles();

    connect(mplayerProcess, &QProcess::readyReadStandardOutput, this, &video::handleMPlayerOutput);
    connect(mplayerProcess, &QProcess::readyReadStandardError, this, &video::handleMPlayerError);

    // 初始化播放按钮
    ui->start_pause->setIcon(QIcon(":/images/pause.svg"));
    ui->start_pause->setIconSize(QSize(48, 48));

    // 初始化音量播放器
    volume_timer = new QTimer(this);
    connect(volume_timer, &QTimer::timeout, this, [this]() {
            volume_timer->stop();
            ui->volume_widget->setVisible(false);
        });
    ui->volume_Slider->setValue(80);
    ui->volume_label->setText("80%");
    ui->volume_widget->setVisible(false);
    ui->volume_widget->raise(); // 【关键】强行置于顶层

    // 把音量按钮也连上，用于显示/隐藏音量条
    connect(ui->volume, &QPushButton::clicked, this, [this](){
        ui->volume_widget->setVisible(true);
        ui->volume_widget->raise();
        volume_timer->start(1000);
    });

}


video::~video()
{
    delete ui;
}


void video::loadvideoFiles()
{
    QStringList filters;
        filters << "*.mp4" << "*.mkv" << "*.avi";

        QString path;
#ifdef __arm__
        path = defaultVideoPath; // /video
#else
    // 路径与环境适配
    // 模拟模式：指向你 Ubuntu 下存放视频的目录
    // 强制先寻找可执行程序同级的 video 目录
    // 这样无论你在 Qt Creator 运行还是双击运行，都能找到
    path = QCoreApplication::applicationDirPath() + "/video";

    // 如果该目录不存在，则尝试用户视频目录
    QDir testDir(path);
    if (!testDir.exists()) {
        path = QDir::homePath() + "/Videos";
    }
#endif
//    QStringList filters;
//    filters << "*.mp4";
//    QDir dir(defaultVideoPath);
//    dir.setNameFilters(filters);
//    videoFiles = dir.entryList(QDir::Files);
//    for (int i = 0; i < videoFiles.size(); ++i) {
//        videoFiles[i] = dir.absoluteFilePath(videoFiles.at(i));
//    }
    QDir dir(path);
    // 自动创建目录（防止目录不存在导致读取失败）
    if (!dir.exists()) {
        dir.mkpath(path);
    }

    dir.setNameFilters(filters);
    // 获取绝对路径列表
    videoFiles.clear(); // 清空旧数据
    QStringList list = dir.entryList(QDir::Files | QDir::NoDotAndDotDot);

    for (const QString &fileName : list) {
        videoFiles << dir.absoluteFilePath(fileName);
    }

    qDebug() << "Total videos found in" << path << ":" << videoFiles.size();
}

void video::handleMPlayerOutput()
{
    while (mplayerProcess->canReadLine()) {
        QString message(mplayerProcess->readLine());
        qDebug() << message << endl;
        QStringList message_list = message.split("=");
    }
}

void video::handleMPlayerError()
{
    QByteArray error = mplayerProcess->readAllStandardError();
    printf("MPlayer Error: %s\r\n", error.constData());
}

void video::mousePressEvent(QMouseEvent *event)
{
    printf("mousePressEvent\r\n");
    if (event->button() == Qt::LeftButton) {

    }
    QMainWindow::mousePressEvent(event); // 保持默认行为
}

void video::playVideo(const QString &filePath)
{
    if (mplayerProcess->state() == QProcess::Running) {
        mplayerProcess->terminate(); // 终止当前正在运行的 mplayer 进程
        if (!mplayerProcess->waitForFinished(3000)) { // 等待进程终止，超时3秒
            mplayerProcess->kill();
        }
    }

//    // 窗口嵌入: 获取Qt界面中一个小部件 video_widget 的的操作系统句柄（Window ID
//    WId windowID = ui->video_widget->winId();
//    qDebug() << "video_widget window ID:" << windowID;

//    // 设置 MPlayer 的参数，包含 -wid 选项
//    // 通过给 MPlayer 传递 -wid 参数，强行命令 MPlayer 不要弹出新窗口，而是直接把视频画面绘制在 Qt 的 video_widget 区域内
//    QStringList args;
//    // fbdev2 (视频输出后端): -vo fbdev2 指示 MPlayer 使用 Framebuffer（帧缓冲）进行输出
//    // 这是嵌入式 Linux 在没有 X11 或 Wayland 图形桌面环境时常用的直接绘图方式
//    args << "-slave" << "-quiet" << "-wid" << QString::number(windowID) << filePath << "-vo" << "fbdev2";
//    qDebug() << "MPlayer arguments:" << args;

//    mplayerProcess->start("mplayer", args);

    // 播放参数适配 (video::playVideo)
    // 在 Ubuntu 上，fbdev2 会报错，我们需要根据平台动态调整
    WId windowID = ui->video_widget->winId();

    QStringList args;
    args << "-slave" << "-quiet" << "-wid" << QString::number(windowID);
    args << filePath;

#ifdef __arm__
    args << "-vo" << "fbdev2"; // ARM 平台直接操作帧缓冲
#else
    // Ubuntu 平台通常使用 xv 或 x11 输出，或者干脆不带 -vo 让它自适应
    args << "-vo" << "x11";
#endif

    mplayerProcess->start("mplayer", args);


    if (!mplayerProcess->waitForStarted()) {
        printf("mplayer error\r\n");
        return;
    }

    play_flag = true;
    ui->start_pause->setIcon(QIcon(":/images/start.svg"));

    // 获取视频名称并设置到 QLabel
    QString videoName = QFileInfo(filePath).completeBaseName();
    ui->label_video_name->setText(videoName);
}

void video::on_start_pause_clicked()
{
//    qDebug() << "Start/Pause Clicked"; // 添加调试打印

//#ifdef __arm__
//    if (mplayerProcess->state() != QProcess::Running) {
//        if (videoFiles.isEmpty()) {
//            QMessageBox::warning(this, "No Video", "No video files found in the default directory.");
//            return;
//        }

//        int randomIndex = QRandomGenerator::global()->bounded(videoFiles.size());
//        currentFilePath = videoFiles.at(randomIndex);
//        currentIndex = randomIndex;

//        playVideo(currentFilePath);
//    } else {
//        mplayerProcess->write("pause\n");
//        if(play_flag == true){
//            ui->start_pause->setIcon(QIcon(":/images/pause.svg"));
//            ui->start_pause->setIconSize(QSize(48, 48));
//            play_flag = false;
//        }
//        else if(play_flag == false){
//            ui->start_pause->setIcon(QIcon(":/images/start.svg"));
//            ui->start_pause->setIconSize(QSize(48, 48));
//            play_flag = true;
//        }
//    }
//#else
//    if(play_flag == true){
//        ui->start_pause->setIcon(QIcon(":/images/pause.svg"));
//        ui->start_pause->setIconSize(QSize(48, 48));
//        play_flag = false;
//    }
//    else if(play_flag == false){
//        ui->start_pause->setIcon(QIcon(":/images/start.svg"));
//        ui->start_pause->setIconSize(QSize(48, 48));
//        play_flag = true;
//    }
//#endif
    qDebug() << "Start/Pause Clicked"; // 添加调试打印

    if (mplayerProcess->state() == QProcess::Running) {
        // 无论 ARM 还是 PC，只要在运行，就发送暂停指令
        mplayerProcess->write("pause\n");

        play_flag = !play_flag;
        if(play_flag) {
            ui->start_pause->setIcon(QIcon(":/images/start.svg"));
        } else {
            ui->start_pause->setIcon(QIcon(":/images/pause.svg"));
        }
    } else {
        // 如果没运行，则随机挑一个播放（仅限 ARM 或你本地有文件的情况）
        if (!videoFiles.isEmpty()) {
            currentIndex = QRandomGenerator::global()->bounded(videoFiles.size());
            playVideo(videoFiles.at(currentIndex));
        }
    }
}

void video::on_video_back_clicked()
{
    printf("on_video_back_clicked\r\n");

    if (mplayerProcess->state() == QProcess::Running) {
        mplayerProcess->write("quit\n");
        mplayerProcess->waitForBytesWritten(1000);
        mplayerProcess->waitForFinished(3000);  // 等待mplayer进程结束
    }

    this->close();

    MainWindow *m = new MainWindow();
    m->show();
}

void video::on_file_list_clicked()
{
    QString path;
#ifdef __arm__
    path = defaultVideoPath;
#else
    path = QCoreApplication::applicationDirPath() + "/video";
#endif

    currentFilePath = QFileDialog::getOpenFileName(this, "Open File", path, "Video(*.mp4 *.avi *.mkv)");

    if (!currentFilePath.isEmpty()) {
        // 如果列表中还没有这个文件，重新刷新一下列表
        if (!videoFiles.contains(currentFilePath)) {
            loadvideoFiles();
        }

        currentIndex = videoFiles.indexOf(currentFilePath);
        playVideo(currentFilePath);
    }
}

void video::on_next_clicked()
{
    printf("on_next_clicked\r\n");

    if (videoFiles.isEmpty()) {
        // 尝试最后一次刷新列表，防止用户中途放了视频进去
        loadvideoFiles();
        if(videoFiles.isEmpty()){
            QMessageBox::warning(this, "No Video", "No video files found!");
            return;
        }
    }

    currentIndex = (currentIndex + 1) % videoFiles.size();

    currentFilePath = videoFiles.at(currentIndex);
    playVideo(currentFilePath);
}

void video::on_previous_clicked()
{
    printf("on_previous_clicked\r\n");

    if (videoFiles.isEmpty()) {
        QMessageBox::warning(this, "No Video", "No video files found in the default directory.");
        return;
    }

    currentIndex = (currentIndex - 1 + videoFiles.size()) % videoFiles.size();

    currentFilePath = videoFiles.at(currentIndex);
    playVideo(currentFilePath);
}


// 音量调节模拟 (on_volume_Slider_valueChanged)
// 在 Ubuntu 上调用 amixer 可能会因为权限或声卡名称不同而失败，建议在 #else 中仅进行打印或使用 pactl 指令
void video::on_volume_Slider_valueChanged(int value)
{
    printf("on_volume_Slider_valueChanged, %d\r\n", value);

    // 1. 关键修复：当用户滑动时，立刻显示音量面板
    ui->volume_widget->setVisible(true);

    ui->volume_label->setText(QString("%1%").arg(value));
#ifdef __arm__
    // 将滑块的值转换为音量百分比
    int volume = value;

    // 发送音量调节命令给 mplayer
    // MPlayer 音量：通过写入 volume 命令改变播放器内部增益
    QString command = QString("volume %1 1\n").arg(volume);
    mplayerProcess->write(command.toUtf8());

    // 系统音量 (amixer)：通过 system() 调用底层的 ALSA 音频驱动指令，直接调节硬件扬声器的增益
    QByteArray byteArray = QString("amixer sset Speaker %1,%1").arg((float)volume*127.0/100.0).toUtf8();
    const char* cmd = byteArray.data();

    system(cmd);
#else
    // 模拟：给 Ubuntu 上的 mplayer 发送指令
    if (mplayerProcess->state() == QProcess::Running) {
        QString command = QString("volume %1 1\n").arg(value);
        mplayerProcess->write(command.toUtf8());
    }
    printf("PC Volume Simulated: %d%%\n", value);
#endif
    // 2. 重新启动定时器：
    // 如果用户一直在滑，定时器会不断重置，直到用户停止滑动 2 秒后才会隐藏
    volume_timer->start(2000);
}
