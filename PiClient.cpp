// pi_client.cpp
#include <iostream>
#include <vector>
#include <cstdio>
#include <cstdlib>
#include <Poco/Net/HTTPClientSession.h>
#include <Poco/Net/HTTPRequest.h>
#include <Poco/Net/HTTPResponse.h>
#include <Poco/StreamCopier.h>
#include <Poco/Path.h>
#include "pca9685.hpp"

// 从 libcamera-still 读取一张 JPEG 到内存
bool captureJPEG(std::vector<unsigned char>& out, int w=320, int h=240, bool vflip=true, bool hflip=false) {
    std::string cmd = "libcamera-still -n -t 1 --autofocus-mode continuous "
                      "--width " + std::to_string(w) +
                      " --height " + std::to_string(h) +
                      (vflip ? " --vflip" : "") +
                      (hflip ? " --hflip" : "") +
                      " -o -"; // 输出到 stdout
    FILE* fp = popen(cmd.c_str(), "r");
    if (!fp) { perror("popen(libcamera-still) 失败"); return false; }

    unsigned char buf[8192];
    out.clear();
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), fp)) > 0) {
        out.insert(out.end(), buf, buf + n);
    }
    int rc = pclose(fp);
    if (rc == -1 || out.empty()) {
        std::cerr << "抓拍失败，返回码=" << rc << ", 大小=" << out.size() << "\n";
        return false;
    }
    std::cout << "抓拍完成，JPEG " << out.size() << " 字节\n";
    return true;
}

// 发送 JPEG 到 PC: http://host:port/upload
bool postJPEG(const std::string& host, unsigned short port, const std::vector<unsigned char>& img) {
    using namespace Poco::Net;
    try {
        HTTPClientSession session(host, port);
        HTTPRequest req(HTTPRequest::HTTP_POST, "/upload", HTTPMessage::HTTP_1_1);
        req.setContentType("image/jpeg");
        req.setContentLength((int)img.size());

        std::ostream& os = session.sendRequest(req);
        os.write(reinterpret_cast<const char*>(img.data()), (std::streamsize)img.size());
        os.flush();

        HTTPResponse res;
        std::istream& rs = session.receiveResponse(res);
        std::cout << "服务器响应: " << res.getStatus() << " " << res.getReason() << "\n";
        if (res.getStatus() != HTTPResponse::HTTP_OK) return false;
        return true;
    } catch (std::exception& e) {
        std::cerr << "POST 失败: " << e.what() << "\n";
        return false;
    }
}

int main(int argc, char** argv) {
    if (argc < 3) {
        std::cerr << "用法: ./pi_client <server_host> <port>\n"
                  << "例如: ./pi_client 192.168.1.100 8080\n";
        return 1;
    }
    std::string host = argv[1];
    unsigned short port = (unsigned short) std::stoi(argv[2]);

    try {
        // 1) 初始化 PCA9685，设 50Hz 舵机频率
        PCA9685 pca(1, 0x40, true);
        pca.setPWMFreq(50.0f);

        // 2) 设定两个舵机角度（与 Python 示例一致）
        //    底座舵机：CH10 -> 90°
        pca.set_servo_angle(10, 90);
        //    顶部舵机：CH9  -> 50°
        pca.set_servo_angle(9, 50);
        usleep(500000); // 0.5s

        // 3) 抓拍一张 320x240（与示例一致、vflip=1）
        std::vector<unsigned char> jpeg;
        if (!captureJPEG(jpeg, 320, 240, /*vflip=*/true, /*hflip=*/false)) {
            return 2;
        }

        // 4) 发送到服务器
        if (!postJPEG(host, port, jpeg)) {
            std::cerr << "上传失败\n";
            return 3;
        }
        std::cout << "上传成功\n";
    } catch (std::exception& e) {
        std::cerr << "异常: " << e.what() << "\n";
        return 10;
    }
    return 0;
}
