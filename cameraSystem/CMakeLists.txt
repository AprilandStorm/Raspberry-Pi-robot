# 树莓派服务端专用
cmake_minimum_required(VERSION 3.12)
project(CameraServer)

set(CMAKE_CXX_STANDARD 17)

# 查找依赖
find_package(Poco COMPONENTS Net Util Foundation REQUIRED)
find_package(OpenCV REQUIRED)

# 使用 pkg-config 查找 libcamera（使用正确的包名）
find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBCAMERA REQUIRED libcamera)
pkg_check_modules(LIBCAMERA_BASE REQUIRED libcamera-base)

add_executable(camera_server 
    src/server.cpp
    src/protocol.cpp
)
target_include_directories(camera_server PUBLIC 
    include
    ${LIBCAMERA_INCLUDE_DIRS}
    ${LIBCAMERA_BASE_INCLUDE_DIRS}
)
target_link_libraries(camera_server PRIVATE 
    Poco::Net Poco::Util Poco::Foundation 
    ${OpenCV_LIBS}
    ${LIBCAMERA_LIBRARIES}
    ${LIBCAMERA_BASE_LIBRARIES}
)

# 添加编译定义
target_compile_definitions(camera_server PRIVATE 
    ${LIBCAMERA_CFLAGS_OTHER}
    ${LIBCAMERA_BASE_CFLAGS_OTHER}
)