using System;
using System.Threading.Tasks;
using KDEOAuth2Client.Models;
using KDEOAuth2Client.Services;

namespace KDEOAuth2Client.Services
{
    /// <summary>
    /// OAuth2 令牌管理器 - 负责获取、验证和刷新访问令牌
    /// </summary>
    public class TokenManager
    {
        private readonly KDEOAuth2Manager _accountManager;
        private readonly OAuth2ApiClient _apiClient;

        /// <summary>
        /// 初始化令牌管理器
        /// </summary>
        /// <param name="accountManager">KDE账户管理器</param>
        /// <param name="apiClient">OAuth2 API客户端</param>
        public TokenManager(KDEOAuth2Manager accountManager, OAuth2ApiClient apiClient)
        {
            _accountManager = accountManager ?? throw new ArgumentNullException(nameof(accountManager));
            _apiClient = apiClient ?? throw new ArgumentNullException(nameof(apiClient));
        }

        /// <summary>
        /// 获取有效的访问令牌
        /// 如果当前令牌有效则直接返回，否则刷新令牌后返回新的令牌
        /// </summary>
        /// <param name="accountId">账户ID，如果为null则获取最新的启用账户</param>
        /// <param name="provider">提供者名称</param>
        /// <returns>有效的访问令牌</returns>
        public async Task<string?> GetValidAccessTokenAsync(int? accountId = null, string? provider = null)
        {
            try
            {
                // 1. 获取当前账户凭证
                var credentials = await _accountManager.GetCredentialsAsync(accountId, provider);
                if (credentials == null)
                {
                    Console.WriteLine("未找到有效的账户凭证");
                    return null;
                }

                // 2. 检查令牌状态
                var tokenStatus = await _accountManager.CheckTokenStatusAsync(credentials);

                if (tokenStatus.IsValid && !tokenStatus.IsExpired)
                {
                    // 令牌有效，直接返回
                    Console.WriteLine("访问令牌有效，直接返回");
                    return credentials.AccessToken;
                }

                // 3. 令牌过期或无效，尝试刷新
                Console.WriteLine($"令牌状态: {tokenStatus.Reason}");

                if (string.IsNullOrEmpty(credentials.RefreshToken))
                {
                    Console.WriteLine("没有刷新令牌，无法刷新访问令牌");
                    return null;
                }

                // 4. 刷新令牌
                var refreshResponse = await _apiClient.RefreshTokenAsync(
                    credentials.AccessToken!,
                    credentials.RefreshToken,
                    credentials.ClientId ?? "");

                if (!refreshResponse.Success || refreshResponse.Data == null)
                {
                    Console.WriteLine($"刷新令牌失败: {refreshResponse.Message}");
                    return null;
                }

                // 5. 更新凭证信息
                var newCredentials = new OAuth2Credentials
                {
                    AccountId = credentials.AccountId,
                    DisplayName = credentials.DisplayName,
                    AccessToken = refreshResponse.Data.AccessToken,
                    RefreshToken = refreshResponse.Data.RefreshToken ?? credentials.RefreshToken, // 如果没有新刷新令牌，保持原有
                    Server = credentials.Server,
                    ClientId = credentials.ClientId,
                    Username = credentials.Username,
                    ExpiresIn = refreshResponse.Data.ExpiresIn
                };

                var updateSuccess = await _accountManager.UpdateAccountCredentialsAsync(
                    credentials.AccountId,
                    newCredentials);

                if (!updateSuccess)
                {
                    Console.WriteLine("更新账户凭证失败");
                    return null;
                }

                // 6. 更新令牌时间戳
                await _accountManager.UpdateTokenTimestampAsync(credentials.AccountId);

                Console.WriteLine("令牌刷新成功，返回新的访问令牌");
                return newCredentials.AccessToken;
            }
            catch (Exception ex)
            {
                Console.WriteLine($"获取访问令牌时发生错误: {ex.Message}");
                return null;
            }
        }

        /// <summary>
        /// 获取当前账户的凭证信息
        /// </summary>
        /// <param name="accountId">账户ID，如果为null则获取最新的启用账户</param>
        /// <param name="provider">提供者名称</param>
        /// <returns>账户凭证信息</returns>
        public async Task<OAuth2Credentials?> GetCurrentAccountCredentialsAsync(int? accountId = null, string? provider = null)
        {
            return await _accountManager.GetCredentialsAsync(accountId, provider);
        }

        /// <summary>
        /// 检查令牌是否过期
        /// </summary>
        /// <param name="accountId">账户ID，如果为null则获取最新的启用账户</param>
        /// <param name="provider">提供者名称</param>
        /// <returns>令牌状态信息</returns>
        public async Task<TokenStatus?> CheckTokenExpirationAsync(int? accountId = null, string? provider = null)
        {
            var credentials = await _accountManager.GetCredentialsAsync(accountId, provider);
            if (credentials == null)
                return null;

            return await _accountManager.CheckTokenStatusAsync(credentials);
        }

        /// <summary>
        /// 手动刷新令牌
        /// </summary>
        /// <param name="accountId">账户ID，如果为null则获取最新的启用账户</param>
        /// <param name="provider">提供者名称</param>
        /// <returns>刷新是否成功</returns>
        public async Task<bool> RefreshTokenAsync(int? accountId = null, string? provider = null)
        {
            var token = await GetValidAccessTokenAsync(accountId, provider);
            return !string.IsNullOrEmpty(token);
        }
    }
}