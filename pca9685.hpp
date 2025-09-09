#pragma once
#include <cstdint>
#include <cstdio>
#include <cmath>
#include <stdexcept>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/i2c-dev.h>
#include <cerrno>

// ==================== PCA9685 驱动类 ====================
class PCA9685 {
private:
    int i2c_fd{-1};//I2C设备文件描述符
    int addr{0x40};//PCA9685默认I2C地址
    bool debug{false};//调试模式标志
    
    //对某个寄存器写 1 字节
    void write8(uint8_t reg, uint8_t value);

    //对某个寄存器读 1 字节
    uint8_t read8(uint8_t reg);

public:
    //构造函数:初始化I2C连接
    PCA9685(int bus = 1, int address = 0x40, bool debug_mode = false);

    //析构函数：关闭I2C连接
    ~PCA9685();

    //设置 PWM 频率（比如 50Hz）
    void setPWMFreq(float freq);

    //直接设置PWM的ON/OFF值
    void setPWM(uint8_t channel, int on, int off);

    //设置占空比（百分比）
    void setDutyCycle(uint8_t channel, float duty);

    //设置电平，把某通道拉成“常高/常低”（OFF=4095 / OFF=0），等价于“数字输出”
    void setLevel(uint8_t channel, int value);

    // 舵机控制：设置脉冲宽度
    void setServoPulse(uint8_t ch, int pulseUs, float freqHz = 60.0f);

    // 舵机控制：设置角度
    void setServoAngle(uint8_t ch, float angleDeg);
};
