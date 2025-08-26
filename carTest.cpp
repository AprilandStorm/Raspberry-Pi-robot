#include <iostream>
#include <termios.h>
#include <unistd.h>
#include <cstdio>
#include "loborobot.hpp"

// ============ 键盘控制工具 ============
int getch() {
    struct termios oldt, newt;
    int ch;
    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;
    newt.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);
    ch = getchar();
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
        LOBOROBOT robot(true); // true = 开启调试模式;false = 关闭调试模式

        std::cout << "WASD 控制小车，空格停止, +/- 调整速度, 数字 1..9 直接将速度设 10%..90%, 0 设 0, X 退出\n";
        
        float speed = 30.0f;
        std::cout << "当前速度: " << speed << "%\n";
        bool running = true;
        while (running) {
            int key = getch();
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
                case 'A':
                    robot.turnLeft(speed);
                    std::cout << "左转\n";
                    break;
                case 'd':
                case 'D':
                    robot.turnRight(speed);
                    std::cout << "右转\n";
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
                
                //退出    
                case 'x':
                case 'X':
                    running = false;
                    std::cout << "退出指令接收\n";
                    break;
                default:
                    break;
            }
        }
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
