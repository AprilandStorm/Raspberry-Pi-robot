#include <iostream>
#include <iomanip> // for std::setprecision
#include <thread>  // for std::this_thread::sleep_for
#include <chrono>  // for std::chrono::milliseconds
#include <atomic>  // 原子变量（autoRotation）
#include <termios.h> // 终端控制（getch）
#include <unistd.h> //  POSIX 系统调用（getchar, sleep）
#include <cstdio>    // C 风格输入输出
#include "loborobot.hpp"

// ============ 键盘控制工具 ============
int getch() {//实现无回车输入
    struct termios oldt, newt;
    int ch;
    // 获取当前终端设置
    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;
    // 关闭规范模式和回显，实现立即获取输入
    newt.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);
    // 等待并获取单个字符
    ch = getchar();
    //恢复原来的终端设置
    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
    return ch;
}

static float clamp01(float v) {
    if (v < 0) v = 0;
    if (v > 100) v = 100;
    return v;
}

// ==================== 主程序 ====================
int main() {
    
    try {
        LOBOROBOT robot(false); // true = 开启调试模式;false = 关闭调试模式
        
        //初始化小车速度
        float speed = 30.0f;
        //初始化舵机位置
        robot.setServoAngle(10, 80);//底座舵机，底座舵机旋转角度范围0~180度
        robot.setServoAngle(9, 0);//顶部舵机，顶部舵机旋转角度范围0~90度
        //初始化舵机旋转角度和时间间隔
        float t = 0.5f, angle = 10.0f;
        float baseAngle = 80, topAngle = 0;
        bool autoDirection = true;
        std::atomic<bool> autoRotation(false); // 控制自动旋转的标志位（原子变量，线程安全）

        std::cout << "W/w: 控制小车前进\n";
        std::cout << "S/s: 控制小车后退\n";
        std::cout << "a:   控制小车向左转\n";
        std::cout << "d:   控制小车向右转\n";
        std::cout << "A:   控制小车左移\n";
        std::cout << "D:   控制小车右移\n";
        std::cout << "空格停止\n";
        std::cout << "+/- 调整速度";
        std::cout << "数字 1..9 直接将速度设 10%..90%, 0 设 0\n";
        std::cout << "初始化: 舵机每隔 " << t << "s 旋转 " << angle << "度\n";
        std::cout << "J/L: 底座舵机逆/顺时针旋转\n";
        std::cout << "I/K: 顶部舵机上/下旋转\n";
        std::cout << "Z:   开始自动旋转 (底座舵机)\n";
        std::cout << "O:   停止自动旋转\n";
        std::cout << "C:   设置旋转角度\n";
        std::cout << "T:   设置间隔时间\n";
        std::cout << "X:   退出程序\n";
        std::cout << "当前速度: " << speed << "%\n";

        // 新开一个线程处理自动旋转
        std::thread autoThread([&]() {
            while (true) {
                if (autoRotation) {
                    if (autoDirection) {
                        baseAngle += angle;
                        if (baseAngle >= 180) {
                            baseAngle = 180;
                            autoDirection = false;
                            std::cout << "[自动] 到达边界 190 度, 反转\n";
                        }
                    } else {
                        baseAngle -= angle;
                        if (baseAngle <= 0) {
                            baseAngle = 0;
                            autoDirection = true;
                            std::cout << "[自动] 到达边界 2 度, 反转\n";
                        }
                    }
                    robot.setServoAngle(10, baseAngle);
                    std::cout << "\r[自动] 底座角度: " << baseAngle << "度" << std::flush;
                    std::this_thread::sleep_for(std::chrono::duration<float>(t));
                } else {
                    std::this_thread::sleep_for(std::chrono::milliseconds(50));
                }
            }
        });

        std:;atomic<bool> running = true;
        while (running) {
            int key = getch();
            if (key != EOF) { // 如果按下了按键
                std::cout << "检测到按键: " << static_cast<char>(key) << std::endl;
                switch(key) {
                    //方向
                    case 'w':
                    case 'W':
                        robot.t_up(speed);
                        std::cout << "前进\n";
                        break;
                    case 's':
                    case 'S':
                        robot.t_back(speed);
                        std::cout << "后退\n";
                        break;
                    case 'a':
                        robot.turnLeft(speed);
                        std::cout << "左转\n";
                        break;
                    case 'A':
                        robot.moveLeft(speed);
                        std::cout << "左移\n";
                        break;
                    case 'd':
                        robot.turnRight(speed);
                        std::cout << "右转\n";
                        break;
                    case 'D':
                        robot.moveRight(speed);
                        std::cout << "右移\n";
                        break;

                    //停止   
                    case ' ':
                        robot.t_stop();
                        std::cout << "停止\n";
                        break;
                
                    //速度调节
                    case '+':
                    case '=':
                        speed = clamp01(speed + 5);
                        std::cout << "速度 +" << 5 << "% => " << speed << "%\n";
                        break;
                    case '-':
                    case '_':
                        speed = clamp01(speed - 5);
                        std::cout << "速度 -" << 5 << "% => " << speed << "%\n";
                        break;
                
                   // 档位：数字键
                    case '0': 
                        speed = 0;  
                        std::cout << "速度 = 0%\n";  
                        robot.t_stop(); 
                        break;
                    case '1': 
                        speed = 10; 
                        std::cout << "速度 = 10%\n"; 
                        break;
                    case '2': 
                        speed = 20; 
                        std::cout << "速度 = 20%\n"; 
                        break;
                    case '3': 
                        speed = 30; 
                        std::cout << "速度 = 30%\n"; 
                        break;
                    case '4': 
                        speed = 40; 
                        std::cout << "速度 = 40%\n"; 
                        break;
                    case '5': 
                        speed = 50; 
                        std::cout << "速度 = 50%\n"; 
                        break;
                    case '6': 
                        speed = 60; 
                        std::cout << "速度 = 60%\n"; 
                        break;
                    case '7': 
                        speed = 70; 
                        std::cout << "速度 = 70%\n"; 
                        break;
                    case '8': 
                        speed = 80; 
                        std::cout << "速度 = 80%\n"; 
                        break;
                    case '9': 
                        speed = 90; 
                        std::cout << "速度 = 90%\n"; 
                        break;

                    //控制底座舵机旋转---------------------------------------------------
                    case 'j':
                    case 'J':
                        autoRotation = false; // 切换到手动模式
                        baseAngle += angle;
                        if (baseAngle > 190) {
                            baseAngle = 190;
                        }
                        robot.setServoAngle(10, baseAngle);
                        std::cout << "[操作] 底座舵机角度设置为: " << baseAngle << "度\n";
                        break;
                    
                    case 'L':
                    case 'l':
                        autoRotation = false; // 切换到手动模式
                        baseAngle -= angle;
                        if (baseAngle < 2) {
                            baseAngle = 2;
                        }
                        robot.setServoAngle(10, baseAngle);
                        std::cout << "[操作] 底座舵机角度设置为: " << baseAngle << "度\n";
                        break;
                    
                    //控制顶部舵机旋转----------------------------------------------------
                    case 'K':
                    case 'k':
                        autoRotation = false; // 切换到手动模式
                        topAngle += angle;
                        if (topAngle > 90) {
                            topAngle = 90;
                        }
                        robot.setServoAngle(9, topAngle);
                        std::cout << "[操作] 顶部舵机角度设置为: " << topAngle << "度\n";
                        break;
                    
                    case 'I':
                    case 'i':
                        autoRotation = false; // 切换到手动模式
                        topAngle -= angle;
                        if (topAngle < 0) {
                            topAngle = 0;
                        }
                        robot.setServoAngle(9, topAngle);
                        std::cout << "[操作] 顶部舵机角度设置为: " << topAngle << "度\n";
                        break;
                    //------------------------------------------------------------------

                    //开始自动旋转
                    case 'z':
                    case 'Z':
                        autoRotation = true;
                        std::cout << "\n[状态] 开始自动旋转 (底座在0-180度间往返)...\n";
                        break;
                        
                    //暂停舵机自动旋转
                    case 'o':
                    case 'O':
                        autoRotation = false;
                        std::cout << "\n[状态] 自动旋转已暂停。\n";
                        break;

                    //改变选择的角度
                    case 'c':
                    case 'C':{
                        autoRotation = false; // 切换到手动模式
                        float oldAngle = angle;
                        std::cout << "请输入新的旋转角度 (当前 " << angle << "): ";
                        std::cin >> angle;
                        std::cin.ignore(1000, '\n'); // 清除输入缓冲区
                        std::cout << "[设置] 旋转角度已从 " << oldAngle << " 改为 " << angle << " 度。\n";
                        break;
                    }

                    //改变间隔时间
                    case 't':
                    case 'T':{
                        autoRotation = false; // 切换到手动模式
                        float oldT = t;
                        std::cout << "请输入新的间隔时间 (秒, 当前 " << t << "): ";
                        std::cin >> t;
                        std::cin.ignore(1000, '\n'); // 清除输入缓冲区
                        std::cout << "[设置] 间隔时间已从 " << oldT << "s 改为 " << t << "s。\n";
                        break;
                    }
                
                    //退出    
                    case 'x':
                    case 'X':
                        //恢复到初始位置
                        autoRotation = false;
                        robot.setServoAngle(10, 80);//底座舵机
                        robot.setServoAngle(9, 0);//顶部舵机
                        baseAngle = 90, topAngle = 0;
                        running = false;
                        std::cout << "退出指令接收\n";
                        break;
                    default:
                        break;
                }
            }
        }
        autoThread.detach(); // 主程序退出时释放线程
        std::cout << "\n退出程序\n";
    } catch (const std::exception& e) {
        std::cerr << "程序异常终止: " << e.what() << std::endl;
        return 1;
    }

    //把 PCA9685 寄存器恢复到上电时的状态
    /*const char *i2cDevice = "/dev/i2c-1";
    int fd = open(i2cDevice, O_RDWR);
    // MODE1 写 0x00（正常模式），相当于 reset
    unsigned char buf[2] = {0x00, 0x00};
    if (write(fd, buf, 2) != 2) {
        perror("写入失败");
    } else {
        std::cout << "PCA9685 已恢复默认值" << std::endl;
    }
    close(fd);*/

    return 0;
}
