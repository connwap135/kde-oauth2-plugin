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
#include <QDesktopServices>
#include <QTimer>

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
};