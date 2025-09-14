# KDE OAuth2 Plugin

一个KDE桌面环境的OAuth2账户认证插件。

## 核心文件结构

```
├── src/                              # 插件源代码
│   ├── kdeoauth2plugin.cpp          # 主要插件实现
│   ├── kdeoauth2plugin.h            # 插件头文件
│   └── kdeoauth2plugin.json         # 插件元数据
├── build/                            # 编译输出
│   └── gzweibo_oauth2_plugin.so     # 编译后的插件库
├── gzweibo-oauth2.provider          # KDE账户提供者配置
├── gzweibo-oauth2.service           # KDE账户服务定义
├── CMakeLists.txt                   # CMake构建配置
├── Makefile                         # 简化构建
└── honest_test.py                   # 真实性测试脚本
```

## 插件特点

- ✅ **自包含**: 不需要外部Python服务依赖
- ✅ **完整集成**: 直接集成到KDE账户管理系统
- ✅ **内置DBus接口**: 提供完整的多语言调用接口
- ✅ **标准KDE插件**: 遵循KDE插件架构规范

## 编译安装

```bash
make clean && make
```

## 真实性测试

```bash
python3 honest_test.py
```

该脚本会诚实地告诉您当前环境是否能启动真正的KDE对话框。

## 架构说明

此插件是一个完整的KDE插件，**不依赖任何外部服务**：

1. **内置DBus适配器** - 直接在C++插件中实现
2. **标准KDE集成** - 通过KAccounts框架工作  
3. **独立OAuth2流程** - 包含完整的认证逻辑
4. **多语言接口支持** - 通过内置DBus接口

在有KDE桌面环境时，可以通过系统设置中的账户管理来使用此插件。