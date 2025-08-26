#include <iostream>
#include <opencv2/opencv.hpp>
#include <opencv2/videoio.hpp>

int main() {
    // 使用 cv::CAP_LIBCAMERA 作为后端
    // 这是访问新版树莓派摄像头（CSI接口）的推荐方法
    cv::VideoCapture cap(0);

    // 检查摄像头是否成功打开
    if (!cap.isOpened()) {
        std::cerr << "错误: 无法打开摄像头. 确保摄像头已正确连接且libcamera已安装.\n";
        return -1;
    }

    // 设置分辨率 (可选)
    // 根据你的摄像头支持的分辨率进行调整，例如 640x480 或 1280x720
    cap.set(cv::CAP_PROP_FRAME_WIDTH, 640);
    cap.set(cv::CAP_PROP_FRAME_HEIGHT, 480);

    // 创建一个窗口来显示视频流
    cv::namedWindow("Live Camera Feed", cv::WINDOW_AUTOSIZE);

    cv::Mat frame;
    std::cout << "按 'q' 退出视频流...\n";

    while (true) {
        // 从摄像头捕获一帧
        cap >> frame;

        // 检查帧是否为空
        if (frame.empty()) {
            std::cerr << "错误: 捕获到空帧. 视频流可能已中断.\n";
            break;
        }

        // 在窗口中显示帧
        cv::imshow("Live Camera Feed", frame);

        // 按 'q' 键退出循环
        if (cv::waitKey(1) == 'q') {
            break;
        }
    }

    // 释放摄像头
    cap.release();
    cv::destroyAllWindows();

    return 0;
}
