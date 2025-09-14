#!/usr/bin/env python3
"""
真实测试OAuth2插件的新建账户对话框功能
这个脚本会诚实地告诉您是否真的启动了对话框
"""

import dbus
import sys
import os
import subprocess

def check_gui_environment():
    """检查图形界面环境"""
    print("🖥️  检查图形界面环境...")
    
    display = os.environ.get('DISPLAY', '')
    if not display:
        print("❌ DISPLAY环境变量未设置 - 无图形界面")
        return False
    
    # 检查X11服务器
    try:
        result = subprocess.run(['xset', 'q'], capture_output=True)
        if result.returncode == 0:
            print(f"✅ 图形界面可用 (DISPLAY={display})")
            return True
        else:
            print("❌ X11服务器无响应")
            return False
    except FileNotFoundError:
        print("❌ X11工具不可用")
        return False

def check_kde_environment():
    """检查KDE环境"""
    print("🔍 检查KDE环境...")
    
    # 检查KDE相关进程
    try:
        result = subprocess.run(['pgrep', 'kwin'], capture_output=True)
        if result.returncode == 0:
            print("✅ KDE窗口管理器 (kwin) 正在运行")
            kde_running = True
        else:
            print("❌ KDE窗口管理器 (kwin) 未运行")
            kde_running = False
            
        # 检查账户管理服务
        result = subprocess.run(['pgrep', 'kaccounts'], capture_output=True)
        if result.returncode == 0:
            print("✅ KDE账户管理服务正在运行")
        else:
            print("⚠️  KDE账户管理服务未运行")
            
        return kde_running
    except Exception as e:
        print(f"❌ 检查KDE环境时出错: {e}")
        return False

def test_real_plugin():
    """测试真正的C++插件是否可用"""
    print("🔌 测试真正的OAuth2插件...")
    
    try:
        # 尝试加载实际的插件库
        plugin_path = "/home/ubuntu/kde-oauth2-plugin/build/gzweibo_oauth2_plugin.so"
        if os.path.exists(plugin_path):
            print(f"✅ 找到插件库: {plugin_path}")
        else:
            print(f"❌ 插件库不存在: {plugin_path}")
            return False
            
        # 检查是否有真正的KDE插件服务
        try:
            bus = dbus.SessionBus()
            # 这里应该是真正的KDE账户管理服务，不是我们的Python模拟
            services = bus.list_names()
            real_services = [s for s in services if 'kaccounts' in s.lower()]
            if real_services:
                print(f"✅ 找到真正的账户管理服务: {real_services}")
                return True
            else:
                print("❌ 未找到真正的KDE账户管理服务")
                return False
        except Exception as e:
            print(f"❌ 检查KDE服务时出错: {e}")
            return False
            
    except Exception as e:
        print(f"❌ 测试插件时出错: {e}")
        return False

def honest_test():
    """诚实的测试"""
    print("🚀 OAuth2插件真实性测试")
    print("=" * 50)
    
    # 检查图形环境
    gui_ok = check_gui_environment()
    
    # 检查KDE环境  
    kde_ok = check_kde_environment()
    
    # 检查真正的插件
    plugin_ok = test_real_plugin()
    
    print("\n📊 测试结果总结:")
    print("=" * 30)
    
    if gui_ok and kde_ok and plugin_ok:
        print("✅ 具备启动真正KDE对话框的所有条件")
        
        # 尝试启动真正的账户管理界面
        print("\n🚀 尝试启动KDE账户管理...")
        try:
            subprocess.Popen(['systemsettings5', 'kcm_kaccounts'])
            print("✅ 已启动KDE账户管理界面")
            print("👀 请查看KDE系统设置中的账户管理页面")
            return True
        except Exception as e:
            print(f"❌ 启动KDE账户管理失败: {e}")
            return False
            
    else:
        print("❌ 当前环境无法启动真正的KDE对话框")
        print("\n💡 实际情况:")
        print("   • 我们之前运行的是Python模拟服务")
        print("   • 返回'成功'只是模拟状态，不是真正的对话框")
        print("   • 要看到真正的对话框需要:")
        print("     1. 图形界面环境 (X11/Wayland)")
        print("     2. KDE桌面环境")
        print("     3. 真正的插件集成到KDE账户管理系统")
        
        print("\n🔧 要真正测试对话框，需要:")
        print("   1. 在有KDE图形界面的环境中运行")
        print("   2. 将插件安装到KDE系统中")
        print("   3. 通过KDE账户管理界面添加账户")
        
        return False

if __name__ == "__main__":
    success = honest_test()
    
    if not success:
        print("\n🤔 所以您说得对 - 我之前确实是'自我感觉良好'")
        print("   我们的Python脚本只是调用了模拟的DBus方法")
        print("   并没有真正启动KDE的账户对话框")
        print("\n💭 要看到真正的效果，需要在KDE桌面环境中:")
        print("   systemsettings5 -> 账户 -> 添加账户 -> OAuth2")
        
    sys.exit(0 if success else 1)