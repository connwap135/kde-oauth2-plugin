#!/bin/bash

# 创建测试数据的脚本

DB_PATH="$HOME/.config/libaccounts-glib/accounts.db"

echo "=== 创建测试 OAuth2 账户数据 ==="
echo

# 备份原始数据库
if [ -f "$DB_PATH" ]; then
    cp "$DB_PATH" "$DB_PATH.backup.$(date +%s)"
    echo "✅ 已备份原始数据库"
fi

# 插入测试账户
sqlite3 "$DB_PATH" <<EOF
-- 插入测试账户
INSERT INTO Accounts (name, provider, enabled) VALUES ('Test OAuth2 Account', 'gzweibo-oauth2', 1);

-- 获取账户ID
SELECT 'Account ID: ' || last_insert_rowid() as info;

-- 插入OAuth2设置
INSERT INTO Settings (account, service, key, type, value) VALUES 
(last_insert_rowid(), NULL, 'access_token', 'string', 'test_access_token_abcdef123456789'),
(last_insert_rowid(), NULL, 'refresh_token', 'string', 'test_refresh_token_987654321'),
(last_insert_rowid(), NULL, 'server', 'string', 'http://192.168.1.12:9007'),
(last_insert_rowid(), NULL, 'client_id', 'string', '10001'),
(last_insert_rowid(), NULL, 'username', 'string', 'testuser'),
(last_insert_rowid(), NULL, 'expires_in', 'string', '3600');

-- 验证插入的数据
SELECT 'Inserted account:' as info;
SELECT id, name, provider, enabled FROM Accounts WHERE provider = 'gzweibo-oauth2';

SELECT '';
SELECT 'Settings for account:' as info;
SELECT s.account, s.key, s.value 
FROM Settings s 
JOIN Accounts a ON s.account = a.id 
WHERE a.provider = 'gzweibo-oauth2';
EOF

echo
echo "✅ 测试数据创建完成！"
echo
echo "现在可以运行以下命令测试 C# 客户端："
echo "  cd /home/ubuntu/kde-oauth2-plugin/csharp-oauth2-client"
echo "  dotnet run --list"
echo "  dotnet run --stats" 
echo "  dotnet run --userinfo"
echo "  dotnet run"
echo
echo "要恢复原始数据库，请运行："
echo "  rm $DB_PATH"
echo "  mv $DB_PATH.backup.* $DB_PATH"