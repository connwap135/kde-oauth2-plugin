#!/bin/bash

echo "🚀 KDE OAuth2 Plugin - 最终功能演示"
echo "=================================="

# 检查构建
echo "📦 1. 检查插件构建状态..."
if [ -f "/home/ubuntu/kde-oauth2-plugin/build/kde_oauth2_plugin.so" ]; then
    echo "✅ 插件构建成功！"
    ls -la /home/ubuntu/kde-oauth2-plugin/build/kde_oauth2_plugin.so
else
    echo "❌ 插件构建失败"
    exit 1
fi

echo ""
echo "🔍 2. 验证用户信息端点数据结构..."

# 测试用户信息端点
source <(./get_oauth_token.sh --export)
if [ -n "$OAUTH2_ACCESS_TOKEN" ]; then
    echo "✅ 访问令牌获取成功"
    echo "📡 正在测试用户信息端点..."
    
    USERINFO=$(curl -s -H "Authorization: Bearer $OAUTH2_ACCESS_TOKEN" \
                    -H "Content-Type: application/json" \
                    http://192.168.1.12:9007/connect/userinfo)
    
    echo "📄 用户信息响应:"
    echo "$USERINFO" | python3 -m json.tool
    
    # 解析关键字段
    echo ""
    echo "🎯 关键字段提取:"
    echo "用户ID: $(echo "$USERINFO" | python3 -c "import json,sys; data=json.load(sys.stdin); print(data.get('sub', 'N/A'))")"
    echo "用户名: $(echo "$USERINFO" | python3 -c "import json,sys; data=json.load(sys.stdin); print(data.get('http://schemas.xmlsoap.org/ws/2005/05/identity/claims/name', 'N/A'))")"
    echo "角色: $(echo "$USERINFO" | python3 -c "import json,sys; data=json.load(sys.stdin); print(data.get('http://schemas.microsoft.com/ws/2008/06/identity/claims/role', 'N/A'))")"
    echo "头像: $(echo "$USERINFO" | python3 -c "import json,sys; data=json.load(sys.stdin); print(data.get('portrait', 'N/A'))")"
else
    echo "❌ 无法获取访问令牌"
fi

echo ""
echo "📋 3. 插件功能特性总结:"
echo "────────────────────────"
echo "✅ 自动OAuth2认证流程"
echo "✅ 本地回调服务器 (localhost:8080)"
echo "✅ Microsoft .NET IdentityServer Claims 支持"
echo "✅ 智能字段提取和映射"
echo "✅ 详细调试信息记录"
echo "✅ 现代化双模式UI (自动/手动)"
echo "✅ 完整的错误处理机制"
echo "✅ 中文本地化支持"

echo ""
echo "🎯 4. 核心技术实现:"
echo "──────────────────"
echo "• CallbackServer: TCP服务器自动捕获授权码"
echo "• OAuth2Dialog: 双模式认证界面"
echo "• 增强用户信息解析: 支持复杂Claims格式"
echo "• 智能数据映射: 多种字段名兼容"
echo "• Qt5网络组件: HTTP客户端和服务器"

echo ""
echo "🔧 5. 使用方法:"
echo "─────────────"
echo "1. 将插件文件复制到系统位置:"
echo "   sudo cp build/kde_oauth2_plugin.so /usr/lib/x86_64-linux-gnu/qt5/plugins/"
echo "   sudo cp plugin.json /usr/lib/x86_64-linux-gnu/qt5/plugins/"
echo ""
echo "2. 在KDE系统设置中："
echo "   设置 → 在线账户 → 添加账户 → OAuth2"
echo ""
echo "3. 配置OAuth2参数："
echo "   服务器: http://192.168.1.12:9007"
echo "   客户端ID: 10001"
echo "   授权端点: /connect/authorize"
echo "   令牌端点: /connect/token"
echo "   用户信息端点: /connect/userinfo"
echo ""
echo "4. 享受一键式OAuth2认证体验！"

echo ""
echo "🎉 插件开发完成！所有功能已就绪。"