QT += core gui widgets network

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++11

# -------------------------------------------------
# 1. 根据构建类型配置 (Debug/Release)
# -------------------------------------------------
CONFIG(debug, debug|release) {
    CONFIG -= release
    CONFIG += debug
    TARGET = $$join(TARGET,,,_debug)
    DESTDIR = debug
    DEFINES += QT_DEBUG
    # 注意：-g 和 -O0 是 GCC 参数，MSVC 会自动处理 debug 配置，加了也不报错但没必要
    win32-g++: QMAKE_CXXFLAGS += -g -O0
    win32-msvc: QMAKE_CXXFLAGS += /Zi /Od
} else {
    CONFIG -= debug
    CONFIG += release
    DESTDIR = release
    DEFINES += QT_NO_DEBUG_OUTPUT
    # 注意：-O2 是 GCC 参数，MSVC 对应 /O2
    win32-g++: QMAKE_CXXFLAGS += -O2
    win32-msvc: QMAKE_CXXFLAGS += /O2
}

# -------------------------------------------------
# 2. Windows 特定配置 (核心修改部分)
# -------------------------------------------------
win32 {
    # 设置应用程序图标（可选）
    # RC_ICONS = app.ico

    # 确保生成 GUI 应用基础配置
    CONFIG += windows

    # --- 分支 A: 如果是 MinGW (GCC) ---
    win32-g++ {
        # 链接 MinGW 启动库
        LIBS += -lmingw32

        # 静态链接运行时库 (生成独立 exe，不依赖 libgcc 等 dll)
        QMAKE_LFLAGS += -static -static-libgcc -static-libstdc++

        # 设置子系统
        QMAKE_LFLAGS += -Wl,-subsystem,windows

        # 调试版本添加控制台
        CONFIG(debug, debug|release) {
            CONFIG += console
            # 覆盖上面的 windows 设置，强制显示控制台
            QMAKE_LFLAGS += -Wl,-subsystem,console
        }

        # 字符集设置 (GCC 专用)
        QMAKE_CXXFLAGS += -finput-charset=UTF-8
        QMAKE_CXXFLAGS += -fexec-charset=UTF-8
    }

    # --- 分支 B: 如果是 MSVC (Visual Studio) ---
    win32-msvc* {
        # MSVC 不需要 -lmingw32，也不要加 static 链接参数 (除非你有静态版 Qt 库)
        # 这里的 LIBS 留空，或者添加 Windows 特有库，例如:
        # LIBS += -luser32 -lgdi32

        # MSVC 的子系统设置通常由 CONFIG += windows/console 自动处理
        # 如果必须手动指定，语法是 /SUBSYSTEM:CONSOLE (注意斜杠和冒号)
        # 但通常不需要手动写 QMAKE_LFLAGS

        # 调试版本添加控制台
        CONFIG(debug, debug|release) {
            CONFIG += console
        }

        # MSVC 的 UTF-8 设置
        # /utf-8 标志告诉 MSVC 源代码和执行字符集都是 UTF-8 (VS2015 Update3 及以上支持)
        QMAKE_CXXFLAGS += /utf-8
        DEFINES += _USE_MATH_DEFINES
        # 如果需要静态链接 CRT (对应 GCC 的 -static)，需取消注释下面这行
        # 但注意：官方下载的 Qt MSVC 版通常是动态链接 (/MD)，混用可能导致崩溃
        # QMAKE_CXXFLAGS_RELEASE += /MT
        # QMAKE_CXXFLAGS_DEBUG += /MTd

    }
}

# -------------------------------------------------
# 3. 源文件和头文件
# -------------------------------------------------
SOURCES += \
    main.cpp \
    mainwindow.cpp \
    powertopology.cpp \
    telnetconsole.cpp\
    telnetclient.cpp\
    logwindow.cpp\
    topologyselectdialog.cpp\
    pwralloc/pau_broker.c\
    pwralloc/pau_tactic.c\
    pwralloc/pau_directed.c\
    pwralloc/pau_topolog.c  \
    pwralloc/pau_vector.c\
    pwralloc/pau_fsm.c

HEADERS += \
    mainwindow.h \
    powertopology.h \
    telnetconsole.h\
    telnetclient.h\
    logwindow.h\
    topologyselectdialog.h

FORMS += \
    mainwindow.ui

RESOURCES += resources.qrc
RC_ICONS = appicon.ico
DEFINES += CUSTOM_HEAP_SIZE=0x00010000\
           QTDEMOD_NODES_CAPACITY=36
# -------------------------------------------------
# 4. 其他平台配置 (Unix/Linux/Mac)
# -------------------------------------------------
unix {
    QMAKE_CXXFLAGS += -std=c++11 -Wall -Wextra
    LIBS += -lm
}

# -------------------------------------------------
# 5. 输出目录设置
# -------------------------------------------------
OBJECTS_DIR = $$DESTDIR/.obj
MOC_DIR = $$DESTDIR/.moc
RCC_DIR = $$DESTDIR/.rcc
UI_DIR = $$DESTDIR/.ui

win32-msvc* {
    # 1. 获取 windeployqt 的绝对路径
    windeployqt_path = $$[QT_INSTALL_BINS]/windeployqt.exe
    # 将正斜杠转换为反斜杠 (Windows cmd 更友好)
    windeployqt_path ~= s,/,\\,g

    # 2. 根据 Debug/Release 分别处理
    CONFIG(debug, debug|release) {
        # --- Debug 模式 ---
        # 先构造完整的输出目录路径 (例如: D:\build\debug)
        # 注意：这里使用 / 连接，qmake 会自动处理，或者后面统一转换
        out_dir = $$OUT_PWD/debug

        # 构造完整的 exe 路径
        target_path = $$out_dir/$$TARGET

        # 构造最终命令
        # 技巧：使用 $$quote() 或者手动拼接，确保引号正确
        # 这里我们分步构建字符串，避免转义混乱
        cmd_cd = cd /d \"$$out_dir\"
        cmd_deploy = \"$$windeployqt_path\" --debug \"$$TARGET\".exe

        QMAKE_POST_LINK = $$cmd_cd && $$cmd_deploy

    } else {
        # --- Release 模式 ---
        out_dir = $$OUT_PWD/release
        target_path = $$out_dir/$$TARGET

        cmd_cd = cd /d \"$$out_dir\"
        cmd_deploy = \"$$windeployqt_path\" \"$$TARGET\".exe

        QMAKE_POST_LINK = $$cmd_cd && $$cmd_deploy
    }

    # 3. 打印调试信息 (关键步骤，用于确认生成的命令是否正确)
    message(----------------------------------------)
    message(Post-Link Command: $$QMAKE_POST_LINK)
    message(----------------------------------------)
}
