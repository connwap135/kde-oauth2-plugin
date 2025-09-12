#!/usr/bin/env python3
"""
OAuth2凭证管理器 - 根据readme.md中的说明实现
用于获取和测试KDE账户系统中存储的OAuth2凭证
"""

import sqlite3
import os
import json
import sys
import argparse
from datetime import datetime

class OAuth2CredentialManager:
    def __init__(self, db_path=None):
        if db_path is None:
            db_path = os.path.expanduser("~/.config/libaccounts-glib/accounts.db")
        self.db_path = db_path

    def get_credentials(self, account_id=None, provider="gzweibo-oauth2"):
        """获取OAuth2凭证"""
        if not os.path.exists(self.db_path):
            print(f"错误: 数据库文件不存在: {self.db_path}")
            return None

        try:
            conn = sqlite3.connect(self.db_path)
            cursor = conn.cursor()

            # 如果没有指定账户ID，查找第一个匹配的账户
            if account_id is None:
                cursor.execute(
                    "SELECT id FROM Accounts WHERE provider=? AND enabled=1 ORDER BY id DESC LIMIT 1",
                    (provider,)
                )
                result = cursor.fetchone()
                if not result:
                    print(f"错误: 未找到启用的{provider}账户")
                    return None
                account_id = result[0]

            # 获取所有设置
            cursor.execute(
                "SELECT key, value FROM Settings WHERE account=?",
                (account_id,)
            )
            settings = dict(cursor.fetchall())

            conn.close()

            # 清理字符串值（移除SQLite的引号）
            for key, value in settings.items():
                if isinstance(value, str) and value.startswith("'") and value.endswith("'"):
                    settings[key] = value[1:-1]

            return settings

        except Exception as e:
            print(f"错误: 获取凭证失败: {e}")
            return None

    def list_accounts(self, provider="gzweibo-oauth2"):
        """列出所有账户"""
        if not os.path.exists(self.db_path):
            print(f"错误: 数据库文件不存在: {self.db_path}")
            return []

        try:
            conn = sqlite3.connect(self.db_path)
            cursor = conn.cursor()

            cursor.execute(
                "SELECT id, name, provider, enabled FROM Accounts WHERE provider=?",
                (provider,)
            )
            accounts = cursor.fetchall()
            conn.close()

            return accounts

        except Exception as e:
            print(f"错误: 列出账户失败: {e}")
            return []

    def test_token_validity(self, access_token, server_url):
        """测试令牌有效性"""
        try:
            import requests
        except ImportError:
            print("警告: 未安装requests库，无法测试令牌有效性")
            print("请运行: pip install requests")
            return False

        try:
            headers = {'Authorization': f'Bearer {access_token}'}
            response = requests.get(f'{server_url}/connect/userinfo', headers=headers, timeout=10)

            if response.status_code == 200:
                print("✅ 令牌有效")
                user_info = response.json()
                print(f"用户信息: {json.dumps(user_info, indent=2, ensure_ascii=False)}")
                return True
            else:
                print(f"❌ 令牌无效 (HTTP {response.status_code})")
                print(f"响应: {response.text}")
                return False

        except Exception as e:
            print(f"❌ 令牌测试失败: {e}")
            return False

def main():
    parser = argparse.ArgumentParser(description='OAuth2凭证管理器')
    parser.add_argument('--list', action='store_true', help='列出所有账户')
    parser.add_argument('--account', type=int, help='指定账户ID')
    parser.add_argument('--json', action='store_true', help='JSON格式输出')
    parser.add_argument('--test', action='store_true', help='测试令牌有效性')
    parser.add_argument('--provider', default='gzweibo-oauth2', help='提供者名称')

    args = parser.parse_args()

    manager = OAuth2CredentialManager()

    if args.list:
        accounts = manager.list_accounts(args.provider)
        if args.json:
            print(json.dumps([{
                'id': acc[0],
                'name': acc[1],
                'provider': acc[2],
                'enabled': bool(acc[3])
            } for acc in accounts], indent=2, ensure_ascii=False))
        else:
            print("账户列表:")
            print("ID\t名称\t\t提供者\t\t启用")
            print("-" * 50)
            for account in accounts:
                enabled = "是" if account[3] else "否"
                print(f"{account[0]}\t{account[1]}\t\t{account[2]}\t\t{enabled}")
        return

    # 获取凭证
    credentials = manager.get_credentials(args.account, args.provider)

    if not credentials:
        sys.exit(1)

    if args.json:
        print(json.dumps(credentials, indent=2, ensure_ascii=False))
    else:
        print("=== OAuth2凭证信息 ===")
        print(f"账户ID: {args.account or '自动检测'}")
        print(f"用户名: {credentials.get('username', 'N/A')}")
        print(f"服务器: {credentials.get('server', 'N/A')}")
        print(f"客户端ID: {credentials.get('client_id', 'N/A')}")

        access_token = credentials.get('access_token')
        if access_token:
            print(f"访问令牌: {access_token[:50]}...")
        else:
            print("访问令牌: 未找到")

        refresh_token = credentials.get('refresh_token')
        if refresh_token:
            print(f"刷新令牌: {refresh_token}")
        else:
            print("刷新令牌: 未找到")

        # 显示JWT过期时间
        exp = credentials.get('exp')
        if exp:
            try:
                exp_time = datetime.fromtimestamp(int(exp))
                print(f"令牌过期: {exp_time}")
                now = datetime.now()
                if exp_time > now:
                    remaining = exp_time - now
                    print(f"剩余时间: {remaining}")
                else:
                    print("❌ 令牌已过期")
            except:
                print(f"令牌过期时间戳: {exp}")

        print(f"用户ID: {credentials.get('user_id', 'N/A')}")
        print(f"角色: {credentials.get('role', 'N/A')}")

        portrait_url = credentials.get('portrait_url')
        if portrait_url:
            print(f"头像URL: {portrait_url}")

    # 测试令牌有效性
    if args.test:
        print("\n测试令牌有效性...")
        access_token = credentials.get('access_token')
        server = credentials.get('server')
        if access_token and server:
            manager.test_token_validity(access_token, server)
        else:
            print("❌ 缺少访问令牌或服务器信息")

if __name__ == "__main__":
    main()