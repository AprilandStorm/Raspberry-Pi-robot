#include "Poco/Net/HTTPServer.h"
#include "Poco/Net/HTTPRequestHandler.h"
#include "Poco/Net/HTTPRequestHandlerFactory.h"
#include "Poco/Net/HTTPServerRequest.h"
#include "Poco/Net/HTTPServerResponse.h"
#include "Poco/Util/ServerApplication.h"
#include "Poco/StreamCopier.h"
#include "Poco/Thread.h"
#include <opencv2/opencv.hpp>
#include <iostream>
#include <sstream>
#include <vector>
#include <mutex>
#include <memory>
#include <queue>
#include <condition_variable>

// Include the necessary libcamera headers
#include <libcamera/libcamera.h>
#include <libcamera/camera.h>
#include <libcamera/camera_manager.h>
#include <libcamera/stream.h>
#include <libcamera/framebuffer.h>
#include <libcamera/framebuffer_allocator.h>



// --- 全局共享数据 ---
// Mutex to protect the shared frame buffer and condition variable
std::mutex g_frameMutex;
// Condition variable to signal when a new frame is ready
std::condition_variable g_frameCV;
// Shared queue to hold the latest captured frame data
std::queue<std::vector<uchar>> g_frameQueue;
// Flag to indicate if the camera is ready
bool g_cameraReady = false;

// --- Libcamera 摄像头管理类 ---
class LibcameraManager {
private:
    std::unique_ptr<CameraManager> m_cameraManager;
    std::shared_ptr<Camera> m_camera;
    std::unique_ptr<Request> m_request;
    std::unique_ptr<FrameBufferAllocator> m_allocator;
    std::vector<std::unique_ptr<FrameBuffer>> m_buffers;
    Stream* m_stream = nullptr;
    bool m_capturing = false;

public:
    LibcameraManager() {
        m_cameraManager = std::make_unique<CameraManager>();
    }

    // Initialize the camera and prepare for capture
    bool init() {
        if (m_cameraManager->start()) {
            std::cerr << "Error: Camera manager failed to start." << std::endl;
            return false;
        }

        auto cameras = m_cameraManager->cameras();
        if (cameras.empty()) {
            std::cerr << "Error: No cameras found." << std::endl;
            m_cameraManager->stop();
            return false;
        }
        m_camera = cameras[0];

        if (m_camera->acquire()) {
            std::cerr << "Error: Failed to acquire camera." << std::endl;
            m_cameraManager->stop();
            return false;
        }

        // Configure the camera stream
        auto config = m_camera->createStillCaptureConfiguration();
        auto& streamConfig = config->at(0);
        streamConfig.size.width = 640;
        streamConfig.size.height = 480;
        streamConfig.pixelFormat = libcamera::formats::RGB888;

        if (m_camera->configure(config.get()) < 0) {
            std::cerr << "Error: Failed to configure camera stream." << std::endl;
            m_camera->release();
            m_cameraManager->stop();
            return false;
        }

        m_stream = streamConfig.stream();
        m_allocator = std::make_unique<FrameBufferAllocator>(m_camera);
        if (m_allocator->allocate(m_stream) < 0) {
            std::cerr << "Error: Failed to allocate buffers." << std::endl;
            m_camera->release();
            m_cameraManager->stop();
            return false;
        }

        // Populate the buffers vector and queue the first request
        m_buffers = m_allocator->buffers(m_stream);
        for (const auto& buffer : m_buffers) {
            auto req = m_camera->createRequest();
            if (!req) {
                std::cerr << "Error: Failed to create request." << std::endl;
                return false;
            }
            req->addBuffer(m_stream, buffer.get());
            m_camera->queueRequest(req.get());
        }

        // Set up the callback to process completed requests
        m_camera->requestCompleted.connect(this, &LibcameraManager::requestComplete);

        g_cameraReady = true;
        return true;
    }

    void startCapture() {
        if (!m_capturing) {
            m_camera->start();
            m_capturing = true;
        }
    }

    void stopCapture() {
        if (m_capturing) {
            m_camera->stop();
            m_capturing = false;
        }
    }

    // The callback function that is triggered when a request (frame) is completed
    void requestComplete(Request* request) {
        if (request->status() == Request::RequestCancelled) {
            return;
        }

        auto const& buffers = request->buffers();
        if (buffers.empty()) {
            return;
        }

        const FrameBuffer* buffer = buffers.begin()->second;
        const FrameBuffer::Plane& plane = buffer->planes()[0];
        const void* data = plane.data();
        size_t dataSize = plane.length();

        // Convert libcamera frame to OpenCV Mat
        cv::Mat mat(m_stream->configuration().size.height, m_stream->configuration().size.width, CV_8UC3, (void*)data, plane.bytesUsed());
        
        // Push the frame into the shared queue and notify waiting threads
        std::vector<uchar> encodedBuf;
        cv::imencode(".jpg", mat, encodedBuf, {cv::IMWRITE_JPEG_QUALITY, 80});

        {
            std::lock_guard<std::mutex> lock(g_frameMutex);
            g_frameQueue.push(encodedBuf);
            if (g_frameQueue.size() > 10) { // Limit queue size to avoid memory issues
                g_frameQueue.pop();
            }
        }
        g_frameCV.notify_all();

        // Re-queue the request to keep the stream going
        request->reuse(Request::ReuseBuffers);
        m_camera->queueRequest(request);
    }

    ~LibcameraManager() {
        stopCapture();
        if (m_camera) {
            m_camera->release();
        }
        if (m_cameraManager) {
            m_cameraManager->stop();
        }
    }
};

// Global instance of the camera manager
std::shared_ptr<LibcameraManager> g_cameraManager;

// --- 视频流处理器（MJPEG） ---
class StreamHandler : public Poco::Net::HTTPRequestHandler {
public:
    void handleRequest(Poco::Net::HTTPServerRequest &request, Poco::Net::HTTPServerResponse &response) override {
        response.setChunkedTransferEncoding(true);
        response.setContentType("multipart/x-mixed-replace; boundary=frame");

        std::ostream &ostr = response.send();

        if (!g_cameraReady) {
            ostr << "Camera not ready";
            return;
        }

        while (true) {
            std::vector<uchar> frameData;
            {
                std::unique_lock<std::mutex> lock(g_frameMutex);
                // Wait for a new frame to become available
                g_frameCV.wait(lock, [&]{ return !g_frameQueue.empty(); });
                if (!g_frameQueue.empty()) {
                    frameData = g_frameQueue.front();
                    g_frameQueue.pop();
                }
            }

            if (frameData.empty()) {
                Poco::Thread::sleep(100);
                continue;
            }

            ostr << "--frame\r\n";
            ostr << "Content-Type: image/jpeg\r\n";
            ostr << "Content-Length: " << frameData.size() << "\r\n\r\n";
            ostr.write(reinterpret_cast<const char*>(frameData.data()), frameData.size());
            ostr << "\r\n";
            ostr.flush();
        }
    }
};

// --- 拍照处理器 ---
class CaptureHandler : public Poco::Net::HTTPRequestHandler {
public:
    void handleRequest(Poco::Net::HTTPServerRequest &request, Poco::Net::HTTPServerResponse &response) override {
        response.setContentType("image/jpeg");

        if (!g_cameraReady) {
            response.setStatus(Poco::Net::HTTPResponse::HTTP_INTERNAL_SERVER_ERROR);
            std::ostream &ostr = response.send();
            ostr << "Camera not ready";
            return;
        }

        std::vector<uchar> frameData;
        {
            std::unique_lock<std::mutex> lock(g_frameMutex);
            g_frameCV.wait_for(lock, std::chrono::seconds(2), [&]{ return !g_frameQueue.empty(); });
            if (!g_frameQueue.empty()) {
                frameData = g_frameQueue.front();
            }
        }

        if (frameData.empty()) {
            response.setStatus(Poco::Net::HTTPResponse::HTTP_INTERNAL_SERVER_ERROR);
            std::ostream &ostr = response.send();
            ostr << "No frame available in time";
            return;
        }

        std::ostream &ostr = response.send();
        ostr.write(reinterpret_cast<const char*>(frameData.data()), frameData.size());
    }
};

// --- HTML 页面（视频 & 拍照按钮） ---
class IndexHandler : public Poco::Net::HTTPRequestHandler {
public:
    void handleRequest(Poco::Net::HTTPServerRequest &request, Poco::Net::HTTPServerResponse &response) override {
        response.setContentType("text/html");
        std::ostream &ostr = response.send();

        ostr << "<html><head>";
        ostr << "<style>";
        ostr << "body { font-family: sans-serif; text-align: center; }";
        ostr << "img { border: 2px solid #ccc; border-radius: 8px; box-shadow: 0 4px 8px rgba(0,0,0,0.1); }";
        ostr << "a { display: inline-block; padding: 10px 20px; margin-top: 20px; background-color: #007bff; color: white; text-decoration: none; border-radius: 5px; }";
        ostr << "</style>";
        ostr << "</head><body>";
        ostr << "<h1>Raspberry Pi Camera Server</h1>";
        ostr << "<p>Live stream from the camera.</p>";
        ostr << "<img src=\"/stream\" width=\"640\" height=\"480\" />";
        ostr << "<h2>Capture a Photo</h2>";
        ostr << "<a href=\"/capture\" target=\"_blank\">Take Photo</a>";
        ostr << "</body></html>";
    }
};

// --- Handler Factory ---
class MyRequestHandlerFactory : public Poco::Net::HTTPRequestHandlerFactory {
public:
    Poco::Net::HTTPRequestHandler* createRequestHandler(const Poco::Net::HTTPServerRequest &request) override {
        if (request.getURI() == "/") return new IndexHandler;
        else if (request.getURI() == "/stream") return new StreamHandler;
        else if (request.getURI() == "/capture") return new CaptureHandler;
        else return nullptr;
    }
};

// --- 主应用 ---
class CameraServer : public Poco::Util::ServerApplication {
protected:
    int main(const std::vector<std::string> &) override {
        g_cameraManager = std::make_shared<LibcameraManager>();
        if (!g_cameraManager->init()) {
            std::cerr << "Error: Camera initialization failed." << std::endl;
            return 1;
        }

        g_cameraManager->startCapture();
        
        Poco::Net::HTTPServer s(new MyRequestHandlerFactory, Poco::Net::ServerSocket(8080), new Poco::Net::HTTPServerParams);
        s.start();
        std::cout << "Server started on http://localhost:8080" << std::endl;

        waitForTerminationRequest(); // Ctrl+C 退出
        
        g_cameraManager->stopCapture();
        s.stop();
        return 0;
    }
};

POCO_SERVER_MAIN(CameraServer)
