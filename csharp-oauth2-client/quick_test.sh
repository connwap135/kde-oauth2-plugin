#!/bin/bash

# C# OAuth2 客户端快速测试# 3. 创建账户
echo "3. 创建账户"
CREATE_OUTPUT=$(dotnet run -- --create "快速测试" "test.quick.com" "quick123" "token123" "1800" 2>&1)
TEST_ID=$(echo "$CREATE_OUTPUT" | grep "账户ID:" | grep -o '[0-9]\+' | head -1)echo "=== KDE OAuth2 C# 客户端快速测试 ==="
echo

# 检查项目是否可以运行
echo "🔨 编译项目..."
dotnet build -q

if [ $? -ne 0 ]; then
    echo "❌ 编译失败"
    exit 1
fi

echo "✅ 编译成功"
echo

# 快速功能验证
echo "📋 测试基本功能:"

# 1. 帮助
echo "1. 帮助功能"
dotnet run -- --help > /dev/null 2>&1
[ $? -eq 0 ] && echo "   ✅ 帮助" || echo "   ❌ 帮助"

# 2. 列表
echo "2. 账户列表"
dotnet run -- --list > /dev/null 2>&1
[ $? -eq 0 ] && echo "   ✅ 列表" || echo "   ❌ 列表"

# 3. 创建测试账户
echo "3. 创建账户"
CREATE_OUTPUT=$(dotnet run -- --create "快速测试" "test.quick.com" "quick123" "token123" 2>&1)
TEST_ID=$(echo "$CREATE_OUTPUT" | grep "账户ID:" | grep -o '[0-9]\+' | head -1)

if [ -n "$TEST_ID" ]; then
    echo "   ✅ 创建 (ID: $TEST_ID)"
    
    # 4. 获取信息
    echo "4. 获取信息"
    dotnet run -- --get $TEST_ID > /dev/null 2>&1
    [ $? -eq 0 ] && echo "   ✅ 信息" || echo "   ❌ 信息"
    
    # 5. 禁用
    echo "5. 禁用账户"
    dotnet run -- --disable $TEST_ID > /dev/null 2>&1
    [ $? -eq 0 ] && echo "   ✅ 禁用" || echo "   ❌ 禁用"
    
    # 6. 启用
    echo "6. 启用账户"
    dotnet run -- --enable $TEST_ID > /dev/null 2>&1
    [ $? -eq 0 ] && echo "   ✅ 启用" || echo "   ❌ 启用"
    
    # 7. 更新
    echo "7. 更新令牌"
    dotnet run -- --update $TEST_ID "new_token" > /dev/null 2>&1
    [ $? -eq 0 ] && echo "   ✅ 更新" || echo "   ❌ 更新"
    
    # 8. 状态检查
    echo "8. 令牌状态"
    dotnet run -- --status $TEST_ID > /dev/null 2>&1
    [ $? -eq 0 ] && echo "   ✅ 状态" || echo "   ❌ 状态"
    
    # 9. 删除
    echo "9. 删除账户"
    echo "y" | dotnet run -- --delete $TEST_ID > /dev/null 2>&1
    [ $? -eq 0 ] && echo "   ✅ 删除" || echo "   ❌ 删除"
    
else
    echo "   ❌ 创建"
fi

echo
echo "🎉 快速测试完成!"
echo "运行完整测试: ./test.sh"