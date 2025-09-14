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
├── csharp-oauth2-client/             # C# OAuth2 客户端
│   ├── KDEOAuth2Client.csproj       # .NET 项目文件
│   ├── Program.cs                   # 主程序入口
│   ├── KDEOAuth2Manager.cs          # OAuth2 管理器
│   ├── OAuth2ApiClient.cs           # API 客户端
│   ├── Models.cs                    # 数据模型
│   └── README.md                    # C# 客户端说明
├── gzweibo-oauth2.provider          # KDE账户提供者配置
├── gzweibo-oauth2.service           # KDE账户服务定义
├── oauth2_credentials.py            # Python OAuth2 客户端
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

## 🆕 C# OAuth2 客户端

本项目现在包含一个完整的 C# .NET 客户端，用于访问 KDE 账户系统中存储的 OAuth2 凭证。

### 特性

- ✅ **SQLite 直接访问**: 通过 SQLite 直接读取 KDE 账户数据库
- ✅ **异步操作**: 所有数据库和网络操作都是异步的  
- ✅ **完整的 OAuth2 支持**: 包括令牌刷新、用户信息获取等
- ✅ **命令行界面**: 提供丰富的命令行选项
- ✅ **跨平台**: 支持 .NET 8.0+

### 快速开始

```bash
# 进入 C# 客户端目录
cd csharp-oauth2-client

# 编译项目
dotnet build

# 运行默认操作
dotnet run

# 列出所有账户
dotnet run -- --list

# 获取用户信息
dotnet run -- --userinfo

# 查看帮助
dotnet run -- --help
```

### 使用示例

```bash
# 显示数据库统计
dotnet run -- --stats

# 测试令牌有效性
dotnet run -- --test

# 指定特定账户
dotnet run -- --account 5
```

详细说明请参考 [C# 客户端文档](csharp-oauth2-client/README.md)。

### 系统要求

- .NET 8.0 或更高版本
- 已配置的 KDE OAuth2 账户
- libaccounts-glib 数据库访问权限