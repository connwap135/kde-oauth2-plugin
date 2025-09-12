#!/bin/bash

# KDE OAuth2 Plugin - 兼容性自动打包脚本
# 为不同Ubuntu版本创建兼容的DEB包

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

echo -e "${BLUE}🚀 KDE OAuth2 Plugin - 兼容性打包脚本${NC}"
echo -e "${BLUE}==========================================${NC}"

# 检测当前Ubuntu版本
UBUNTU_VERSION=$(lsb_release -r | awk '{print $2}')
UBUNTU_CODENAME=$(lsb_release -c | awk '{print $2}')

echo "当前系统: Ubuntu $UBUNTU_VERSION ($UBUNTU_CODENAME)"

# 根据版本选择依赖
if [[ "$UBUNTU_VERSION" > "24.00" ]] || [[ "$UBUNTU_VERSION" == "24."* ]]; then
    # Ubuntu 24.04+
    QT_DEPS="libqt5core5t64, libqt5network5t64, libqt5widgets5t64, libqt5gui5t64"
    SUFFIX="ubuntu24"
    echo -e "${GREEN}✅ 检测到Ubuntu 24.04+，使用t64依赖${NC}"
elif [[ "$UBUNTU_VERSION" > "20.00" ]] || [[ "$UBUNTU_VERSION" == "22."* ]] || [[ "$UBUNTU_VERSION" == "20."* ]]; then
    # Ubuntu 20.04, 22.04
    QT_DEPS="libqt5core5a, libqt5network5, libqt5widgets5, libqt5gui5"
    SUFFIX="ubuntu22"
    echo -e "${GREEN}✅ 检测到Ubuntu 22.04/20.04，使用传统依赖${NC}"
else
    # 其他版本，使用传统依赖
    QT_DEPS="libqt5core5a, libqt5network5, libqt5widgets5, libqt5gui5"
    SUFFIX="ubuntu-legacy"
    echo -e "${YELLOW}⚠️  未知Ubuntu版本，使用传统依赖${NC}"
fi

DEB_FILE="${PROJECT_NAME}_${VERSION}-${SUFFIX}_${ARCHITECTURE}.deb"

echo "目标DEB包: $DEB_FILE"
echo "Qt依赖: $QT_DEPS"
echo ""

# 临时修改build_deb.sh中的依赖
echo -e "${YELLOW}🔧 临时修改依赖配置...${NC}"

# 备份原始build_deb.sh
cp build_deb.sh build_deb.sh.backup

# 修改依赖行
sed -i "s/Depends: libqt5core5t64, libqt5network5t64, libqt5widgets5t64, libqt5gui5t64/Depends: $QT_DEPS/g" build_deb.sh

# 修改包名以区分版本
sed -i "s/DEB_FILE=\"\${PROJECT_NAME}_\${VERSION}_\${ARCHITECTURE}.deb\"/DEB_FILE=\"${DEB_FILE}\"/g" build_deb.sh

echo -e "${GREEN}✅ 依赖配置已更新${NC}"

# 运行构建
echo -e "${YELLOW}🔨 开始构建兼容版本...${NC}"
./build_deb.sh

# 恢复原始build_deb.sh
echo -e "${YELLOW}🔄 恢复原始配置...${NC}"
mv build_deb.sh.backup build_deb.sh

echo -e "${GREEN}🎉 兼容版本构建完成！${NC}"
echo ""
echo -e "${BLUE}📦 生成的包:${NC}"
ls -lh *${SUFFIX}*.deb

echo ""
echo -e "${BLUE}💡 使用说明:${NC}"
echo "Ubuntu 24.04+: 使用 *ubuntu24*.deb"
echo "Ubuntu 22.04/20.04: 使用 *ubuntu22*.deb"
echo "其他版本: 使用 *ubuntu-legacy*.deb"