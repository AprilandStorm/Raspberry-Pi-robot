#include "loborobot.hpp"
#include <wiringPi.h>
#include <iostream>
#include <cmath>

LOBOROBOT::LOBOROBOT(bool debug) : pwm(1, 0x40, debug) {
    //初始化PCA9685：I2C总线1（树莓派默认）、地址0x40（PCA9685默认）、调试模式
    pwm.setPWMFreq(50);
    //初始化树莓派GPIO（仅用于电机的方向控制，因DIN1/DIN2是GPIO引脚，非PCA9685通道）
    wiringPiSetupGpio();//初始化 WiringPi 库，并使用 BCM GPIO 编号方式（即 GPIOxx 编号）
    //配置 DIN1/DIN2 为输出并默认拉低（D电机用到了树莓派的两个方向脚）。
    pinMode(DIN1, OUTPUT);// BCM GPIO18
    pinMode(DIN2, OUTPUT);// BCM GPIO21
}

void LOBOROBOT::MotorRun(int motor, std::string index, float speed) {
    std::cout << "[MotorRun] motor=" << motor << ", dir=" << index << ", speed=" << speed << std::endl;

    if (speed > 100) speed = 100;
    if (motor == 0) {// 电机A（左前）
        pwm.setDutyCycle(PWMA, speed);//设置占空比，调整速度
        if (index == "forward") {
            pwm.setLevel(AIN1, LOW);
            pwm.setLevel(AIN2, HIGH);
            std::cout << "[GPIO] AIN1=LOW, AIN2=HIGH\n";
        } else if(index == "backward"){
            pwm.setLevel(AIN1, HIGH);
            pwm.setLevel(AIN2, LOW);
            std::cout << "[GPIO] AIN1=HIGH, AIN2=LOW\n";
        }
    } else if (motor == 1) {//电机B（右前）
        pwm.setDutyCycle(PWMB, speed);
        if (index == "forward") {
            pwm.setLevel(BIN1, HIGH);
            pwm.setLevel(BIN2, LOW);
            std::cout << "[GPIO] BIN1=HIGH, BIN2=LOW\n";
        } else if(index == "backward"){
            pwm.setLevel(BIN1, LOW);
            pwm.setLevel(BIN2, HIGH);
            std::cout << "[GPIO] BIN1=LOW, BIN2=HIGH\n";
        }
    } else if (motor == 2) {//电机C（左后）
        pwm.setDutyCycle(PWMC, speed);
        if (index == "forward") {
            pwm.setLevel(CIN1, HIGH);
            pwm.setLevel(CIN2, LOW);
            std::cout << "[GPIO] CIN1=HIGH, CIN2=LOW\n";
        } else if(index == "backward"){
            pwm.setLevel(CIN1, LOW);
            pwm.setLevel(CIN2, HIGH);
            std::cout << "[GPIO] CIN1=LOW, CIN2=HIGH\n";
        }
    } else if (motor == 3) {
        pwm.setDutyCycle(PWMD, speed);
        if (index == "forward") {
            digitalWrite(DIN1, LOW);
            digitalWrite(DIN2, HIGH);
            std::cout << "[GPIO] DIN1=LOW, DIN2=HIGH\n";
        } else if(index == "backward"){
            digitalWrite(DIN1, HIGH);
            digitalWrite(DIN2, LOW);
            std::cout << "[GPIO] DIN1=HIGH, DIN2=LOW\n";
        }
    }
}

void LOBOROBOT::MotorStop(int motor) {
    // 核心逻辑：将对应电机的PWM占空比设为0（切断电机电源）
    pwm.setDutyCycle(motor == 0 ? PWMA : motor == 1 ? PWMB : motor == 2 ? PWMC : PWMD, 0);
    std::cout << "[MotorStop] motor=" << motor << std::endl;
}

//前进
void LOBOROBOT::t_up(float speed) {
    MotorRun(0, "forward", speed);
    MotorRun(1, "forward", speed);
    MotorRun(2, "forward", speed);
    MotorRun(3, "forward", speed);
}
    
//后退
void LOBOROBOT::t_back(float speed) {
    MotorRun(0, "backward", speed);
    MotorRun(1, "backward", speed);
    MotorRun(2, "backward", speed);
    MotorRun(3, "backward", speed);
}
    
//左转
void LOBOROBOT::turnLeft(float speed) {
    MotorRun(0, "backward", speed);
    MotorRun(1, "forward", speed);
    MotorRun(2, "backward", speed);
    MotorRun(3, "forward", speed);
}

//右转
void LOBOROBOT::turnRight(float speed) {
    MotorRun(0, "forward", speed);
    MotorRun(1, "backward", speed);
    MotorRun(2, "forward", speed);
    MotorRun(3, "backward", speed); 
}

//左移
void LOBOROBOT::moveLeft(float speed) {
    MotorRun(0, "backward", speed);
    MotorRun(1, "forward", speed);
    MotorRun(2, "forward", speed);
    MotorRun(3, "backward", speed);
}

//右移
void LOBOROBOT::moveRight(float speed) {
    MotorRun(0, "forward", speed);
    MotorRun(1, "backward", speed);
    MotorRun(2, "backward", speed);
    MotorRun(3, "forward", speed);
}

//前左斜
void LOBOROBOT::forwardLeft(float speed) {
    MotorStop(0);
    MotorRun(1, "forward", speed);
    MotorRun(2, "forward", speed);
    MotorStop(3);
}

//前右斜
void LOBOROBOT::forwardRight(float speed) {
    MotorRun(0, "forward", speed);
    MotorStop(1);
    MotorStop(2);
    MotorRun(3, "forward", speed);
}

//后左斜
void LOBOROBOT::backwardLeft(float speed) {
    MotorRun(0, "backward", speed);
    MotorStop(1);
    MotorStop(2);
    MotorRun(3, "backward", speed);
}

//后右斜
void LOBOROBOT::backwardRight(float speed) {
    MotorStop(0);
    MotorRun(1, "backward", speed);
    MotorRun(2, "backward", speed);
    MotorStop(3);
}

//停止
void LOBOROBOT::t_stop() {
    MotorStop(0);
    MotorStop(1);
    MotorStop(2);
    MotorStop(3);
}

void LOBOROBOT::setServoAngle(uint8_t ch, float angleDeg) {
    float pulseUs = (angleDeg * 11.0f) + 500.0f; 
    
    // 通过 pwm 实例调用 PCA9685 的 setServoPulse 方法
    // 注意：这里的调用方式是 pwm.setServoPulse，因为 pwm 是 LOBOROBOT 类的一个成员变量
    pwm.setServoPulse(ch, (int)std::round(pulseUs), 50.0f);
}
