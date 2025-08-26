#include <sys/mman.h>
#include <unistd.h>
#include <fcntl.h>
#include <iostream>
#include <vector>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <memory>

#include <libcamera/libcamera.h>
#include <libcamera/camera_manager.h>
#include <libcamera/framebuffer_allocator.h>
#include <opencv2/opencv.hpp>

#include "Poco/Net/HTTPServer.h"
#include "Poco/Net/HTTPRequestHandler.h"
#include "Poco/Net/HTTPRequestHandlerFactory.h"
#include "Poco/Net/HTTPServerRequest.h"
#include "Poco/Net/HTTPServerResponse.h"
#include "Poco/Util/ServerApplication.h"
#include "Poco/Thread.h"

std::mutex g_frameMutex;
std::condition_variable g_frameCV;
std::queue<std::vector<uchar>> g_frameQueue;
bool g_cameraReady = false;

class CameraManagerWrapper {
public:
    std::shared_ptr<libcamera::CameraManager> cameraManager;
    std::shared_ptr<libcamera::Camera> camera;
    std::unique_ptr<libcamera::FrameBufferAllocator> allocator;
    libcamera::Stream* stream = nullptr;
    std::vector<libcamera::FrameBuffer*> buffers;
    bool capturing = false;

    bool init() {
        cameraManager = std::make_shared<libcamera::CameraManager>();
        if (cameraManager->start()) {
            std::cerr << "Failed to start camera manager\n";
            return false;
        }

        auto cams = cameraManager->cameras();
        if (cams.empty()) {
            std::cerr << "No cameras found\n";
            cameraManager->stop();
            return false;
        }

        camera = cams[0];
        if (camera->acquire()) {
            std::cerr << "Failed to acquire camera\n";
            cameraManager->stop();
            return false;
        }

        // 使用默认生成的配置，不强制设置 RGB，避免 PiSP 配置失败
        std::unique_ptr<libcamera::CameraConfiguration> config =
            camera->generateConfiguration({libcamera::StreamRole::VideoRecording});
        if (!config) return false;

        config->at(0).size.width = 640;
        config->at(0).size.height = 480;
        //config->at(0).pixelFormat = libcamera::formats::PC1g;

        if (camera->configure(config.get()) < 0) {
            std::cerr << "Failed to configure camera\n";
            camera->release();
            cameraManager->stop();
            return false;
        }

        stream = config->at(0).stream();
        allocator = std::make_unique<libcamera::FrameBufferAllocator>(camera);
        if (allocator->allocate(stream) < 0) {
            std::cerr << "Failed to allocate buffers\n";
            camera->release();
            cameraManager->stop();
            return false;
        }

        auto& tmpBuffers = allocator->buffers(stream);
        buffers.reserve(tmpBuffers.size());
        for (auto &buf : tmpBuffers) buffers.push_back(buf.get());

        camera->requestCompleted.connect(this, &CameraManagerWrapper::onRequestComplete);
        g_cameraReady = true;
        return true;
    }

    void startCapture() {
        if (!capturing) {
            camera->start();
            capturing = true;
            
            for (auto &buf : buffers) {
                auto req = camera->createRequest();
                if (!req) continue;
                req->addBuffer(stream, buf);
                camera->queueRequest(req.get());
            }
        }
    }

    void stopCapture() {
        if (capturing) {
            camera->stop();
            capturing = false;
        }
    }

    void onRequestComplete(libcamera::Request* req) {
        if (req->status() != libcamera::Request::RequestComplete) return;
        if (req->buffers().empty()) return;

        auto buf = req->buffers().begin()->second;
        const auto& plane = buf->planes()[0];

        void* data_ptr = mmap(nullptr, plane.length, PROT_READ, MAP_SHARED, plane.fd.get(), 0);
        if (data_ptr == MAP_FAILED) return;

        // YUV420 数据直接映射成 cv::Mat
        cv::Mat raw(stream->configuration().size.height * 3 / 2,
                   stream->configuration().size.width,
                   CV_8UC1, data_ptr);

        // 如果需要 RGB，做 Bayer2RGB
        cv::Mat rgb;
        cv::cvtColor(raw, rgb, cv::COLOR_YUV2RGB_I420);

        // 编码 JPEG 内存流
        std::vector<uchar> jpegBuf;
        cv::imencode(".jpg", rgb, jpegBuf, {cv::IMWRITE_JPEG_QUALITY, 80});

        {
            std::lock_guard<std::mutex> lock(g_frameMutex);
            g_frameQueue.push(jpegBuf);
            if (g_frameQueue.size() > 10) g_frameQueue.pop();
        }
        g_frameCV.notify_all();

        munmap(data_ptr, plane.length);
        req->reuse(libcamera::Request::ReuseBuffers);
        camera->queueRequest(req);
    }

    ~CameraManagerWrapper() {
        stopCapture();
        if (camera) camera->release();
        if (cameraManager) cameraManager->stop();
    }
};

// HTTP Handlers
class StreamHandler : public Poco::Net::HTTPRequestHandler {
public:
    void handleRequest(Poco::Net::HTTPServerRequest& request, Poco::Net::HTTPServerResponse& response) override {
        response.setChunkedTransferEncoding(true);
        response.setContentType("multipart/x-mixed-replace; boundary=frame");
        std::ostream& ostr = response.send();

        while (true) {
            std::vector<uchar> frameData;
            {
                std::unique_lock<std::mutex> lock(g_frameMutex);
                g_frameCV.wait(lock, []{ return !g_frameQueue.empty(); });
                frameData = g_frameQueue.front();
            }

            ostr << "--frame\r\n";
            ostr << "Content-Type: image/jpeg\r\n";
            ostr << "Content-Length: " << frameData.size() << "\r\n\r\n";
            ostr.write(reinterpret_cast<const char*>(frameData.data()), frameData.size());
            ostr << "\r\n";
            ostr.flush();
            Poco::Thread::sleep(30);
        }
    }
};

class CaptureHandler : public Poco::Net::HTTPRequestHandler {
public:
    void handleRequest(Poco::Net::HTTPServerRequest& request, Poco::Net::HTTPServerResponse& response) override {
        response.setContentType("image/jpeg");
        std::vector<uchar> frameData;
        {
            std::unique_lock<std::mutex> lock(g_frameMutex);
            g_frameCV.wait_for(lock, std::chrono::seconds(2), []{ return !g_frameQueue.empty(); });
            if (!g_frameQueue.empty()) frameData = g_frameQueue.front();
        }
        std::ostream& ostr = response.send();
        ostr.write(reinterpret_cast<const char*>(frameData.data()), frameData.size());
    }
};

class IndexHandler : public Poco::Net::HTTPRequestHandler {
public:
    void handleRequest(Poco::Net::HTTPServerRequest& request, Poco::Net::HTTPServerResponse& response) override {
        response.setContentType("text/html");
        std::ostream& ostr = response.send();
        ostr << "<html><body>"
             << "<h1>Raspberry Pi Camera</h1>"
             << "<img src='/stream' width='640' height='480'><br>"
             << "<a href='/capture' target='_blank'>Take Photo</a>"
             << "</body></html>";
    }
};

class MyRequestHandlerFactory : public Poco::Net::HTTPRequestHandlerFactory {
public:
    Poco::Net::HTTPRequestHandler* createRequestHandler(const Poco::Net::HTTPServerRequest& request) override {
        if (request.getURI() == "/") return new IndexHandler;
        if (request.getURI() == "/stream") return new StreamHandler;
        if (request.getURI() == "/capture") return new CaptureHandler;
        return nullptr;
    }
};

class CameraServer : public Poco::Util::ServerApplication {
protected:
    int main(const std::vector<std::string>&) override {
        auto camManager = std::make_shared<CameraManagerWrapper>();
        if (!camManager->init()) return 1;
        camManager->startCapture();

        Poco::Net::HTTPServer server(
            new MyRequestHandlerFactory(),
            Poco::Net::ServerSocket(8080),
            new Poco::Net::HTTPServerParams()
        );
        server.start();
        std::cout << "Server started at http://localhost:8080\n";

        waitForTerminationRequest(); // Ctrl+C
        camManager->stopCapture();
        server.stop();
        return 0;
    }
};

POCO_SERVER_MAIN(CameraServer)
