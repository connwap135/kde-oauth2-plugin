# KDE OAuth2 C# 客户端

这是一个 C# .NET 项目，用于通过 SQLite 数据库直接访问 KDE 账户系统中存储的 OAuth2 凭证，并调用 gzweibo API。

## 特性

- ✅ **直接数据库访问**: 通过 SQLite 直接读取 KDE 账户数据库
- ✅ **完整的 CRUD 操作**: 创建、读取、更新、删除 OAuth2 账户
- ✅ **账户状态管理**: 启用、禁用账户功能
- ✅ **异步操作**: 所有数据库和网络操作都是异步的
- ✅ **完整的 OAuth2 支持**: 包括令牌刷新、用户信息获取等
- ✅ **事务安全**: 数据库操作使用事务确保数据一致性
- ✅ **命令行界面**: 提供丰富的命令行选项
- ✅ **错误处理**: 完善的异常处理和错误报告
- ✅ **跨平台**: 支持 .NET 8.0+

## 系统要求

- .NET 6.0 或更高版本
- 已配置的 KDE OAuth2 账户
- libaccounts-glib 数据库 (`~/.config/libaccounts-glib/accounts.db`)

## 安装

1. 确保已安装 .NET 6.0+:
```bash
dotnet --version
```

2. 进入项目目录:
```bash
cd csharp-oauth2-client
```

3. 还原 NuGet 包:
```bash
dotnet restore
```

4. 编译项目:
```bash
dotnet build
```

## 使用方法

### 基本用法

```bash
# 默认操作 - 获取凭证并测试
dotnet run

# 列出所有 OAuth2 账户
dotnet run -- --list

# 获取账户详细信息
dotnet run -- --get <account_id>

# 获取账户凭据
dotnet run -- --credentials <account_id>

# 测试账户认证
dotnet run -- --test <account_id>
```

### 账户管理 (CRUD 操作)

```bash
# 创建新账户
dotnet run -- --create <显示名称> <服务器> <客户端ID> <访问令牌>

# 删除账户
dotnet run -- --delete <account_id>

# 启用账户
dotnet run -- --enable <account_id>

# 禁用账户
dotnet run -- --disable <account_id>

# 更新访问令牌
dotnet run -- --update <account_id> <新访问令牌>

# 显示帮助
dotnet run -- --help
```

### 使用示例

```bash
# 创建微博测试账户
dotnet run -- --create "我的微博账户" "api.weibo.com" "123456789" "access_token_abc123"

# 查看所有账户
dotnet run -- --list

# 获取特定账户信息
dotnet run -- --get 5

# 禁用账户
dotnet run -- --disable 5

# 重新启用账户
dotnet run -- --enable 5

# 更新访问令牌
dotnet run -- --update 5 "new_access_token_xyz789"

# 删除账户
dotnet run -- --delete 5
```

### 命令行选项

| 选项 | 参数 | 说明 |
|------|------|------|
| `--list` | 无 | 列出所有OAuth2账户 |
| `--get` | `<account_id>` | 获取指定账户详细信息 |
| `--credentials` | `<account_id>` | 获取指定账户凭据 |
| `--test` | `<account_id>` | 测试指定账户令牌有效性 |
| `--create` | `<name> <server> <client_id> <token>` | 创建新OAuth2账户 |
| `--delete` | `<account_id>` | 删除指定账户 |
| `--enable` | `<account_id>` | 启用指定账户 |
| `--disable` | `<account_id>` | 禁用指定账户 |
| `--update` | `<account_id> <new_token>` | 更新账户访问令牌 |
| `--help` | 无 | 显示完整帮助信息 |

### 示例输出

#### 列出账户
```
=== KDE OAuth2 C# 客户端 ===

� OAuth2 账户列表:

ID    显示名称                 提供者             状态      
--------------------------------------------------
5     Test OAuth2 Account  gzweibo-oauth2  ✅ 启用    
7     我的微博账户               gzweibo-oauth2  ✅ 启用    

总计: 2 个账户
```

#### 创建账户
```
=== KDE OAuth2 C# 客户端 ===

➕ 创建新账户:

✅ 成功创建账户:
   账户ID: 8
   显示名称: 新测试账户
   服务器: api.example.com
   客户端ID: client123
```

#### 获取凭据
```
=== KDE OAuth2 C# 客户端 ===

�🚀 默认操作 - 获取凭证并测试:

📋 凭证信息:
   账户ID: 5
   显示名称: Test OAuth2 Account
   服务器: http://192.168.1.12:9007
   客户端ID: 10001
   用户名: testuser
   访问令牌: test_access_token_ab...
   有刷新令牌: 是
   过期时间: 3600 秒

🔍 测试令牌有效性...
✅ 令牌有效

💡 使用 --help 查看更多选项
```

## 项目结构

```
csharp-oauth2-client/
├── KDEOAuth2Client.csproj     # 项目文件
├── Program.cs                 # 主程序入口
├── Models.cs                  # 数据模型定义
├── KDEOAuth2Manager.cs        # KDE账户管理器
├── OAuth2ApiClient.cs         # OAuth2 API客户端
└── README.md                  # 说明文档
```

## 核心组件

### KDEOAuth2Manager

负责与 KDE 账户数据库交互:

```csharp
var manager = new KDEOAuth2Manager();

// 获取凭证
var credentials = await manager.GetCredentialsAsync();

// 列出账户
var accounts = await manager.GetAccountsAsync();

// 测试令牌
bool isValid = await manager.TestTokenValidityAsync(credentials);
```

### OAuth2ApiClient

用于调用 OAuth2 API:

```csharp
using var apiClient = new OAuth2ApiClient("http://192.168.1.12:9007");
apiClient.SetAccessToken(accessToken);

// 获取用户信息
var userInfo = await apiClient.GetUserInfoAsync();

// 刷新令牌
var newToken = await apiClient.RefreshTokenAsync(refreshToken, clientId);
```

## 在代码中使用

### 获取 OAuth2 凭证

```csharp
using KDEOAuth2Client.Services;
using KDEOAuth2Client.Models;

var manager = new KDEOAuth2Manager();
var credentials = await manager.GetCredentialsAsync();

if (credentials?.IsValid == true)
{
    Console.WriteLine($"访问令牌: {credentials.AccessToken}");
    Console.WriteLine($"服务器: {credentials.Server}");
}
```

### 调用 API

```csharp
using var apiClient = new OAuth2ApiClient(credentials.Server);
apiClient.SetAccessToken(credentials.AccessToken);

var response = await apiClient.GetUserInfoAsync();
if (response.Success)
{
    var userInfo = response.Data;
    Console.WriteLine($"用户: {userInfo.Name}");
    Console.WriteLine($"邮箱: {userInfo.Email}");
}
```

### 上传文件

```csharp
var uploadResponse = await apiClient.UploadFileAsync(
    "api/upload", 
    "/path/to/file.jpg", 
    "file"
);

if (uploadResponse.Success)
{
    Console.WriteLine("文件上传成功");
}
```

## 数据库结构

该客户端直接访问 KDE 账户数据库 (`~/.config/libaccounts-glib/accounts.db`)，主要使用以下表：

- **Accounts**: 存储账户基本信息
- **Settings**: 存储账户设置和凭证

## 错误处理

项目包含完整的错误处理机制：

```csharp
try
{
    var credentials = await manager.GetCredentialsAsync();
    // 使用凭证...
}
catch (FileNotFoundException)
{
    Console.WriteLine("KDE账户数据库不存在");
}
catch (Exception ex)
{
    Console.WriteLine($"发生错误: {ex.Message}");
}
```

# 更新刷新令牌
dotnet run -- --update 5 --refresh-token "new_refresh_token"

# 更新过期时间
dotnet run -- --update 5 --expires-in 7200

# 同时更新多个参数
dotnet run -- --update 5 --refresh-token "refresh_123" --expires-in 3600 --name "新名称"

# 更新访问令牌（会自动更新时间戳）
dotnet run -- --update 5 --access-token "new_access_token"

## 注意事项

1. **数据库权限**: 确保应用程序有权限读取 KDE 账户数据库
2. **令牌安全**: 访问令牌应该安全存储，避免泄露
3. **网络连接**: API 调用需要网络连接到 gzweibo 服务器
4. **令牌过期**: 定期检查令牌有效性，必要时刷新

## 开发

### 添加新的 API 端点

在 `OAuth2ApiClient.cs` 中添加新方法：

```csharp
public async Task<ApiResponse<MyData>> GetMyDataAsync()
{
    try
    {
        var response = await _httpClient.GetAsync($"{_baseUrl}/api/mydata");
        // 处理响应...
    }
    catch (Exception ex)
    {
        // 错误处理...
    }
}
```

### 扩展数据模型

在 `Models.cs` 中添加新的数据模型：

```csharp
public class MyData
{
    public string? Property1 { get; set; }
    public int Property2 { get; set; }
}
```

## 许可证

该项目是 KDE OAuth2 Plugin 项目的一部分，遵循相同的许可证。

## 相关项目

- [KDE OAuth2 Plugin](../): 主要的 KDE 插件项目
- [Python OAuth2 Client](../oauth2_credentials.py): Python 版本的客户端

## 贡献

欢迎提交 Issue 和 Pull Request！