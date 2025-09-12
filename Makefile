# KDE OAuth2 Plugin Makefile
# 简化构建和打包过程

PROJECT_NAME = gzweibo-oauth2-plugin
VERSION = 1.0.0
ARCHITECTURE = amd64
DEB_FILE = $(PROJECT_NAME)_$(VERSION)_$(ARCHITECTURE).deb

.PHONY: all build clean deb install uninstall test help

all: deb

# 编译项目
build:
	@echo "🔨 编译项目..."
	@mkdir -p build
	@cd build && cmake .. && make

# 生成DEB包
deb: build
	@echo "📦 生成DEB包..."
	@./build_deb.sh

# 快速构建（清理+编译+打包）
quick:
	@echo "🚀 快速构建..."
	@./quick_build.sh

# 安装到系统
install: build
	@echo "📥 安装到系统..."
	@cd build && sudo make install
	@echo "♻️  重启服务..."
	@pkill kded5 || true
	@sleep 2

# 安装DEB包
install-deb: deb
	@echo "📥 安装DEB包..."
	@sudo apt install ./$(DEB_FILE)

# 卸载
uninstall:
	@echo "🗑️  卸载插件..."
	@sudo rm -f /usr/lib/x86_64-linux-gnu/qt5/plugins/kaccounts/ui/kde_oauth2_plugin.so
	@sudo rm -f /usr/lib/x86_64-linux-gnu/qt5/plugins/kaccounts/ui/kde_oauth2_plugin.so.json
	@sudo rm -f /usr/share/accounts/providers/kde/gzweibo-oauth2.provider
	@sudo rm -f /usr/share/accounts/services/kde/gzweibo-oauth2*.service
	@sudo rm -f /usr/bin/kde-oauth2-token
	@pkill kded5 || true

# 清理构建文件
clean:
	@echo "🧹 清理构建文件..."
	@rm -rf build debian *.deb

# 测试令牌获取
test:
	@echo "🧪 测试OAuth2令牌..."
	@./get_oauth_token.sh

# 查看已安装的配置
status:
	@echo "📋 检查安装状态..."
	@echo "插件文件:"
	@ls -la /usr/lib/x86_64-linux-gnu/qt5/plugins/kaccounts/ui/kde_oauth2_plugin.* 2>/dev/null || echo "未安装"
	@echo "配置文件:"
	@ls -la /usr/share/accounts/providers/kde/gzweibo-oauth2.provider 2>/dev/null || echo "未安装"
	@ls -la /usr/share/accounts/services/kde/gzweibo-oauth2*.service 2>/dev/null || echo "未安装"

# 重启KDE服务
restart:
	@echo "♻️  重启KDE服务..."
	@pkill kded5 || true
	@sleep 2
	@rm -rf ~/.cache/kaccounts || true

# 显示帮助
help:
	@echo "KDE OAuth2 Plugin - 构建系统"
	@echo "=============================="
	@echo ""
	@echo "可用命令:"
	@echo "  make build       - 编译项目"
	@echo "  make deb         - 生成DEB包"
	@echo "  make quick       - 快速构建（清理+编译+打包）"
	@echo "  make install     - 直接安装到系统"
	@echo "  make install-deb - 安装DEB包"
	@echo "  make uninstall   - 卸载插件"
	@echo "  make clean       - 清理构建文件"
	@echo "  make test        - 测试OAuth2令牌"
	@echo "  make status      - 检查安装状态"
	@echo "  make restart     - 重启KDE服务"
	@echo "  make help        - 显示此帮助"
	@echo ""
	@echo "示例："
	@echo "  make             - 默认构建DEB包"
	@echo "  make quick       - 一键式完整构建"
	@echo "  make install-deb - 构建并安装DEB包"