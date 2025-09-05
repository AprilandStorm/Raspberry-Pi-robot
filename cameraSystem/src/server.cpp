#include <iostream>
#include <thread>
#include <atomic>
#include <memory>
#include <mutex>
#include <queue>
#include <condition_variable>
#include <cstdio>
#include <vector>
#include <cstring>
#include <filesystem>

#include <Poco/Net/TCPServer.h>
#include <Poco/Net/TCPServerConnection.h>
#include <Poco/Net/TCPServerConnectionFactory.h>
#include <Poco/Net/TCPServerParams.h>
#include <Poco/Net/StreamSocket.h>
#include <Poco/Net/SocketAddress.h>
#include <Poco/Util/ServerApplication.h>

#include <opencv2/opencv.hpp>
#include "../include/protocol.hpp"

// ========== 工具：安全日志辅助 ==========
#define LOG_I(msg) std::cout << "[INFO] " << msg << std::endl
#define LOG_W(msg) std::cerr << "[WARN] " << msg << std::endl
#define LOG_E(msg) std::cerr << "[ERR ] " << msg << std::endl

// ========== 全局配置：rpicam-vid 命令行 ==========
// -n: no preview; --codec mjpeg: 输出 MJPEG；--output -: 输出到 stdout；
// 可调整分辨率/帧率/质量等参数。
static const char* RPICAM_CMD =
    "rpicam-vid -n --codec mjpeg --width 640 --height 480 --framerate 20 --quality 80 --output -";

// ========== 从 stdout 解析 MJPEG 的帧抓取器 ==========
class RpiCamMjpegReader {
public:
    RpiCamMjpegReader(std::queue<cv::Mat>& q, std::mutex& m, std::condition_variable& cv)
        : q_(q), m_(m), cv_(cv), running_(false) {}

    ~RpiCamMjpegReader() { stop(); }

    void start() {
        if (running_) return;
        running_ = true;
        worker_ = std::thread(&RpiCamMjpegReader::run, this);
    }

    void stop() {
        running_ = false;
        if (proc_) {
            LOG_I("Stopping rpicam-vid process...");
            pclose(proc_);
            proc_ = nullptr;
        }
        if (worker_.joinable()) worker_.join();
    }

private:
    void run() {
        while (running_) {
            LOG_I("Spawning rpicam-vid: " << RPICAM_CMD);
            proc_ = popen(RPICAM_CMD, "r");
            if (!proc_) {
                LOG_E("popen failed! Will retry in 2s.");
                std::this_thread::sleep_for(std::chrono::seconds(2));
                continue;
            }

            // 读取 stdout，并以 JPEG SOI/EOI 边界切帧
            std::vector<uint8_t> buffer;
            buffer.reserve(1 << 20); // 预留 1MB
            const size_t CHUNK = 4096;
            std::vector<uint8_t> chunk(CHUNK);

            bool ok = true;
            while (running_) {
                size_t n = fread(chunk.data(), 1, CHUNK, proc_);
                if (n == 0) {
                    if (feof(proc_)) {
                        LOG_W("rpicam-vid EOF.");
                        ok = false;
                        break;
                    }
                    if (ferror(proc_)) {
                        LOG_E("fread error from rpicam-vid.");
                        ok = false;
                        break;
                    }
                    continue;
                }
                buffer.insert(buffer.end(), chunk.begin(), chunk.begin() + n);

                // 解析 JPEG 帧：FFD8 ... FFD9
                // 简单起见：一次最多解析多个完整帧
                size_t searchStart = 0;
                while (true) {
                    // 找 SOI
                    auto itSOI = std::search(buffer.begin() + searchStart, buffer.end(),
                                             kSOI, kSOI + 2);
                    if (itSOI == buffer.end()) {
                        // 没有 SOI，丢弃前面的无用数据
                        if (buffer.size() > 2 * CHUNK) {
                            // 保留末尾少量数据避免截断 SOI
                            buffer.erase(buffer.begin(), buffer.end() - 2 * CHUNK);
                        }
                        break;
                    }
                    // 找 EOI（从 SOI 后面找）
                    auto itEOI = std::search(itSOI + 2, buffer.end(),
                                             kEOI, kEOI + 2);
                    if (itEOI == buffer.end()) {
                        // 还没读到完整帧
                        // 下次从 SOI 前一个位置继续搜索，避免重复扫描太多
                        searchStart = std::distance(buffer.begin(), itSOI);
                        break;
                    }

                    // 取出 [SOI, EOI] 的完整 JPEG
                    itEOI += 2; // 包含 EOI
                    std::vector<uint8_t> jpeg(itSOI, itEOI);

                    // 从缓冲区移除已消费部分
                    buffer.erase(buffer.begin(), itEOI);

                    // 解码为 BGR Mat
                    cv::Mat mat = cv::imdecode(jpeg, cv::IMREAD_COLOR);
                    if (!mat.empty()) {
                        // 入队（限制队列长度）
                        {
                            std::lock_guard<std::mutex> lk(m_);
                            if (!q_.empty()) q_.pop();// 丢弃旧帧
                            q_.push(std::move(mat));// 入队最新帧
                        }
                        cv_.notify_one();
                        frames_++;
                        if (frames_ % 30 == 0) {
                            LOG_I("[RpiCamMjpegReader] received frames=" << frames_);
                        }
                    } else {
                        LOG_W("imdecode failed for one JPEG.");
                    }

                    // 本轮继续从缓冲区起始处找下一帧
                    searchStart = 0;
                }
            }

            // 退出或崩溃，清理并重启
            int rc = pclose(proc_);
            proc_ = nullptr;
            LOG_W("rpicam-vid exited with code " << rc << ". Restarting in 1s...");
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }

    static constexpr uint8_t kSOI[2] = {0xFF, 0xD8};
    static constexpr uint8_t kEOI[2] = {0xFF, 0xD9};

    std::queue<cv::Mat>& q_;
    std::mutex& m_;
    std::condition_variable& cv_;
    std::atomic<bool> running_;
    std::thread worker_;
    FILE* proc_ = nullptr;
    size_t frames_ = 0;
};

// ========== 网络连接：保持你的协议 & 逻辑 ==========
class CameraConnection : public Poco::Net::TCPServerConnection {
public:
    CameraConnection(const Poco::Net::StreamSocket& socket,
                     std::queue<cv::Mat>& frame_queue,
                     std::mutex& frame_mutex,
                     std::condition_variable& frame_cv)
        : Poco::Net::TCPServerConnection(socket),
          frame_queue_(frame_queue),
          frame_mutex_(frame_mutex),
          frame_cv_(frame_cv),
          running_(false),
          next_message_id_(0) {}

    void run() override {
        Poco::Net::StreamSocket& socket = this->socket();
        LOG_I("=== CameraConnection started for " << socket.peerAddress().toString() << " ===");

        running_ = true;
        int frame_count = 0;

        auto sendAll = [&](const void* data, size_t len) {
            const char* p = static_cast<const char*>(data);
            size_t sent = 0;
            while (sent < len) {
                int n = socket.sendBytes(p + sent, int(len - sent));
                if (n <= 0) throw std::runtime_error("send failed");
                sent += size_t(n);
            }
        };

        try {
            while (running_) {
                // 客户端命令
                if (socket.available() > 0) {
                    handleClientCommand(socket, sendAll);
                }

                // 取帧
                cv::Mat frame;
                {
                    std::unique_lock<std::mutex> lock(frame_mutex_);
                    if (!frame_cv_.wait_for(lock, std::chrono::milliseconds(200),
                        [&]{ return !frame_queue_.empty(); })) {
                        // 没帧，继续等
                        continue;
                    }

                    frame = frame_queue_.front();
                    frame_queue_.pop();
                }

                if (frame.empty()) {
                    LOG_W("[run] got empty frame.");
                    continue;
                }
                
                // 翻转画面（水平+垂直翻转）
                cv::flip(frame, frame, -1);

                // 编码 JPEG（你客户端就是收 JPEG）
                std::vector<uint8_t> frame_data;
                bool ok = cv::imencode(".jpg", frame, frame_data, {cv::IMWRITE_JPEG_QUALITY, 80});
                if (!ok || frame_data.empty()) {
                    LOG_W("[run] JPEG encode failed.");
                    continue;
                }

                Protocol::MessageHeader header;
                header.message_id = next_message_id_++;
                header.type = Protocol::MessageType::VIDEO_FRAME;
                header.payload_length = static_cast<uint32_t>(frame_data.size());
                header.timestamp = static_cast<uint32_t>(std::time(nullptr));
                header.toNetworkOrder();

                sendAll(&header, sizeof(header));
                sendAll(frame_data.data(), frame_data.size());

                frame_count++;
                if (frame_count % 30 == 0) {
                    LOG_I("[run] sent frames=" << frame_count);
                }
            }
        } catch (const std::exception& e) {
            LOG_E("[CameraConnection] exception: " << e.what());
        }
        LOG_I("=== CameraConnection closed for " << socket.peerAddress().toString() << " ===");
    }

    void stop() { running_ = false; }

private:
    template <typename SendAllFn>
    void handleClientCommand(Poco::Net::StreamSocket& socket, SendAllFn sendAll) {
        char buffer[1024];
        int bytes_received = socket.receiveBytes(buffer, sizeof(buffer));
        if (bytes_received < (int)sizeof(Protocol::MessageHeader)) return;

        Protocol::MessageHeader header;
        std::memcpy(&header, buffer, sizeof(header));
        header.toHostOrder();

        if (header.type == Protocol::MessageType::CAPTURE_COMMAND) {
            LOG_I("Received capture command");
            // 取最近一帧并返回
            cv::Mat photo;
            {
                std::unique_lock<std::mutex> lock(frame_mutex_);
                if (!frame_queue_.empty()) {
                    photo = frame_queue_.back(); // 最近一帧
                    cv::flip(photo, photo, -1);  // 翻转抓拍图
                }
            }
            if (photo.empty()) {
                LOG_W("No frame to capture.");
                return;
            }

            std::vector<uint8_t> photo_data;
            cv::imencode(".jpg", photo, photo_data, {cv::IMWRITE_JPEG_QUALITY, 95});

            Protocol::MessageHeader respHeader;
            respHeader.message_id = next_message_id_++;
            respHeader.type = Protocol::MessageType::CAPTURE_RESPONSE;
            respHeader.payload_length = static_cast<uint32_t>(sizeof(Protocol::CaptureResponse) + photo_data.size());
            respHeader.timestamp = static_cast<uint32_t>(std::time(nullptr));
            respHeader.toNetworkOrder();
            sendAll(&respHeader, sizeof(respHeader));

            Protocol::CaptureResponse response;
            response.capture_id = header.message_id;
            response.image_size = static_cast<uint32_t>(photo_data.size());
            std::snprintf(response.filename, sizeof(response.filename), "capture_%u.jpg", header.message_id);
            response.toNetworkOrder();
            sendAll(&response, sizeof(response));

            if (!photo_data.empty()) {
                sendAll(photo_data.data(), photo_data.size());
            }
            LOG_I("Photo captured and sent: " << response.filename);
        }
    }

    std::queue<cv::Mat>& frame_queue_;
    std::mutex& frame_mutex_;
    std::condition_variable& frame_cv_;
    std::atomic<bool> running_;
    std::atomic<uint32_t> next_message_id_;
};

// ========== 连接工厂 ==========
class CameraConnectionFactory : public Poco::Net::TCPServerConnectionFactory {
public:
    CameraConnectionFactory(std::queue<cv::Mat>& frame_queue,
                            std::mutex& frame_mutex,
                            std::condition_variable& frame_cv)
        : frame_queue_(frame_queue),
          frame_mutex_(frame_mutex),
          frame_cv_(frame_cv) {}

    Poco::Net::TCPServerConnection* createConnection(const Poco::Net::StreamSocket& socket) override {
        return new CameraConnection(socket, frame_queue_, frame_mutex_, frame_cv_);
    }

private:
    std::queue<cv::Mat>& frame_queue_;
    std::mutex& frame_mutex_;
    std::condition_variable& frame_cv_;
};

// ========== 主服务 ==========
class CameraServer : public Poco::Util::ServerApplication {
public:
    CameraServer() = default;

protected:
    int main(const std::vector<std::string>& args) override {
        unsigned short port = 8888;
        if (!args.empty()) port = static_cast<unsigned short>(std::stoi(args[0]));
        LOG_I("=== 启动相机服务器（rpicam-vid MJPEG）=== 端口: " << port);

        // 启动 rpicam 采集线程
        reader_ = std::make_unique<RpiCamMjpegReader>(frame_queue_, frame_mutex_, frame_cv_);
        reader_->start();

        // TCP 服务器
        Poco::Net::ServerSocket server_socket(port);
        CameraConnectionFactory factory(frame_queue_, frame_mutex_, frame_cv_);
        Poco::Net::TCPServer server(&factory, server_socket);
        server.start();
        LOG_I("TCP server started on port " << port);

        waitForTerminationRequest();

        server.stop();
        reader_->stop();
        LOG_I("Server stopped.");
        return 0;
    }

private:
    std::unique_ptr<RpiCamMjpegReader> reader_;
    std::queue<cv::Mat> frame_queue_;
    std::mutex frame_mutex_;
    std::condition_variable frame_cv_;
};

// ========== 入口 ==========
int main(int argc, char** argv) {
    CameraServer app;
    return app.run(argc, argv);
}
