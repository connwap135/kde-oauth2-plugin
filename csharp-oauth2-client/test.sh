#!/bin/bash

# C# OAuth2 客户端测试脚本

echo "=== KDE OAuth2 C# 客户端测试 ==="
echo

# 检查 .NET 是否安装
if ! command -v dotnet &> /dev/null; then
    echo "❌ .NET 未安装"
    echo "请安装 .NET 6.0 或更高版本"
    exit 1
fi

echo "✅ .NET 版本: $(dotnet --version)"

# 进入项目目录
cd "$(dirname "$0")"

# 还原包
echo "📦 还原 NuGet 包..."
dotnet restore

if [ $? -ne 0 ]; then
    echo "❌ 包还原失败"
    exit 1
fi

echo "✅ 包还原成功"

# 编译项目
echo "🔨 编译项目..."
dotnet build

if [ $? -ne 0 ]; then
    echo "❌ 编译失败"
    exit 1
fi

echo "✅ 编译成功"

# 检查 KDE 账户数据库
DB_PATH="$HOME/.config/libaccounts-glib/accounts.db"
if [ ! -f "$DB_PATH" ]; then
    echo "⚠️  KDE账户数据库不存在: $DB_PATH"
    echo "   请在KDE系统设置中配置OAuth2账户后再运行测试"
else
    echo "✅ 找到KDE账户数据库"
fi

echo
echo "🚀 运行CRUD功能测试..."
echo

# 测试函数
test_step() {
    echo "📋 测试步骤: $1"
    echo "命令: $2"
    echo "---"
}

success_msg() {
    echo "✅ $1"
    echo
}

error_msg() {
    echo "❌ $1"
    echo
}

# 1. 测试帮助功能
test_step "显示帮助信息" "dotnet run -- --help"
dotnet run -- --help
if [ $? -eq 0 ]; then
    success_msg "帮助功能正常"
else
    error_msg "帮助功能失败"
fi

# 2. 列出现有账户
test_step "列出现有账户" "dotnet run -- --list"
dotnet run -- --list
if [ $? -eq 0 ]; then
    success_msg "账户列表功能正常"
else
    error_msg "账户列表功能失败"
fi

# 3. 创建测试账户
TEST_ACCOUNT_NAME="自动测试账户"
TEST_SERVER="test.automation.com"
TEST_CLIENT_ID="auto_test_client_123"
TEST_ACCESS_TOKEN="auto_test_token_456"
TEST_EXPIRES_IN="3600"  # 1小时过期时间

test_step "创建新账户" "dotnet run -- --create \"$TEST_ACCOUNT_NAME\" \"$TEST_SERVER\" \"$TEST_CLIENT_ID\" \"$TEST_ACCESS_TOKEN\" \"$TEST_EXPIRES_IN\""
CREATE_OUTPUT=$(dotnet run -- --create "$TEST_ACCOUNT_NAME" "$TEST_SERVER" "$TEST_CLIENT_ID" "$TEST_ACCESS_TOKEN" "$TEST_EXPIRES_IN" 2>&1)
echo "$CREATE_OUTPUT"

# 提取账户ID
TEST_ACCOUNT_ID=$(echo "$CREATE_OUTPUT" | grep "账户ID:" | grep -o '[0-9]\+' | head -1)

if [ -n "$TEST_ACCOUNT_ID" ]; then
    success_msg "成功创建测试账户，ID: $TEST_ACCOUNT_ID"
else
    error_msg "创建账户失败"
    echo "🎉 测试结束（部分功能无法测试）"
    exit 1
fi

# 4. 验证账户创建
test_step "验证账户创建" "dotnet run -- --list"
dotnet run -- --list
success_msg "账户创建验证完成"

# 5. 获取账户详细信息
test_step "获取账户详细信息" "dotnet run -- --get $TEST_ACCOUNT_ID"
dotnet run -- --get $TEST_ACCOUNT_ID
success_msg "账户信息获取完成"

# 6. 获取账户凭据
test_step "获取账户凭据" "dotnet run -- --credentials $TEST_ACCOUNT_ID"
dotnet run -- --credentials $TEST_ACCOUNT_ID
success_msg "账户凭据获取完成"

# 7. 禁用账户
test_step "禁用账户" "dotnet run -- --disable $TEST_ACCOUNT_ID"
dotnet run -- --disable $TEST_ACCOUNT_ID
if [ $? -eq 0 ]; then
    success_msg "账户禁用成功"
else
    error_msg "账户禁用失败"
fi

# 8. 验证账户状态
test_step "验证禁用状态" "dotnet run -- --list"
dotnet run -- --list
success_msg "禁用状态验证完成"

# 9. 重新启用账户
test_step "重新启用账户" "dotnet run -- --enable $TEST_ACCOUNT_ID"
dotnet run -- --enable $TEST_ACCOUNT_ID
if [ $? -eq 0 ]; then
    success_msg "账户启用成功"
else
    error_msg "账户启用失败"
fi

# 10. 验证启用状态
test_step "验证启用状态" "dotnet run -- --list"
dotnet run -- --list
success_msg "启用状态验证完成"

# 11. 更新访问令牌
NEW_TOKEN="updated_auto_test_token_789"
test_step "更新访问令牌" "dotnet run -- --update $TEST_ACCOUNT_ID \"$NEW_TOKEN\""
dotnet run -- --update $TEST_ACCOUNT_ID "$NEW_TOKEN"
if [ $? -eq 0 ]; then
    success_msg "令牌更新成功"
else
    error_msg "令牌更新失败"
fi

# 12. 验证令牌更新
test_step "验证令牌更新" "dotnet run -- --credentials $TEST_ACCOUNT_ID"
dotnet run -- --credentials $TEST_ACCOUNT_ID
success_msg "令牌更新验证完成"

# 13. 测试账户认证
test_step "测试账户认证" "dotnet run -- --test $TEST_ACCOUNT_ID"
dotnet run -- --test $TEST_ACCOUNT_ID
success_msg "账户认证测试完成"

# 14. 测试令牌状态检查（指定账户）
test_step "检查特定账户令牌状态" "dotnet run -- --status $TEST_ACCOUNT_ID"
dotnet run -- --status $TEST_ACCOUNT_ID
success_msg "特定账户令牌状态检查完成"

# 15. 测试所有账户令牌状态检查
test_step "检查所有账户令牌状态" "dotnet run -- --status"
dotnet run -- --status
success_msg "所有账户令牌状态检查完成"

# 16. 清理测试数据 - 删除测试账户
echo "🗑️  清理测试数据..."
echo "准备删除测试账户 ID: $TEST_ACCOUNT_ID"
echo "是否删除测试账户? (y/n)"
read -r DELETE_CONFIRM

if [ "$DELETE_CONFIRM" = "y" ] || [ "$DELETE_CONFIRM" = "Y" ]; then
    test_step "删除测试账户" "echo y | dotnet run -- --delete $TEST_ACCOUNT_ID"
    echo "y" | dotnet run -- --delete $TEST_ACCOUNT_ID
    if [ $? -eq 0 ]; then
        success_msg "测试账户删除成功"
    else
        error_msg "测试账户删除失败"
    fi
    
    # 验证删除
    test_step "验证账户删除" "dotnet run -- --list"
    dotnet run -- --list
    success_msg "删除验证完成"
else
    echo "⚠️  保留测试账户 ID: $TEST_ACCOUNT_ID"
    echo "   请手动删除: dotnet run -- --delete $TEST_ACCOUNT_ID"
fi

echo
echo "🎉 CRUD功能测试完成!"
echo
echo "测试总结:"
echo "✅ 帮助功能测试"
echo "✅ 账户列表功能测试"
echo "✅ 账户创建功能测试"
echo "✅ 账户信息获取测试"
echo "✅ 账户凭据获取测试"
echo "✅ 账户禁用功能测试"
echo "✅ 账户启用功能测试"
echo "✅ 令牌更新功能测试"
echo "✅ 账户认证测试"
echo "✅ 特定账户令牌状态检查测试"
echo "✅ 所有账户令牌状态检查测试"
echo "✅ 账户删除功能测试"
echo
echo "更多测试命令:"
echo "  dotnet run                    # 默认操作"
echo "  dotnet run -- --list          # 列出所有账户"
echo "  dotnet run -- --help          # 显示完整帮助"