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

## 定制
### 定义变量
将文件名对应的字符串存储起来，在cmake里定义变量需要使用```set```
```
# []中的参数为可选项，如不需要可以不写
SET(VAR [VALUE] [CACHE TYPE DOCSTRING [FORCE]])
set(SRC_LIST add.c;div.c;mian.c;mult.c;sub.c)
add_executable(app ${SRC_LIST})
```
### 指定使用的C++标准
