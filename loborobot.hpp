#pragma once
#include "pca9685.hpp"
#include <string>

// ==================== LOBOROBOT 类 ====================
class LOBOROBOT {
private:
    PCA9685 pwm;//依赖的PWM控制器对象（初始化时创建，用于输出PWM信号）

    // PWM通道映射
    int PWMA = 0, AIN1 = 2, AIN2 = 1;//A是左前
    int PWMB = 5, BIN1 = 3, BIN2 = 4;//B是右前
    int PWMC = 6, CIN2 = 7, CIN1 = 8;//C是左后
    int PWMD = 11, DIN1 = 25, DIN2 = 24;//D是右后

public:
    LOBOROBOT(bool debug = false);

    void MotorRun(int motor, std::string index, float speed);// 电机运转（指定电机、方向、速度）

    void MotorStop(int motor);// 单个电机停止
    
    //前进
    void t_up(float speed);
    
    //后退
    void t_back(float speed);
    
    //左转
    void turnLeft(float speed);

    //右转
    void turnRight(float speed);
    //左移
    void moveLeft(float speed);

    //右移
    void moveRight(float speed);

    //前左斜
    void forwardLeft(float speed);

    //前右斜
    void forwardRight(float speed);

    //后左斜
    void backwardLeft(float speed);

    //后右斜
    void backwardRight(float speed);

    //停止
    void t_stop(); 

    //舵机控制函数
    void setServoAngle(uint8_t ch, float angleDeg);
};
