#!/bin/bash

# KDE OAuth2 Plugin 卸载脚本
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

echo -e "${BLUE}🗑️  KDE OAuth2 Plugin 卸载脚本${NC}"
echo -e "${BLUE}=====================================${NC}"
echo "项目: ${PROJECT_NAME}"
echo "版本: ${VERSION}"
echo ""

# 检查是否以root权限运行
if [ "$EUID" -ne 0 ]; then
    echo -e "${RED}❌ 请使用sudo运行此脚本${NC}"
    echo "用法: sudo $0"
    exit 1
fi

echo -e "${YELLOW}🔍 检查当前安装状态...${NC}"

# 定义要删除的文件
PLUGIN_FILES=(
    "/usr/lib/x86_64-linux-gnu/qt5/plugins/kaccounts/ui/gzweibo_oauth2_plugin.so"
    "/usr/lib/x86_64-linux-gnu/qt5/plugins/kaccounts/ui/gzweibo_oauth2_plugin.so.json"
)

PROVIDER_FILES=(
    "/usr/share/accounts/providers/kde/gzweibo-oauth2.provider"
)

SERVICE_FILES=(
    "/usr/share/accounts/services/kde/gzweibo-oauth2.service"
    "/usr/share/accounts/services/kde/gzweibo-oauth2-email.service"
    "/usr/share/accounts/services/kde/gzweibo-oauth2-profile.service"
)

# 检查文件是否存在
echo -e "${YELLOW}📋 检查插件文件...${NC}"
for file in "${PLUGIN_FILES[@]}"; do
    if [ -f "$file" ]; then
        echo -e "${GREEN}✅ 找到: $file${NC}"
    else
        echo -e "${YELLOW}⚠️  未找到: $file${NC}"
    fi
done

echo -e "${YELLOW}📋 检查提供商配置文件...${NC}"
for file in "${PROVIDER_FILES[@]}"; do
    if [ -f "$file" ]; then
        echo -e "${GREEN}✅ 找到: $file${NC}"
    else
        echo -e "${YELLOW}⚠️  未找到: $file${NC}"
    fi
done

echo -e "${YELLOW}📋 检查服务配置文件...${NC}"
for file in "${SERVICE_FILES[@]}"; do
    if [ -f "$file" ]; then
        echo -e "${GREEN}✅ 找到: $file${NC}"
    else
        echo -e "${YELLOW}⚠️  未找到: $file${NC}"
    fi
done

echo ""

# 询问用户是否继续
read -p "是否继续卸载? (y/N): " -n 1 -r
echo
if [[ ! $REPLY =~ ^[Yy]$ ]]; then
    echo -e "${YELLOW}ℹ️  卸载已取消${NC}"
    exit 0
fi

echo -e "${YELLOW}🛑 停止KDE相关服务...${NC}"

# 停止KDE相关进程
pkill -f "kded5" 2>/dev/null || true
pkill -f "kaccounts" 2>/dev/null || true

echo -e "${GREEN}✅ 服务已停止${NC}"

echo -e "${YELLOW}🗑️  删除插件文件...${NC}"

# 删除插件文件
for file in "${PLUGIN_FILES[@]}"; do
    if [ -f "$file" ]; then
        echo "删除: $file"
        rm -f "$file"
        echo -e "${GREEN}✅ 已删除: $file${NC}"
    else
        echo -e "${YELLOW}⚠️  文件不存在: $file${NC}"
    fi
done

echo -e "${YELLOW}🗑️  删除提供商配置文件...${NC}"

# 删除提供商文件
for file in "${PROVIDER_FILES[@]}"; do
    if [ -f "$file" ]; then
        echo "删除: $file"
        rm -f "$file"
        echo -e "${GREEN}✅ 已删除: $file${NC}"
    else
        echo -e "${YELLOW}⚠️  文件不存在: $file${NC}"
    fi
done

echo -e "${YELLOW}🗑️  删除服务配置文件...${NC}"

# 删除服务文件
for file in "${SERVICE_FILES[@]}"; do
    if [ -f "$file" ]; then
        echo "删除: $file"
        rm -f "$file"
        echo -e "${GREEN}✅ 已删除: $file${NC}"
    else
        echo -e "${YELLOW}⚠️  文件不存在: $file${NC}"
    fi
done

echo -e "${YELLOW}🧹 清理用户数据...${NC}"

# 清理用户数据（可选）
USER_DATA_DIR="$HOME/.config/libaccounts-glib"
if [ -d "$USER_DATA_DIR" ]; then
    echo "清理用户账户数据..."
    # 注意：这里不会删除整个目录，只会删除相关的账户数据
    # 如果需要完全清理，请手动删除 $USER_DATA_DIR
    echo -e "${YELLOW}ℹ️  用户数据目录: $USER_DATA_DIR${NC}"
    echo -e "${YELLOW}ℹ️  如需清理，请手动删除相关账户${NC}"
fi

echo -e "${YELLOW}🔄 重启KDE服务...${NC}"

# 重启KDE服务
if command -v systemctl &> /dev/null; then
    systemctl --user restart plasma-kded.service 2>/dev/null || true
    systemctl --user restart kaccounts.service 2>/dev/null || true
fi

echo -e "${GREEN}✅ KDE服务已重启${NC}"

echo -e "${YELLOW}🔍 验证卸载...${NC}"

# 验证删除
echo -e "${YELLOW}📋 检查插件文件...${NC}"
for file in "${PLUGIN_FILES[@]}"; do
    if [ -f "$file" ]; then
        echo -e "${RED}❌ 仍存在: $file${NC}"
    else
        echo -e "${GREEN}✅ 已删除: $file${NC}"
    fi
done

echo -e "${YELLOW}📋 检查提供商配置文件...${NC}"
for file in "${PROVIDER_FILES[@]}"; do
    if [ -f "$file" ]; then
        echo -e "${RED}❌ 仍存在: $file${NC}"
    else
        echo -e "${GREEN}✅ 已删除: $file${NC}"
    fi
done

echo -e "${YELLOW}📋 检查服务配置文件...${NC}"
for file in "${SERVICE_FILES[@]}"; do
    if [ -f "$file" ]; then
        echo -e "${RED}❌ 仍存在: $file${NC}"
    else
        echo -e "${GREEN}✅ 已删除: $file${NC}"
    fi
done

echo ""
echo -e "${GREEN}🎉 卸载完成！${NC}"
echo ""
echo -e "${YELLOW}📝 后续操作:${NC}"
echo "1. 重启系统或注销/重新登录以完全生效"
echo "2. 如果使用KDE Online Accounts，请重新配置账户"
echo "3. 如需重新安装，请运行: make && sudo make install"
echo ""
echo -e "${BLUE}感谢使用 KDE OAuth2 Plugin！${NC}"