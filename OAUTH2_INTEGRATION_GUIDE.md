# OAuth2凭证访问指南 - 其他应用集成

本文档为其他应用程序开发者提供完整的OAuth2凭证访问和用户信息获取指南。

## 📋 目录

1. [快速开始](#快速开始)
2. [凭证获取方法](#凭证获取方法)
3. [编程语言示例](#编程语言示例)
4. [API调用示例](#api调用示例)
5. [安全注意事项](#安全注意事项)
6. [错误处理](#错误处理)
7. [最佳实践](#最佳实践)
8. [故障排除](#故障排除)

## 🚀 快速开始

### 前置条件

确保您的系统已安装并配置了KDE OAuth2插件：

```bash
# 检查插件是否安装
ls -la /usr/lib/x86_64-linux-gnu/qt5/plugins/kaccounts/ui/gzweibo_oauth2_plugin.so

# 检查账户是否存在
ag-tool list-accounts
```

### 获取凭证

最简单的方式是使用提供的工具脚本：

```bash
cd /path/to/kde-oauth2-plugin

# 获取基本信息
./get_oauth_token.sh

# 导出环境变量（推荐）
source <(./get_oauth_token.sh --export)

# 在Python中获取
python3 oauth2_credentials.py
```

## 🔑 凭证获取方法

### 方法1: Shell脚本 (最简单)

```bash
#!/bin/bash
# 获取OAuth2凭证

# 方法1: 使用工具脚本
source <(./get_oauth_token.sh --export)

# 方法2: 直接查询数据库
ACCESS_TOKEN=$(sqlite3 ~/.config/libaccounts-glib/accounts.db \
    "SELECT value FROM Settings WHERE account=12 AND key='access_token'" | \
    sed "s/^'//;s/'$//")

SERVER=$(sqlite3 ~/.config/libaccounts-glib/accounts.db \
    "SELECT value FROM Settings WHERE account=12 AND key='server'" | \
    sed "s/^'//;s/'$//")

echo "Access Token: $ACCESS_TOKEN"
echo "Server: $SERVER"
```

### 方法2: Python脚本

```python
#!/usr/bin/env python3
import sqlite3
import os
from typing import Optional, Dict, Any

class KDEOAuth2Client:
    def __init__(self, db_path: str = None, account_id: int = None):
        self.db_path = db_path or os.path.expanduser("~/.config/libaccounts-glib/accounts.db")
        self.account_id = account_id

    def get_credentials(self) -> Optional[Dict[str, Any]]:
        """获取OAuth2凭证"""
        if not os.path.exists(self.db_path):
            return None

        try:
            conn = sqlite3.connect(self.db_path)
            cursor = conn.cursor()

            # 查找账户ID
            if self.account_id is None:
                cursor.execute(
                    "SELECT id FROM Accounts WHERE provider='gzweibo-oauth2' AND enabled=1 ORDER BY id DESC LIMIT 1"
                )
                result = cursor.fetchone()
                if not result:
                    return None
                self.account_id = result[0]

            # 获取所有设置
            cursor.execute(
                "SELECT key, value FROM Settings WHERE account=?",
                (self.account_id,)
            )

            credentials = {}
            for key, value in cursor.fetchall():
                # 清理SQLite字符串引号
                if isinstance(value, str) and value.startswith("'") and value.endswith("'"):
                    value = value[1:-1]
                credentials[key] = value

            conn.close()
            return credentials

        except Exception as e:
            print(f"获取凭证失败: {e}")
            return None

# 使用示例
if __name__ == "__main__":
    client = KDEOAuth2Client()
    creds = client.get_credentials()

    if creds:
        print(f"Access Token: {creds.get('access_token', 'N/A')[:50]}...")
        print(f"Server: {creds.get('server', 'N/A')}")
        print(f"Username: {creds.get('username', 'N/A')}")
    else:
        print("未找到OAuth2凭证")
```

### 方法3: Node.js脚本

```javascript
const sqlite3 = require('sqlite3').verbose();
const os = require('os');
const path = require('path');

class KDEOAuth2Client {
    constructor(accountId = null) {
        this.dbPath = path.join(os.homedir(), '.config/libaccounts-glib/accounts.db');
        this.accountId = accountId;
    }

    async getCredentials() {
        return new Promise((resolve, reject) => {
            if (!require('fs').existsSync(this.dbPath)) {
                reject(new Error('KDE账户数据库不存在'));
                return;
            }

            const db = new sqlite3.Database(this.dbPath);

            // 查找账户ID
            const accountQuery = this.accountId
                ? 'SELECT id FROM Accounts WHERE id=? AND enabled=1'
                : "SELECT id FROM Accounts WHERE provider='gzweibo-oauth2' AND enabled=1 ORDER BY id DESC LIMIT 1";

            const accountParams = this.accountId ? [this.accountId] : [];

            db.get(accountQuery, accountParams, (err, row) => {
                if (err) {
                    db.close();
                    reject(err);
                    return;
                }

                if (!row) {
                    db.close();
                    reject(new Error('未找到OAuth2账户'));
                    return;
                }

                const accountId = row.id;

                // 获取凭证
                db.all(
                    "SELECT key, value FROM Settings WHERE account=?",
                    [accountId],
                    (err, rows) => {
                        if (err) {
                            db.close();
                            reject(err);
                            return;
                        }

                        const credentials = {};
                        rows.forEach(row => {
                            let value = row.value;
                            // 清理引号
                            if (typeof value === 'string' && value.startsWith("'") && value.endsWith("'")) {
                                value = value.slice(1, -1);
                            }
                            credentials[row.key] = value;
                        });

                        db.close();
                        resolve(credentials);
                    }
                );
            });
        });
    }
}

// 使用示例
async function main() {
    try {
        const client = new KDEOAuth2Client();
        const credentials = await client.getCredentials();

        console.log('Access Token:', credentials.access_token?.substring(0, 50) + '...');
        console.log('Server:', credentials.server);
        console.log('Username:', credentials.username);

    } catch (error) {
        console.error('获取凭证失败:', error.message);
    }
}

main();
```

## 🌐 API调用示例

### 获取用户信息

```python
import requests
from typing import Dict, Any

def get_user_info(access_token: str, server: str) -> Dict[str, Any]:
    """获取用户信息"""
    headers = {
        'Authorization': f'Bearer {access_token}',
        'Content-Type': 'application/json'
    }

    try:
        response = requests.get(f'{server}/connect/userinfo', headers=headers, timeout=10)
        response.raise_for_status()
        return response.json()
    except requests.RequestException as e:
        print(f"获取用户信息失败: {e}")
        return None

# 使用示例
if __name__ == "__main__":
    # 假设已经获取了凭证
    access_token = "your_access_token_here"
    server = "http://192.168.1.12:9007"

    user_info = get_user_info(access_token, server)
    if user_info:
        print("用户信息:")
        for key, value in user_info.items():
            print(f"  {key}: {value}")
```

### 文件上传示例

```python
import requests

def upload_file(access_token: str, server: str, file_path: str, upload_url: str = None) -> bool:
    """上传文件"""
    if upload_url is None:
        upload_url = f"{server}/api/upload"

    headers = {
        'Authorization': f'Bearer {access_token}'
    }

    try:
        with open(file_path, 'rb') as file:
            files = {'file': file}
            response = requests.post(upload_url, headers=headers, files=files, timeout=30)
            response.raise_for_status()

            print(f"文件上传成功: {response.json()}")
            return True

    except FileNotFoundError:
        print(f"文件不存在: {file_path}")
    except requests.RequestException as e:
        print(f"文件上传失败: {e}")

    return False
```

### 令牌刷新

```python
import requests
from datetime import datetime

def refresh_access_token(refresh_token: str, server: str, client_id: str) -> Dict[str, str]:
    """刷新访问令牌"""
    token_url = f"{server}/connect/token"

    data = {
        'grant_type': 'refresh_token',
        'refresh_token': refresh_token,
        'client_id': client_id
    }

    headers = {
        'Content-Type': 'application/x-www-form-urlencoded'
    }

    try:
        response = requests.post(token_url, data=data, headers=headers, timeout=10)
        response.raise_for_status()

        token_data = response.json()
        return {
            'access_token': token_data.get('access_token'),
            'refresh_token': token_data.get('refresh_token'),
            'expires_in': token_data.get('expires_in')
        }

    except requests.RequestException as e:
        print(f"令牌刷新失败: {e}")
        return None

def is_token_expired(exp_timestamp: int) -> bool:
    """检查令牌是否过期"""
    current_time = datetime.now().timestamp()
    return current_time >= exp_timestamp
```

## 🔒 安全注意事项

### 1. 令牌保护

```python
# ❌ 不安全的做法
print(f"Access Token: {access_token}")  # 不要在日志中打印完整令牌

# ✅ 安全的做法
print(f"Access Token: {access_token[:20]}...")  # 只显示前缀
```

### 2. 存储安全

```python
import keyring

def store_token_securely(token: str, service_name: str = "kde-oauth2"):
    """安全存储令牌"""
    keyring.set_password(service_name, "access_token", token)

def get_token_securely(service_name: str = "kde-oauth2"):
    """安全获取令牌"""
    return keyring.get_password(service_name, "access_token")
```

### 3. HTTPS使用

```python
# 始终使用HTTPS（如果服务器支持）
import requests

# 强制HTTPS
response = requests.get("https://your-server.com/api/data",
                       headers={'Authorization': f'Bearer {token}'},
                       verify=True)  # 验证SSL证书
```

### 4. 令牌过期处理

```python
import time
from typing import Optional

class TokenManager:
    def __init__(self):
        self._access_token = None
        self._refresh_token = None
        self._expires_at = None

    def get_valid_token(self) -> Optional[str]:
        """获取有效的访问令牌"""
        if self._is_token_expired():
            if not self._refresh_token():
                return None
        return self._access_token

    def _is_token_expired(self) -> bool:
        """检查令牌是否过期"""
        if not self._expires_at:
            return True
        return time.time() >= self._expires_at

    def _refresh_token(self) -> bool:
        """刷新令牌"""
        # 实现令牌刷新逻辑
        # 返回True如果刷新成功
        pass
```

## ⚠️ 错误处理

### 网络错误处理

```python
import requests
from requests.exceptions import RequestException, Timeout, ConnectionError

def safe_api_call(url: str, headers: dict, timeout: int = 10):
    """安全的API调用"""
    try:
        response = requests.get(url, headers=headers, timeout=timeout)
        response.raise_for_status()
        return response.json()

    except Timeout:
        print("请求超时")
        return None
    except ConnectionError:
        print("网络连接错误")
        return None
    except requests.HTTPError as e:
        if response.status_code == 401:
            print("令牌无效或过期")
        elif response.status_code == 403:
            print("权限不足")
        else:
            print(f"HTTP错误: {e}")
        return None
    except RequestException as e:
        print(f"请求错误: {e}")
        return None
```

### 数据库错误处理

```python
import sqlite3
from contextlib import contextmanager

@contextmanager
def get_db_connection(db_path: str):
    """安全的数据库连接"""
    conn = None
    try:
        conn = sqlite3.connect(db_path)
        yield conn
    except sqlite3.Error as e:
        print(f"数据库错误: {e}")
        raise
    finally:
        if conn:
            conn.close()

def safe_get_credentials(db_path: str, account_id: int):
    """安全获取凭证"""
    try:
        with get_db_connection(db_path) as conn:
            cursor = conn.cursor()
            cursor.execute(
                "SELECT key, value FROM Settings WHERE account=?",
                (account_id,)
            )
            return dict(cursor.fetchall())
    except Exception as e:
        print(f"获取凭证失败: {e}")
        return None
```

## 📋 最佳实践

### 1. 凭证缓存

```python
import time
from typing import Dict, Any, Optional

class CredentialCache:
    def __init__(self, cache_duration: int = 300):  # 5分钟缓存
        self._cache = {}
        self._cache_time = {}
        self._cache_duration = cache_duration

    def get(self, key: str) -> Optional[Any]:
        """获取缓存的凭证"""
        if key in self._cache:
            if time.time() - self._cache_time[key] < self._cache_duration:
                return self._cache[key]
            else:
                # 缓存过期，清理
                del self._cache[key]
                del self._cache_time[key]
        return None

    def set(self, key: str, value: Any):
        """设置缓存的凭证"""
        self._cache[key] = value
        self._cache_time[key] = time.time()

# 使用示例
cache = CredentialCache()

def get_credentials_with_cache():
    """带缓存的凭证获取"""
    creds = cache.get('oauth2_credentials')
    if creds is None:
        # 从数据库获取
        creds = get_credentials_from_db()
        if creds:
            cache.set('oauth2_credentials', creds)
    return creds
```

### 2. 重试机制

```python
import time
import random

def retry_api_call(func, max_retries: int = 3, base_delay: float = 1.0):
    """API调用重试机制"""
    for attempt in range(max_retries):
        try:
            return func()
        except (requests.Timeout, requests.ConnectionError) as e:
            if attempt == max_retries - 1:
                raise e

            # 指数退避 + 随机抖动
            delay = base_delay * (2 ** attempt) + random.uniform(0, 1)
            print(f"请求失败，重试 {attempt + 1}/{max_retries}，等待 {delay:.2f} 秒")
            time.sleep(delay)

    return None
```

### 3. 日志记录

```python
import logging
import sys

# 配置日志
logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s - %(name)s - %(levelname)s - %(message)s',
    handlers=[
        logging.FileHandler('oauth2_client.log'),
        logging.StreamHandler(sys.stdout)
    ]
)

logger = logging.getLogger('OAuth2Client')

class OAuth2Client:
    def __init__(self):
        self.logger = logging.getLogger('OAuth2Client')

    def get_credentials(self):
        """获取凭证（带日志）"""
        self.logger.info("开始获取OAuth2凭证")

        try:
            creds = self._fetch_credentials()
            self.logger.info("成功获取OAuth2凭证")
            return creds
        except Exception as e:
            self.logger.error(f"获取凭证失败: {e}")
            raise
```

## 🔧 故障排除

### 常见问题

#### 1. "KDE账户数据库不存在"

**原因**: KDE账户系统未初始化或数据库损坏

**解决**:
```bash
# 检查数据库是否存在
ls -la ~/.config/libaccounts-glib/accounts.db

# 如果不存在，尝试重新创建账户
# 在KDE系统设置中添加OAuth2账户
```

#### 2. "未找到OAuth2账户"

**原因**: 没有配置OAuth2账户或账户被禁用

**解决**:
```bash
# 检查账户状态
ag-tool list-accounts

# 启用账户（如果被禁用）
ag-tool set-account 12 enabled true
```

#### 3. "令牌无效或过期"

**原因**: Access Token已过期

**解决**:
```python
# 检查令牌过期时间
import jwt

def check_token_expiry(token: str) -> bool:
    try:
        payload = jwt.decode(token, options={"verify_signature": False})
        exp = payload.get('exp')
        return time.time() >= exp
    except:
        return True

# 如果过期，使用refresh_token获取新令牌
```

#### 4. "网络连接错误"

**原因**: 服务器不可达或网络问题

**解决**:
```bash
# 检查网络连接
ping your-oauth2-server.com

# 检查服务器状态
curl -I http://your-oauth2-server.com/health

# 检查防火墙设置
sudo ufw status
```

### 调试技巧

#### 启用详细日志

```bash
# 启用OAuth2客户端调试
export OAUTH2_DEBUG=1

# 启用HTTP请求调试
export REQUESTS_DEBUG=1

# 查看KDE账户调试信息
export QT_LOGGING_RULES="kaccounts.debug=true"
```

#### 验证令牌

```python
import jwt

def validate_jwt_token(token: str) -> dict:
    """验证JWT令牌（不验证签名）"""
    try:
        # 解码而不验证签名
        payload = jwt.decode(token, options={"verify_signature": False})
        return payload
    except jwt.ExpiredSignatureError:
        print("令牌已过期")
    except jwt.InvalidTokenError:
        print("无效令牌")
    return {}
```

## 📚 完整示例应用

### 命令行工具

```python
#!/usr/bin/env python3
"""
OAuth2 API客户端 - 命令行工具
"""

import argparse
import json
import sys
from pathlib import Path

# 导入我们的OAuth2客户端
from kde_oauth2_client import KDEOAuth2Client

def main():
    parser = argparse.ArgumentParser(description='OAuth2 API客户端')
    parser.add_argument('--list', action='store_true', help='列出所有账户')
    parser.add_argument('--account', type=int, help='指定账户ID')
    parser.add_argument('--get', help='获取用户信息')
    parser.add_argument('--upload', help='上传文件')
    parser.add_argument('--test', action='store_true', help='测试令牌有效性')

    args = parser.parse_args()

    client = KDEOAuth2Client()

    if args.list:
        accounts = client.list_accounts()
        for account in accounts:
            print(f"ID: {account[0]}, 名称: {account[1]}, 提供者: {account[2]}")
        return

    # 获取凭证
    credentials = client.get_credentials(args.account)
    if not credentials:
        print("未找到OAuth2凭证")
        sys.exit(1)

    access_token = credentials.get('access_token')
    server = credentials.get('server')

    if args.test:
        if client.test_token_validity(access_token, server):
            print("✅ 令牌有效")
        else:
            print("❌ 令牌无效")
            sys.exit(1)

    elif args.get:
        user_info = client.get_user_info(access_token, server)
        if user_info:
            print(json.dumps(user_info, indent=2, ensure_ascii=False))

    elif args.upload:
        file_path = Path(args.upload)
        if not file_path.exists():
            print(f"文件不存在: {file_path}")
            sys.exit(1)

        if client.upload_file(access_token, server, str(file_path)):
            print("✅ 文件上传成功")
        else:
            print("❌ 文件上传失败")
            sys.exit(1)

if __name__ == "__main__":
    main()
```

### Web应用集成

```python
# Flask Web应用示例
from flask import Flask, request, jsonify, g
import requests
from functools import wraps

app = Flask(__name__)

class OAuth2Middleware:
    def __init__(self):
        self.client = KDEOAuth2Client()

    def get_credentials(self):
        return self.client.get_credentials()

# 创建中间件实例
oauth2 = OAuth2Middleware()

def require_oauth2(f):
    @wraps(f)
    def decorated_function(*args, **kwargs):
        # 获取凭证
        creds = oauth2.get_credentials()
        if not creds:
            return jsonify({'error': 'OAuth2 credentials not found'}), 401

        # 验证令牌
        if not oauth2.client.test_token_validity(
            creds['access_token'],
            creds['server']
        ):
            return jsonify({'error': 'Invalid or expired token'}), 401

        # 将凭证存储在请求上下文中
        g.oauth2_creds = creds
        return f(*args, **kwargs)
    return decorated_function

@app.route('/api/user')
@require_oauth2
def get_user():
    """获取当前用户信息"""
    creds = g.oauth2_creds
    user_info = oauth2.client.get_user_info(
        creds['access_token'],
        creds['server']
    )

    if user_info:
        return jsonify(user_info)
    else:
        return jsonify({'error': 'Failed to get user info'}), 500

@app.route('/api/upload', methods=['POST'])
@require_oauth2
def upload_file():
    """上传文件"""
    if 'file' not in request.files:
        return jsonify({'error': 'No file provided'}), 400

    file = request.files['file']
    creds = g.oauth2_creds

    # 保存文件到临时位置
    temp_path = f"/tmp/{file.filename}"
    file.save(temp_path)

    # 上传到OAuth2服务器
    if oauth2.client.upload_file(
        creds['access_token'],
        creds['server'],
        temp_path
    ):
        return jsonify({'message': 'File uploaded successfully'})
    else:
        return jsonify({'error': 'Upload failed'}), 500

if __name__ == '__main__':
    app.run(debug=True)
```

---

## 📞 支持

如果您在使用过程中遇到问题，请：

1. 检查本文档的故障排除部分
2. 查看应用程序日志
3. 验证OAuth2服务器状态
4. 联系技术支持团队

## 📄 许可证

本指南遵循与KDE OAuth2插件相同的许可证。</content>
<parameter name="filePath">/home/ubuntu/kde-oauth2-plugin/OAUTH2_INTEGRATION_GUIDE.md