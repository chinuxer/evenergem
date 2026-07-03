@echo off
chcp 65001 >nul
title Qt MinGW 64-bit Build Script

:: ==========================================
:: 1. 配置区域 (请根据实际路径确认)
:: ==========================================
:: 设置正确的 64位 MinGW 路径
set QT_MINGW_PATH=C:\Philistor\Qt\Qt5.12.12\5.12.12\mingw73_64\bin
set MINGW_PATH=C:\Philistor\Qt\Qt5.12.12\Tools\mingw730_64\bin
:: 设置 pro 文件名 (如果和 bat 文件名不同，请修改这里)
set PRO_FILE=RingTopologyPower.pro

:: ==========================================
:: 2. 环境检查与设置
:: ==========================================
echo [INFO] 正在配置 64位 MinGW 环境...
if not exist "%MINGW_PATH%\g++.exe" (
    echo [ERROR] 错误：找不到编译器！请检查路径是否正确：
    echo        %MINGW_PATH%
    pause
    exit /b 1
)

:: 将正确的 qt软件mingw bin 目录添加到 PATH 最前面 (临时生效，仅当前窗口)
set PATH=%QT_MINGW_PATH%;%PATH%
set PATH=%MINGW_PATH%;%PATH%
:: 验证版本 (可选，用于确认是否切换成功)
echo [INFO] 当前使用的 g++ 版本:
g++ --version | findstr /C:"g++"
echo.

:: ==========================================
:: 3. 清理旧构建 (防止 32/64 位文件混用)
:: ==========================================
echo [INFO] 正在清理旧的构建文件...
if exist Makefile (del /q Makefile)
if exist Makefile.Release (del /q Makefile.Release)
if exist Makefile.Debug (del /q Makefile.Debug)
if exist release (rmdir /s /q release)
if exist debug (rmdir /s /q debug)
echo [INFO] 清理完成。
echo.

:: ==========================================
:: 4. 执行 qmake
:: ==========================================
echo [INFO] 正在运行 qmake (Spec: win32-g++)...
qmake -spec win32-g++ "%PRO_FILE%"
if errorlevel 1 (
    echo [ERROR] qmake 失败！请检查 .pro 文件。
    pause
    exit /b 1
)
echo.

:: ==========================================
:: 5. 执行 make 编译
:: ==========================================
echo [INFO] 开始编译 (使用 %NUMBER_OF_PROCESSORS% 个线程)...
mingw32-make -j%NUMBER_OF_PROCESSORS%
if errorlevel 1 (
    echo [ERROR] 编译失败！请查看上面的错误信息。
    pause
    exit /b 1
)

:: ==========================================
:: 6. 部署 DLL (自动复制 Qt 依赖库)
:: ==========================================
echo.
echo [INFO] 正在部署 Qt DLL (windeployqt)...
if exist release\RingTopologyPower.exe (
    windeployqt --release release\RingTopologyPower.exe
    echo [SUCCESS] 发布版构建并部署完成！
    echo          可执行文件位置：release\RingTopologyPower.exe
) else if exist debug\RingTopologyPower.exe (
    :: 如果你编译的是 debug 版，把上面的 release 改成 debug，并加上 --debug 参数
    windeployqt --debug debug\RingTopologyPower.exe
    echo [SUCCESS] 调试版构建并部署完成！
    echo          可执行文件位置：debug\RingTopologyPower.exe
) else (
    echo [WARNING] 未找到生成的 exe 文件，跳过部署步骤。
)

echo.
echo ==========================================
echo 所有任务完成！
echo ==========================================
pause