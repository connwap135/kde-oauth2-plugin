#!/bin/bash

# KDE OAuth2 Plugin - 自动打包脚本
# 版本: 1.0.0
# 作者: KDE OAuth2 Plugin Developer

set -e  # 遇到错误立即退出

# 颜色定义
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# 项目信息
PROJECT_NAME="gzweibo-oauth2-plugin"
VERSION="1.0.0"
ARCHITECTURE="amd64"
BUILD_DIR="build"
DEB_DIR="debian"
PACKAGE_DIR="${DEB_DIR}/${PROJECT_NAME}"

echo -e "${BLUE}🚀 KDE OAuth2 Plugin - 自动打包脚本${NC}"
echo -e "${BLUE}======================================${NC}"
echo "项目: ${PROJECT_NAME}"
echo "版本: ${VERSION}"
echo "架构: ${ARCHITECTURE}"
echo ""

# 检查必要工具
echo -e "${YELLOW}🔍 检查必要工具...${NC}"
REQUIRED_TOOLS=("cmake" "make" "g++" "dpkg-deb" "gzip")
for tool in "${REQUIRED_TOOLS[@]}"; do
    if ! command -v "$tool" &> /dev/null; then
        echo -e "${RED}❌ 缺少必要工具: $tool${NC}"
        echo "请安装: sudo apt install $tool"
        exit 1
    fi
    echo -e "${GREEN}✅ $tool${NC}"
done

# 检查Qt5和KDE开发库
echo -e "${YELLOW}🔍 检查开发库...${NC}"
if ! dpkg -l | grep -q "qtbase5-dev"; then
    echo -e "${RED}❌ 缺少Qt5开发库${NC}"
    echo "请安装: sudo apt install qtbase5-dev libkaccounts-dev libkf5i18n-dev"
    exit 1
fi
echo -e "${GREEN}✅ Qt5和KDE开发库${NC}"

# 清理之前的构建
echo -e "${YELLOW}🧹 清理之前的构建...${NC}"
if [ -d "$BUILD_DIR" ]; then
    echo "删除构建目录: $BUILD_DIR"
    rm -rf "$BUILD_DIR"
fi

if [ -d "$DEB_DIR" ]; then
    echo "删除DEB目录: $DEB_DIR"
    rm -rf "$DEB_DIR"
fi

if [ -f "${PROJECT_NAME}_${VERSION}_${ARCHITECTURE}.deb" ]; then
    echo "删除旧的DEB包"
    rm -f "${PROJECT_NAME}_${VERSION}_${ARCHITECTURE}.deb"
fi

# 创建构建目录
echo -e "${YELLOW}📁 创建构建目录...${NC}"
mkdir -p "$BUILD_DIR"

# 编译项目
echo -e "${YELLOW}🔨 编译项目...${NC}"
cd "$BUILD_DIR"
cmake .. || {
    echo -e "${RED}❌ CMake配置失败${NC}"
    exit 1
}

make || {
    echo -e "${RED}❌ 编译失败${NC}"
    exit 1
}
echo -e "${GREEN}✅ 编译成功${NC}"
cd ..

# 检查编译产物
if [ ! -f "$BUILD_DIR/gzweibo_oauth2_plugin.so" ]; then
    echo -e "${RED}❌ 找不到编译产物: gzweibo_oauth2_plugin.so${NC}"
    exit 1
fi

# 创建DEB包目录结构
echo -e "${YELLOW}📦 创建DEB包目录结构...${NC}"
mkdir -p "${PACKAGE_DIR}/DEBIAN"
mkdir -p "${PACKAGE_DIR}/usr/lib/x86_64-linux-gnu/qt5/plugins/kaccounts/ui"
mkdir -p "${PACKAGE_DIR}/usr/share/accounts/providers/kde"
mkdir -p "${PACKAGE_DIR}/usr/share/accounts/services/kde"
mkdir -p "${PACKAGE_DIR}/usr/share/doc/${PROJECT_NAME}"
mkdir -p "${PACKAGE_DIR}/usr/bin"

# 读取当前control文件内容（如果存在）
echo -e "${YELLOW}📝 创建control文件...${NC}"

# 检查是否已有control文件
if [ -f "${PACKAGE_DIR}/DEBIAN/control" ]; then
    echo "使用现有的control文件"
else
    # 创建control文件
    cat > "${PACKAGE_DIR}/DEBIAN/control" << EOF
Package: ${PROJECT_NAME}
Version: ${VERSION}
Section: kde
Priority: optional
Architecture: ${ARCHITECTURE}
Depends: libqt5core5a, libqt5network5, libqt5widgets5, libqt5gui5, libkaccounts2, libkf5i18n5
Maintainer: KDE OAuth2 Plugin Developer <developer@example.com>
Description: KDE Online Accounts OAuth2 Plugin
 A custom OAuth2 authentication plugin for KDE Online Accounts system.
 This plugin enables OAuth2 authentication with custom servers and
 supports automatic authorization code capture through local callback server.
 .
 Features:
  - Complete OAuth2 authorization code flow
  - Automatic authorization code capture
  - Integration with KDE Online Accounts
  - Support for Microsoft .NET IdentityServer Claims format
  - Modern dual-mode authentication interface
  - Comprehensive debugging and logging

EOF
fi

# 创建postinst脚本
echo -e "${YELLOW}📝 创建postinst脚本...${NC}"
cat > "${PACKAGE_DIR}/DEBIAN/postinst" << 'EOF'
#!/bin/bash
# Post-installation script for kde-oauth2-plugin

echo "Configuring kde-oauth2-plugin..."

# 重启KDE相关服务
if pgrep -x "kded5" > /dev/null; then
    echo "Restarting kded5 service..."
    pkill kded5 || true
    sleep 2
fi

# 清理账户缓存
if [ -d "/home"* ]; then
    for user_home in /home/*; do
        if [ -d "$user_home/.cache/kaccounts" ]; then
            echo "Clearing kaccounts cache for $(basename $user_home)..."
            rm -rf "$user_home/.cache/kaccounts" || true
        fi
    done
fi

echo "kde-oauth2-plugin installation completed successfully!"
echo ""
echo "To use the plugin:"
echo "1. Open System Settings -> Online Accounts"
echo "2. Click 'Add Account' -> Select 'gzweibo OAuth2 Server'"
echo "3. Configure your OAuth2 server settings"
echo "4. Enjoy one-click OAuth2 authentication!"

exit 0
EOF

# 创建prerm脚本
echo -e "${YELLOW}📝 创建prerm脚本...${NC}"
cat > "${PACKAGE_DIR}/DEBIAN/prerm" << 'EOF'
#!/bin/bash
# Pre-removal script for kde-oauth2-plugin

echo "Preparing to remove kde-oauth2-plugin..."

# 停止KDE相关服务
if pgrep -x "kded5" > /dev/null; then
    echo "Stopping kded5 service..."
    pkill kded5 || true
    sleep 2
fi

exit 0
EOF

# 复制文件
echo -e "${YELLOW}📋 复制文件...${NC}"

# 复制插件文件
echo "复制插件文件..."
cp "$BUILD_DIR/gzweibo_oauth2_plugin.so" "${PACKAGE_DIR}/usr/lib/x86_64-linux-gnu/qt5/plugins/kaccounts/ui/"
cp "src/kdeoauth2plugin.json" "${PACKAGE_DIR}/usr/lib/x86_64-linux-gnu/qt5/plugins/kaccounts/ui/gzweibo_oauth2_plugin.so.json"

# 复制配置文件
echo "复制配置文件..."
cp "gzweibo-oauth2.provider" "${PACKAGE_DIR}/usr/share/accounts/providers/kde/"
cp gzweibo-oauth2*.service "${PACKAGE_DIR}/usr/share/accounts/services/kde/"

# 复制工具脚本
echo "复制工具脚本..."
cp "get_oauth_token.sh" "${PACKAGE_DIR}/usr/bin/kde-oauth2-token"

# 复制文档
echo "复制文档..."
cp readme.md "${PACKAGE_DIR}/usr/share/doc/${PROJECT_NAME}/"
if [ -f "PROJECT_COMPLETION_SUMMARY.md" ]; then
    cp "PROJECT_COMPLETION_SUMMARY.md" "${PACKAGE_DIR}/usr/share/doc/${PROJECT_NAME}/"
fi
if [ -f "DEBUGGING_SUMMARY.md" ]; then
    cp "DEBUGGING_SUMMARY.md" "${PACKAGE_DIR}/usr/share/doc/${PROJECT_NAME}/"
fi
if [ -f "DEB_INSTALLATION_GUIDE.md" ]; then
    cp "DEB_INSTALLATION_GUIDE.md" "${PACKAGE_DIR}/usr/share/doc/${PROJECT_NAME}/"
fi

# 创建版权文件
echo "创建版权文件..."
cat > "${PACKAGE_DIR}/usr/share/doc/${PROJECT_NAME}/copyright" << 'EOF'
Copyright (C) 2025 KDE OAuth2 Plugin Developer

This package is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This package is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this package; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA

On Debian systems, the complete text of the GNU General Public License
version 2 can be found in `/usr/share/common-licenses/GPL-2'.

The Debian packaging is:
    Copyright (C) 2025 KDE OAuth2 Plugin Developer
and is licensed under the GPL version 2, see above.
EOF

# 创建changelog
echo "创建changelog..."
cat > "${PACKAGE_DIR}/usr/share/doc/${PROJECT_NAME}/changelog.Debian" << EOF
${PROJECT_NAME} (${VERSION}) unstable; urgency=low

  * Initial release of KDE OAuth2 Plugin
  * Features:
    - Complete OAuth2 authorization code flow implementation
    - Automatic authorization code capture via local callback server
    - Integration with KDE Online Accounts system
    - Support for Microsoft .NET IdentityServer Claims format
    - Modern dual-mode authentication interface (automatic/manual)
    - Comprehensive debugging and logging capabilities
    - Support for gzweibo OAuth2 servers
    - User-friendly error handling and recovery
    - Chinese localization support
    - Detailed documentation and usage guides

 -- KDE OAuth2 Plugin Developer <developer@example.com>  $(date -R)
EOF

# 压缩changelog
echo "压缩changelog..."
gzip -9 "${PACKAGE_DIR}/usr/share/doc/${PROJECT_NAME}/changelog.Debian"

# 设置权限
echo -e "${YELLOW}🔐 设置权限...${NC}"
find "${PACKAGE_DIR}" -type f -exec chmod 644 {} \;
find "${PACKAGE_DIR}" -type d -exec chmod 755 {} \;

# 可执行文件权限
chmod 755 "${PACKAGE_DIR}/usr/lib/x86_64-linux-gnu/qt5/plugins/kaccounts/ui/gzweibo_oauth2_plugin.so"
chmod 755 "${PACKAGE_DIR}/usr/bin/kde-oauth2-token"
chmod 755 "${PACKAGE_DIR}/DEBIAN/postinst"
chmod 755 "${PACKAGE_DIR}/DEBIAN/prerm"

# 构建DEB包
echo -e "${YELLOW}📦 构建DEB包...${NC}"
DEB_FILE="${PROJECT_NAME}_${VERSION}_${ARCHITECTURE}.deb"

dpkg-deb --build "${PACKAGE_DIR}" "${DEB_FILE}" || {
    echo -e "${RED}❌ DEB包构建失败${NC}"
    exit 1
}

# 验证DEB包
echo -e "${YELLOW}🔍 验证DEB包...${NC}"
if [ ! -f "${DEB_FILE}" ]; then
    echo -e "${RED}❌ DEB包未生成${NC}"
    exit 1
fi

# 显示包信息
echo -e "${GREEN}✅ DEB包构建成功！${NC}"
echo ""
echo -e "${BLUE}📦 包信息:${NC}"
dpkg-deb -I "${DEB_FILE}"
echo ""

echo -e "${BLUE}📁 包大小:${NC}"
ls -lh "${DEB_FILE}"
echo ""

echo -e "${BLUE}📋 包内容:${NC}"
echo "主要文件:"
dpkg-deb -c "${DEB_FILE}" | grep -E "\.(so|json|provider|service)$"
echo ""

# 清理临时目录
echo -e "${YELLOW}🧹 清理临时目录...${NC}"
rm -rf "${DEB_DIR}"

echo -e "${GREEN}🎉 打包完成！${NC}"
echo ""
echo -e "${BLUE}安装命令:${NC}"
echo "sudo apt install ./${DEB_FILE}"
echo ""
echo -e "${BLUE}或者:${NC}"
echo "sudo dpkg -i ${DEB_FILE}"
echo "sudo apt-get install -f  # 如果有依赖问题"
echo ""
echo -e "${GREEN}享受您的KDE OAuth2插件！${NC} 🚀"