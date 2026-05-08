#include "mainwindow.h"
#include "icm20608.h"
#include "ui_icm20608.h"

/* icm20608实验模块:六轴加速度计 & 陀螺仪 (SPI 接口)
    实现了一个功能相当强大的姿态解算系统。
    它不仅仅是读取数据，还通过数学算法将传感器原始的加速度和角速度转变成了人类直观可见的欧拉角（航向角、俯仰角、横滚角）*/

icm20608::icm20608(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::icm20608)
{
    ui->setupUi(this);
    this->setWindowTitle("Icm20608");
    this->setFixedSize(800,480);

    timer = new QTimer();
#if __arm__
    system("cd /lib/modules/4.1.15");
    system("depmod");
    system("modprobe icm20608.ko");
#endif
    //
    connect(timer, SIGNAL(timeout()), this, SLOT(icm20608_timer_timeout()));

#if __arm__
    fd = open(filename, O_RDWR);
    if(fd < 0) {
        printf("can't open file %s\r\n", filename);
        return;
    }

    calibrate_gyroscope(gyro_bias);

#endif
    // 更新频率：定时器设置为 10ms (100Hz)，这符合姿态解算对实时性的要求
    timer->start(10);
}


icm20608::~icm20608()
{
    delete ui;
    delete timer;
}


// 核心算法层 (四元数与互补滤波)
// calibrate_gyroscope (陀螺仪校准)
void icm20608::calibrate_gyroscope(int16_t *gyro_bias)
{
    int32_t gyro_sum[3] = {0, 0, 0};
    int16_t gyro_raw[3];
    int ret = 0;
    unsigned int i;

    for (i = 0; i < NUM_SAMPLES; i++)
    {
        // 数据采集: 从 SPI 驱动中获取原始数据。databuf[0-2] 是陀螺仪（角速度），databuf[3-5] 是加速度计
        ret = read(fd, databuf, sizeof(databuf));
        if(ret == 0) {
            gyro_raw[0] = databuf[0];
            gyro_raw[1] = databuf[1];
            gyro_raw[2] = databuf[2];
            gyro_sum[0] += gyro_raw[0];
            gyro_sum[1] += gyro_raw[1];
            gyro_sum[2] += gyro_raw[2];
        }
    }

    gyro_bias[0] = gyro_sum[0] / NUM_SAMPLES;
    gyro_bias[1] = gyro_sum[1] / NUM_SAMPLES;
    gyro_bias[2] = gyro_sum[2] / NUM_SAMPLES;

    printf("Calibrate_Gyroscope done!\r\n");
}



void icm20608::icm20608_get_six_axis_data(float *gyro, float *acc)
{
    unsigned char i = 0;
    int ret = 0;
    short gyro_raw[3], acc_raw[3];

    ret = read(fd, databuf, sizeof(databuf));
    if(ret == 0) {
        gyro_raw[0] = databuf[0];
        gyro_raw[1] = databuf[1];
        gyro_raw[2] = databuf[2];
        acc_raw[0] = databuf[3];
        acc_raw[1] = databuf[4];
        acc_raw[2] = databuf[5];
    }


    for(i=0;i<3;i++){
        gyro[i]= (float) (gyro_raw[i]-gyro_bias[i]) * gyroscale;
        acc[i]= (float) acc_raw[i] * accscale;
        //acc[i] = acc_calib_k[i] * acc[i] + acc_calib_a[i];
    }
}



float icm20608::icm20608_get_temperature()
{
    short raw = 0;
    float temp;
    int ret = 0;

    ret = read(fd, databuf, sizeof(databuf));
    if(ret == 0) {
        raw = databuf[6];
    }

    temp = 21 + ((double)raw) / 333.87;

    return temp;;
}


// icm20608_update (姿态解算)
void icm20608::icm20608_update(float gx, float gy, float gz, float ax, float ay, float az)
{
    float norm;
    float vx, vy, vz;
    float ex, ey, ez;

    float q0q0 = q0 * q0;
    float q0q1 = q0 * q1;
    float q0q2 = q0 * q2;
    //float q0q3 = q0 * q3;
    float q1q1 = q1 * q1;
    //float q1q2 = q1 * q2;
    float q1q3 = q1 * q3;
    float q2q2 = q2 * q2;
    float q2q3 = q2 * q3;
    float q3q3 = q3 * q3;

    if (ax * ay * az == 0)
    {
        return;
    }

    norm = sqrt(ax * ax + ay * ay + az * az);       //
    ax = ax / norm;
    ay = ay / norm;
    az = az / norm;

    // estimated direction of gravity and flux (v and w)
    vx = 2 * (q1q3 - q0q2);
    vy = 2 * (q0q1 + q2q3);
    vz = q0q0 - q1q1 - q2q2 + q3q3 ;

    // error is sum of cross product between reference direction of fields and direction measured by sensors
    ex = (ay * vz - az * vy) ;
    ey = (az * vx - ax * vz) ;
    ez = (ax * vy - ay * vx) ;

    exInt = exInt + ex * Ki;
    eyInt = eyInt + ey * Ki;
    ezInt = ezInt + ez * Ki;

    // adjusted gyroscope measurements
    gx = gx + Kp * ex + exInt;
    gy = gy + Kp * ey + eyInt;
    gz = gz + Kp * ez + ezInt;

    // integrate quaternion rate and normalise
    q0 = q0 + (-q1 * gx - q2 * gy - q3 * gz) * halfT;
    q1 = q1 + (q0 * gx + q2 * gz - q3 * gy) * halfT;
    q2 = q2 + (q0 * gy - q1 * gz + q3 * gx) * halfT;
    q3 = q3 + (q0 * gz + q1 * gy - q2 * gx) * halfT;

    // normalise quaternion
    norm = sqrt(q0 * q0 + q1 * q1 + q2 * q2 + q3 * q3);
    q0 = q0 / norm;
    q1 = q1 / norm;
    q2 = q2 / norm;
    q3 = q3 / norm;

    yaw = atan2(2 * q1 * q2 + 2 * q0 * q3, -2 * q2 * q2 - 2 * q3 * q3 + 1) * 57.3; // unit:degree
    pitch  = asin(-2 * q1 * q3 + 2 * q0 * q2) * 57.3; // unit:degree
    roll = atan2(2 * q2 * q3 + 2 * q0 * q1, -2 * q1 * q1 - 2 * q2 * q2 + 1) * 57.3; // unit:degree
}


// UI 显示层 (ADI 仪表盘)
void icm20608::update_attitude_3D()
{
    // ui->ADI: 通常是一个自定义控件（Attitude Direction Indicator，姿态指引仪），模拟了飞机的地平仪
    ui->ADI->setRoll(pitch);
    ui->ADI->setPitch(roll);
    ui->ADI->update();
    ui->label_x->setNum(yaw);
    ui->label_y->setNum(pitch);
    ui->label_z->setNum(roll);
}

void icm20608::icm20608_timer_timeout()
{
#ifdef __arm__
    icm20608_get_six_axis_data(gyro, acc);
    icm20608_update(gyro[0], gyro[1], gyro[2], acc[0], acc[1], acc[2]);
#else
//    // 目前的源码中 #else 部分只是简单的 pitch ++，这会导致数据线性增加
//    pitch ++;
//    roll ++;
//    yaw ++;
//    pitch = pitch > 180?(-180):pitch;
//    roll = roll > 180?(-180):roll;
//    yaw = yaw > 180?(-180):yaw;

//    //pitch = QRandomGenerator::global()->bounded(-5,5);
//    //roll = QRandomGenerator::global()->bounded(-5,5);
//    //yaw  = QRandomGenerator::global()->bounded(-5,5);

    // 为了让模拟更真实（例如模拟一个晃动的效果）
    // [SIMULATION] 使用三角函数模拟平滑的晃动效果
    static float angle = 0;
    angle += 0.05; // 旋转步长

    // 模拟正弦波晃动
    pitch = 20 * sin(angle);         // 俯仰角在 ±20度 摆动
    roll  = 30 * cos(angle * 0.8);   // 横滚角在 ±30度 摆动
    yaw   = (int)(angle * 57.3) % 360; // 航向角持续旋转

    // 可以在模拟模式下手动控制仪表盘，验证 UI 渲染是否流畅
#endif
    update_attitude_3D();
}



void icm20608::on_icm20608_back_clicked()
{
    printf("on_icm20608_back_clicked\r\n");
    timer->stop();

#if __arm__
    system("rmmod icm20608.ko");
#endif

    this->close();
    MainWindow *m = new MainWindow();
    m->show();
}


