#ifndef GPIO_H
#define GPIO_H

#include <QMainWindow>
#include <QFile>

// 一种解耦设计:声明了一个由 Qt Designer（.ui 文件）生成的界面类。
// 目的：保证逻辑代码（gpio.cpp）和界面设计代码（ui_gpio.h）互不干扰
namespace Ui { class gpio; }

class gpio : public QMainWindow
{
    // Q_OBJECT 宏:告诉 Qt 的预编译器（MOC - Meta Object Compiler），这个类需要使用信号与槽机制
    Q_OBJECT
public:
    explicit gpio(QWidget *parent = nullptr);
    ~gpio();
signals:

// 信号与槽（Signals and Slots）
// 通过 “自动关联（Automatic Connection）” 机制实现的。你不需要手动写 connect(...) 代码，Qt 会根据函数的命名规则自动帮你接好线
// 如果一个函数满足以下特定的命名格式，Qt 就会在编译 UI 文件时自动将其与对应的组件绑定
//      on_<组件对象名>_<信号名>(参数)
//          # on: 固定前缀，告诉 Qt 这是一个自动连接的槽函数。
//          # led: 对应你在 Qt Designer（UI 编辑器）里给那个按钮起的 objectName。
//          # clicked: 对应按钮发出的 信号（Signal） 名。当按钮被点击或选中状态改变时，会发射这个信号。
//          # (bool checked): 信号传递的参数。因为你的按钮是 checkable（可选中）的，所以它会传回一个布尔值，告诉你现在是“开(true)”还是“关(false)”

// 私有槽函数
private slots:
    void on_gpio_back_clicked();
    void on_beep_clicked(bool checked);
    void on_led_clicked(bool checked);


private:
    // 指向界面所有组件的指针
    Ui::gpio *ui;
    // 硬件在软件中的“代理人”
    // 原理：在 Linux 嵌入式开发中，驱动程序把硬件映射为文件。
    // 通过声明这两个 QFile 变量，程序可以像读写文本一样控制红灯和蜂鸣器
    QFile led_file;
    QFile beep_file;

    // 封装: 避免了重复代码
    /* 设置lED的状态 */
    void setGpioState(QFile &file, bool value);
    /* 获取lED的状态 */
    bool getGpioState(QFile &file);

    // 重写: 对基类 QMainWindow 虚函数的重写
    /* 重写按键按下和释放事件 */
    void keyReleaseEvent(QKeyEvent *event);
    void keyPressEvent(QKeyEvent *event);

};


#endif // GPIO_H

// 嵌入式 Qt 的三层结构：
//  # 表现层：Ui::gpio *ui（你看到的界面）。
//  # 逻辑层：slots 和事件重写（你操作后的反应）。
//  # 驱动层（模拟）：QFile（你最终控制的硬件文件）
