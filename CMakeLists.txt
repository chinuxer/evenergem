cmake_minimum_required(VERSION 3.16)
project(PowerTopologyDemo)

# 设置C++标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 强制使用系统Qt，避免查找Windows Qt
set(CMAKE_PREFIX_PATH "/usr/lib/x86_64-linux-gnu/cmake/Qt5")
# 自动查找Qt5
find_package(Qt5 REQUIRED COMPONENTS Core Widgets Gui)

# 设置自动处理MOC、UIC、RCC
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

# 添加可执行文件
add_executable(PowerTopologyDemo
    main.cpp
    mainwindow.cpp
    mainwindow.h
    mainwindow.ui
    powertopology.cpp
    powertopology.h
)

# 链接Qt库
target_link_libraries(PowerTopologyDemo Qt5::Core Qt5::Widgets Qt5::Gui)

# 设置包含目录
target_include_directories(PowerTopologyDemo PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

# 在Linux上设置运行时库路径
if(UNIX AND NOT APPLE)
    set_target_properties(PowerTopologyDemo PROPERTIES
        INSTALL_RPATH "$ORIGIN"
    )
endif()