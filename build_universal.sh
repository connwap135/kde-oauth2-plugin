#!/bin/bash

# 创建兼容所有Ubuntu版本的DEB包
# 使用"或"依赖语法来支持多个版本

set -e

# 颜色定义
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

PROJECT_NAME="gzweibo-oauth2-plugin"
VERSION="1.0.0"
ARCHITECTURE="amd64"

echo -e "${BLUE}🚀 创建通用兼容DEB包${NC}"
echo -e "${BLUE}========================${NC}"

# 备份原始build_deb.sh
cp build_deb.sh build_deb.sh.backup

# 使用"或"依赖语法，支持所有Ubuntu版本
UNIVERSAL_DEPS="libqt5core5t64 | libqt5core5a, libqt5network5t64 | libqt5network5, libqt5widgets5t64 | libqt5widgets5, libqt5gui5t64 | libqt5gui5"

echo -e "${YELLOW}🔧 创建通用依赖配置...${NC}"
echo "通用依赖: $UNIVERSAL_DEPS"

# 修改build_deb.sh中的依赖
sed -i "s/Depends: libqt5core5t64, libqt5network5t64, libqt5widgets5t64, libqt5gui5t64/Depends: $UNIVERSAL_DEPS/g" build_deb.sh

# 修改包名
DEB_FILE="${PROJECT_NAME}_${VERSION}-universal_${ARCHITECTURE}.deb"
sed -i "s/DEB_FILE=\"\${PROJECT_NAME}_\${VERSION}_\${ARCHITECTURE}.deb\"/DEB_FILE=\"$DEB_FILE\"/g" build_deb.sh

echo -e "${GREEN}✅ 配置已更新为通用兼容模式${NC}"

# 运行构建
echo -e "${YELLOW}🔨 构建通用兼容包...${NC}"
./build_deb.sh

# 恢复原始配置
mv build_deb.sh.backup build_deb.sh

echo -e "${GREEN}🎉 通用兼容包构建完成！${NC}"
echo ""
echo -e "${BLUE}📦 生成的通用包:${NC}"
ls -lh *universal*.deb

echo ""
echo -e "${BLUE}💡 兼容性说明:${NC}"
echo "✅ 支持 Ubuntu 24.04+ (优先使用t64库)"
echo "✅ 支持 Ubuntu 22.04及更早版本 (回退到传统库)"
echo "✅ 自动选择可用的Qt5库版本"

echo ""
echo -e "${BLUE}📋 验证依赖:${NC}"
dpkg -I "$DEB_FILE" | grep -A2 "Depends:"