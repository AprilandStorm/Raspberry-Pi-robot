# CMake概述
CMake是一个项目构建工具，并且是跨平台的。通过CMake实现项目的自动化管理。
- 相比于Makefile（通常依赖于当前的编译平台，工作量巨大，解决依赖关系时容易出错）， CMake允许开发者指定整个工程的编译流程，根据编译平台，自动生成本地化的Makefile和工程文件，最后用户只需make编译即可，可以把CMake看成是一款自动生成Makefile的工具。
- 流程图
项目--->CMakeList.txt--->cmake--->生成Makefile--->make
- 检查cmake版本```cmake --version```
- ```touch CMakeLists.txt```在目前目录下创建CMakeLists.txt文件

# CMake使用
CMake支持大写、小写、混合大小写的命令
## 注释
- ```#```行注释
- ```#[[ ]]```块注释
## 命令
- ```cmake_minimum_required(VERSION 3.0)```指定使用的CMake的最低版本（非必须）
- project指定工程名字
  语法：
  ```
  project(<PROJECT-NAME>[<language-name>...])
  project(<PROJECT-NAME>
          [VERSION <major>[.<minot>[.<patch>[.<tweak>]]]]
          [DESCRIPTION<project-description-string>]
          [HOMEPAGE_URL<url-string>]
          [LANGUAGE<language-name>...])
  ```
  - ```add_executable(可执行程序名 源文件名称)```定义工程会生成一个可执行程序（源文件可以是一个也可以是多个，多个用空格或者 ；间隔）

## 执行CMake命令
```
cmake CMakeLists.txt文件所在路径
cmake .
cmake ..
```
## ```make``` 生成可执行程序

## set
### 定义变量
将文件名对应的字符串存储起来，在cmake里定义变量需要使用```set```
```
# []中的参数为可选项，如不需要可以不写
SET(VAR [VALUE] [CACHE TYPE DOCSTRING [FORCE]])
set(SRC_LIST add.c;div.c;mian.c;mult.c;sub.c)
add_executable(app ${SRC_LIST})
```

### 指定使用的C++标准
```
set(CMAKE_CXX_STANDARD 11)
```
或
```
cmake CMakeList.txt文件路径 -DCMAKE_CXX_STANDARD=11
```

### 指定输出的路径
指定可执行程序输出的路径，叫作```EXECUTABLE_OUTPUT_PATH```,它的值可以通过set命令进行设置
```
set(HOME /home/robin/Linux/Sort)
set(EXECURTABLE_OUTPUT_PATH ${HOME}/bin)
```
- 第一行：定义一个变量用于存储一个绝对路径
- 第二行：将拼接好的路径值设置给```EXECUTABLE_OUTPUT_PATH```宏（如果这个路径中的子目录不存在，会自动生成，无需手动创建）
- 如果生成路径的时候使用的是相对路径```./xxx/xxx```，那么这个路径中的```./```对应的就是makefile文件所在的那个目录（建议使用绝对路径）

## 搜索文件
如果一个项目里边的源文件很多，在编写CMakeList.txt文件的时候不可能将项目目录下的各个文件都罗列出来，这样过于麻烦。在CMake中为我们提供了搜索文件的命令，可以使用aux_source_directory命令或者file命令。
### 方式1
```
aux_source_directory(<dir><variable>)
```
- dir: 要搜索的目录
- variable: 将从dir目录下搜索的源文件列表存储于该变量中
```
include_directories(${PROJECT_SOURCE_DIR}/include)
aux_source_directory(${PROJECT_CURRENT_SOURCE_DIR}/src SRC_LIST)
add_executable(app ${SRC_LIST})
```

|变量名|含义|
|----|----|
|PROJECT_SOURCE_DIR|当前project()指令所在目录（项目源码根）|
|CMAKE_SOURCE_DIR|整个构建树的根目录（顶层 CMakeLists.txt 所在目录），单项目场景下和前者一致；嵌套子 project 时，前者指向子项目目录，后者不变|
|PROJECT_BINARY_DIR|项目的构建输出根目录（编译产物存放目录，比如build/），和源码目录对应|
|CMAKE_CURRENT_SOURCE_DIR|当前正在处理的 CMakeLists.txt 所在目录（比如子目录的 CMakeLists.txt）|

### 方式2
```
file(GLOB或GLOB_RECURSE 变量名 要搜索的文件路径和文件类型）
```
- GLOB:将指定目录下搜索到的满足的所有文件名生成一个列表，并将其存储于变量中
- GLOB_RECURSE：递归搜索指定目录，将搜索到的满足条件的文件名生成一个列表，并将其存储于一个变量中
```
file(GLOB MAIN_SRC ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp)
file(GLOB MAIN_HEAD ${CMAKE_CURRENT_SOURCE_DIR}/include/*.h)
```
- CMAKE_CURRRENT_SOURCE_DIR 表示当前访问的CMakeList.txt文件所在的路径
- 搜索的文件路径和类型可以加双引号，也可以不加

## 包含头文件
在编译项目源文件的时候，很多时候都需要将源文件对应的头文件路径指定出来，这样才能保证在编译过程中编译中编译器能够找到这些头文件，并顺利通过编译。设置语句```include_directory```
```
include_directory(头文件所在路径)
```

- ```tree -L 1```显示目录结构
- 
