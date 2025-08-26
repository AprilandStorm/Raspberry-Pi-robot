#include <iostream>
#include <fstream>
#include <memory>
#include <libcamera/libcamera.h>
#include <libcamera/camera.h>
#include <libcamera/camera_manager.h>
#include <libcamera/stream_configuration.h>
#include <libcamera/framebuffer.h>
#include <libcamera/request.h>
#include <libcamera/controls.h>

// 主函数，负责运行整个程序
int main() {
    // 1. 创建相机管理器
    std::unique_ptr<libcamera::CameraManager> cm = std::make_unique<libcamera::CameraManager>();
    int ret = cm->start();
    if (ret) {
        std::cerr << "Failed to start camera manager: " << ret << std::endl;
        return -1;
    }

    // 2. 找到第一个可用的相机
    if (cm->cameras().empty()) {
        std::cerr << "No cameras found!" << std::endl;
        return -1;
    }

    std::shared_ptr<libcamera::Camera> camera = cm->cameras()[0];
    ret = camera->acquire();
    if (ret) {
        std::cerr << "Failed to acquire camera: " << ret << std::endl;
        return -1;
    }

    // 3. 配置图像流
    libcamera::StreamConfiguration config = camera->generateConfiguration({ libcamera::StreamRole::VideoRecording });
    // 可以根据需要修改分辨率
    config.size.width = 640;
    config.size.height = 480;
    config.pixelFormat = libcamera::formats::YUV420;

    ret = camera->configure(config);
    if (ret) {
        std::cerr << "Failed to configure stream: " << ret << std::endl;
        return -1;
    }

    // 4. 创建帧缓冲区（FrameBuffer）
    // 帧缓冲区是存储捕获图像数据的内存区域
    std::vector<std::unique_ptr<libcamera::FrameBuffer>> buffers;
    for (const auto& buf : config.stream()->buffers) {
        buffers.push_back(std::unique_ptr<libcamera::FrameBuffer>(buf.get()));
    }
    ret = camera->createFrameBuffers(buffers);
    if (ret) {
        std::cerr << "Failed to create frame buffers: " << ret << std::endl;
        return -1;
    }

    // 5. 创建请求（Request）并将其排队
    // Request 是 libcamera 的核心概念，每个请求代表一个捕获任务
    for (const auto& buffer : buffers) {
        std::unique_ptr<libcamera::Request> request = camera->createRequest();
        if (!request) {
            std::cerr << "Failed to create request" << std::endl;
            return -1;
        }
        
        // 将帧缓冲区附加到请求上
        request->addBuffer(config.stream(), buffer.get());
        
        // 将请求排队
        camera->queueRequest(std::move(request));
    }

    // 6. 启动相机
    ret = camera->start();
    if (ret) {
        std::cerr << "Failed to start camera: " << ret << std::endl;
        return -1;
    }

    // 7. 处理捕获的帧
    // 这里我们只捕获第一帧
    std::unique_ptr<libcamera::Request> capturedRequest = camera->wait();
    if (!capturedRequest) {
        std::cerr << "Failed to capture frame" << std::endl;
        camera->stop();
        return -1;
    }

    // 8. 从请求中获取帧数据并保存
    libcamera::FrameBuffer* buffer = capturedRequest->buffers().at(config.stream());
    void* data = buffer->planes()[0].data();
    size_t size = buffer->planes()[0].length();

    std::string filename = "captured_image.yuv";
    std::ofstream ofs(filename, std::ios::binary);
    if (ofs.is_open()) {
        ofs.write(reinterpret_cast<const char*>(data), size);
        std::cout << "Captured image saved to: " << filename << std::endl;
    } else {
        std::cerr << "Failed to open file: " << filename << std::endl;
    }

    // 9. 停止相机并释放资源
    camera->stop();
    camera->release();
    cm->stop();

    return 0;
}