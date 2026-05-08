#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include "camera.h"
#include "ap3216c.h"
#include "icm20608.h"
#include "uart.h"
#include "music_player.h"
#include "sketchpad.h"
#include "adc.h"
#include "gpio.h"
#include "can.h"
#include "ble.h"
#include "config.h"
#include "video.h"
#include "recording.h"


QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    // Q_OBJECT 宏:告诉 Qt 的预编译器（MOC - Meta Object Compiler），这个类需要使用信号与槽机制
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

protected:
    void closeEvent(QCloseEvent *event);
    void showEvent(QShowEvent *event);

private:
    Ui::MainWindow *ui;
    // QScrollArea *pScroll;
    QPoint lastPos;         // 记录鼠标（或触摸屏）上一次点击的坐标
    QTimer *pressTimer;     // 长按计时器
    QPushButton *currentButton;     // 记录当前正在被按下的到底是哪一个图标,方便在长按超时时知道该打开哪一个窗口

    // 在 Qt 中，窗口对鼠标/触摸的感知是通过**“事件处理器（Event Handlers）”实现的。
    // 这不是通过简单的 connect 绑定的，而是通过重写（Override）**基类 QWidget 的虚函数来拦截系统事件
    // 按照特定的函数名（如 mouseMoveEvent）写好逻辑，Qt 就会在对应的物理动作发生时自动调用你的代码
    void mousePressEvent(QMouseEvent *event);
    void mouseMoveEvent(QMouseEvent *event);
    void connectButton(QPushButton *button);
    void handleLongPress();
};
#endif // MAINWINDOW_H
