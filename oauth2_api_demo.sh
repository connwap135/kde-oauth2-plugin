#!/bin/bash
# OAuth2 API调用演示脚本
# 演示如何使用获取的OAuth2凭证调用API

set -e

echo "=== OAuth2 API调用演示 ==="

# 检查是否设置了环境变量
if [ -z "$OAUTH2_ACCESS_TOKEN" ] || [ -z "$OAUTH2_SERVER" ]; then
    echo "错误: 未设置OAuth2环境变量"
    echo "请先运行: source <(./get_oauth_token.sh --export)"
    exit 1
fi

echo "服务器: $OAUTH2_SERVER"
echo "用户名: $OAUTH2_USERNAME"
echo "访问令牌: ${OAUTH2_ACCESS_TOKEN:0:30}..."

# 测试1: 获取用户信息
echo ""
echo "=== 测试1: 获取用户信息 ==="
RESPONSE=$(curl -s -w "\nHTTP_STATUS:%{http_code}" \
    -H "Authorization: Bearer $OAUTH2_ACCESS_TOKEN" \
    -H "Content-Type: application/json" \
    "$OAUTH2_SERVER/connect/userinfo")

HTTP_STATUS=$(echo "$RESPONSE" | grep "HTTP_STATUS:" | cut -d: -f2)
BODY=$(echo "$RESPONSE" | sed '/HTTP_STATUS:/d')

if [ "$HTTP_STATUS" = "200" ]; then
    echo "✅ 用户信息获取成功"
    echo "用户信息:"
    echo "$BODY" | python3 -c "import sys, json; print(json.dumps(json.load(sys.stdin), indent=2, ensure_ascii=False))" 2>/dev/null || echo "$BODY"
else
    echo "❌ 用户信息获取失败 (HTTP $HTTP_STATUS)"
    echo "响应: $BODY"
fi

# 测试2: 模拟API调用（如果有其他端点可以测试）
echo ""
echo "=== 测试2: 检查服务器健康状态 ==="
HEALTH_RESPONSE=$(curl -s -w "\nHTTP_STATUS:%{http_code}" \
    -H "Authorization: Bearer $OAUTH2_ACCESS_TOKEN" \
    "$OAUTH2_SERVER/health" 2>/dev/null || echo "HTTP_STATUS:000")

HEALTH_STATUS=$(echo "$HEALTH_RESPONSE" | grep "HTTP_STATUS:" | cut -d: -f2)
if [ "$HEALTH_STATUS" = "200" ]; then
    echo "✅ 服务器健康检查通过"
else
    echo "ℹ️  服务器健康检查端点不可用 (HTTP $HEALTH_STATUS)"
fi

# 测试3: 令牌过期时间检查
echo ""
echo "=== 测试3: 令牌过期时间检查 ==="
# 从JWT中提取过期时间（简单解析）
EXP_PAYLOAD=$(echo "$OAUTH2_ACCESS_TOKEN" | cut -d. -f2)
# 补齐base64 padding
while [ $((${#EXP_PAYLOAD} % 4)) -ne 0 ]; do
    EXP_PAYLOAD="${EXP_PAYLOAD}="
done

# 尝试解码JWT payload（如果有jq工具）
if command -v jq &> /dev/null && command -v base64 &> /dev/null; then
    EXP_TIME=$(echo "$EXP_PAYLOAD" | base64 -d 2>/dev/null | jq -r '.exp' 2>/dev/null || echo "")
    if [ -n "$EXP_TIME" ]; then
        EXP_DATE=$(date -d "@$EXP_TIME" 2>/dev/null || echo "")
        if [ -n "$EXP_DATE" ]; then
            echo "令牌过期时间: $EXP_DATE"
            NOW=$(date +%s)
            if [ "$EXP_TIME" -gt "$NOW" ]; then
                REMAINING=$((EXP_TIME - NOW))
                HOURS=$((REMAINING / 3600))
                MINUTES=$(((REMAINING % 3600) / 60))
                echo "剩余时间: ${HOURS}小时 ${MINUTES}分钟"
            else
                echo "❌ 令牌已过期"
            fi
        fi
    fi
else
    echo "ℹ️  安装 jq 和 base64 工具可查看详细的令牌过期信息"
    echo "    sudo apt install jq"
fi

echo ""
echo "=== 演示完成 ==="
echo "所有测试均已完成。您现在可以在自己的应用程序中使用这些OAuth2凭证了。"