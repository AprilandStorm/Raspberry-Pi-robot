#pragma once

#include <cstdint>
#include <vector>
#include <string>
#include <arpa/inet.h>  // 用于 htonl/ntohl

#pragma pack(push, 1)  //强制 1 字节对齐

// 通信协议定义
namespace Protocol {
    // 消息类型
    enum class MessageType : uint8_t {
        UNKNOWN = 0x00,
        VIDEO_FRAME = 0x01,    // 视频帧数据
        CAPTURE_COMMAND = 0x02, // 拍照命令
        CAPTURE_RESPONSE = 0x03, // 拍照响应
        HEARTBEAT = 0x04       // 心跳包
    };

    // 消息头
    struct MessageHeader {
        MessageHeader() : message_id(0), type(MessageType::UNKNOWN), payload_length(0), timestamp(0) {}
        
        uint32_t message_id;      // 消息ID
        MessageType type;         // 消息类型
        //uint32_t type;         // 消息类型
        uint32_t payload_length;  // 数据长度
        uint32_t timestamp;       // 时间戳

        // 转换为网络字节序
        void toNetworkOrder() {
            message_id = htonl(message_id);
            payload_length = htonl(payload_length);
            timestamp = htonl(timestamp);
        }

        // 从网络字节序转换回主机字节序
        void toHostOrder() {
            message_id = ntohl(message_id);
            payload_length = ntohl(payload_length);
            timestamp = ntohl(timestamp);
        }
    };

    // 拍照响应
    struct CaptureResponse {
        uint32_t capture_id;      // 拍照ID
        uint32_t image_size;      // 图片大小
        char filename[256];       // 文件名

        void toNetworkOrder() {
            capture_id = htonl(capture_id);
            image_size = htonl(image_size);
        }

        void toHostOrder() {
            capture_id = ntohl(capture_id);
            image_size = ntohl(image_size);
        }
    };

    // 序列化工具
    std::vector<uint8_t> serializeHeader(const MessageHeader& header);
    bool parseHeader(const std::vector<uint8_t>& data, MessageHeader& header);
}

#pragma pack(pop)  // 恢复对齐