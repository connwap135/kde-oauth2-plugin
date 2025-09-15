using System.Text.Json;
using KDEOAuth2Client.Models;
using KDEOAuth2Client.Services;

namespace KDEOAuth2Client
{
    class Program
    {
        static async Task Main(string[] args)
        {
            Console.WriteLine("=== KDE OAuth2 C# 客户端 ===");
            Console.WriteLine();

            // 解析命令行参数
            var options = ParseArgs(args);

            try
            {
                var manager = new KDEOAuth2Manager();

                // 检查数据库是否存在
                if (!manager.IsDatabaseAvailable())
                {
                    Console.WriteLine("❌ KDE账户数据库不存在!");
                    Console.WriteLine($"   数据库路径: {manager.GetDatabasePath()}");
                    Console.WriteLine("   请确保您已在KDE系统设置中配置了OAuth2账户。");
                    return;
                }

                // 根据选项执行不同操作
                switch (options.Action)
                {
                    case "list":
                        await ListAccountsAsync(manager);
                        break;
                    case "stats":
                        await ShowStatsAsync(manager);
                        break;
                    case "test":
                        await TestTokenAsync(manager, options.AccountId);
                        break;
                    case "userinfo":
                        await GetUserInfoAsync(manager, options.AccountId);
                        break;
                    case "refresh":
                        await RefreshTokenAsync(manager, options.AccountId);
                        break;
                    case "create":
                    await CreateAccountAsync(manager, options);
                    break;
                case "delete":
                    await DeleteAccountAsync(manager, options.AccountId);
                    break;
                case "enable":
                    await SetAccountEnabledAsync(manager, options.AccountId, true);
                    break;
                case "disable":
                    await SetAccountEnabledAsync(manager, options.AccountId, false);
                    break;
                case "update":
                    await UpdateAccountAsync(manager, options);
                    break;
                case "status":
                    await CheckTokenStatusAsync(manager, options.AccountId);
                    break;
                case "help":
                    ShowHelp();
                    break;
                default:
                    Console.WriteLine("🚀 默认操作 - 获取凭证并测试:");
                    
                    // 获取第一个账户进行演示
                    var accounts = await manager.GetAccountsAsync();
                    if (accounts.Any())
                    {
                        var account = accounts.First();
                        var credentials = await manager.GetCredentialsAsync(account.Id);
                        if (credentials != null)
                        {
                            Console.WriteLine($"📋 凭证信息:");
                            Console.WriteLine($"   账户ID: {credentials.AccountId}");
                            Console.WriteLine($"   显示名称: {credentials.DisplayName}");
                            Console.WriteLine($"   服务器: {credentials.Server}");
                            Console.WriteLine($"   客户端ID: {credentials.ClientId}");
                            Console.WriteLine($"   用户名: {credentials.Username}");
                            
                            if (!string.IsNullOrEmpty(credentials.AccessToken))
                            {
                                string displayToken = credentials.AccessToken.Length > 20 
                                    ? credentials.AccessToken.Substring(0, 20) + "..." 
                                    : credentials.AccessToken;
                                Console.WriteLine($"   访问令牌: {displayToken}");
                            }
                            
                            Console.WriteLine($"   有刷新令牌: {(!string.IsNullOrEmpty(credentials.RefreshToken) ? "是" : "否")}");
                            if (credentials.ExpiresIn > 0)
                                Console.WriteLine($"   过期时间: {credentials.ExpiresIn} 秒");
                            
                            // 简单的令牌验证
                            Console.WriteLine("\n🔍 测试令牌有效性...");
                            if (!string.IsNullOrEmpty(credentials.Server) && !string.IsNullOrEmpty(credentials.AccessToken))
                            {
                                var apiClient = new OAuth2ApiClient(credentials.Server);
                                apiClient.SetAccessToken(credentials.AccessToken);
                                var userInfoResponse = await apiClient.GetUserInfoAsync();
                                if (userInfoResponse.Success && userInfoResponse.Data != null)
                                {
                                    Console.WriteLine("✅ 令牌有效");
                                }
                                else
                                {
                                    Console.WriteLine("❌ 令牌无效");
                                }
                            }
                            else
                            {
                                Console.WriteLine("⚠️ 无法测试令牌（缺少服务器或令牌信息）");
                            }
                        }
                    }
                    else
                    {
                        Console.WriteLine("❌ 未找到任何账户");
                    }
                    
                    Console.WriteLine("\n💡 使用 --help 查看更多选项");
                    break;
                }
            }
            catch (Exception ex)
            {
                Console.WriteLine($"❌ 发生错误: {ex.Message}");
                if (args.Contains("--verbose"))
                {
                    Console.WriteLine($"详细错误: {ex}");
                }
            }
        }

    static CommandOptions ParseArgs(string[] args)
    {
        var options = new CommandOptions();
        
        for (int i = 0; i < args.Length; i++)
        {
            switch (args[i])
            {
                case "--list":
                    options.Action = "list";
                    break;
                case "--get":
                    options.Action = "get";
                    if (i + 1 < args.Length && int.TryParse(args[++i], out int getId))
                        options.AccountId = getId;
                    break;
                case "--credentials":
                    options.Action = "credentials";
                    if (i + 1 < args.Length && int.TryParse(args[++i], out int credId))
                        options.AccountId = credId;
                    break;
                case "--test":
                    options.Action = "test";
                    if (i + 1 < args.Length && int.TryParse(args[++i], out int testId))
                        options.AccountId = testId;
                    break;
                case "--create":
                    options.Action = "create";
                    if (i + 1 < args.Length) 
                        options.DisplayName = args[++i];
                    if (i + 1 < args.Length) 
                        options.Server = args[++i];
                    if (i + 1 < args.Length) 
                        options.ClientId = args[++i];
                    if (i + 1 < args.Length) 
                        options.AccessToken = args[++i];
                    if (i + 1 < args.Length && int.TryParse(args[++i], out int expiresIn))
                        options.ExpiresIn = expiresIn;
                    break;
                case "--delete":
                    options.Action = "delete";
                    if (i + 1 < args.Length && int.TryParse(args[++i], out int deleteId))
                        options.AccountId = deleteId;
                    break;
                case "--enable":
                    options.Action = "enable";
                    if (i + 1 < args.Length && int.TryParse(args[++i], out int enableId))
                        options.AccountId = enableId;
                    break;
                case "--disable":
                    options.Action = "disable";
                    if (i + 1 < args.Length && int.TryParse(args[++i], out int disableId))
                        options.AccountId = disableId;
                    break;
                case "--status":
                    options.Action = "status";
                    if (i + 1 < args.Length && int.TryParse(args[++i], out int statusId))
                        options.AccountId = statusId;
                    break;
                case "--update":
                    options.Action = "update";
                    if (i + 1 < args.Length && int.TryParse(args[++i], out int updateId))
                        options.AccountId = updateId;
                    break;
                case "--name":
                    if (i + 1 < args.Length)
                        options.DisplayName = args[++i];
                    break;
                case "--server":
                    if (i + 1 < args.Length)
                        options.Server = args[++i];
                    break;
                case "--client-id":
                    if (i + 1 < args.Length)
                        options.ClientId = args[++i];
                    break;
                case "--access-token":
                    if (i + 1 < args.Length)
                        options.AccessToken = args[++i];
                    break;
                case "--refresh-token":
                    if (i + 1 < args.Length)
                        options.RefreshToken = args[++i];
                    break;
                case "--expires-in":
                    if (i + 1 < args.Length && int.TryParse(args[++i], out int expiresInUpdate))
                        options.ExpiresIn = expiresInUpdate;
                    break;
                case "--help":
                    options.Action = "help";
                    break;
            }
        }
        
        return options;
    }        static void ShowHelp()
    {
        Console.WriteLine("KDE OAuth2 Client");
        Console.WriteLine("使用方法:");
        Console.WriteLine("  --list                                       列出所有账户");
        Console.WriteLine("  --get <account_id>                          获取账户信息");
        Console.WriteLine("  --credentials <account_id>                  获取账户凭据");
        Console.WriteLine("  --test <account_id>                         测试账户认证");
        Console.WriteLine("  --status [account_id]                       检查令牌状态（可选指定账户ID）");
        Console.WriteLine("  --create <name> <server> <client_id> <token> [expires_in] 创建新账户");
        Console.WriteLine("  --delete <account_id>                       删除账户");
        Console.WriteLine("  --enable <account_id>                       启用账户");
        Console.WriteLine("  --disable <account_id>                      禁用账户");
        Console.WriteLine("  --update <account_id> [options]             更新账户信息");
        Console.WriteLine("    可用选项: --name, --server, --client-id, --access-token, --refresh-token, --expires-in");
        Console.WriteLine("  --help                                      显示帮助信息");
        Console.WriteLine();
        Console.WriteLine("示例:");
        Console.WriteLine("  dotnet run -- --list");
        Console.WriteLine("  dotnet run -- --get 1");
        Console.WriteLine("  dotnet run -- --status      # 检查所有账户令牌状态");
        Console.WriteLine("  dotnet run -- --status 1    # 检查指定账户令牌状态");
        Console.WriteLine("  dotnet run -- --create \"测试账户\" \"api.weibo.com\" \"123\" \"abc123\"");
        Console.WriteLine("  dotnet run -- --create \"测试账户\" \"api.weibo.com\" \"123\" \"abc123\" 3600  # 带过期时间");
        Console.WriteLine("  dotnet run -- --delete 1");
        Console.WriteLine("  dotnet run -- --update 1 --access-token \"new_token_123\"");
        Console.WriteLine("  dotnet run -- --update 1 --refresh-token \"refresh_123\" --expires-in 7200");
        Console.WriteLine("  dotnet run -- --update 1 --name \"新名称\" --server \"new.api.com\"");
    }

        static async Task ListAccountsAsync(KDEOAuth2Manager manager)
        {
            Console.WriteLine("📋 OAuth2 账户列表:");
            Console.WriteLine();

            var accounts = await manager.GetAccountsAsync();
            if (accounts.Count == 0)
            {
                Console.WriteLine("   没有找到OAuth2账户。");
                return;
            }

            Console.WriteLine($"{"ID",-5} {"显示名称",-20} {"提供者",-15} {"状态",-8}");
            Console.WriteLine(new string('-', 50));

            foreach (var account in accounts)
            {
                string status = account.Enabled ? "✅ 启用" : "❌ 禁用";
                Console.WriteLine($"{account.Id,-5} {account.DisplayName,-20} {account.Provider,-15} {status,-8}");
            }

            Console.WriteLine();
            Console.WriteLine($"总计: {accounts.Count} 个账户");
        }

        static async Task ShowStatsAsync(KDEOAuth2Manager manager)
        {
            Console.WriteLine("📊 数据库统计信息:");
            Console.WriteLine();

            var stats = await manager.GetDatabaseStatsAsync();

            foreach (var stat in stats)
            {
                string value = stat.Value?.ToString() ?? "N/A";
                string key = stat.Key.Replace("_", " ").ToUpper();
                Console.WriteLine($"   {key}: {value}");
            }
        }

        static async Task TestTokenAsync(KDEOAuth2Manager manager, int? accountId)
        {
            Console.WriteLine("🔍 测试令牌有效性:");
            Console.WriteLine();

            var credentials = await manager.GetCredentialsAsync(accountId);
            if (credentials == null)
            {
                Console.WriteLine("❌ 未找到有效的OAuth2凭证");
                return;
            }

            Console.WriteLine($"   账户ID: {credentials.AccountId}");
            Console.WriteLine($"   显示名称: {credentials.DisplayName ?? "未设置"}");
            Console.WriteLine($"   服务器: {credentials.Server}");
            Console.WriteLine();

            bool isValid = await manager.TestTokenValidityAsync(credentials);
            if (isValid)
            {
                Console.WriteLine("✅ 令牌有效");
            }
            else
            {
                Console.WriteLine("❌ 令牌无效或已过期");
            }
        }

        static async Task GetUserInfoAsync(KDEOAuth2Manager manager, int? accountId)
        {
            Console.WriteLine("👤 获取用户信息:");
            Console.WriteLine();

            var credentials = await manager.GetCredentialsAsync(accountId);
            if (credentials == null)
            {
                Console.WriteLine("❌ 未找到有效的OAuth2凭证");
                return;
            }

            using var apiClient = new OAuth2ApiClient(credentials.Server!);
            apiClient.SetAccessToken(credentials.AccessToken!);

            var response = await apiClient.GetUserInfoAsync();
            if (response.Success && response.Data != null)
            {
                var userInfo = response.Data;
                Console.WriteLine("✅ 用户信息获取成功:");
                Console.WriteLine($"   用户ID: {userInfo.Sub ?? "未设置"}");
                Console.WriteLine($"   用户名: {userInfo.Name ?? "未设置"}");
                Console.WriteLine($"   邮箱: {userInfo.Email ?? "未设置"}");
                Console.WriteLine($"   昵称: {userInfo.PreferredUsername ?? "未设置"}");
                Console.WriteLine($"   姓: {userInfo.GivenName ?? "未设置"}");
                Console.WriteLine($"   名: {userInfo.FamilyName ?? "未设置"}");
                Console.WriteLine($"   头像: {userInfo.Picture ?? "未设置"}");
            }
            else
            {
                Console.WriteLine($"❌ 获取用户信息失败: {response.Message}");
            }
        }

        static async Task RefreshTokenAsync(KDEOAuth2Manager manager, int? accountId)
        {
            Console.WriteLine("🔄 刷新访问令牌:");
            Console.WriteLine();

            var credentials = await manager.GetCredentialsAsync(accountId);
            if (credentials == null)
            {
                Console.WriteLine("❌ 未找到有效的OAuth2凭证");
                return;
            }

            if (string.IsNullOrEmpty(credentials.RefreshToken))
            {
                Console.WriteLine("❌ 没有可用的刷新令牌");
                return;
            }

            using var apiClient = new OAuth2ApiClient(credentials.Server!);
            var response = await apiClient.RefreshTokenAsync(credentials.AccessToken!, credentials.RefreshToken!, credentials.ClientId ?? "");

            if (response.Success && response.Data != null)
            {
                Console.WriteLine("✅ 令牌刷新成功");
                Console.WriteLine($"   新访问令牌: {response.Data.AccessToken?[..20]}...");
                Console.WriteLine($"   过期时间: {response.Data.ExpiresIn} 秒");
                Console.WriteLine();
                Console.WriteLine("注意: 新令牌需要手动更新到KDE账户系统中");
            }
            else
            {
                Console.WriteLine($"❌ 令牌刷新失败: {response.Message}");
            }
        }

        static async Task DefaultActionAsync(KDEOAuth2Manager manager)
        {
            Console.WriteLine("🚀 默认操作 - 获取凭证并测试:");
            Console.WriteLine();

            var credentials = await manager.GetCredentialsAsync();
            if (credentials == null)
            {
                Console.WriteLine("❌ 未找到有效的OAuth2凭证");
                Console.WriteLine("   请在KDE系统设置中配置OAuth2账户。");
                return;
            }

            // 显示凭证信息
            Console.WriteLine("📋 凭证信息:");
            Console.WriteLine($"   账户ID: {credentials.AccountId}");
            Console.WriteLine($"   显示名称: {credentials.DisplayName ?? "未设置"}");
            Console.WriteLine($"   服务器: {credentials.Server}");
            Console.WriteLine($"   客户端ID: {credentials.ClientId ?? "未设置"}");
            Console.WriteLine($"   用户名: {credentials.Username ?? "未设置"}");
            Console.WriteLine($"   访问令牌: {credentials.AccessToken?[..20]}...");
            Console.WriteLine($"   有刷新令牌: {(!string.IsNullOrEmpty(credentials.RefreshToken) ? "是" : "否")}");

            if (credentials.ExpiresIn > 0)
            {
                Console.WriteLine($"   过期时间: {credentials.ExpiresIn} 秒");
            }

            Console.WriteLine();

            // 测试令牌
            Console.WriteLine("🔍 测试令牌有效性...");
            bool isValid = await manager.TestTokenValidityAsync(credentials);
            Console.WriteLine(isValid ? "✅ 令牌有效" : "❌ 令牌无效");

            if (isValid)
            {
                Console.WriteLine();
                Console.WriteLine("👤 获取用户信息...");

                using var apiClient = new OAuth2ApiClient(credentials.Server!);
                apiClient.SetAccessToken(credentials.AccessToken!);

                var userResponse = await apiClient.GetUserInfoAsync();
                if (userResponse.Success && userResponse.Data != null)
                {
                    var userInfo = userResponse.Data;
                    Console.WriteLine($"   用户: {userInfo.Name ?? userInfo.PreferredUsername ?? "未知"}");
                    Console.WriteLine($"   邮箱: {userInfo.Email ?? "未设置"}");
                }
                else
                {
                    Console.WriteLine($"   获取用户信息失败: {userResponse.Message}");
                }
            }

            Console.WriteLine();
            Console.WriteLine("💡 使用 --help 查看更多选项");
        }

        static async Task CreateAccountAsync(KDEOAuth2Manager manager, CommandOptions options)
        {
            Console.WriteLine("➕ 创建新账户:");
            Console.WriteLine();

            if (string.IsNullOrEmpty(options.DisplayName))
            {
                Console.WriteLine("❌ 请使用 --name 指定账户显示名称");
                return;
            }

            if (string.IsNullOrEmpty(options.Server))
            {
                Console.WriteLine("❌ 请使用 --server 指定服务器地址");
                return;
            }

            var credentials = new OAuth2Credentials
            {
                Server = options.Server,
                ClientId = options.ClientId ?? "10001",
                AccessToken = options.AccessToken ?? $"demo_token_{DateTime.Now.Ticks}",
                Username = "demo_user",
                ExpiresIn = options.ExpiresIn ?? 3600  // 默认1小时过期
            };

            try
            {
                int accountId = await manager.CreateAccountAsync(options.DisplayName, credentials);
                Console.WriteLine($"✅ 成功创建账户:");
                Console.WriteLine($"   账户ID: {accountId}");
                Console.WriteLine($"   显示名称: {options.DisplayName}");
                Console.WriteLine($"   服务器: {options.Server}");
                Console.WriteLine($"   客户端ID: {credentials.ClientId}");
                Console.WriteLine($"   过期时间: {credentials.ExpiresIn} 秒");
                
                // 显示过期信息
                if (credentials.ExpiresIn > 0)
                {
                    var expiresAt = DateTime.Now.AddSeconds(credentials.ExpiresIn);
                    Console.WriteLine($"   将于: {expiresAt:yyyy-MM-dd HH:mm:ss} 过期");
                }
            }
            catch (Exception ex)
            {
                Console.WriteLine($"❌ 创建账户失败: {ex.Message}");
            }
        }

        static async Task DeleteAccountAsync(KDEOAuth2Manager manager, int? accountId)
        {
            Console.WriteLine("🗑️ 删除账户:");
            Console.WriteLine();

            if (!accountId.HasValue)
            {
                Console.WriteLine("❌ 请使用 --account 指定要删除的账户ID");
                return;
            }

            try
            {
                // 先显示要删除的账户信息
                var accounts = await manager.GetAccountsAsync();
                var account = accounts.FirstOrDefault(a => a.Id == accountId.Value);

                if (account == null)
                {
                    Console.WriteLine($"❌ 账户 {accountId} 不存在");
                    return;
                }

                Console.WriteLine($"将要删除账户:");
                Console.WriteLine($"   ID: {account.Id}");
                Console.WriteLine($"   名称: {account.DisplayName}");
                Console.WriteLine($"   提供者: {account.Provider}");
                Console.WriteLine();

                // 确认删除
                Console.Write("确认删除此账户吗? (y/N): ");
                var confirmation = Console.ReadLine();

                if (confirmation?.ToLower() != "y" && confirmation?.ToLower() != "yes")
                {
                    Console.WriteLine("❌ 取消删除操作");
                    return;
                }

                bool success = await manager.DeleteAccountAsync(accountId.Value);
                if (success)
                {
                    Console.WriteLine($"✅ 成功删除账户 {accountId}");
                }
                else
                {
                    Console.WriteLine($"❌ 删除账户失败");
                }
            }
            catch (Exception ex)
            {
                Console.WriteLine($"❌ 删除账户时发生错误: {ex.Message}");
            }
        }

        static async Task SetAccountEnabledAsync(KDEOAuth2Manager manager, int? accountId, bool enabled)
        {
            string action = enabled ? "启用" : "禁用";
            Console.WriteLine($"{(enabled ? "✅" : "❌")} {action}账户:");
            Console.WriteLine();

            if (!accountId.HasValue)
            {
                Console.WriteLine($"❌ 请使用 --account 指定要{action}的账户ID");
                return;
            }

            try
            {
                bool success = await manager.SetAccountEnabledAsync(accountId.Value, enabled);
                if (success)
                {
                    Console.WriteLine($"✅ 成功{action}账户 {accountId}");
                }
                else
                {
                    Console.WriteLine($"❌ {action}账户失败（账户可能不存在）");
                }
            }
            catch (Exception ex)
            {
                Console.WriteLine($"❌ {action}账户时发生错误: {ex.Message}");
            }
        }

        static async Task UpdateAccountAsync(KDEOAuth2Manager manager, CommandOptions options)
        {
            Console.WriteLine("🔄 更新账户信息:");
            Console.WriteLine();

            if (!options.AccountId.HasValue)
            {
                Console.WriteLine("❌ 请使用 --account 指定要更新的账户ID");
                return;
            }

            try
            {
                bool hasUpdates = false;

                // 更新显示名称
                if (!string.IsNullOrEmpty(options.DisplayName))
                {
                    bool success = await manager.UpdateAccountDisplayNameAsync(options.AccountId.Value, options.DisplayName);
                    if (success)
                    {
                        Console.WriteLine($"✅ 更新显示名称为: {options.DisplayName}");
                        hasUpdates = true;
                    }
                    else
                    {
                        Console.WriteLine("❌ 更新显示名称失败");
                    }
                }

                // 更新服务器地址
                if (!string.IsNullOrEmpty(options.Server))
                {
                    bool success = await manager.UpdateAccountSettingAsync(options.AccountId.Value, "server", options.Server);
                    if (success)
                    {
                        Console.WriteLine($"✅ 更新服务器地址为: {options.Server}");
                        hasUpdates = true;
                    }
                    else
                    {
                        Console.WriteLine("❌ 更新服务器地址失败");
                    }
                }

                // 更新客户端ID
                if (!string.IsNullOrEmpty(options.ClientId))
                {
                    bool success = await manager.UpdateAccountSettingAsync(options.AccountId.Value, "client_id", options.ClientId);
                    if (success)
                    {
                        Console.WriteLine($"✅ 更新客户端ID为: {options.ClientId}");
                        hasUpdates = true;
                    }
                    else
                    {
                        Console.WriteLine("❌ 更新客户端ID失败");
                    }
                }

                // 更新访问令牌
                if (!string.IsNullOrEmpty(options.AccessToken))
                {
                    bool success = await manager.UpdateAccountSettingAsync(options.AccountId.Value, "access_token", options.AccessToken);
                    if (success)
                    {
                        // 更新令牌时间戳
                        await manager.UpdateTokenTimestampAsync(options.AccountId.Value);
                        Console.WriteLine($"✅ 更新访问令牌");
                        hasUpdates = true;
                    }
                    else
                    {
                        Console.WriteLine("❌ 更新访问令牌失败");
                    }
                }

                // 更新刷新令牌
                if (!string.IsNullOrEmpty(options.RefreshToken))
                {
                    bool success = await manager.UpdateAccountSettingAsync(options.AccountId.Value, "refresh_token", options.RefreshToken);
                    if (success)
                    {
                        Console.WriteLine($"✅ 更新刷新令牌");
                        hasUpdates = true;
                    }
                    else
                    {
                        Console.WriteLine("❌ 更新刷新令牌失败");
                    }
                }

                // 更新过期时间
                if (options.ExpiresIn.HasValue)
                {
                    bool success = await manager.UpdateAccountSettingAsync(options.AccountId.Value, "expires_in", options.ExpiresIn.Value.ToString());
                    if (success)
                    {
                        Console.WriteLine($"✅ 更新过期时间为: {options.ExpiresIn.Value} 秒");
                        hasUpdates = true;
                    }
                    else
                    {
                        Console.WriteLine("❌ 更新过期时间失败");
                    }
                }

                if (!hasUpdates)
                {
                    Console.WriteLine("⚠️ 没有指定要更新的内容");
                    Console.WriteLine("可用选项: --name, --server, --client-id, --access-token, --refresh-token, --expires-in");
                }
            }
            catch (Exception ex)
            {
                Console.WriteLine($"❌ 更新账户时发生错误: {ex.Message}");
            }
        }

        static async Task CheckTokenStatusAsync(KDEOAuth2Manager manager, int? accountId)
        {
            Console.WriteLine("🔍 检查令牌状态:");
            Console.WriteLine();

            try
            {
                // 如果没有指定账户ID，获取所有账户并检查
                if (!accountId.HasValue)
                {
                    var accounts = await manager.GetAccountsAsync();
                    if (!accounts.Any())
                    {
                        Console.WriteLine("❌ 未找到任何账户");
                        return;
                    }

                    Console.WriteLine("📋 所有账户令牌状态:");
                    Console.WriteLine();

                    foreach (var account in accounts)
                    {
                        await CheckSingleTokenStatus(manager, account.Id, account.DisplayName);
                        Console.WriteLine();
                    }
                }
                else
                {
                    // 检查指定账户
                    var account = (await manager.GetAccountsAsync()).FirstOrDefault(a => a.Id == accountId.Value);
                    if (account == null)
                    {
                        Console.WriteLine($"❌ 未找到账户 ID: {accountId.Value}");
                        return;
                    }

                    await CheckSingleTokenStatus(manager, accountId.Value, account.DisplayName);
                }
            }
            catch (Exception ex)
            {
                Console.WriteLine($"❌ 检查令牌状态时发生错误: {ex.Message}");
            }
        }

        static async Task CheckSingleTokenStatus(KDEOAuth2Manager manager, int accountId, string? displayName)
        {
            try
            {
                var credentials = await manager.GetCredentialsAsync(accountId);
                if (credentials == null)
                {
                    Console.WriteLine($"❌ 账户 {accountId} ({displayName ?? "未知"}) - 无法获取凭据");
                    return;
                }

                Console.WriteLine($"🔐 账户 {accountId}: {displayName ?? "未知"}");

                // 检查基本信息
                Console.WriteLine($"   服务器: {credentials.Server ?? "未设置"}");
                Console.WriteLine($"   客户端ID: {credentials.ClientId ?? "未设置"}");
                
                if (!string.IsNullOrEmpty(credentials.AccessToken))
                {
                    string displayToken = credentials.AccessToken.Length > 20 
                        ? credentials.AccessToken.Substring(0, 20) + "..." 
                        : credentials.AccessToken;
                    Console.WriteLine($"   访问令牌: {displayToken}");
                }

                // 检查过期状态
                var expirationStatus = manager.CheckTokenExpiration(credentials);
                if (expirationStatus.HasExpiration)
                {
                    Console.WriteLine($"   创建时间: {expirationStatus.CreatedAt:yyyy-MM-dd HH:mm:ss}");
                    Console.WriteLine($"   过期时间: {expirationStatus.ExpiresAt:yyyy-MM-dd HH:mm:ss}");
                    
                    if (expirationStatus.IsExpired)
                    {
                        Console.WriteLine("   状态: ❌ 已过期");
                    }
                    else
                    {
                        Console.WriteLine($"   状态: ✅ 有效");
                        Console.WriteLine($"   剩余时间: {FormatTimeSpan(expirationStatus.RemainingTime)}");
                    }
                }
                else
                {
                    Console.WriteLine("   过期信息: ⚠️ 无时间戳信息");
                }

                // 在线验证
                Console.WriteLine("   在线验证: 检查中...");
                var tokenStatus = await manager.CheckTokenStatusAsync(credentials);
                
                if (tokenStatus.IsValid)
                {
                    Console.WriteLine("   在线状态: ✅ 令牌有效");
                }
                else if (tokenStatus.IsExpired)
                {
                    Console.WriteLine("   在线状态: ❌ 令牌已过期或无效");
                }
                else
                {
                    Console.WriteLine($"   在线状态: ⚠️ {tokenStatus.Reason}");
                }
            }
            catch (Exception ex)
            {
                Console.WriteLine($"❌ 检查账户 {accountId} 时发生错误: {ex.Message}");
            }
        }

        static string FormatTimeSpan(TimeSpan? timeSpan)
        {
            if (!timeSpan.HasValue)
                return "未知";

            var time = timeSpan.Value;
            if (time.TotalDays >= 1)
                return $"{(int)time.TotalDays}天{time.Hours}小时{time.Minutes}分钟";
            else if (time.TotalHours >= 1)
                return $"{(int)time.TotalHours}小时{time.Minutes}分钟";
            else if (time.TotalMinutes >= 1)
                return $"{(int)time.TotalMinutes}分钟{time.Seconds}秒";
            else
                return $"{(int)time.TotalSeconds}秒";
        }
    }

    class CommandOptions
    {
        public string Action { get; set; } = "default";
        public int? AccountId { get; set; }
        public string? DisplayName { get; set; }
        public string? Server { get; set; }
        public string? ClientId { get; set; }
        public string? AccessToken { get; set; }
        public string? RefreshToken { get; set; }
        public int? ExpiresIn { get; set; }
    }
}