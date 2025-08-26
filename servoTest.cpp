#include <iostream>
#include <iomanip> // for std::setprecision
#include <thread>  // for std::this_thread::sleep_for
#include <chrono>  // for std::chrono::milliseconds
#include <cstdio>
#include <termios.h>
#include "loborobot.hpp"
#include <atomic>

// ============ 键盘控制工具 ============
int getch() {
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

// ==================== 主程序 ====================
int main() {
    
    try {

        LOBOROBOT robot(false); // true = 开启调试模式;false = 关闭调试模式
        
        //初始化舵机位置
        robot.setServoAngle(10, 80);//底座舵机
        robot.setServoAngle(9, 0);//顶部舵机
        //底座舵机旋转角度范围0~180度
        //顶部舵机旋转角度范围0~90度

        float t = 0.5f, angle = 10.0f;
        float baseAngle = 80, topAngle = 0;
        bool autoDirection = true;
        std::atomic<bool> autoRotation(false); // 控制自动旋转的标志位

        std::cout << "初始化: 舵机每隔 " << t << "s 旋转 " << angle << "度\n";
        std::cout << "控制舵机: \n";
        std::cout << "  J/L: 底座舵机逆/顺时针旋转\n";
        std::cout << "  I/K: 顶部舵机上/下旋转\n";
        std::cout << "  Z:   开始自动旋转 (底座舵机)\n";
        std::cout << "  O:   停止自动旋转\n";
        std::cout << "  X:   退出程序\n";
        std::cout << "  C:   设置旋转角度\n";
        std::cout << "  T:   设置间隔时间\n";

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
        
        bool running = true;
        while (running) {
            int key = getch();
            if (key != EOF) { // 如果按下了按键
                std::cout << "检测到按键: " << static_cast<char>(key) << std::endl;
                switch(key) {  
                    //控制底座舵机旋转
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
                    
                    //控制顶部舵机旋转
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
                        
                    //开始
                    case 'z':
                    case 'Z':
                        autoRotation = true;
                        std::cout << "\n[状态] 开始自动旋转 (底座在0-180度间往返)...\n";
                        break;
                                        
                    //暂停
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
                        
                    //出退    
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

    return 0;
}