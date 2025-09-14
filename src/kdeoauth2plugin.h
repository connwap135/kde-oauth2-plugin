#pragma once
#include <KAccounts/kaccountsuiplugin.h>
#include <KPluginFactory>
#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QDialog>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QLineEdit>
#include <QTextEdit>
#include <QTcpServer>
#include <QTcpSocket>
#include <QUrl>
#include <QUrlQuery>
#include <QDBusAbstractAdaptor>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDesktopServices>
#include <QTimer>

// 前置声明
class KDEOAuth2PluginDBusAdapter;

// 前置声明
class CallbackServer;

// OAuth2认证对话框
class OAuth2Dialog : public QDialog
{
    Q_OBJECT
    
public:
    explicit OAuth2Dialog(const QString &authUrl, const QString &redirectUri, QWidget *parent = nullptr);
    ~OAuth2Dialog();
    
    QString getAuthorizationCode() const { return m_authCode; }
    
private slots:
    void onOpenBrowser();
    void onCodeEntered();
    void onCancel();
    void onWebViewModeToggle();
    void onAuthorizationCodeReceived(const QString &code);
    void onAuthorizationError(const QString &error, const QString &description);
    
private:
    void startCallbackServer();
    
    QVBoxLayout *m_layout;
    QLabel *m_instructionLabel;
    QLabel *m_statusLabel;
    QTextEdit *m_urlDisplay;
    QPushButton *m_openBrowserButton;
    QPushButton *m_webViewModeButton;
    QLabel *m_codeLabel;
    QLineEdit *m_codeEdit;
    QHBoxLayout *m_buttonLayout;
    QPushButton *m_okButton;
    QPushButton *m_cancelButton;
    
    // 回调服务器
    CallbackServer *m_callbackServer;
    bool m_useWebView; // 这里实际表示手动模式
    
    QString m_authUrl;
    QString m_authCode;
    QString m_redirectUri;
};

class KDEOAuth2Plugin : public KAccountsUiPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.kde.kaccounts.UiPlugin")
    Q_INTERFACES(KAccountsUiPlugin)
    
public:
    explicit KDEOAuth2Plugin(QObject *parent = nullptr);
    ~KDEOAuth2Plugin() override;

    void init(KAccountsUiPlugin::UiType type) override;
    void setProviderName(const QString &providerName) override;
    void showNewAccountDialog() override;
    void showConfigureAccountDialog(const quint32 accountId) override;
    QStringList supportedServicesForConfig() const override;
    
    // DBus API 辅助方法
    QString dbusGetProviderName() const { return m_providerName; }
    void dbusSetProviderName(const QString &providerName) { m_providerName = providerName; }
    QStringList dbusGetAccountsList() const;
    bool dbusDeleteAccount(quint32 accountId);
    bool dbusEnableAccount(quint32 accountId, bool enabled);
    QVariantMap dbusGetAccountDetails(quint32 accountId);
    bool dbusRefreshToken(quint32 accountId);
    QVariantMap dbusGetPluginStatus();
    
    // 扩展的DBus接口方法
    void dbusInitNewAccountWithConfig(const QVariantMap &config);
    void dbusCancelCurrentDialog();
    QString dbusGetCurrentDialogState() const;
    QVariantMap dbusGetDialogInfo() const;
    void dbusSetOAuth2ServerUrl(const QString &serverUrl);
    void dbusSetOAuth2ClientId(const QString &clientId);
    void dbusSetOAuth2RedirectUri(const QString &redirectUri);
    void dbusSetOAuth2Scope(const QString &scope);
    QVariantMap dbusGetOAuth2Configuration() const;
    bool dbusTestConnection();
    QStringList dbusGetSupportedAuthMethods() const;
    void dbusSetAuthMethod(const QString &method);
    
    // 新增的DBus方法
    bool dbusSetOAuth2Config(const QString &server, const QString &clientId, const QString &authPath, const QString &tokenPath);
    QString dbusGetAuthMethod() const;
    int dbusGetAccountCount() const;
    QString dbusGetPluginVersion() const;
    QString dbusGetPluginInfo() const;
    QString dbusGetLastError() const;
    bool dbusClearError();
    QVariantMap dbusGetCurrentDialogInfo() const;
    
    // 获取DBus适配器实例（用于发送信号）
    KDEOAuth2PluginDBusAdapter* getDBusAdapter() const { return m_dbusAdapter; }

private slots:
    void onTokenRequestFinished();
    void onUserInfoRequestFinished();

private:
    void startOAuth2Flow();
    void exchangeCodeForToken(const QString &authCode);
    void fetchUserInfo(const QString &accessToken);
    QString generateAuthUrl() const;
    void createAccountWithBasicInfo();
    void loadProviderConfiguration();  // 从provider文件加载配置
    void loadConfigurationFromEnvironment();  // 从环境变量加载配置
    void loadFallbackConfiguration();  // 使用默认配置
    // 查询指定 provider 已存在的账户数量（用于限制单账户）
    int getAccountCountForProvider(const QString &providerId) const;
    
    QString m_providerName;
    QNetworkAccessManager *m_networkManager;
    
    // OAuth2 配置
    QString m_serverUrl;
    QString m_clientId;
    QString m_authPath;
    QString m_tokenPath;
    QString m_userInfoPath;
    QString m_redirectUri;
    QString m_scope;  // 添加scope字段
    
    // 当前认证状态
    QString m_currentAccessToken;
    QString m_currentRefreshToken;
    int m_currentExpiresIn = 0;
    QVariantMap m_currentUserInfo;
    
    // 状态跟踪
    // DBus适配器需要访问私有成员
    friend class KDEOAuth2PluginDBusAdapter;
    
private:
    QString m_currentDialogState = "none";    // "none", "creating", "configuring", "authenticating"
    QVariantMap m_dialogInfo;                 // 当前对话框的信息
    QString m_authMethod = "auto";            // 当前认证方法: "auto", "manual", "callback"
    QString m_lastError;                      // 最后的错误信息
    
    // DBus适配器
    class KDEOAuth2PluginDBusAdapter *m_dbusAdapter;
};

class KDEOAuth2PluginDBusAdapter : public QDBusAbstractAdaptor
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.kde.kaccounts.OAuth2Plugin")
    
public:
    explicit KDEOAuth2PluginDBusAdapter(KDEOAuth2Plugin *parent);
    
signals:
    // 账户操作结果信号
    void accountCreated(quint32 accountId, const QString &displayName, const QVariantMap &accountData);
    void accountCreationCanceled(const QString &reason);
    void accountCreationError(const QString &errorCode, const QString &errorMessage);
    void accountConfigured(quint32 accountId, const QVariantMap &accountData);
    void accountConfigurationCanceled(quint32 accountId, const QString &reason);
    void accountConfigurationError(quint32 accountId, const QString &errorCode, const QString &errorMessage);
    
    // 状态变化信号
    void dialogStateChanged(const QString &dialogType, const QString &state, const QVariantMap &info);
    
    // 配置变化信号
    void oauth2ConfigChanged(const QString &serverUrl, const QString &clientId, const QString &authPath, const QString &tokenPath);
    
public slots:
    // 基本账户操作 - DBus方法
    Q_NOREPLY void dbusInitNewAccount();
    Q_NOREPLY void dbusInitNewAccountWithConfig(const QString &server, const QString &clientId, const QString &authPath, const QString &tokenPath);
    Q_NOREPLY void dbusCancelCurrentDialog();
    
    // 状态查询 - DBus方法  
    QString dbusGetCurrentDialogState();
    QVariantMap dbusGetCurrentDialogInfo();
    
    // 配置管理 - DBus方法
    bool dbusSetOAuth2Config(const QString &server, const QString &clientId, const QString &authPath, const QString &tokenPath);
    QVariantMap dbusGetOAuth2Config();
    bool dbusSetAuthMethod(const QString &method);
    QString dbusGetAuthMethod();
    
    // 信息查询 - DBus方法
    int dbusGetAccountCount();
    QString dbusGetPluginVersion();
    QString dbusGetPluginInfo();
    bool dbusTestConnection();
    QString dbusGetLastError();
    bool dbusClearError();
    
    // 兼容性方法 - 保持向后兼容
    Q_NOREPLY void initNewAccount();
    Q_NOREPLY void initNewAccountWithConfig(const QVariantMap &config);
    Q_NOREPLY void initConfigureAccount(quint32 accountId);
    Q_NOREPLY void cancelCurrentDialog();
    
    // 账户管理
    QString getProviderName();
    void setProviderName(const QString &providerName);
    QStringList getAccountsList();
    bool deleteAccount(quint32 accountId);
    bool enableAccount(quint32 accountId, bool enabled);
    QVariantMap getAccountDetails(quint32 accountId);
    bool refreshToken(quint32 accountId);
    
    // 状态查询
    QVariantMap getPluginStatus();
    bool isHeadlessEnvironment();
    QString getCurrentDialogState();
    QVariantMap getDialogInfo();
    
    // 配置管理
    void setOAuth2ServerUrl(const QString &serverUrl);
    void setOAuth2ClientId(const QString &clientId);
    void setOAuth2RedirectUri(const QString &redirectUri);
    void setOAuth2Scope(const QString &scope);
    QVariantMap getOAuth2Configuration();
    
    // 高级功能
    bool testConnection();
    QStringList getSupportedAuthMethods();
    void setAuthMethod(const QString &method);
    
private:
    KDEOAuth2Plugin *m_plugin;
};