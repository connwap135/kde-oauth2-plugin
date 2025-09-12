# KDE OAuth2 Plugin - 打包脚本使用指南

## 🚀 自动化构建系统

本项目提供了多种打包方式，满足不同的使用需求。

## 📁 打包脚本文件

### 主要脚本
- **`build_deb.sh`** - 完整的DEB打包脚本（推荐）
- **`quick_build.sh`** - 快速构建脚本
- **`Makefile`** - Make构建系统

### 配置文件
- **`debian/*/DEBIAN/control`** - DEB包控制信息
- **`debian/*/DEBIAN/postinst`** - 安装后脚本
- **`debian/*/DEBIAN/prerm`** - 卸载前脚本

## 🎯 使用方法

### 方法1: 使用自动打包脚本（推荐）
```bash
# 完整构建过程
./build_deb.sh

# 快速构建（清理+编译+打包）
./quick_build.sh

# 指定版本号
./quick_build.sh 1.0.1
```

### 方法2: 使用Make系统
```bash
# 查看所有可用命令
make help

# 默认构建DEB包
make

# 快速构建
make quick

# 编译项目
make build

# 生成DEB包
make deb

# 安装到系统
make install

# 安装DEB包
make install-deb

# 清理构建文件
make clean

# 检查安装状态
make status

# 重启KDE服务
make restart
```

### 方法3: 手动构建
```bash
# 1. 编译项目
mkdir -p build && cd build
cmake .. && make && cd ..

# 2. 运行打包脚本
./build_deb.sh
```

## 📦 生成的文件

### DEB包
- **文件名**: `kde-oauth2-plugin_1.0.0_amd64.deb`
- **大小**: ~67 KB
- **架构**: amd64 (x86_64)

### 包含内容
```
插件文件:
├── /usr/lib/x86_64-linux-gnu/qt5/plugins/kaccounts/ui/kde_oauth2_plugin.so
└── /usr/lib/x86_64-linux-gnu/qt5/plugins/kaccounts/ui/kde_oauth2_plugin.so.json

配置文件:
├── /usr/share/accounts/providers/kde/gzweibo-oauth2.provider
├── /usr/share/accounts/services/kde/gzweibo-oauth2.service
├── /usr/share/accounts/services/kde/gzweibo-oauth2-email.service
└── /usr/share/accounts/services/kde/gzweibo-oauth2-profile.service

工具和文档:
├── /usr/bin/kde-oauth2-token
├── /usr/share/doc/kde-oauth2-plugin/readme.md
├── /usr/share/doc/kde-oauth2-plugin/PROJECT_COMPLETION_SUMMARY.md
├── /usr/share/doc/kde-oauth2-plugin/DEBUGGING_SUMMARY.md
└── /usr/share/doc/kde-oauth2-plugin/DEB_INSTALLATION_GUIDE.md
```

## 🔧 脚本特性

### `build_deb.sh` 特性
- ✅ **依赖检查** - 自动检查必要工具和库
- ✅ **彩色输出** - 友好的用户界面
- ✅ **错误处理** - 遇到错误立即停止
- ✅ **自动清理** - 清理临时文件和旧构建
- ✅ **完整验证** - 验证DEB包完整性
- ✅ **详细日志** - 显示构建过程每一步

### 自动处理的内容
1. **项目编译** - CMake配置和Make编译
2. **目录创建** - 标准DEB包目录结构
3. **文件复制** - 所有必要文件到正确位置
4. **权限设置** - 正确的文件和目录权限
5. **文档生成** - changelog、版权等标准文件
6. **包验证** - 检查包完整性和内容

## 🎛️ 自定义配置

### 修改版本号
编辑脚本开头的变量：
```bash
VERSION="1.0.0"        # 版本号
PROJECT_NAME="kde-oauth2-plugin"  # 项目名
ARCHITECTURE="amd64"   # 架构
```

### 修改依赖
编辑 `control` 文件的 `Depends` 字段：
```
Depends: libqt5core5a, libqt5network5, libqt5widgets5, libqt5gui5, libkaccounts2, libkf5i18n5
```

### 修改描述
编辑 `control` 文件的 `Description` 字段

## 🐛 故障排除

### 常见问题

#### 1. "缺少必要工具"
```bash
# 安装构建工具
sudo apt install build-essential cmake qtbase5-dev libkaccounts-dev libkf5i18n-dev dpkg-dev
```

#### 2. "编译失败"
```bash
# 清理重新编译
make clean
make build
```

#### 3. "DEB包构建失败"
```bash
# 检查文件权限和依赖
ls -la build/kde_oauth2_plugin.so
dpkg-deb --build debian/kde-oauth2-plugin . -v
```

#### 4. "安装失败"
```bash
# 检查依赖
sudo apt-get install -f

# 手动安装依赖
sudo apt install libqt5core5a libqt5network5 libkaccounts2
```

### 调试技巧

#### 查看详细构建过程
```bash
# 启用详细输出
./build_deb.sh 2>&1 | tee build.log
```

#### 检查包内容
```bash
# 查看包信息
dpkg-deb -I kde-oauth2-plugin_1.0.0_amd64.deb

# 查看包文件
dpkg-deb -c kde-oauth2-plugin_1.0.0_amd64.deb
```

#### 验证安装
```bash
# 检查安装状态
make status

# 测试插件
kde-oauth2-token
```

## 📋 最佳实践

### 1. 构建前检查
- 确保所有源文件都已保存
- 检查配置文件语法正确
- 确认依赖库已安装

### 2. 版本管理
- 每次发布前更新版本号
- 更新changelog记录变更
- 标记Git版本标签

### 3. 测试流程
```bash
# 1. 构建包
make quick

# 2. 在测试环境安装
sudo apt install ./kde-oauth2-plugin_1.0.0_amd64.deb

# 3. 测试功能
kde-oauth2-token

# 4. 卸载测试
sudo apt remove kde-oauth2-plugin
```

## 🎉 分发和部署

### 本地安装
```bash
sudo apt install ./kde-oauth2-plugin_1.0.0_amd64.deb
```

### 远程分发
```bash
# 复制到其他机器
scp kde-oauth2-plugin_1.0.0_amd64.deb user@remote-host:~

# 在远程机器安装
ssh user@remote-host
sudo apt install ./kde-oauth2-plugin_1.0.0_amd64.deb
```

---

**现在您拥有了完整的自动化构建系统，可以轻松生成专业的DEB安装包！** 🚀