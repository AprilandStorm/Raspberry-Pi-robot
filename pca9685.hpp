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
    int i2c_fd{-1};
    int addr{0x40};
    bool debug{false};
    
    //对某个寄存器写 1 字节
    void write8(uint8_t reg, uint8_t value);

    //对某个寄存器读 1 字节
    uint8_t read8(uint8_t reg);

public:
    //构造函数里通过 /dev/i2c-1 打开 I²C 总线，并用 ioctl(fd, I2C_SLAVE, 0x40) 选到 PCA9685 的地址（默认 0x40）
    PCA9685(int bus = 1, int address = 0x40, bool debug_mode = false);
    
    ~PCA9685();

    //把 PWM 频率（比如 50Hz）转成 prescale，按照 datasheet 的流程改 MODE1、写 PRESCALE，最后让芯片进入自动递增+响应时钟状态
    void setPWMFreq(float freq);

    //把第 N 路的比较值写进去，on/off 是 0~4095 的 12 位周期位置
    void setPWM(uint8_t channel, int on, int off);

    //把 100% 的占空比换算成 4095，并调用 setPWM(...)
    void setDutyCycle(uint8_t channel, float duty);

    //把某通道拉成“常高/常低”（OFF=4095 / OFF=0），等价于“数字输出”
    void setLevel(uint8_t channel, int value);

    // 与 Python 一致的两种舵机接口：
    void setServoPulse(uint8_t ch, int pulseUs, float freqHz = 60.0f);

    void setServoAngle(uint8_t ch, float angleDeg);
};