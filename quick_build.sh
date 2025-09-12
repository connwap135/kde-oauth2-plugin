#!/bin/bash

# 快速打包脚本 - KDE OAuth2 Plugin
# 使用方法: ./quick_build.sh [版本号]

VERSION=${1:-"1.0.0"}
PROJECT_NAME="kde-oauth2-plugin"

echo "🚀 快速构建 ${PROJECT_NAME} v${VERSION}"

# 清理并构建
rm -rf build debian *.deb
mkdir -p build && cd build
cmake .. && make && cd ..

# 一键打包
./build_deb.sh

echo "✅ 完成！生成文件: ${PROJECT_NAME}_${VERSION}_amd64.deb"