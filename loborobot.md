# 基于 PCA9685 PWM 控制器 和 树莓派 GPIO 开发的 LOBOROBOT 机器人驱动类，核心功能是控制机器人的 4 个电机实现全向运动（前进、后退、左转、右转、斜移等），同时支持舵机控制
- try...catch 的作用
```
try {
    // 可能出错的代码
} catch (const std::exception& e) {
    // 捕获并处理异常
}
```
try 块：包裹可能抛出异常的代码\
catch 块：捕获并处理异常，防止程序崩溃\
目的：优雅地处理运行时错误，而不是直接崩溃

- 创建一个线程
```
std::thread autoThread([&]() {
    while (true) {
        if (autoRotation) {
            // ... 自动旋转逻辑 ...
        } else {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
    }
});
```
以上作为 std::thread 的参数传入，用于创建一个后台线程。
-  拆解这个 Lambda：
```
[&]() { ... }
```
[&]	捕获列表：按引用方式捕获外部变量（如 autoRotation, baseAngle, angle, t, robot 等）\
()	参数列表（这里为空，表示不接收参数）\
{ ... }	函数体（线程执行的代码）

- 你可以在任何需要并发执行任务的地方使用 std::thread + Lambda 的组合，例如：

| 场景 | 示例 |
|----|----|
|后台定时任务|传感器轮询、舵机自动扫描|
|实时输入监听|键盘、遥控器、串口指令接收|
|网络通信|接收 TCP/UDP 数据包|
|日志写入|异步写日志到文件|
|视频采集|摄像头帧捕获|
|多机器人协同|每个机器人控制一个线程|

- 标准模板：如何在任意地方创建新线程
```
std::thread myThread([&]() {
    // 你的任务逻辑
    while (running) {
        // 做一些事
        doSomething();
        // 控制频率     std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
});
```
然后记得管理线程生命周期：
```
myThread.join();   // 等待线程结束（推荐）
// 或
myThread.detach(); // 分离线程（慎用）
```

- std::thread 类的主要成员函数

|函数|说明|
|----|----|
|thread()|默认构造，创建一个不表示任何线程的对象|
|thread(Function&& f, Args&&... args)|构造并启动新线程，执行 f(args...)|
|~thread()|析构函数：如果线程是 joinable 的，会调用 std::terminate()（程序崩溃！）|
|join()|等待线程结束|
|detach()|分离线程，使其独立运行|
|joinable()|返回 true 如果线程正在运行且未被 join 或 detach|
|get_id()|获取线程 ID|
|native_handle()|获取底层操作系统线程句柄（平台相关）|
|swap(thread& other)|交换两个 thread 对象的状态|
|hardware_concurrency()|静态函数：返回系统支持的并发线程数（如 4 核返回 4）|

- std::this_thread 命名空间（常用）, 提供对当前线程的操作。

|函数|说明|
|----|----|
|get_id()|获取当前线程的 ID|
|yield()|提示系统调度其他线程（用于忙等待优化）|
|sleep_for(const chrono::duration& rel_time)|当前线程休眠指定时间|
|sleep_until(const chrono::time_point& abs_time)|休眠到指定时间点|

- 使用 join() 的注意事项

|规则|说明|
|----|----|
|不能对同一个线程 join() 两次|否则会抛出 std::system_error|
|不能对 std::thread() 默认构造的线程 join()|它不表示任何线程|
|不能对已 detach() 的线程 join()|它已分离，无法再连接|
|join() 后，线程对象变为 不可连接（non-joinable）|可以安全析构|

- 为什么 join() 比 detach() 更安全？

|对比项|join()|detach()|
|---|---|---|
|控制权|主线程等待子线程结束|子线程独立运行|
|生命周期|可预测，主线程控制|不可预测，可能在 main() 后继续运行|
|资源清理|安全，线程栈自动回收|风险高，可能访问已销毁的变量|
|推荐场景|大多数情况|仅用于“火与忘”（fire-and-forget）任务|

## getch() 函数
- 为什么需要这个函数？\
默认终端行为（规范模式）,在 Linux 终端中，默认情况下：
   1. 输入字符会显示在屏幕上（回显）
   2. 输入的内容会缓存，直到你按下 回车（Enter）
   3. 程序才能读取整行输入
这叫 规范模式（Canonical Mode），适合输入命令、文本等。\
但我们想要的是：
   1. 按一个键（比如 w）立即响应，不用按回车
   2. 输入时不显示字符（无回显）
   3. 实现类似“游戏控制”或“机器人遥控”的体验
所以需要关闭规范模式，进入非规范模式（非阻塞、立即读取）

- 关键知识点
|概念|说明|
|---|---|
|struct termios|描述终端（TTY）设备属性的结构体|
|tcgetattr()|获取当前终端设置|
|tcsetattr()|设置终端属性|
|STDIN_FILENO|标准输入的文件描述符（通常是 0）|
|ICANON|规范模式标志，关闭它才能“按一个键就读一个键”|
|ECHO|回显标志，关闭它才能“不显示输入字符”|
