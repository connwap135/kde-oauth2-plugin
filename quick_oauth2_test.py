#!/usr/bin/env python3
"""
OAuth2集成快速测试脚本
用于验证OAuth2凭证获取和API调用的完整流程
"""

import os
import sys
import json
from typing import Dict, Any, Optional

class QuickOAuth2Tester:
    def __init__(self):
        self.db_path = os.path.expanduser("~/.config/libaccounts-glib/accounts.db")
        self.credentials = None

    def check_prerequisites(self) -> bool:
        """检查前置条件"""
        print("🔍 检查前置条件...")

        # 检查数据库
        if not os.path.exists(self.db_path):
            print("❌ KDE账户数据库不存在")
            return False
        print("✅ KDE账户数据库存在")

        # 检查sqlite3
        try:
            import sqlite3
        except ImportError:
            print("❌ sqlite3模块未安装")
            return False
        print("✅ sqlite3模块可用")

        return True

    def get_credentials(self) -> Optional[Dict[str, Any]]:
        """获取OAuth2凭证"""
        print("\n🔑 获取OAuth2凭证...")

        try:
            import sqlite3

            conn = sqlite3.connect(self.db_path)
            cursor = conn.cursor()

            # 查找OAuth2账户
            cursor.execute(
                "SELECT id FROM Accounts WHERE provider='gzweibo-oauth2' AND enabled=1 ORDER BY id DESC LIMIT 1"
            )
            result = cursor.fetchone()

            if not result:
                print("❌ 未找到启用的gzweibo-oauth2账户")
                return None

            account_id = result[0]
            print(f"✅ 找到账户ID: {account_id}")

            # 获取所有设置
            cursor.execute(
                "SELECT key, value FROM Settings WHERE account=?",
                (account_id,)
            )

            credentials = {}
            for key, value in cursor.fetchall():
                # 清理SQLite字符串引号
                if isinstance(value, str) and value.startswith("'") and value.endswith("'"):
                    value = value[1:-1]
                credentials[key] = value

            conn.close()

            # 检查必要的凭证
            if 'access_token' not in credentials:
                print("❌ 缺少access_token")
                return None

            print("✅ 成功获取OAuth2凭证")
            print(f"   用户名: {credentials.get('username', 'N/A')}")
            print(f"   服务器: {credentials.get('server', 'N/A')}")
            print(f"   令牌前缀: {credentials.get('access_token', '')[:30]}...")

            return credentials

        except Exception as e:
            print(f"❌ 获取凭证失败: {e}")
            return None

    def test_basic_functionality(self, credentials: Dict[str, Any]) -> bool:
        """测试基本功能"""
        print("\n🧪 测试基本功能...")

        # 检查令牌格式
        token = credentials.get('access_token', '')
        if not token:
            print("❌ 令牌为空")
            return False

        # 检查是否是JWT格式
        if token.count('.') != 2:
            print("⚠️  令牌可能不是JWT格式")
        else:
            print("✅ 令牌格式正确（JWT）")

        # 检查服务器URL
        server = credentials.get('server', '')
        if not server:
            print("❌ 服务器URL为空")
            return False

        if not server.startswith('http'):
            print("⚠️  服务器URL可能不正确")
        else:
            print("✅ 服务器URL格式正确")

        return True

    def generate_usage_examples(self, credentials: Dict[str, Any]):
        """生成使用示例"""
        print("\n💻 使用示例代码:")

        server = credentials.get('server', 'YOUR_SERVER_URL')

        print("=" * 60)
        print("# Python 使用示例")
        print("=" * 60)
        print(f"""
import requests

# 从KDE账户获取的凭证
ACCESS_TOKEN = "{credentials.get('access_token', '')[:50]}..."
SERVER_URL = "{server}"

def call_oauth2_api():
    headers = {{
        'Authorization': f'Bearer {{ACCESS_TOKEN}}',
        'Content-Type': 'application/json'
    }}

    response = requests.get(f'{{SERVER_URL}}/connect/userinfo', headers=headers)
    return response.json()

# 使用
user_info = call_oauth2_api()
print(user_info)
""")

        print("=" * 60)
        print("# Shell 使用示例")
        print("=" * 60)
        print(f"""
# 获取令牌
ACCESS_TOKEN=$(sqlite3 ~/.config/libaccounts-glib/accounts.db \\
    "SELECT value FROM Settings WHERE account=12 AND key='access_token'" | \\
    sed "s/^'//;s/'$//")

# 调用API
curl -H "Authorization: Bearer $ACCESS_TOKEN" \\
     -H "Content-Type: application/json" \\
     {server}/connect/userinfo
""")

    def run_test(self):
        """运行测试"""
        print("🚀 OAuth2集成快速测试")
        print("=" * 50)

        # 检查前置条件
        if not self.check_prerequisites():
            return False

        # 获取凭证
        credentials = self.get_credentials()
        if not credentials:
            return False

        # 测试基本功能
        if not self.test_basic_functionality(credentials):
            return False

        # 生成使用示例
        self.generate_usage_examples(credentials)

        print("\n" + "=" * 50)
        print("🎉 测试完成！")
        print("📖 详细使用指南请参考: OAUTH2_INTEGRATION_GUIDE.md")
        return True

def main():
    tester = QuickOAuth2Tester()
    success = tester.run_test()

    if success:
        print("\n✅ 所有检查通过！您现在可以在您的应用中使用OAuth2凭证了")
    else:
        print("\n❌ 测试失败，请检查配置")
        sys.exit(1)

if __name__ == "__main__":
    main()</content>
<parameter name="filePath">/home/ubuntu/kde-oauth2-plugin/quick_oauth2_test.py