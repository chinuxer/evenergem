#!/bin/bash

# =============================================
# Qt项目自动化构建脚本
# 使用系统qmake（/usr/bin/qmake）进行构建
# =============================================

# 设置错误处理
set -e  # 遇到任何错误立即退出脚本

# 颜色定义（用于美化输出）
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # 无颜色

# 打印颜色信息的工具函数
print_info() { echo -e "${BLUE}[INFO]${NC} $1"; }
print_success() { echo -e "${GREEN}[SUCCESS]${NC} $1"; }
print_warning() { echo -e "${YELLOW}[WARNING]${NC} $1"; }
print_error() { echo -e "${RED}[ERROR]${NC} $1"; }

# 变量配置
PROJECT_NAME="RingTopologyPower"
BUILD_DIR="build"
QMAKE_PATH="/usr/bin/qmake"
MAKE_JOBS=$(nproc)  # 使用所有可用的CPU核心进行并行编译

# =============================================
# 函数定义
# =============================================

# 检查必要工具是否存在
check_dependencies() {
    print_info "检查构建依赖..."
    
    local missing_tools=()
    
    # 检查qmake
    if [ ! -f "$QMAKE_PATH" ]; then
        print_error "未找到qmake: $QMAKE_PATH"
        missing_tools+=("qmake")
    else
        print_info "找到qmake: $($QMAKE_PATH -v 2>/dev/null | head -n1 || echo '未知版本')"
    fi
    
    # 检查make
    if ! command -v make &> /dev/null; then
        print_error "未找到make工具"
        missing_tools+=("make")
    else
        print_info "找到make: $(make --version | head -n1)"
    fi
    
    # 检查g++
    if ! command -v g++ &> /dev/null; then
        print_error "未找到g++编译器"
        missing_tools+=("g++")
    else
        print_info "找到g++: $(g++ --version | head -n1)"
    fi
    
    # 如果有缺失的工具，退出脚本
    if [ ${#missing_tools[@]} -ne 0 ]; then
        print_error "缺失必要的构建工具: ${missing_tools[*]}"
        print_info "请安装缺失的工具: sudo apt install qt5-default build-essential g++"
        exit 1
    fi
}

# 清理构建目录
clean_build() {
    if [ -d "$BUILD_DIR" ]; then
        print_info "清理构建目录: $BUILD_DIR"
        rm -rf "$BUILD_DIR"
    fi
    mkdir -p "$BUILD_DIR"
}

# 生成Makefile
generate_makefile() {
    print_info "生成Makefile..."
    
    cd "$BUILD_DIR"
    
    # 使用qmake生成Makefile
    if ! $QMAKE_PATH ../*.pro; then
        print_error "qmake执行失败"
        exit 1
    fi
    
    cd ..
}

# 编译项目
compile_project() {
    print_info "开始编译项目 (使用 $MAKE_JOBS 个并行任务)..."
    
    cd "$BUILD_DIR"
    
    # 执行编译
    if ! make -j$MAKE_JOBS; then
        print_error "编译失败"
        exit 1
    fi
    
    cd ..
}

# 检查可执行文件
check_executable() {
    local executable="$BUILD_DIR/$PROJECT_NAME"
    
    if [ -f "$executable" ] && [ -x "$executable" ]; then
        print_success "可执行文件已生成: $executable"
        print_info "文件信息: $(file "$executable")"
        return 0
    else
        print_error "未找到可执行文件: $executable"
        return 1
    fi
}

# 显示构建摘要
show_summary() {
    print_success "构建完成!"
    echo "================================"
    print_info "项目名称: $PROJECT_NAME"
    print_info "构建目录: $BUILD_DIR"
    print_info "qmake路径: $QMAKE_PATH"
    print_info "并行任务数: $MAKE_JOBS"
    print_info "可执行文件: ./$BUILD_DIR/$PROJECT_NAME"
    echo "================================"    
    print_info "运行程序: ./$BUILD_DIR/$PROJECT_NAME"
}

# =============================================
# 主执行流程
# =============================================

main() {
    echo "============================================="
    echo "  开始构建Qt项目: $PROJECT_NAME"
    echo "============================================="
    
    # 检查依赖
    check_dependencies
    
    # 清理并准备构建目录
    clean_build
    
    # 生成Makefile
    generate_makefile
    
    # 编译项目
    compile_project
    
    # 验证结果
    if check_executable; then       
        show_summary
    else
        print_error "构建失败: 未生成可执行文件"
        exit 1
    fi     
}

# 脚本执行入口
main "$@"