#!/bin/bash
# 获取OAuth2访问令牌的脚本

# 查找最新的gzweibo-oauth2账户
ACCOUNT_ID=$(sqlite3 ~/.config/libaccounts-glib/accounts.db "SELECT id FROM Accounts WHERE provider='gzweibo-oauth2' AND enabled=1 ORDER BY id DESC LIMIT 1" 2>/dev/null)

if [ -z "$ACCOUNT_ID" ]; then
    echo "错误: 未找到启用的gzweibo-oauth2账户"
    echo "请先在KDE系统设置中添加OAuth2账户"
    exit 1
fi

# 获取访问令牌
ACCESS_TOKEN=$(sqlite3 ~/.config/libaccounts-glib/accounts.db "SELECT value FROM Settings WHERE account=$ACCOUNT_ID AND key='access_token'" 2>/dev/null | sed "s/^'//;s/'$//")

if [ -z "$ACCESS_TOKEN" ]; then
    echo "错误: 无法获取访问令牌"
    exit 1
fi

echo "账户ID: $ACCOUNT_ID"
echo "访问令牌: ${ACCESS_TOKEN:0:50}..." # 只显示前50个字符保护隐私

# 获取其他有用信息
SERVER=$(sqlite3 ~/.config/libaccounts-glib/accounts.db "SELECT value FROM Settings WHERE account=$ACCOUNT_ID AND key='server'" 2>/dev/null | sed "s/^'//;s/'$//")
USERNAME=$(sqlite3 ~/.config/libaccounts-glib/accounts.db "SELECT value FROM Settings WHERE account=$ACCOUNT_ID AND key='username'" 2>/dev/null | sed "s/^'//;s/'$//")
REFRESH_TOKEN=$(sqlite3 ~/.config/libaccounts-glib/accounts.db "SELECT value FROM Settings WHERE account=$ACCOUNT_ID AND key='refresh_token'" 2>/dev/null | sed "s/^'//;s/'$//")

echo "服务器: $SERVER"
echo "用户名: $USERNAME"
echo "刷新令牌: ${REFRESH_TOKEN:0:30}..." # 只显示前30个字符

# 如果指定了 --full 参数，显示完整的token
if [ "$1" = "--full" ]; then
    echo ""
    echo "=== 完整访问令牌 ==="
    echo "$ACCESS_TOKEN"
    echo ""
    echo "=== 完整刷新令牌 ==="
    echo "$REFRESH_TOKEN"
fi

# 如果指定了 --export 参数，导出环境变量
if [ "$1" = "--export" ]; then
    echo ""
    echo "=== 导出环境变量 ==="
    echo "export OAUTH2_ACCESS_TOKEN='$ACCESS_TOKEN'"
    echo "export OAUTH2_SERVER='$SERVER'"
    echo "export OAUTH2_USERNAME='$USERNAME'"
    echo "export OAUTH2_REFRESH_TOKEN='$REFRESH_TOKEN'"
    echo ""
    echo "使用方法: source <(./get_oauth_token.sh --export)"
fi

# 可选：测试令牌有效性
if command -v curl >/dev/null 2>&1 && [ "$1" != "--export" ]; then
    echo ""
    echo "测试令牌有效性..."
    HTTP_CODE=$(curl -s -o /dev/null -w "%{http_code}" -H "Authorization: Bearer $ACCESS_TOKEN" "$SERVER/connect/userinfo" 2>/dev/null)
    if [ "$HTTP_CODE" = "200" ]; then
        echo "✅ 令牌有效"
    elif [ "$HTTP_CODE" = "401" ]; then
        echo "❌ 令牌已过期或无效 (HTTP $HTTP_CODE)"
    elif [ "$HTTP_CODE" = "000" ]; then
        echo "⚠️  无法连接到服务器"
    else
        echo "⚠️  服务器返回 HTTP $HTTP_CODE"
    fi
fi

echo ""
echo "使用帮助:"
echo "  ./get_oauth_token.sh         # 显示基本信息"
echo "  ./get_oauth_token.sh --full  # 显示完整token"
echo "  ./get_oauth_token.sh --export # 导出环境变量"