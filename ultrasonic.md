# 基于树莓派（Raspberry Pi）使用 lgpio 库实现的超声波测距系统

## 整体功能概述
- 功能目标：
  1. 使用 HC-SR04 超声波传感器 测量距离
  2. 利用 lgpio 库实现 高精度时间戳捕获
  3. 多次测量取平均值，并剔除异常值（去噪）
  4. 当检测到障碍物较近时，通过标志位通知另一个线程启动摄像头
  5. 支持 Ctrl+C 安全退出
  6. 线程安全、资源释放完整

- 硬件接线说明（HC-SR04）

|传感器引脚|树莓派GPIO 引脚|
|---|---|
|VCC|5V|
|GND|GND|
|TRIG|GPIO 20|
|ECHO|GPIO 21|

- ECHO 引脚中断回调函数
```
void echo_edge_event_handler(int e, lgGpioAlert_s* evt, void *data)
```
   1. 参数说明：\
      e：事件数量\
      evt：事件数组（包含时间戳、电平变化等）\
      data：用户数据（未使用）\
   2.  关键优势：\
      使用 lgpio 的 硬件级边沿检测 + 精确时间戳（纳秒级）\
      不依赖轮询，响应快、精度高\
      时间戳来自内核，不受用户程序延迟影响

- 使用 lgGpioClaimAlert() + 回调：事件驱动
   1. 你不用主动去“看”引脚状态
   2. 内核在硬件层面捕获每一个边沿变化，并附带纳秒级时间戳
   3. 你的程序只需“事后处理”这些事件即可

- 传统方式（不推荐）：轮询检测
```
while (digitalRead(ECHO) == LOW); // 等待上升沿 —— 浪费 CPU！
start = micros();
while (digitalRead(ECHO) == HIGH); // 等待下降沿 —— 不精确！
stop = micros();
```
缺点：占用 CPU（空转等待）; 时间精度差（micros() 分辨率有限）; 可能错过边沿

|函数|作用|
|----|----|
|lgGpioClaimAlert()|“我要监听这个引脚的边沿变化” → 注册监听|
|lgGpioSetAlertsFunc()|“当有变化时，调用这个函数处理” → 设置回调|

### 信号处理函数(Signal Handler)
```
void signal_handler(int signum) {
    if (signum == SIGINT) {
        std::cout << "\nSIGINT received, shutting down gracefully..." << std::endl;
        run_loop = false;
    }
}
```
- 这个函数的作用是：当用户按下 Ctrl+C 时，程序不会立刻崩溃退出，而是“优雅地”停止主循环，释放资源，然后退出。这叫 “优雅关闭”（Graceful Shutdown）。
- 什么是信号（Signal）？\
在 Linux/Unix 系统中，信号（Signal） 是一种进程间通信机制，用于通知程序发生了某些特殊事件。

|常见信号|触发方式|含义|
|----|----|----|
|SIGINT|Ctrl+C|中断程序（Interrupt）|
|SIGTERM|kill <pid>|请求终止（Terminate）|
|SIGKILL|kill -9 <pid>|强制杀死（不能被捕获）|

SIGKILL 不能被程序捕获或忽略 —— 是“终极杀招”。
- 它是在哪里被调用的？\
这个函数本身不是你主动调用的，而是由操作系统在收到信号时自动调用。\
你需要先注册它：
```
std::signal(SIGINT, signal_handler);
```
- 为什么需要它？（对比没有的情况）\
  1. 没有信号处理：\
     直接终止\
     GPIO 可能还处于高电平\
     其他程序再使用 GPIO 时会出错\
     文件未保存、资源未释放
  2. 有信号处理：
     输出提示\
     主循环自然退出\
     执行 lgGpioFree() 等清理操作\
     系统资源恢复干净
