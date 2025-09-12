#!/bin/bash

# KDE OAuth2 Plugin 兼容性检查脚本
# 检查当前DEB包在不同Ubuntu版本上的兼容性

# 颜色定义
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

echo -e "${BLUE}🔍 KDE OAuth2 Plugin 兼容性检查${NC}"
echo -e "${BLUE}=====================================${NC}"

# 获取系统信息
UBUNTU_VERSION=$(lsb_release -r | awk '{print $2}')
UBUNTU_CODENAME=$(lsb_release -c | awk '{print $2}')

echo "当前系统: Ubuntu $UBUNTU_VERSION ($UBUNTU_CODENAME)"
echo ""

# 检查Qt5库版本
echo -e "${YELLOW}📋 检查Qt5库状态...${NC}"

# t64版本库（Ubuntu 24.04+）
T64_LIBS=(
    "libqt5core5t64"
    "libqt5network5t64"
    "libqt5widgets5t64"
    "libqt5gui5t64"
)

# 传统版本库（Ubuntu 22.04及更早）
LEGACY_LIBS=(
    "libqt5core5a"
    "libqt5network5"
    "libqt5widgets5"
    "libqt5gui5"
)

echo -e "${BLUE}T64版本库 (Ubuntu 24.04+):${NC}"
for lib in "${T64_LIBS[@]}"; do
    if dpkg -l | grep -q "^ii.*$lib"; then
        echo -e "${GREEN}✅ $lib - 已安装${NC}"
    elif apt list 2>/dev/null | grep -q "$lib/"; then
        echo -e "${YELLOW}⚠️  $lib - 可用但未安装${NC}"
    else
        echo -e "${RED}❌ $lib - 不可用${NC}"
    fi
done

echo ""
echo -e "${BLUE}传统版本库 (Ubuntu 22.04及更早):${NC}"
for lib in "${LEGACY_LIBS[@]}"; do
    if dpkg -l | grep -q "^ii.*$lib"; then
        echo -e "${GREEN}✅ $lib - 已安装${NC}"
    elif apt list 2>/dev/null | grep -q "$lib/"; then
        echo -e "${YELLOW}⚠️  $lib - 可用但未安装${NC}"
    else
        echo -e "${RED}❌ $lib - 不可用${NC}"
    fi
done

echo ""
echo -e "${YELLOW}🔍 分析兼容性...${NC}"

# 分析当前DEB包依赖
if [ -f "gzweibo-oauth2-plugin_1.0.0_amd64.deb" ]; then
    echo -e "${BLUE}当前DEB包依赖:${NC}"
    dpkg -I gzweibo-oauth2-plugin_1.0.0_amd64.deb | grep "Depends:" | sed 's/^ Depends: //'
    
    CURRENT_DEPS=$(dpkg -I gzweibo-oauth2-plugin_1.0.0_amd64.deb | grep "Depends:" | sed 's/^ Depends: //')
    
    if [[ "$CURRENT_DEPS" == *"t64"* ]]; then
        echo -e "${YELLOW}⚠️  当前包使用t64依赖，仅兼容Ubuntu 24.04+${NC}"
    else
        echo -e "${GREEN}✅ 当前包使用传统依赖，兼容Ubuntu 22.04及更早版本${NC}"
    fi
else
    echo -e "${RED}❌ 未找到DEB包文件${NC}"
fi

echo ""
echo -e "${YELLOW}💡 兼容性建议:${NC}"

case "$UBUNTU_VERSION" in
    "24.04"|"24."*)
        echo "✅ Ubuntu 24.04+ - 使用t64版本依赖"
        echo "   推荐依赖: libqt5core5t64, libqt5network5t64, libqt5widgets5t64, libqt5gui5t64"
        ;;
    "22.04"|"22."*|"20.04"|"20."*|"18.04"|"18."*)
        echo "⚠️  Ubuntu 22.04及更早版本 - 使用传统依赖"
        echo "   推荐依赖: libqt5core5a, libqt5network5, libqt5widgets5, libqt5gui5"
        ;;
    *)
        echo "❓ 未知版本 - 请手动检查依赖"
        ;;
esac

echo ""
echo -e "${BLUE}🛠️  创建兼容版本建议:${NC}"
echo "1. 创建双版本DEB包（推荐）"
echo "2. 使用虚拟依赖包"
echo "3. 运行时检测和适配"