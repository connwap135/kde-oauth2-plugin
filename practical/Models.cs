using System.Text.Json.Serialization;

namespace KDEOAuth2Client.Models
{
    /// <summary>
    /// OAuth2 凭证信息
    /// </summary>
    public class OAuth2Credentials
    {
        /// <summary>
        /// 访问令牌
        /// </summary>
        public string? AccessToken { get; set; }

        /// <summary>
        /// 刷新令牌
        /// </summary>
        public string? RefreshToken { get; set; }

        /// <summary>
        /// 服务器地址
        /// </summary>
        public string? Server { get; set; }

        /// <summary>
        /// 客户端ID
        /// </summary>
        public string? ClientId { get; set; }

        /// <summary>
        /// 用户名
        /// </summary>
        public string? Username { get; set; }

        /// <summary>
        /// 账户显示名称
        /// </summary>
        public string? DisplayName { get; set; }

        /// <summary>
        /// 账户ID
        /// </summary>
        public int AccountId { get; set; }

        /// <summary>
        /// 令牌过期时间（秒）
        /// </summary>
        public int ExpiresIn { get; set; }

        /// <summary>
        /// 是否有效
        /// </summary>
        public bool IsValid => !string.IsNullOrEmpty(AccessToken) && !string.IsNullOrEmpty(Server);
    }

    /// <summary>
    /// KDE 账户信息
    /// </summary>
    public class AccountInfo
    {
        public int Id { get; set; }
        public string? DisplayName { get; set; }
        public string? Provider { get; set; }
        public bool Enabled { get; set; }
    }

    /// <summary>
    /// 用户信息响应
    /// </summary>
    public class UserInfo
    {
        [JsonPropertyName("sub")]
        public string? Sub { get; set; }

        [JsonPropertyName("name")]
        public string? Name { get; set; }

        [JsonPropertyName("email")]
        public string? Email { get; set; }

        [JsonPropertyName("preferred_username")]
        public string? PreferredUsername { get; set; }

        [JsonPropertyName("picture")]
        public string? Picture { get; set; }

        [JsonPropertyName("given_name")]
        public string? GivenName { get; set; }

        [JsonPropertyName("family_name")]
        public string? FamilyName { get; set; }
    }

    /// <summary>
    /// API 响应基类
    /// </summary>
    public class ApiResponse<T>
    {
        public bool Success { get; set; }
        public string? Message { get; set; }
        public T? Data { get; set; }
        public string? ErrorCode { get; set; }
    }

    /// <summary>
    /// 令牌状态信息
    /// </summary>
    public class TokenStatus
    {
        /// <summary>
        /// 令牌是否有效
        /// </summary>
        public bool IsValid { get; set; }

        /// <summary>
        /// 令牌是否过期
        /// </summary>
        public bool IsExpired { get; set; }

        /// <summary>
        /// 状态说明
        /// </summary>
        public string Reason { get; set; } = string.Empty;

        /// <summary>
        /// 过期时间
        /// </summary>
        public DateTime? ExpiresAt { get; set; }

        /// <summary>
        /// 剩余有效时间
        /// </summary>
        public TimeSpan? RemainingTime { get; set; }

        /// <summary>
        /// 格式化的剩余时间显示
        /// </summary>
        public string RemainingTimeFormatted
        {
            get
            {
                if (!RemainingTime.HasValue)
                    return "未知";

                var time = RemainingTime.Value;
                if (time.TotalDays >= 1)
                    return $"{(int)time.TotalDays}天{time.Hours}小时";
                else if (time.TotalHours >= 1)
                    return $"{(int)time.TotalHours}小时{time.Minutes}分钟";
                else if (time.TotalMinutes >= 1)
                    return $"{(int)time.TotalMinutes}分钟";
                else
                    return $"{(int)time.TotalSeconds}秒";
            }
        }
    }

    /// <summary>
    /// 过期状态信息
    /// </summary>
    public class ExpirationStatus
    {
        /// <summary>
        /// 是否有过期时间信息
        /// </summary>
        public bool HasExpiration { get; set; }

        /// <summary>
        /// 令牌创建时间
        /// </summary>
        public DateTime? CreatedAt { get; set; }

        /// <summary>
        /// 过期时间
        /// </summary>
        public DateTime? ExpiresAt { get; set; }

        /// <summary>
        /// 是否已过期
        /// </summary>
        public bool IsExpired { get; set; }

        /// <summary>
        /// 剩余时间
        /// </summary>
        public TimeSpan? RemainingTime { get; set; }
    }

    /// <summary>
    /// 令牌时间戳信息
    /// </summary>
    public class TokenTimestampInfo
    {
        /// <summary>
        /// 是否有时间戳信息
        /// </summary>
        public bool HasTimestamp { get; set; }

        /// <summary>
        /// 令牌创建时间
        /// </summary>
        public DateTime? CreatedAt { get; set; }
    }
}