using System;
using System.Threading.Tasks;
using KDEOAuth2Client.Services;

namespace KDEOAuth2Client.Examples
{
    /// <summary>
    /// TokenManager 使用示例
    /// </summary>
    public class TokenManagerExample
    {
        /// <summary>
        /// 演示如何使用 TokenManager 获取有效的访问令牌
        /// </summary>
        public static async Task DemonstrateTokenManagerAsync()
        {
            try
            {
                // 初始化组件
                var accountManager = new KDEOAuth2Manager();
                var apiClient = new OAuth2ApiClient("https://your-oauth-server.com");
                var tokenManager = new TokenManager(accountManager, apiClient);

                Console.WriteLine("=== TokenManager 使用示例 ===\n");

                // 1. 获取当前账户凭证
                Console.WriteLine("1. 获取当前账户凭证...");
                var credentials = await tokenManager.GetCurrentAccountCredentialsAsync();
                if (credentials == null)
                {
                    Console.WriteLine("未找到有效的账户凭证，请确保已配置OAuth2账户");
                    return;
                }

                Console.WriteLine($"账户ID: {credentials.AccountId}");
                Console.WriteLine($"显示名称: {credentials.DisplayName}");
                Console.WriteLine($"服务器: {credentials.Server}");
                Console.WriteLine($"用户名: {credentials.Username}");
                Console.WriteLine();

                // 2. 检查令牌状态
                Console.WriteLine("2. 检查令牌状态...");
                var tokenStatus = await tokenManager.CheckTokenExpirationAsync();
                if (tokenStatus != null)
                {
                    Console.WriteLine($"令牌有效: {tokenStatus.IsValid}");
                    Console.WriteLine($"令牌过期: {tokenStatus.IsExpired}");
                    Console.WriteLine($"状态说明: {tokenStatus.Reason}");

                    if (tokenStatus.ExpiresAt.HasValue)
                    {
                        Console.WriteLine($"过期时间: {tokenStatus.ExpiresAt.Value:yyyy-MM-dd HH:mm:ss}");
                    }

                    if (tokenStatus.RemainingTime.HasValue)
                    {
                        Console.WriteLine($"剩余时间: {tokenStatus.RemainingTimeFormatted}");
                    }
                }
                Console.WriteLine();

                // 3. 获取有效的访问令牌（自动处理过期情况）
                Console.WriteLine("3. 获取有效的访问令牌...");
                var accessToken = await tokenManager.GetValidAccessTokenAsync();

                if (!string.IsNullOrEmpty(accessToken))
                {
                    Console.WriteLine("✅ 成功获取有效的访问令牌");
                    Console.WriteLine($"令牌长度: {accessToken.Length} 字符");

                    // 这里可以安全地使用 accessToken 调用API
                    // 例如：
                    // apiClient.SetAccessToken(accessToken);
                    // var userInfo = await apiClient.GetUserInfoAsync();
                }
                else
                {
                    Console.WriteLine("❌ 获取访问令牌失败");
                }
                Console.WriteLine();

                // 4. 手动刷新令牌示例
                Console.WriteLine("4. 手动刷新令牌...");
                bool refreshSuccess = await tokenManager.RefreshTokenAsync();
                Console.WriteLine($"刷新结果: {(refreshSuccess ? "成功" : "失败")}");

                Console.WriteLine("\n=== 示例完成 ===");
            }
            catch (Exception ex)
            {
                Console.WriteLine($"示例执行失败: {ex.Message}");
            }
        }

        /// <summary>
        /// 简单的令牌获取方法，适合在其他代码中直接调用
        /// </summary>
        /// <returns>有效的访问令牌，如果失败返回null</returns>
        public static async Task<string?> GetAccessTokenSimpleAsync()
        {
            try
            {
                var accountManager = new KDEOAuth2Manager();
                var apiClient = new OAuth2ApiClient("https://your-oauth-server.com");
                var tokenManager = new TokenManager(accountManager, apiClient);

                return await tokenManager.GetValidAccessTokenAsync();
            }
            catch
            {
                return null;
            }
        }
    }
}