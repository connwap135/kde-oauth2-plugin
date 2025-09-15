using System.Text.Json;
using KDEOAuth2Client.Models;

namespace KDEOAuth2Client.Services
{
    /// <summary>
    /// OAuth2 API 客户端 - 用于调用 gzweibo API
    /// </summary>
    public class OAuth2ApiClient : IDisposable
    {
        private readonly HttpClient _httpClient;
        private readonly string _baseUrl;
        private bool _disposed = false;

        /// <summary>
        /// 初始化 OAuth2 API 客户端
        /// </summary>
        /// <param name="baseUrl">API 基础URL</param>
        public OAuth2ApiClient(string baseUrl)
        {
            _baseUrl = baseUrl.TrimEnd('/');
            _httpClient = new HttpClient();
            _httpClient.Timeout = TimeSpan.FromSeconds(30);
        }

        /// <summary>
        /// 设置访问令牌
        /// </summary>
        /// <param name="accessToken">访问令牌</param>
        public void SetAccessToken(string accessToken)
        {
            _httpClient.DefaultRequestHeaders.Authorization =
                new System.Net.Http.Headers.AuthenticationHeaderValue("Bearer", accessToken);
        }

        /// <summary>
        /// 获取用户信息
        /// </summary>
        /// <returns>用户信息</returns>
        public async Task<ApiResponse<UserInfo>> GetUserInfoAsync()
        {
            try
            {
                var response = await _httpClient.GetAsync($"{_baseUrl}/connect/userinfo");
                
                if (response.IsSuccessStatusCode)
                {
                    var json = await response.Content.ReadAsStringAsync();
                    var userInfo = JsonSerializer.Deserialize<UserInfo>(json, new JsonSerializerOptions
                    {
                        PropertyNameCaseInsensitive = true
                    });

                    return new ApiResponse<UserInfo>
                    {
                        Success = true,
                        Data = userInfo,
                        Message = "获取用户信息成功"
                    };
                }
                else
                {
                    var errorContent = await response.Content.ReadAsStringAsync();
                    return new ApiResponse<UserInfo>
                    {
                        Success = false,
                        Message = $"API请求失败: {response.StatusCode}",
                        ErrorCode = response.StatusCode.ToString()
                    };
                }
            }
            catch (HttpRequestException ex)
            {
                return new ApiResponse<UserInfo>
                {
                    Success = false,
                    Message = $"网络请求失败: {ex.Message}",
                    ErrorCode = "NETWORK_ERROR"
                };
            }
            catch (TaskCanceledException)
            {
                return new ApiResponse<UserInfo>
                {
                    Success = false,
                    Message = "请求超时",
                    ErrorCode = "TIMEOUT"
                };
            }
            catch (JsonException ex)
            {
                return new ApiResponse<UserInfo>
                {
                    Success = false,
                    Message = $"JSON解析失败: {ex.Message}",
                    ErrorCode = "JSON_PARSE_ERROR"
                };
            }
            catch (Exception ex)
            {
                return new ApiResponse<UserInfo>
                {
                    Success = false,
                    Message = $"未知错误: {ex.Message}",
                    ErrorCode = "UNKNOWN_ERROR"
                };
            }
        }

        /// <summary>
        /// 测试令牌有效性
        /// </summary>
        /// <returns>令牌是否有效</returns>
        public async Task<bool> TestTokenAsync()
        {
            try
            {
                var response = await _httpClient.GetAsync($"{_baseUrl}/connect/userinfo");
                return response.IsSuccessStatusCode;
            }
            catch
            {
                return false;
            }
        }

        /// <summary>
        /// 刷新访问令牌
        /// </summary>
        /// <param name="accessToken">当前访问令牌</param>
        /// <param name="refreshToken">刷新令牌</param>
        /// <param name="clientId">客户端ID</param>
        /// <returns>新的访问令牌信息</returns>
        public async Task<ApiResponse<OAuth2Credentials>> RefreshTokenAsync(string accessToken, string refreshToken, string clientId)
        {
            try
            {
                var parameters = new Dictionary<string, string>
                {
                    {"grant_type", "refresh_token"},
                    {"access_token", accessToken},
                    {"refresh_token", refreshToken},
                    {"client_id", clientId}
                };

                var content = new FormUrlEncodedContent(parameters);
                var response = await _httpClient.PostAsync($"{_baseUrl}/connect/token", content);

                if (response.IsSuccessStatusCode)
                {
                    var json = await response.Content.ReadAsStringAsync();
                    var tokenResponse = JsonSerializer.Deserialize<Dictionary<string, object>>(json);

                    if (tokenResponse == null)
                    {
                        return new ApiResponse<OAuth2Credentials>
                        {
                            Success = false,
                            Message = "令牌响应解析失败",
                            ErrorCode = "PARSE_ERROR"
                        };
                    }

                    var credentials = new OAuth2Credentials
                    {
                        AccessToken = tokenResponse.GetValueOrDefault("access_token")?.ToString(),
                        RefreshToken = tokenResponse.GetValueOrDefault("refresh_token")?.ToString(),
                        ExpiresIn = int.Parse(tokenResponse.GetValueOrDefault("expires_in")?.ToString() ?? "0")
                    };

                    return new ApiResponse<OAuth2Credentials>
                    {
                        Success = true,
                        Data = credentials,
                        Message = "令牌刷新成功"
                    };
                }
                else
                {
                    var errorContent = await response.Content.ReadAsStringAsync();
                    return new ApiResponse<OAuth2Credentials>
                    {
                        Success = false,
                        Message = $"令牌刷新失败: {response.StatusCode}",
                        ErrorCode = response.StatusCode.ToString()
                    };
                }
            }
            catch (Exception ex)
            {
                return new ApiResponse<OAuth2Credentials>
                {
                    Success = false,
                    Message = $"刷新令牌时发生错误: {ex.Message}",
                    ErrorCode = "REFRESH_ERROR"
                };
            }
        }

        /// <summary>
        /// 发送GET请求到指定端点
        /// </summary>
        /// <param name="endpoint">API端点</param>
        /// <returns>响应内容</returns>
        public async Task<ApiResponse<string>> GetAsync(string endpoint)
        {
            try
            {
                var response = await _httpClient.GetAsync($"{_baseUrl}/{endpoint.TrimStart('/')}");
                var content = await response.Content.ReadAsStringAsync();

                return new ApiResponse<string>
                {
                    Success = response.IsSuccessStatusCode,
                    Data = content,
                    Message = response.IsSuccessStatusCode ? "请求成功" : $"请求失败: {response.StatusCode}",
                    ErrorCode = response.IsSuccessStatusCode ? null : response.StatusCode.ToString()
                };
            }
            catch (Exception ex)
            {
                return new ApiResponse<string>
                {
                    Success = false,
                    Message = $"请求失败: {ex.Message}",
                    ErrorCode = "REQUEST_ERROR"
                };
            }
        }

        /// <summary>
        /// 发送POST请求到指定端点
        /// </summary>
        /// <param name="endpoint">API端点</param>
        /// <param name="jsonData">JSON数据</param>
        /// <returns>响应内容</returns>
        public async Task<ApiResponse<string>> PostAsync(string endpoint, string jsonData)
        {
            try
            {
                var content = new StringContent(jsonData, System.Text.Encoding.UTF8, "application/json");
                var response = await _httpClient.PostAsync($"{_baseUrl}/{endpoint.TrimStart('/')}", content);
                var responseContent = await response.Content.ReadAsStringAsync();

                return new ApiResponse<string>
                {
                    Success = response.IsSuccessStatusCode,
                    Data = responseContent,
                    Message = response.IsSuccessStatusCode ? "请求成功" : $"请求失败: {response.StatusCode}",
                    ErrorCode = response.IsSuccessStatusCode ? null : response.StatusCode.ToString()
                };
            }
            catch (Exception ex)
            {
                return new ApiResponse<string>
                {
                    Success = false,
                    Message = $"请求失败: {ex.Message}",
                    ErrorCode = "REQUEST_ERROR"
                };
            }
        }

        /// <summary>
        /// 上传文件
        /// </summary>
        /// <param name="endpoint">上传端点</param>
        /// <param name="filePath">文件路径</param>
        /// <param name="fieldName">表单字段名</param>
        /// <returns>上传结果</returns>
        public async Task<ApiResponse<string>> UploadFileAsync(string endpoint, string filePath, string fieldName = "file")
        {
            try
            {
                if (!File.Exists(filePath))
                {
                    return new ApiResponse<string>
                    {
                        Success = false,
                        Message = "文件不存在",
                        ErrorCode = "FILE_NOT_FOUND"
                    };
                }

                using var form = new MultipartFormDataContent();
                using var fileStream = File.OpenRead(filePath);
                using var fileContent = new StreamContent(fileStream);
                
                fileContent.Headers.ContentType = new System.Net.Http.Headers.MediaTypeHeaderValue("application/octet-stream");
                form.Add(fileContent, fieldName, Path.GetFileName(filePath));

                var response = await _httpClient.PostAsync($"{_baseUrl}/{endpoint.TrimStart('/')}", form);
                var responseContent = await response.Content.ReadAsStringAsync();

                return new ApiResponse<string>
                {
                    Success = response.IsSuccessStatusCode,
                    Data = responseContent,
                    Message = response.IsSuccessStatusCode ? "文件上传成功" : $"文件上传失败: {response.StatusCode}",
                    ErrorCode = response.IsSuccessStatusCode ? null : response.StatusCode.ToString()
                };
            }
            catch (Exception ex)
            {
                return new ApiResponse<string>
                {
                    Success = false,
                    Message = $"文件上传失败: {ex.Message}",
                    ErrorCode = "UPLOAD_ERROR"
                };
            }
        }

        /// <summary>
        /// 释放资源
        /// </summary>
        public void Dispose()
        {
            Dispose(true);
            GC.SuppressFinalize(this);
        }

        protected virtual void Dispose(bool disposing)
        {
            if (!_disposed)
            {
                if (disposing)
                {
                    _httpClient?.Dispose();
                }
                _disposed = true;
            }
        }
    }
}