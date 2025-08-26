#include "pca9685.hpp"
#include <iostream>
#include <cstring>
#include <cstdlib>

void PCA9685::write8(uint8_t reg, uint8_t value) {
    uint8_t buf[2] = {reg, value};
    if (write(i2c_fd, buf, 2) != 2) {
        perror("I2C 写入失败");
    } else if (debug) {
        printf("[I2C 写入] reg=0x%02X, val=0x%02X\n", reg, value);
    }
}


uint8_t PCA9685::read8(uint8_t reg) {
    if (write(i2c_fd, &reg, 1) != 1) {
        perror("I2C 寄存器地址写入失败");
        return 0;
    }
    uint8_t data;
    if (read(i2c_fd, &data, 1) != 1) {
        perror("I2C 读取失败");
        return 0;
    }
    if (debug) {
        printf("[I2C 读取] reg=0x%02X, val=0x%02X\n", reg, data);
    }
    return data;
}

PCA9685::PCA9685(int bus, int address, bool debug_mode)
    : addr(address), debug(debug_mode) {
    char filename[20];
    snprintf(filename, sizeof(filename), "/dev/i2c-%d", bus);
    if ((i2c_fd = open(filename, O_RDWR)) < 0) {
        perror("无法打开I2C总线");
        exit(1);
    }
    if (ioctl(i2c_fd, I2C_SLAVE, addr) < 0) {
        perror("无法连接PCA9685");
        close(i2c_fd);
        exit(1);
    }
    write8(0x00, 0x00); // MODE1复位
}

PCA9685::~PCA9685() {
    if (i2c_fd >= 0) close(i2c_fd);
}

void PCA9685::setPWMFreq(float freq) {
    float prescaleval = 25000000.0 / 4096.0 / freq - 1.0f;
    uint8_t prescale = static_cast<uint8_t>(std::floor(prescaleval + 0.5f));
    uint8_t oldmode = read8(0x00);// MODE1
    uint8_t newmode = (oldmode & 0x7F) | 0x10;
    write8(0x00, newmode);
    write8(0xFE, prescale);
    write8(0x00, oldmode);
    usleep(5000);
    write8(0x00, oldmode | 0x80);// 重启(自动增量)

    if (debug) {
        printf("[PWM频率设置] freq=%.2fHz, prescale=%d\n", freq, prescale);
    }
}

void PCA9685::setPWM(uint8_t channel, int on, int off) {
    write8(0x06 + 4 * channel, on & 0xFF);
    write8(0x07 + 4 * channel, (on >> 8) & 0x0F);
    write8(0x08 + 4 * channel, off & 0xFF);
    write8(0x09 + 4 * channel, (off >> 8) & 0x0F);

    if (debug) {
        printf("[PWM输出] CH=%d, ON=%d, OFF=%d\n", channel, on, off);
    }
}

void PCA9685::setDutyCycle(uint8_t channel, float duty) {
    if (duty < 0) duty = 0;
    if (duty > 100) duty = 100;
    int off = static_cast<int>(4096 * duty / 100.0f);
    setPWM(channel, 0, off);
    if (debug) {
        printf("[占空比] CH=%d, duty=%.1f%%\n", channel, duty);
    }
}

void PCA9685::setLevel(uint8_t channel, int value) {
    if (value == 1) setPWM(channel, 0, 4095);
    else setPWM(channel, 0, 0);
    if (debug) {
        printf("[电平] CH=%d, val=%d\n", channel, value);
    }
}

void PCA9685::setServoPulse(uint8_t ch, int pulseUs, float freqHz) {
    // 1s / freq = periodUs；每周期 4096 个刻度 => 每刻度 us = periodUs / 4096
    // off = pulseUs / (period_us/4096) = pulseUs * 4096 / periodUs
    float periodUs = 1e6f / freqHz;
    int offServo = (int) std::round(pulseUs * 4096.0f / periodUs);
    if (offServo < 0) {
        offServo = 0; 
    }
    if (offServo > 4095) {
        offServo = 4095;
    }
    setPWM(ch, 0, offServo);
    if (debug) {
        std::printf("[SERVO PULSE] ch=%u, %dus @ %.1fHz => offServo=%d\n", ch, pulseUs, freqHz, offServo);
    }
}

void PCA9685::setServoAngle(uint8_t ch, float angleDeg) {
    // Python 版：angle = 4096*((angle*11)+500)/20000  (freq=50Hz 假设 20ms 周期)
    // 这里直接复刻：500–2500us 对应 0–180 度（≈11us/deg）
    float pulseUs = (angleDeg * 11.0f) + 500.0f; // 与原脚本一致
    setServoPulse(ch, (int)std::round(pulseUs), 50.0f);
}

