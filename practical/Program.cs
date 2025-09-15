using System;
using System.Threading.Tasks;
using KDEOAuth2Client.Services;

class Program
{
    static async Task Main(string[] args)
    {
        Console.WriteLine("Running GetAccessTokenSimpleAsync...");

        // 添加调试信息
        var accountManager = new KDEOAuth2Manager();
        Console.WriteLine($"Database available: {accountManager.IsDatabaseAvailable()}");
        Console.WriteLine($"Database path: {accountManager.GetDatabasePath()}");

        var accounts = await accountManager.GetAccountsAsync();
        Console.WriteLine($"Found {accounts.Count} accounts");
        if (accounts.Count == 0)
        {
            Console.WriteLine("No accounts found. Please add an account first.");
            return;
        }
        foreach (var account in accounts)
        {
            Console.WriteLine($"Account: {account.Id} - {account.DisplayName} ({account.Provider})");
        }

        var token = await GetAccessTokenSimpleAsync();
        Console.WriteLine($"Token: {token ?? "null"}");
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

            // 1. 先获取凭证以获取服务器地址
            var credentials = await accountManager.GetCredentialsAsync();
            if (credentials == null)
            {
                Console.WriteLine("DEBUG: GetCredentialsAsync returned null");
                return null;
            }

            if (string.IsNullOrEmpty(credentials.Server))
            {
                Console.WriteLine("DEBUG: Server URL not found in credentials");
                return null;
            }

            Console.WriteLine($"DEBUG: Found credentials - AccountId: {credentials.AccountId}, Server: {credentials.Server}, HasToken: {!string.IsNullOrEmpty(credentials.AccessToken)}");

            // 2. 使用获取到的服务器地址创建OAuth2ApiClient
            var apiClient = new OAuth2ApiClient(credentials.Server);
            var tokenManager = new TokenManager(accountManager, apiClient);

            // 3. 获取有效的访问令牌
            return await tokenManager.GetValidAccessTokenAsync();
        }
        catch (Exception ex)
        {
            Console.WriteLine($"DEBUG: Exception in GetAccessTokenSimpleAsync: {ex.Message}");
            return null;
        }
    }
}