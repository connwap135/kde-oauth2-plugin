using Microsoft.Data.Sqlite;
using KDEOAuth2Client.Models;

namespace KDEOAuth2Client.Services
{
    /// <summary>
    /// KDE OAuth2 客户端 - 通过 SQLite 数据库访问 KDE 账户系统
    /// </summary>
    public class KDEOAuth2Manager
    {
        private readonly string _dbPath;
        private readonly string _defaultProvider = "gzweibo-oauth2";

        /// <summary>
        /// 初始化 KDE OAuth2 管理器
        /// </summary>
        /// <param name="dbPath">数据库路径，默认为 ~/.config/libaccounts-glib/accounts.db</param>
        public KDEOAuth2Manager(string? dbPath = null)
        {
            _dbPath = dbPath ?? Path.Combine(
                Environment.GetEnvironmentVariable("HOME") ?? Environment.GetFolderPath(Environment.SpecialFolder.UserProfile),
                ".config/libaccounts-glib/accounts.db"
            );
        }

        /// <summary>
        /// 检查数据库是否存在
        /// </summary>
        public bool IsDatabaseAvailable()
        {
            return File.Exists(_dbPath);
        }

        /// <summary>
        /// 获取所有账户列表
        /// </summary>
        /// <param name="provider">提供者名称，默认为 gzweibo-oauth2</param>
        /// <returns>账户信息列表</returns>
        public async Task<List<AccountInfo>> GetAccountsAsync(string? provider = null)
        {
            if (!IsDatabaseAvailable())
                throw new FileNotFoundException($"KDE账户数据库不存在: {_dbPath}");

            provider ??= _defaultProvider;
            var accounts = new List<AccountInfo>();

            using var connection = new SqliteConnection($"Data Source={_dbPath}");
            await connection.OpenAsync();

            var command = connection.CreateCommand();
            command.CommandText = @"
                SELECT id, name, provider, enabled 
                FROM Accounts 
                WHERE provider = @provider 
                ORDER BY id DESC";
            command.Parameters.AddWithValue("@provider", provider);

            using var reader = await command.ExecuteReaderAsync();
            while (await reader.ReadAsync())
            {
                accounts.Add(new AccountInfo
                {
                    Id = reader.GetInt32(0),
                    DisplayName = reader.IsDBNull(1) ? $"Account {reader.GetInt32(0)}" : reader.GetString(1),
                    Provider = reader.GetString(2),
                    Enabled = reader.GetInt32(3) != 0
                });
            }

            return accounts;
        }

        /// <summary>
        /// 获取 OAuth2 凭证
        /// </summary>
        /// <param name="accountId">账户ID，如果为null则获取最新的启用账户</param>
        /// <param name="provider">提供者名称</param>
        /// <returns>OAuth2 凭证</returns>
        public async Task<OAuth2Credentials?> GetCredentialsAsync(int? accountId = null, string? provider = null)
        {
            if (!IsDatabaseAvailable())
                return null;

            provider ??= _defaultProvider;

            using var connection = new SqliteConnection($"Data Source={_dbPath}");
            await connection.OpenAsync();

            // 查找账户ID
            int actualAccountId;
            if (accountId.HasValue)
            {
                // 验证指定的账户ID是否存在且属于正确的提供者
                var verifyCommand = connection.CreateCommand();
                verifyCommand.CommandText = @"
                    SELECT id FROM Accounts 
                    WHERE id = @accountId AND provider = @provider AND enabled = 1";
                verifyCommand.Parameters.AddWithValue("@accountId", accountId.Value);
                verifyCommand.Parameters.AddWithValue("@provider", provider);

                var result = await verifyCommand.ExecuteScalarAsync();
                if (result == null)
                    return null;

                actualAccountId = accountId.Value;
            }
            else
            {
                // 查找有完整OAuth2设置的最新启用账户
                var findCommand = connection.CreateCommand();
                findCommand.CommandText = @"
                    SELECT DISTINCT a.id FROM Accounts a
                    INNER JOIN Settings s1 ON a.id = s1.account AND s1.service = 0 AND s1.key = 'access_token'
                    INNER JOIN Settings s2 ON a.id = s2.account AND s2.service = 0 AND s2.key = 'server'
                    WHERE a.provider = @provider AND a.enabled = 1 
                    ORDER BY a.id DESC LIMIT 1";
                findCommand.Parameters.AddWithValue("@provider", provider);

                var result = await findCommand.ExecuteScalarAsync();
                if (result == null)
                    return null;

                actualAccountId = Convert.ToInt32(result);
            }

            // 获取账户基本信息
            var accountCommand = connection.CreateCommand();
            accountCommand.CommandText = @"
                SELECT name FROM Accounts WHERE id = @accountId";
            accountCommand.Parameters.AddWithValue("@accountId", actualAccountId);

            string? displayName = null;
            var accountResult = await accountCommand.ExecuteScalarAsync();
            if (accountResult != null)
                displayName = accountResult.ToString();

            // 获取设置
            var settingsCommand = connection.CreateCommand();
            settingsCommand.CommandText = @"
                SELECT key, value FROM Settings 
                WHERE account = @accountId AND service = 0";
            settingsCommand.Parameters.AddWithValue("@accountId", actualAccountId);

            var credentials = new OAuth2Credentials
            {
                AccountId = actualAccountId,
                DisplayName = displayName
            };

            using var reader = await settingsCommand.ExecuteReaderAsync();
            while (await reader.ReadAsync())
            {
                string key = reader.GetString(0);
                string value = reader.GetString(1);

                // 清理SQLite字符串引号
                if (value.StartsWith("'") && value.EndsWith("'"))
                    value = value.Substring(1, value.Length - 2);

                switch (key)
                {
                    case "access_token":
                        credentials.AccessToken = value;
                        break;
                    case "refresh_token":
                        credentials.RefreshToken = value;
                        break;
                    case "server":
                        credentials.Server = value;
                        break;
                    case "client_id":
                        credentials.ClientId = value;
                        break;
                    case "username":
                        credentials.Username = value;
                        break;
                    case "expires_in":
                        if (int.TryParse(value, out int expiresIn))
                            credentials.ExpiresIn = expiresIn;
                        break;
                }
            }

            return credentials.IsValid ? credentials : null;
        }

        /// <summary>
        /// 检查令牌是否过期
        /// </summary>
        /// <param name="credentials">OAuth2 凭证</param>
        /// <returns>令牌状态信息</returns>
        public async Task<TokenStatus> CheckTokenStatusAsync(OAuth2Credentials credentials)
        {
            if (!credentials.IsValid)
                return new TokenStatus { IsValid = false, Reason = "凭证信息不完整" };

            // 1. 检查过期时间戳（如果存在）
            var expirationStatus = CheckTokenExpiration(credentials);
            if (expirationStatus.IsExpired)
            {
                return new TokenStatus
                {
                    IsValid = false,
                    IsExpired = true,
                    Reason = $"令牌已过期，过期时间: {expirationStatus.ExpiresAt:yyyy-MM-dd HH:mm:ss}",
                    ExpiresAt = expirationStatus.ExpiresAt
                };
            }

            // 2. 主动验证令牌（网络请求）
            try
            {
                using var httpClient = new HttpClient();
                httpClient.Timeout = TimeSpan.FromSeconds(10);
                httpClient.DefaultRequestHeaders.Authorization =
                    new System.Net.Http.Headers.AuthenticationHeaderValue("Bearer", credentials.AccessToken);

                var response = await httpClient.GetAsync($"{credentials.Server}/connect/userinfo");

                if (response.IsSuccessStatusCode)
                {
                    return new TokenStatus
                    {
                        IsValid = true,
                        Reason = "令牌有效",
                        ExpiresAt = expirationStatus.ExpiresAt,
                        RemainingTime = expirationStatus.RemainingTime
                    };
                }
                else if (response.StatusCode == System.Net.HttpStatusCode.Unauthorized)
                {
                    return new TokenStatus
                    {
                        IsValid = false,
                        IsExpired = true,
                        Reason = "令牌无效或已过期 (401 Unauthorized)"
                    };
                }
                else
                {
                    return new TokenStatus
                    {
                        IsValid = false,
                        Reason = $"服务器响应错误: {response.StatusCode}"
                    };
                }
            }
            catch (TaskCanceledException)
            {
                return new TokenStatus
                {
                    IsValid = false,
                    Reason = "请求超时，无法验证令牌状态"
                };
            }
            catch (HttpRequestException ex)
            {
                return new TokenStatus
                {
                    IsValid = false,
                    Reason = $"网络错误: {ex.Message}"
                };
            }
            catch (Exception ex)
            {
                return new TokenStatus
                {
                    IsValid = false,
                    Reason = $"验证失败: {ex.Message}"
                };
            }
        }

        /// <summary>
        /// 检查令牌过期时间戳
        /// </summary>
        /// <param name="credentials">OAuth2 凭证</param>
        /// <returns>过期状态信息</returns>
        public ExpirationStatus CheckTokenExpiration(OAuth2Credentials credentials)
        {
            // 从数据库获取令牌创建时间和过期时间
            var tokenInfo = GetTokenTimestampInfo(credentials.AccountId);

            if (tokenInfo.CreatedAt.HasValue && credentials.ExpiresIn > 0)
            {
                var expiresAt = tokenInfo.CreatedAt.Value.AddSeconds(credentials.ExpiresIn);
                var now = DateTime.Now;

                return new ExpirationStatus
                {
                    HasExpiration = true,
                    CreatedAt = tokenInfo.CreatedAt.Value,
                    ExpiresAt = expiresAt,
                    IsExpired = now >= expiresAt,
                    RemainingTime = expiresAt > now ? expiresAt - now : TimeSpan.Zero
                };
            }

            // 如果没有时间戳信息，无法判断过期状态
            return new ExpirationStatus
            {
                HasExpiration = false,
                IsExpired = false,
                RemainingTime = null
            };
        }

        /// <summary>
        /// 获取令牌时间戳信息
        /// </summary>
        /// <param name="accountId">账户ID</param>
        /// <returns>时间戳信息</returns>
        public TokenTimestampInfo GetTokenTimestampInfo(int accountId)
        {
            if (!IsDatabaseAvailable())
                return new TokenTimestampInfo();

            try
            {
                using var connection = new SqliteConnection($"Data Source={_dbPath}");
                connection.Open();

                var command = connection.CreateCommand();
                command.CommandText = @"
                    SELECT value FROM Settings 
                    WHERE account = @accountId AND key = 'token_created_at' AND service = 0";
                command.Parameters.AddWithValue("@accountId", accountId);

                var result = command.ExecuteScalar();
                if (result != null && DateTime.TryParse(result.ToString(), out DateTime createdAt))
                {
                    return new TokenTimestampInfo
                    {
                        CreatedAt = createdAt,
                        HasTimestamp = true
                    };
                }
            }
            catch
            {
                // 忽略错误，返回空信息
            }

            return new TokenTimestampInfo();
        }

        /// <summary>
        /// 更新令牌创建时间戳
        /// </summary>
        /// <param name="accountId">账户ID</param>
        /// <param name="createdAt">创建时间</param>
        /// <returns>是否更新成功</returns>
        public async Task<bool> UpdateTokenTimestampAsync(int accountId, DateTime? createdAt = null)
        {
            createdAt ??= DateTime.Now;

            return await UpdateAccountSettingAsync(accountId, "token_created_at",
                createdAt.Value.ToString("yyyy-MM-dd HH:mm:ss"));
        }

        /// <summary>
        /// 测试令牌有效性（保持向后兼容）
        /// </summary>
        /// <param name="credentials">OAuth2 凭证</param>
        /// <returns>是否有效</returns>
        public async Task<bool> TestTokenValidityAsync(OAuth2Credentials credentials)
        {
            var status = await CheckTokenStatusAsync(credentials);
            return status.IsValid;
        }

        /// <summary>
        /// 获取数据库路径
        /// </summary>
        public string GetDatabasePath() => _dbPath;

        /// <summary>
        /// 创建新的 OAuth2 账户
        /// </summary>
        /// <param name="displayName">账户显示名称</param>
        /// <param name="credentials">OAuth2 凭证信息</param>
        /// <param name="provider">提供者名称</param>
        /// <returns>创建的账户ID</returns>
        public async Task<int> CreateAccountAsync(string displayName, OAuth2Credentials credentials, string? provider = null)
        {
            if (!IsDatabaseAvailable())
                throw new FileNotFoundException($"KDE账户数据库不存在: {_dbPath}");

            provider ??= _defaultProvider;

            using var connection = new SqliteConnection($"Data Source={_dbPath}");
            await connection.OpenAsync();

            using var transaction = await connection.BeginTransactionAsync();
            try
            {
                // 插入账户记录
                var accountCommand = connection.CreateCommand();
                accountCommand.CommandText = @"
                    INSERT INTO Accounts (name, provider, enabled) 
                    VALUES (@name, @provider, @enabled)";
                accountCommand.Parameters.AddWithValue("@name", displayName);
                accountCommand.Parameters.AddWithValue("@provider", provider);
                accountCommand.Parameters.AddWithValue("@enabled", 1);

                await accountCommand.ExecuteNonQueryAsync();

                // 获取新创建的账户ID
                var getIdCommand = connection.CreateCommand();
                getIdCommand.CommandText = "SELECT last_insert_rowid()";
                var accountId = Convert.ToInt32(await getIdCommand.ExecuteScalarAsync());

                // 插入设置
                await InsertSettingsAsync(connection, accountId, credentials);

                // 记录令牌创建时间戳
                if (!string.IsNullOrEmpty(credentials.AccessToken))
                {
                    var timestampCommand = connection.CreateCommand();
                    timestampCommand.CommandText = @"
                        INSERT INTO Settings (account, service, key, type, value) 
                        VALUES (@account, 0, 'token_created_at', 'string', @value)";
                    timestampCommand.Parameters.AddWithValue("@account", accountId);
                    timestampCommand.Parameters.AddWithValue("@value", DateTime.Now.ToString("yyyy-MM-dd HH:mm:ss"));
                    await timestampCommand.ExecuteNonQueryAsync();
                }

                await transaction.CommitAsync();
                return accountId;
            }
            catch
            {
                await transaction.RollbackAsync();
                throw;
            }
        }

        /// <summary>
        /// 更新账户凭证
        /// </summary>
        /// <param name="accountId">账户ID</param>
        /// <param name="credentials">新的凭证信息</param>
        /// <returns>是否更新成功</returns>
        public async Task<bool> UpdateAccountCredentialsAsync(int accountId, OAuth2Credentials credentials)
        {
            if (!IsDatabaseAvailable())
                return false;

            using var connection = new SqliteConnection($"Data Source={_dbPath}");
            await connection.OpenAsync();

            using var transaction = await connection.BeginTransactionAsync();
            try
            {
                // 验证账户存在
                var verifyCommand = connection.CreateCommand();
                verifyCommand.CommandText = "SELECT COUNT(*) FROM Accounts WHERE id = @accountId";
                verifyCommand.Parameters.AddWithValue("@accountId", accountId);
                var accountExists = Convert.ToInt32(await verifyCommand.ExecuteScalarAsync()) > 0;

                if (!accountExists)
                    return false;

                // 删除现有设置
                var deleteCommand = connection.CreateCommand();
                deleteCommand.CommandText = "DELETE FROM Settings WHERE account = @accountId AND service = 0";
                deleteCommand.Parameters.AddWithValue("@accountId", accountId);
                await deleteCommand.ExecuteNonQueryAsync();

                // 插入新设置
                await InsertSettingsAsync(connection, accountId, credentials);

                // 更新令牌创建时间戳（如果包含新的访问令牌）
                if (!string.IsNullOrEmpty(credentials.AccessToken))
                {
                    var timestampCommand = connection.CreateCommand();
                    timestampCommand.CommandText = @"
                        INSERT OR REPLACE INTO Settings (account, service, key, type, value) 
                        VALUES (@account, NULL, 'token_created_at', 'string', @value)";
                    timestampCommand.Parameters.AddWithValue("@account", accountId);
                    timestampCommand.Parameters.AddWithValue("@value", DateTime.Now.ToString("yyyy-MM-dd HH:mm:ss"));
                    await timestampCommand.ExecuteNonQueryAsync();
                }

                await transaction.CommitAsync();
                return true;
            }
            catch
            {
                await transaction.RollbackAsync();
                return false;
            }
        }

        /// <summary>
        /// 删除账户
        /// </summary>
        /// <param name="accountId">账户ID</param>
        /// <returns>是否删除成功</returns>
        public async Task<bool> DeleteAccountAsync(int accountId)
        {
            if (!IsDatabaseAvailable())
                return false;

            using var connection = new SqliteConnection($"Data Source={_dbPath}");
            await connection.OpenAsync();

            using var transaction = await connection.BeginTransactionAsync();
            try
            {
                // 验证账户存在
                var verifyCommand = connection.CreateCommand();
                verifyCommand.CommandText = "SELECT COUNT(*) FROM Accounts WHERE id = @accountId";
                verifyCommand.Parameters.AddWithValue("@accountId", accountId);
                var accountExists = Convert.ToInt32(await verifyCommand.ExecuteScalarAsync()) > 0;

                if (!accountExists)
                    return false;

                // 删除设置 (由于有触发器，删除账户时会自动删除设置)
                var deleteAccountCommand = connection.CreateCommand();
                deleteAccountCommand.CommandText = "DELETE FROM Accounts WHERE id = @accountId";
                deleteAccountCommand.Parameters.AddWithValue("@accountId", accountId);

                var rowsAffected = await deleteAccountCommand.ExecuteNonQueryAsync();

                await transaction.CommitAsync();
                return rowsAffected > 0;
            }
            catch
            {
                await transaction.RollbackAsync();
                return false;
            }
        }

        /// <summary>
        /// 启用或禁用账户
        /// </summary>
        /// <param name="accountId">账户ID</param>
        /// <param name="enabled">是否启用</param>
        /// <returns>是否更新成功</returns>
        public async Task<bool> SetAccountEnabledAsync(int accountId, bool enabled)
        {
            if (!IsDatabaseAvailable())
                return false;

            using var connection = new SqliteConnection($"Data Source={_dbPath}");
            await connection.OpenAsync();

            try
            {
                var command = connection.CreateCommand();
                command.CommandText = "UPDATE Accounts SET enabled = @enabled WHERE id = @accountId";
                command.Parameters.AddWithValue("@enabled", enabled ? 1 : 0);
                command.Parameters.AddWithValue("@accountId", accountId);

                var rowsAffected = await command.ExecuteNonQueryAsync();
                return rowsAffected > 0;
            }
            catch
            {
                return false;
            }
        }

        /// <summary>
        /// 更新账户显示名称
        /// </summary>
        /// <param name="accountId">账户ID</param>
        /// <param name="displayName">新的显示名称</param>
        /// <returns>是否更新成功</returns>
        public async Task<bool> UpdateAccountDisplayNameAsync(int accountId, string displayName)
        {
            if (!IsDatabaseAvailable())
                return false;

            using var connection = new SqliteConnection($"Data Source={_dbPath}");
            await connection.OpenAsync();

            try
            {
                var command = connection.CreateCommand();
                command.CommandText = "UPDATE Accounts SET name = @name WHERE id = @accountId";
                command.Parameters.AddWithValue("@name", displayName);
                command.Parameters.AddWithValue("@accountId", accountId);

                var rowsAffected = await command.ExecuteNonQueryAsync();
                return rowsAffected > 0;
            }
            catch
            {
                return false;
            }
        }

        /// <summary>
        /// 更新特定的账户设置
        /// </summary>
        /// <param name="accountId">账户ID</param>
        /// <param name="key">设置键</param>
        /// <param name="value">设置值</param>
        /// <returns>是否更新成功</returns>
        public async Task<bool> UpdateAccountSettingAsync(int accountId, string key, string value)
        {
            if (!IsDatabaseAvailable())
                return false;

            using var connection = new SqliteConnection($"Data Source={_dbPath}");
            await connection.OpenAsync();

            try
            {
                // 检查设置是否存在
                var checkCommand = connection.CreateCommand();
                checkCommand.CommandText = "SELECT COUNT(*) FROM Settings WHERE account = @accountId AND key = @key AND service = 0";
                checkCommand.Parameters.AddWithValue("@accountId", accountId);
                checkCommand.Parameters.AddWithValue("@key", key);

                var exists = Convert.ToInt32(await checkCommand.ExecuteScalarAsync()) > 0;

                SqliteCommand command;
                if (exists)
                {
                    // 更新现有设置
                    command = connection.CreateCommand();
                    command.CommandText = "UPDATE Settings SET value = @value WHERE account = @accountId AND key = @key AND service = 0";
                }
                else
                {
                    // 插入新设置
                    command = connection.CreateCommand();
                    command.CommandText = "INSERT INTO Settings (account, service, key, type, value) VALUES (@accountId, 0, @key, 'string', @value)";
                }

                command.Parameters.AddWithValue("@accountId", accountId);
                command.Parameters.AddWithValue("@key", key);
                command.Parameters.AddWithValue("@value", value);

                var rowsAffected = await command.ExecuteNonQueryAsync();
                return rowsAffected > 0;
            }
            catch
            {
                return false;
            }
        }

        /// <summary>
        /// 私有方法：插入账户设置
        /// </summary>
        private async Task InsertSettingsAsync(SqliteConnection connection, int accountId, OAuth2Credentials credentials)
        {
            var settings = new Dictionary<string, string>();

            if (!string.IsNullOrEmpty(credentials.AccessToken))
                settings["access_token"] = credentials.AccessToken;
            if (!string.IsNullOrEmpty(credentials.RefreshToken))
                settings["refresh_token"] = credentials.RefreshToken;
            if (!string.IsNullOrEmpty(credentials.Server))
                settings["server"] = credentials.Server;
            if (!string.IsNullOrEmpty(credentials.ClientId))
                settings["client_id"] = credentials.ClientId;
            if (!string.IsNullOrEmpty(credentials.Username))
                settings["username"] = credentials.Username;
            if (credentials.ExpiresIn > 0)
                settings["expires_in"] = credentials.ExpiresIn.ToString();

            foreach (var setting in settings)
            {
                var command = connection.CreateCommand();
                command.CommandText = @"
                    INSERT INTO Settings (account, service, key, type, value) 
                    VALUES (@account, 0, @key, 'string', @value)";
                command.Parameters.AddWithValue("@account", accountId);
                command.Parameters.AddWithValue("@key", setting.Key);
                command.Parameters.AddWithValue("@value", setting.Value);

                await command.ExecuteNonQueryAsync();
            }
        }

        /// <summary>
        /// 获取数据库统计信息
        /// </summary>
        public async Task<Dictionary<string, object>> GetDatabaseStatsAsync()
        {
            var stats = new Dictionary<string, object>();

            if (!IsDatabaseAvailable())
            {
                stats["error"] = "数据库不存在";
                return stats;
            }

            try
            {
                using var connection = new SqliteConnection($"Data Source={_dbPath}");
                await connection.OpenAsync();

                // 总账户数
                var totalAccountsCommand = connection.CreateCommand();
                totalAccountsCommand.CommandText = "SELECT COUNT(*) FROM Accounts";
                stats["total_accounts"] = await totalAccountsCommand.ExecuteScalarAsync() ?? 0;

                // OAuth2 账户数
                var oauth2AccountsCommand = connection.CreateCommand();
                oauth2AccountsCommand.CommandText = "SELECT COUNT(*) FROM Accounts WHERE provider = @provider";
                oauth2AccountsCommand.Parameters.AddWithValue("@provider", _defaultProvider);
                stats["oauth2_accounts"] = await oauth2AccountsCommand.ExecuteScalarAsync() ?? 0;

                // 启用的账户数
                var enabledAccountsCommand = connection.CreateCommand();
                enabledAccountsCommand.CommandText = "SELECT COUNT(*) FROM Accounts WHERE provider = @provider AND enabled = 1";
                enabledAccountsCommand.Parameters.AddWithValue("@provider", _defaultProvider);
                stats["enabled_accounts"] = await enabledAccountsCommand.ExecuteScalarAsync() ?? 0;

                // 数据库文件大小
                var fileInfo = new FileInfo(_dbPath);
                stats["database_size_bytes"] = fileInfo.Length;
                stats["database_path"] = _dbPath;
            }
            catch (Exception ex)
            {
                stats["error"] = ex.Message;
            }

            return stats;
        }
    }
}