#!/bin/bash
cd "$(dirname "$0")"

# 检查是否已构建
if [ ! -f "build/RingTopologyPower" ]; then
    echo "错误：未找到可执行文件，请先运行 ./build.sh"
    exit 1
fi

# 设置显示（如果需要远程运行）
export DISPLAY=:0

# 运行程序
echo "启动PowerTopology演示程序..."
 # 在本地图形界面登录后直接使用终端：
        export DISPLAY=:0
./build/RingTopologyPower