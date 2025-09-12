cmake_minimum_required(VERSION 3.10)

project(kde_oauth2_plugin)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_AUTOMOC ON)

find_package(Qt5 REQUIRED COMPONENTS Core Network Xml Widgets Gui)
find_package(KAccounts REQUIRED)
find_package(KF5I18n REQUIRED)

add_library(kde_oauth2_plugin SHARED
    src/kdeoauth2plugin.cpp
    src/kdeoauth2plugin.h
)

# 设置输出名称（不包含lib前缀）
set_target_properties(kde_oauth2_plugin PROPERTIES PREFIX "")

target_include_directories(kde_oauth2_plugin PRIVATE src)
target_link_libraries(kde_oauth2_plugin Qt5::Core Qt5::Network Qt5::Widgets Qt5::Gui KAccounts KF5::I18n)

# 安装插件
install(TARGETS kde_oauth2_plugin DESTINATION /usr/lib/x86_64-linux-gnu/qt5/plugins/kaccounts/ui)
install(FILES src/kdeoauth2plugin.json DESTINATION /usr/lib/x86_64-linux-gnu/qt5/plugins/kaccounts/ui RENAME kde_oauth2_plugin.so.json)

# 安装 KDE Online Accounts 配置文件
install(FILES gzweibo-oauth2.provider DESTINATION /usr/share/accounts/providers/kde)
install(FILES gzweibo-oauth2.service DESTINATION /usr/share/accounts/services/kde)
install(FILES gzweibo-oauth2-email.service DESTINATION /usr/share/accounts/services/kde)
install(FILES gzweibo-oauth2-profile.service DESTINATION /usr/share/accounts/services/kde)
