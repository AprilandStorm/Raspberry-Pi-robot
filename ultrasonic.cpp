#include <iostream>
#include <thread>
#include <chrono>
#include <atomic>
#include <vector>
#include <algorithm>
#include <numeric>
#include <csignal> // 包含信号处理头文件,捕获 Ctrl+C（SIGINT）信号
#include <lgpio.h> // 包含 lgpio 库的头文件,树莓派新一代 GPIO 控制库

// 根据接线修改 GPIO 引脚编号
#define TRIG 20
#define ECHO 21

// 全局原子变量，用于线程间安全地传递时间戳
static std::atomic<bool> trigger_active = false;// 是否完成一次回波测量
static std::atomic<uint64_t> start_tick = 0;// 上升沿时间戳（us）
static std::atomic<uint64_t> stop_tick = 0;// 下降沿时间戳（us）

// 用于控制循环退出的标志位
std::atomic<bool> run_loop(true);

// 信号处理函数
void signal_handler(int signum) {
    if (signum == SIGINT) {
        std::cout << "\nSIGINT received, shutting down gracefully..." << std::endl;
        run_loop = false;
    }
}

// ECHO 引脚的边缘触发回调函数
// 当 ECHO 引脚电平变化时，该函数会被 lgpio 库调用
void echo_edge_event_handler(int e, lgGpioAlert_s* evt, void *data) {
    if (e > 0) { // Check if there are any events
        for (int i = 0; i < e; ++i) {
            if (evt[i].report.level == 1) { // Rising edge
                start_tick = evt[i].report.timestamp / 1000;//ns->us
                //std::cout << "start_tick:" << start_tick;
            } else if (evt[i].report.level == 0) { // Falling edge
                stop_tick = evt[i].report.timestamp / 1000;//ns->us
                //std::cout << "stop_tick:" << stop_tick;
                // Set the flag to notify the main thread that measurement is complete
                if(stop_tick > start_tick){
                    trigger_active = true;// 通知主线程：测量完成
                }
            }
        }
    }
}

int main() {
    // 注册信号处理函数
    std::signal(SIGINT, signal_handler);

    // 1. 初始化 lgpio 库
    int handle = lgGpiochipOpen(0); // 打开 GPIO 芯片 0
    if (handle < LG_OKAY) {
        std::cerr << "lgGpiochipOpen failed, error code: " << handle << std::endl;
        return 1;
    }

    // 2. 设置 TRIG 引脚为输出模式，ECHO 为输入模式
    int ret1 = lgGpioClaimOutput(handle, 0, TRIG, 0);//初始为低电平
    if (ret1 != LG_OKAY) {
        std::cerr << "lgGpioClaimOutput(TRIG) failed, error code: " << ret1 << std::endl;
        lgGpiochipClose(handle);
        return 1;
    }

    // 3. Claim the GPIO for alerts, specifying both rising and falling edges
    // This replaces the event flag parameter in the previous function call
    //向 Linux 内核注册一个“边沿触发”的 GPIO 中断监听
    int ret2 = lgGpioClaimAlert(handle, 0, LG_BOTH_EDGES, ECHO, -1);// ECHO 监听双边沿
    if (ret2 < 0) {
        std::cerr << "lgGpioClaimAlert failed, error code: " << ret2 << std::endl;
        lgGpioFree(handle, TRIG);
        lgGpiochipClose(handle);
        return 1;
    }

    // 4. 注册 ECHO 引脚的事件监听器（上升沿和下降沿）
    int ret3 = lgGpioSetAlertsFunc(handle, ECHO, echo_edge_event_handler, nullptr);//当 ECHO 电平变化时，自动调用 echo_edge_event_handler
    if (ret3 != LG_OKAY) {
        std::cerr << "lgGpioSetAlertFunc failed, error code: " << ret3 << std::endl;
        lgGpioFree(handle, TRIG);
        lgGpioFree(handle, ECHO);
        lgGpiochipClose(handle);
        return 1;
    }
    
    // 5. 等待一段时间，确保传感器稳定
    lguSleep(0.5); // 0.5s
    
    std::atomic<bool> flagCamera(false);
    // 新开一个线程监视是否需要打开摄像头和补光灯进行拍照
    std::thread autoThread([&]() {
        while (run_loop.load()) {
            if (flagCamera) {
                //激活摄像头
                //打开补光灯
                //回传图像
            } else {
                //摄像头休眠
                //关闭补光灯
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(50)); // 防止空转
        }
    });

    // 6. 进入主循环
    while (run_loop.load()) {

        const int mearsurementsNum = 5;//一次测量的次数
        std::vector<double> results;
        for(int i = 0; i < mearsurementsNum; i++){
            // 6.1. 触发超声波脉冲,HC-SR04 要求 TRIG 至少拉高 10μs 才会发射超声波
            lgGpioWrite(handle, TRIG, 1);
            lguSleep(0.00001); // 10us
            lgGpioWrite(handle, TRIG, 0);
    
            // 6.2. 等待回调函数通知测量完成
            auto start_wait = std::chrono::high_resolution_clock::now();
            while (!trigger_active.load() && run_loop.load()) {//lgSleep 会让程序进入一个完全阻塞的状态。
                std::this_thread::sleep_for(std::chrono::microseconds(100));
                auto now = std::chrono::high_resolution_clock::now();
                if (std::chrono::duration_cast<std::chrono::milliseconds>(now - start_wait).count() > 60) {
                    std::cout << "timeout, no echo received\n";
                    break;
                }
            }
    
            // 6.3. 如果循环被中断，则退出
            if (!run_loop.load()) {
                break;
            }
    
            // 6.4. 计算并打印距离
            uint64_t diff = stop_tick.load() - start_tick.load(); // us
            //std::cout << "time diff: " << diff << " cast double: " << static_cast<double>(diff) << "\n";
            double distance = static_cast<double>(diff) * 0.034326 / 2.0; // cm
            //std::cout << "Distance: " << distance << "cm" << std::endl;
            //排除太近的误触发
            if(distance >= 2){
                results.emplace_back(distance);
            }
            else{
                continue;
            }
    
            // 6.5. 重置标志，准备下一次测量
            start_tick = 0;
            stop_tick = 0;
            trigger_active = false;
    
            // 6.6. 休眠一段时间，避免过快触发,休眠防抖
            lguSleep(0.1);//0.01s
        }
        
        if(results.size() >= 3){
            std::sort(results.begin(), results.end());
            results.erase(results.begin());//去掉最小值
            results.pop_back();//去掉最大值
            double avg = std::accumulate(results.begin(), results.end(), 0.0) / results.size();
            if(avg <= 200){
                std::cout << "距离障碍物: " << avg << "cm\n";
                flagCamera = true;
            }
            else{
                std::cout << "未探测到障碍物\n";
                flagCamera = false;
            }
        }
    }

    // 7. 清理资源 (虽然这里是无限循环，但良好的习惯是加上)
    autoThread.detach(); // 主程序退出时释放线程
    lgGpioFree(handle, TRIG);
    lgGpioFree(handle, ECHO);
    lgGpiochipClose(handle);
    std::cout << "done\n"; 

    return 0;
}
