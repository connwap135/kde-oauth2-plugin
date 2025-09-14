#include "kdeoauth2plugin.h"
#include <QDebug>
#include <QMessageBox>
#include <QTimer>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QUrlQuery>
#include <QUuid>
#include <QApplication>
#include <QTcpServer>
#include <QTcpSocket>
#include <QHostAddress>
#include <QFile>
#include <QXmlStreamReader>
// Accounts-Qt
#include <Accounts/Manager>
#include <Accounts/Account>

// 本地HTTP服务器类，用于捕获OAuth2回调
class CallbackServer : public QTcpServer
{
    Q_OBJECT
    
public:
    explicit CallbackServer(QObject *parent = nullptr)
        : QTcpServer(parent)
    {
    }
    
signals:
    void authorizationCodeReceived(const QString &code);
    void authorizationError(const QString &error, const QString &description);
    
protected:
    void incomingConnection(qintptr socketDescriptor) override
    {
        QTcpSocket *socket = new QTcpSocket(this);
        socket->setSocketDescriptor(socketDescriptor);
        
        connect(socket, &QTcpSocket::readyRead, [this, socket]() {
            QByteArray data = socket->readAll();
            QString request = QString::fromUtf8(data);
            
            qDebug() << "CallbackServer: received request:" << request;
            
            // 解析HTTP请求获取URL
            QStringList lines = request.split("\r\n");
            if (!lines.isEmpty()) {
                QString firstLine = lines.first();
                QStringList parts = firstLine.split(" ");
                if (parts.size() >= 2) {
                    QString path = parts[1];
                    
                    // 创建完整URL用于解析
                    QUrl url("http://localhost:8080" + path);
                    QUrlQuery query(url);
                    
                    QString response;
                    if (query.hasQueryItem("code")) {
                        QString code = query.queryItemValue("code");
                        response = createSuccessResponse(code);
                        emit authorizationCodeReceived(code);
                    } else if (query.hasQueryItem("error")) {
                        QString error = query.queryItemValue("error");
                        QString errorDesc = query.queryItemValue("error_description");
                        response = createErrorResponse(error, errorDesc);
                        emit authorizationError(error, errorDesc);
                    } else {
                        response = createErrorResponse("invalid_request", "No authorization code or error received");
                    }
                    
                    socket->write(response.toUtf8());
                    socket->flush();
                    socket->disconnectFromHost();
                }
            }
        });
        
        connect(socket, &QTcpSocket::disconnected, socket, &QTcpSocket::deleteLater);
    }
    
private:
    QString createSuccessResponse(const QString &code)
    {
        QString html = QString(
            "<!DOCTYPE html>\n"
            "<html><head><title>认证成功</title></head>\n"
            "<body style='font-family: Arial, sans-serif; text-align: center; padding: 50px; background: #f0f8ff;'>\n"
            "<h2 style='color: #28a745;'>✅ OAuth2 认证成功！</h2>\n"
            "<p>授权码已自动获取</p>\n"
            "<p style='font-size: 12px; color: #666;'>授权码: <code style='background: #f8f9fa; padding: 2px 4px; border-radius: 3px;'>%1</code></p>\n"
            "<p>您可以关闭此页面，账户将自动创建。</p>\n"
            "<script>setTimeout(function(){window.close();}, 3000);</script>\n"
            "</body></html>"
        ).arg(code.left(20) + "...");
        
        return QString(
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: text/html; charset=utf-8\r\n"
            "Content-Length: %1\r\n"
            "Connection: close\r\n"
            "\r\n%2"
        ).arg(html.toUtf8().size()).arg(html);
    }
    
    QString createErrorResponse(const QString &error, const QString &description)
    {
        QString html = QString(
            "<!DOCTYPE html>\n"
            "<html><head><title>认证失败</title></head>\n"
            "<body style='font-family: Arial, sans-serif; text-align: center; padding: 50px; background: #fff5f5;'>\n"
            "<h2 style='color: #dc3545;'>❌ OAuth2 认证失败</h2>\n"
            "<p>错误: %1</p>\n"
            "<p>描述: %2</p>\n"
            "<p>您可以关闭此页面重试。</p>\n"
            "</body></html>"
        ).arg(error, description);
        
        return QString(
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: text/html; charset=utf-8\r\n"
            "Content-Length: %1\r\n"
            "Connection: close\r\n"
            "\r\n%2"
        ).arg(html.toUtf8().size()).arg(html);
    }
};

// OAuth2Dialog 实现
OAuth2Dialog::OAuth2Dialog(const QString &authUrl, const QString &redirectUri, QWidget *parent)
    : QDialog(parent)
    , m_authUrl(authUrl)
    , m_redirectUri(redirectUri)
    , m_callbackServer(nullptr)
    , m_useWebView(false) // false = 自动模式, true = 手动模式
{
    setWindowTitle("OAuth2 认证");
    setModal(true);
    resize(600, 450);
    
    // 创建主布局
    m_layout = new QVBoxLayout(this);
    
    // 标题
    QLabel *titleLabel = new QLabel("<h3>OAuth2 身份认证</h3>");
    titleLabel->setAlignment(Qt::AlignCenter);
    m_layout->addWidget(titleLabel);
    
    // 说明标签
    m_instructionLabel = new QLabel("自动模式 - 智能获取授权码");
    m_instructionLabel->setWordWrap(true);
    m_instructionLabel->setStyleSheet("font-weight: bold; color: #0066cc;");
    m_layout->addWidget(m_instructionLabel);
    
    // 自动模式说明
    QLabel *autoModeLabel = new QLabel(
        "🔹 自动启动本地服务器监听OAuth2回调\n"
        "🔹 点击下面的按钮在浏览器中打开认证页面\n"
        "🔹 完成认证后授权码将自动获取\n"
        "🔹 如果遇到问题，可以切换到手动模式"
    );
    autoModeLabel->setWordWrap(true);
    autoModeLabel->setStyleSheet("padding: 10px; background: #f8f9fa; border-radius: 5px;");
    m_layout->addWidget(autoModeLabel);
    
    // 打开浏览器按钮
    m_openBrowserButton = new QPushButton("🌐 在浏览器中打开认证页面");
    m_openBrowserButton->setStyleSheet("QPushButton { padding: 10px; font-size: 14px; background: #007bff; color: white; border: none; border-radius: 5px; } QPushButton:hover { background: #0056b3; }");
    connect(m_openBrowserButton, &QPushButton::clicked, this, &OAuth2Dialog::onOpenBrowser);
    m_layout->addWidget(m_openBrowserButton);
    
    // 手动模式切换
    m_webViewModeButton = new QPushButton("⚙️ 切换到手动输入模式");
    m_webViewModeButton->setStyleSheet("QPushButton { padding: 8px; background: #6c757d; color: white; border: none; border-radius: 5px; } QPushButton:hover { background: #545b62; }");
    connect(m_webViewModeButton, &QPushButton::clicked, this, &OAuth2Dialog::onWebViewModeToggle);
    m_layout->addWidget(m_webViewModeButton);
    
    // 手动输入区域（默认隐藏）
    m_urlDisplay = new QTextEdit();
    m_urlDisplay->setPlainText(authUrl);
    m_urlDisplay->setMaximumHeight(80);
    m_urlDisplay->setReadOnly(true);
    m_urlDisplay->hide();
    m_layout->addWidget(m_urlDisplay);
    
    m_codeLabel = new QLabel("请输入授权码:");
    m_codeLabel->hide();
    m_layout->addWidget(m_codeLabel);
    
    m_codeEdit = new QLineEdit();
    m_codeEdit->setPlaceholderText("粘贴从浏览器获取的授权码...");
    m_codeEdit->hide();
    connect(m_codeEdit, &QLineEdit::returnPressed, this, &OAuth2Dialog::onCodeEntered);
    m_layout->addWidget(m_codeEdit);
    
    // 状态显示
    m_statusLabel = new QLabel("");
    m_statusLabel->setWordWrap(true);
    m_statusLabel->setStyleSheet("padding: 10px; border-radius: 5px;");
    m_layout->addWidget(m_statusLabel);
    
    // 按钮布局
    m_buttonLayout = new QHBoxLayout();
    m_okButton = new QPushButton("确定");
    m_cancelButton = new QPushButton("取消");
    
    m_okButton->setStyleSheet("QPushButton { padding: 8px 20px; background: #28a745; color: white; border: none; border-radius: 5px; } QPushButton:hover { background: #218838; }");
    m_cancelButton->setStyleSheet("QPushButton { padding: 8px 20px; background: #dc3545; color: white; border: none; border-radius: 5px; } QPushButton:hover { background: #c82333; }");
    
    connect(m_okButton, &QPushButton::clicked, this, &OAuth2Dialog::onCodeEntered);
    connect(m_cancelButton, &QPushButton::clicked, this, &OAuth2Dialog::onCancel);
    
    m_buttonLayout->addStretch();
    m_buttonLayout->addWidget(m_okButton);
    m_buttonLayout->addWidget(m_cancelButton);
    
    m_layout->addLayout(m_buttonLayout);
    
    // 默认隐藏确定按钮（自动模式下不需要）
    m_okButton->hide();
    
    // 启动回调服务器
    startCallbackServer();
}

OAuth2Dialog::~OAuth2Dialog()
{
    if (m_callbackServer) {
        m_callbackServer->close();
        m_callbackServer->deleteLater();
    }
}

void OAuth2Dialog::startCallbackServer()
{
    m_callbackServer = new CallbackServer(this);
    
    connect(m_callbackServer, &CallbackServer::authorizationCodeReceived, 
            this, &OAuth2Dialog::onAuthorizationCodeReceived);
    connect(m_callbackServer, &CallbackServer::authorizationError,
            this, &OAuth2Dialog::onAuthorizationError);
    
    if (m_callbackServer->listen(QHostAddress::LocalHost, 8080)) {
        qDebug() << "OAuth2Dialog: callback server started on port 8080";
        m_statusLabel->setText("✅ 回调服务器已启动，准备接收认证结果...");
        m_statusLabel->setStyleSheet("padding: 10px; background: #d4edda; color: #155724; border-radius: 5px;");
    } else {
        qDebug() << "OAuth2Dialog: failed to start callback server";
        m_statusLabel->setText("❌ 无法启动回调服务器，请使用手动模式");
        m_statusLabel->setStyleSheet("padding: 10px; background: #f8d7da; color: #721c24; border-radius: 5px;");
        onWebViewModeToggle(); // 自动切换到手动模式
    }
}

void OAuth2Dialog::onAuthorizationCodeReceived(const QString &code)
{
    qDebug() << "OAuth2Dialog: received authorization code via callback:" << code;
    m_authCode = code;
    m_statusLabel->setText("✅ 成功获取授权码！正在创建账户...");
    m_statusLabel->setStyleSheet("padding: 10px; background: #d4edda; color: #155724; border-radius: 5px;");
    
    // 延迟一点时间让用户看到成功消息
    QTimer::singleShot(1500, this, &OAuth2Dialog::accept);
}

void OAuth2Dialog::onAuthorizationError(const QString &error, const QString &description)
{
    qDebug() << "OAuth2Dialog: authorization error:" << error << description;
    m_statusLabel->setText(QString("❌ 认证失败: %1").arg(error));
    m_statusLabel->setStyleSheet("padding: 10px; background: #f8d7da; color: #721c24; border-radius: 5px;");
    
    QMessageBox::warning(this, "认证失败", 
        QString("OAuth2认证失败:\n错误: %1\n描述: %2").arg(error, description));
}

void OAuth2Dialog::onOpenBrowser()
{
    qDebug() << "OAuth2Dialog: opening browser with URL:" << m_authUrl;
    bool success = QDesktopServices::openUrl(QUrl(m_authUrl));
    if (success) {
        m_openBrowserButton->setText("✅ 浏览器已打开");
        m_openBrowserButton->setEnabled(false);
        m_statusLabel->setText("⏳ 浏览器已打开，请在浏览器中完成认证...");
        m_statusLabel->setStyleSheet("padding: 10px; background: #fff3cd; color: #856404; border-radius: 5px;");
    } else {
        QMessageBox::warning(this, "错误", "无法打开浏览器。请手动复制URL到浏览器中打开，或切换到手动模式。");
        onWebViewModeToggle(); // 切换到手动模式
    }
}

void OAuth2Dialog::onWebViewModeToggle()
{
    m_useWebView = !m_useWebView;
    
    if (m_useWebView) {
        // 切换到手动模式
        m_instructionLabel->setText("手动模式 - 复制粘贴授权码");
        m_instructionLabel->setStyleSheet("font-weight: bold; color: #dc3545;");
        m_webViewModeButton->setText("🔄 切换到自动模式");
        m_urlDisplay->show();
        m_codeLabel->show();
        m_codeEdit->show();
        m_okButton->show();
        m_statusLabel->setText("📋 请手动复制授权码并粘贴到下面的输入框中");
        m_statusLabel->setStyleSheet("padding: 10px; background: #e2e3e5; color: #383d41; border-radius: 5px;");
        
        // 停止回调服务器
        if (m_callbackServer) {
            m_callbackServer->close();
        }
        
        m_codeEdit->setFocus();
    } else {
        // 切换到自动模式
        m_instructionLabel->setText("自动模式 - 智能获取授权码");
        m_instructionLabel->setStyleSheet("font-weight: bold; color: #0066cc;");
        m_webViewModeButton->setText("⚙️ 切换到手动输入模式");
        m_urlDisplay->hide();
        m_codeLabel->hide();
        m_codeEdit->hide();
        m_okButton->hide();
        
        // 重置浏览器按钮
        m_openBrowserButton->setText("🌐 在浏览器中打开认证页面");
        m_openBrowserButton->setEnabled(true);
        
        // 重新启动回调服务器
        startCallbackServer();
    }
}

void OAuth2Dialog::onCodeEntered()
{
    if (m_useWebView && m_codeEdit) {
        m_authCode = m_codeEdit->text().trimmed();
        if (m_authCode.isEmpty()) {
            QMessageBox::warning(this, "错误", "请输入授权码");
            return;
        }
        
        qDebug() << "OAuth2Dialog: received authorization code via manual input:" << m_authCode;
        m_statusLabel->setText("✅ 已输入授权码，正在创建账户...");
        m_statusLabel->setStyleSheet("padding: 10px; background: #d4edda; color: #155724; border-radius: 5px;");
    }
    
    accept();
}

void OAuth2Dialog::onCancel()
{
    qDebug() << "OAuth2Dialog: user canceled authentication";
    reject();
}


KDEOAuth2Plugin::KDEOAuth2Plugin(QObject *parent)
    : KAccountsUiPlugin(parent)
    , m_networkManager(nullptr)
    , m_serverUrl("http://192.168.1.12:9007")  // 默认值，可被环境变量覆盖
    , m_clientId("10001")                       // 默认值，可被环境变量覆盖
    , m_authPath("/connect/authorize")          // 默认值，可被环境变量覆盖
    , m_tokenPath("/connect/token")             // 默认值，可被环境变量覆盖
    , m_userInfoPath("/connect/userinfo")       // 默认值，可被环境变量覆盖
    , m_redirectUri("http://localhost:8080/callback")  // 默认值，可被环境变量覆盖
    , m_scope("openid profile")                 // 默认值，可被环境变量覆盖
    , m_dbusAdapter(nullptr)
{
    qDebug() << "KDEOAuth2Plugin: Constructor called";
    m_networkManager = new QNetworkAccessManager(this);
    
    // 创建DBus适配器
    m_dbusAdapter = new KDEOAuth2PluginDBusAdapter(this);
    
    // 注册DBus服务
    QDBusConnection sessionBus = QDBusConnection::sessionBus();
    if (sessionBus.registerService("org.kde.kaccounts.OAuth2Plugin")) {
        if (sessionBus.registerObject("/OAuth2Plugin", this)) {
            qDebug() << "KDEOAuth2Plugin: DBus service registered successfully";
            qDebug() << "KDEOAuth2Plugin: DBus adapter created and attached to object";
        } else {
            qDebug() << "KDEOAuth2Plugin: Failed to register DBus object:" << sessionBus.lastError().message();
        }
    } else {
        qDebug() << "KDEOAuth2Plugin: Failed to register DBus service:" << sessionBus.lastError().message();
    }
    
    // 优先从provider文件加载配置
    loadProviderConfiguration();
    // 再从环境变量加载配置（可覆盖provider配置）
    loadConfigurationFromEnvironment();
}

KDEOAuth2Plugin::~KDEOAuth2Plugin()
{
    qDebug() << "KDEOAuth2Plugin: Destructor called";
    
    // 注销DBus服务
    QDBusConnection sessionBus = QDBusConnection::sessionBus();
    sessionBus.unregisterObject("/OAuth2Plugin");
    sessionBus.unregisterService("org.kde.kaccounts.OAuth2Plugin");
    
    // 清理DBus适配器
    if (m_dbusAdapter) {
        delete m_dbusAdapter;
        m_dbusAdapter = nullptr;
    }
}

void KDEOAuth2Plugin::init(KAccountsUiPlugin::UiType type)
{
    qDebug() << "KDEOAuth2Plugin: init called with type" << type;
    qDebug() << "KDEOAuth2Plugin: NewAccountDialog =" << KAccountsUiPlugin::NewAccountDialog;
    qDebug() << "KDEOAuth2Plugin: ConfigureAccountDialog =" << KAccountsUiPlugin::ConfigureAccountDialog;
    
    if (type == KAccountsUiPlugin::NewAccountDialog) {
        qDebug() << "KDEOAuth2Plugin: init requesting to show new account dialog";
        // 在下一个事件循环中调用showNewAccountDialog()
        QTimer::singleShot(0, this, &KDEOAuth2Plugin::showNewAccountDialog);
    } else if (type == KAccountsUiPlugin::ConfigureAccountDialog) {
        qDebug() << "KDEOAuth2Plugin: init for configure account dialog";
        // 配置账户的逻辑会在showConfigureAccountDialog中处理
    } else {
        qDebug() << "KDEOAuth2Plugin: unknown type" << type;
    }
}

void KDEOAuth2Plugin::setProviderName(const QString &providerName)
{
    m_providerName = providerName;
    qDebug() << "KDEOAuth2Plugin: provider name set to" << providerName;
}

void KDEOAuth2Plugin::showNewAccountDialog()
{
    qDebug() << "KDEOAuth2Plugin: showing new account dialog";
    
    // 更新状态
    m_currentDialogState = "creating";
    m_dialogInfo.clear();
    m_dialogInfo["type"] = "new_account";
    m_dialogInfo["provider"] = m_providerName;
    
    // 发送状态变化信号
    if (m_dbusAdapter) {
        emit m_dbusAdapter->dialogStateChanged("new_account", m_currentDialogState, m_dialogInfo);
    }
    
    // 检查是否已存在账户（单账户限制）
    if (!m_providerName.isEmpty()) {
        int accountCount = getAccountCountForProvider(m_providerName);
        qDebug() << "KDEOAuth2Plugin: existing account count for provider" << m_providerName << ":" << accountCount;
        if (accountCount > 0) {
            QString errorMsg = QString("Provider '%1' 已存在账户，无法重复添加。\n如需更换请先删除原账户。").arg(m_providerName);
            QMessageBox::warning(nullptr, "账户限制", errorMsg);
            
            // 发送错误信号
            if (m_dbusAdapter) {
                emit m_dbusAdapter->accountCreationError("account_limit_exceeded", errorMsg);
            }
            
            m_currentDialogState = "none";
            m_dialogInfo.clear();
            emit canceled();
            return;
        }
    }
    startOAuth2Flow();
}

int KDEOAuth2Plugin::getAccountCountForProvider(const QString &providerId) const
{
    qDebug() << "KDEOAuth2Plugin::getAccountCountForProvider: checking provider" << providerId;
    
    // 使用 Accounts-Qt 查询指定 provider 下的账户数
    // 注意：KAccountsUiPlugin 运行于进程内，直接构造 Manager 即可
    Accounts::Manager manager;
    
    // 输出所有可用的 provider
    Accounts::ProviderList allProviders = manager.providerList();
    qDebug() << "KDEOAuth2Plugin::getAccountCountForProvider: total providers available:" << allProviders.size();
    for (const Accounts::Provider &provider : allProviders) {
        qDebug() << "  - Available provider:" << provider.name();
    }
    
    // 修复：使用通用的 accountList() 方法，然后手动过滤
    // 因为 accountList(providerId) 依赖于 provider 注册，可能不可靠
    Accounts::AccountIdList allIds = manager.accountList();
    qDebug() << "KDEOAuth2Plugin::getAccountCountForProvider: total accounts found:" << allIds.size();
    
    int count = 0;
    for (Accounts::AccountId id : allIds) {
        qDebug() << "KDEOAuth2Plugin::getAccountCountForProvider: checking account ID" << id;
        std::unique_ptr<Accounts::Account> account(manager.account(id));
        if (account) {
            QString accountProvider = account->providerName();
            bool enabled = account->enabled();
            qDebug() << "KDEOAuth2Plugin::getAccountCountForProvider: account" << id 
                     << "provider:" << accountProvider << "enabled:" << enabled 
                     << "target provider:" << providerId;
            
            // 手动比较 provider 名称
            if (accountProvider == providerId && enabled) {
                ++count;
                qDebug() << "KDEOAuth2Plugin::getAccountCountForProvider: account" << id << "matches and is enabled, count now:" << count;
            } else if (accountProvider == providerId) {
                qDebug() << "KDEOAuth2Plugin::getAccountCountForProvider: account" << id << "matches but is disabled, skipping";
            } else {
                qDebug() << "KDEOAuth2Plugin::getAccountCountForProvider: account" << id << "provider mismatch, skipping";
            }
        } else {
            qDebug() << "KDEOAuth2Plugin::getAccountCountForProvider: failed to load account" << id;
        }
    }
    
    qDebug() << "KDEOAuth2Plugin::getAccountCountForProvider: final enabled account count for provider" << providerId << ":" << count;
    return count;
}

QStringList KDEOAuth2Plugin::dbusGetAccountsList() const
{
    qDebug() << "KDEOAuth2Plugin::dbusGetAccountsList: getting accounts list";
    
    QStringList result;
    
    // 使用 Accounts-Qt 查询所有账户
    Accounts::Manager manager;
    Accounts::AccountIdList allIds = manager.accountList();
    
    qDebug() << "KDEOAuth2Plugin::dbusGetAccountsList: total accounts found:" << allIds.size();
    
    for (Accounts::AccountId id : allIds) {
        std::unique_ptr<Accounts::Account> account(manager.account(id));
        if (account) {
            QString accountProvider = account->providerName();
            bool enabled = account->enabled();
            QString displayName = account->displayName();
            
            // 只返回当前provider的账户
            if (accountProvider == m_providerName) {
                QString accountInfo = QString("ID:%1|Name:%2|Enabled:%3")
                    .arg(id)
                    .arg(displayName.isEmpty() ? QString("Account %1").arg(id) : displayName)
                    .arg(enabled ? "Yes" : "No");
                
                result.append(accountInfo);
                qDebug() << "KDEOAuth2Plugin::dbusGetAccountsList: found account:" << accountInfo;
            }
        }
    }
    
    qDebug() << "KDEOAuth2Plugin::dbusGetAccountsList: returning" << result.size() << "accounts";
    return result;
}

bool KDEOAuth2Plugin::dbusDeleteAccount(quint32 accountId)
{
    qDebug() << "KDEOAuth2Plugin::dbusDeleteAccount: deleting account" << accountId;
    
    // 使用 Accounts-Qt 删除账户
    Accounts::Manager manager;
    std::unique_ptr<Accounts::Account> account(manager.account(accountId));
    
    if (!account) {
        qDebug() << "KDEOAuth2Plugin::dbusDeleteAccount: account not found" << accountId;
        return false;
    }
    
    // 检查是否是当前provider的账户
    if (account->providerName() != m_providerName) {
        qDebug() << "KDEOAuth2Plugin::dbusDeleteAccount: account provider mismatch" << account->providerName() << "vs" << m_providerName;
        return false;
    }
    
    // 删除账户
    account->remove();
    qDebug() << "KDEOAuth2Plugin::dbusDeleteAccount: delete request sent for account" << accountId;
    
    return true;
}

bool KDEOAuth2Plugin::dbusEnableAccount(quint32 accountId, bool enabled)
{
    qDebug() << "KDEOAuth2Plugin::dbusEnableAccount: setting account" << accountId << "enabled:" << enabled;
    
    // 使用 Accounts-Qt 启用/禁用账户
    Accounts::Manager manager;
    std::unique_ptr<Accounts::Account> account(manager.account(accountId));
    
    if (!account) {
        qDebug() << "KDEOAuth2Plugin::dbusEnableAccount: account not found" << accountId;
        return false;
    }
    
    // 检查是否是当前provider的账户
    if (account->providerName() != m_providerName) {
        qDebug() << "KDEOAuth2Plugin::dbusEnableAccount: account provider mismatch" << account->providerName() << "vs" << m_providerName;
        return false;
    }
    
    // 设置启用状态
    account->setEnabled(enabled);
    account->sync();
    qDebug() << "KDEOAuth2Plugin::dbusEnableAccount: account" << accountId << "enabled set to" << enabled;
    
    return true;
}

QVariantMap KDEOAuth2Plugin::dbusGetAccountDetails(quint32 accountId)
{
    qDebug() << "KDEOAuth2Plugin::dbusGetAccountDetails: getting details for account" << accountId;
    
    QVariantMap result;
    
    // 使用 Accounts-Qt 获取账户详情
    Accounts::Manager manager;
    std::unique_ptr<Accounts::Account> account(manager.account(accountId));
    
    if (!account) {
        qDebug() << "KDEOAuth2Plugin::dbusGetAccountDetails: account not found" << accountId;
        result["error"] = "Account not found";
        return result;
    }
    
    // 检查是否是当前provider的账户
    if (account->providerName() != m_providerName) {
        qDebug() << "KDEOAuth2Plugin::dbusGetAccountDetails: account provider mismatch" << account->providerName() << "vs" << m_providerName;
        result["error"] = "Account provider mismatch";
        return result;
    }
    
    // 获取基本信息
    result["id"] = accountId;
    result["provider"] = account->providerName();
    result["displayName"] = account->displayName();
    result["enabled"] = account->enabled();
    
    // 获取账户设置（如果可用）
    // 注意：Accounts-Qt的设置访问方式可能因版本而异
    // 这里我们只返回基本信息，避免API兼容性问题
    result["server"] = m_serverUrl;
    result["client_id"] = m_clientId;
    
    qDebug() << "KDEOAuth2Plugin::dbusGetAccountDetails: returning basic details";
    return result;
}

bool KDEOAuth2Plugin::dbusRefreshToken(quint32 accountId)
{
    qDebug() << "KDEOAuth2Plugin::dbusRefreshToken: refreshing token for account" << accountId;
    
    // 使用 Accounts-Qt 获取账户
    Accounts::Manager manager;
    std::unique_ptr<Accounts::Account> account(manager.account(accountId));
    
    if (!account) {
        qDebug() << "KDEOAuth2Plugin::dbusRefreshToken: account not found" << accountId;
        return false;
    }
    
    // 检查是否是当前provider的账户
    if (account->providerName() != m_providerName) {
        qDebug() << "KDEOAuth2Plugin::dbusRefreshToken: account provider mismatch" << account->providerName() << "vs" << m_providerName;
        return false;
    }
    
    // 获取当前的refresh token
    // 注意：Accounts-Qt的设置访问方式可能因版本而异
    // 这里我们使用value()方法，如果不可用则返回false
    QString refreshToken;
    try {
        refreshToken = account->value("refresh_token").toString();
    } catch (...) {
        qDebug() << "KDEOAuth2Plugin::dbusRefreshToken: cannot access refresh token";
        return false;
    }
    
    if (refreshToken.isEmpty()) {
        qDebug() << "KDEOAuth2Plugin::dbusRefreshToken: no refresh token available";
        return false;
    }
    
    // 这里应该实现token刷新逻辑
    // 由于这需要网络请求，我们暂时返回false并记录日志
    qDebug() << "KDEOAuth2Plugin::dbusRefreshToken: token refresh not implemented yet";
    return false;
}

QVariantMap KDEOAuth2Plugin::dbusGetPluginStatus()
{
    qDebug() << "KDEOAuth2Plugin::dbusGetPluginStatus: getting plugin status";
    
    QVariantMap status;
    status["providerName"] = m_providerName;
    status["serverUrl"] = m_serverUrl;
    status["clientId"] = m_clientId;
    status["authPath"] = m_authPath;
    status["tokenPath"] = m_tokenPath;
    status["userInfoPath"] = m_userInfoPath;
    status["redirectUri"] = m_redirectUri;
    status["scope"] = m_scope;
    
    // 获取账户统计
    Accounts::Manager manager;
    Accounts::AccountIdList allIds = manager.accountList();
    int totalAccounts = 0;
    int enabledAccounts = 0;
    
    for (Accounts::AccountId id : allIds) {
        std::unique_ptr<Accounts::Account> account(manager.account(id));
        if (account && account->providerName() == m_providerName) {
            totalAccounts++;
            if (account->enabled()) {
                enabledAccounts++;
            }
        }
    }
    
    status["totalAccounts"] = totalAccounts;
    status["enabledAccounts"] = enabledAccounts;
    status["currentDialogState"] = m_currentDialogState;
    status["authMethod"] = m_authMethod;
    
    qDebug() << "KDEOAuth2Plugin::dbusGetPluginStatus: returning status";
    return status;
}

// 扩展的DBus接口方法实现

void KDEOAuth2Plugin::dbusInitNewAccountWithConfig(const QVariantMap &config)
{
    qDebug() << "KDEOAuth2Plugin::dbusInitNewAccountWithConfig: starting with config" << config;
    
    // 更新状态
    m_currentDialogState = "creating";
    m_dialogInfo = config;
    
    // 发送状态变化信号
    if (m_dbusAdapter) {
        QVariantMap data;
        data["config"] = config;
        emit m_dbusAdapter->dialogStateChanged("new_account", m_currentDialogState, data);
    }
    
    // 应用配置
    if (config.contains("serverUrl")) {
        m_serverUrl = config["serverUrl"].toString();
    }
    if (config.contains("clientId")) {
        m_clientId = config["clientId"].toString();
    }
    if (config.contains("redirectUri")) {
        m_redirectUri = config["redirectUri"].toString();
    }
    if (config.contains("scope")) {
        m_scope = config["scope"].toString();
    }
    
    // 启动标准流程
    showNewAccountDialog();
}

void KDEOAuth2Plugin::dbusCancelCurrentDialog()
{
    qDebug() << "KDEOAuth2Plugin::dbusCancelCurrentDialog: canceling current dialog";
    
    QString previousState = m_currentDialogState;
    m_currentDialogState = "none";
    m_dialogInfo.clear();
    
    // 发送取消信号
    if (m_dbusAdapter) {
        if (previousState == "creating") {
            emit m_dbusAdapter->accountCreationCanceled("User requested cancellation");
        } else if (previousState == "configuring") {
            quint32 accountId = m_dialogInfo.value("accountId", 0).toUInt();
            emit m_dbusAdapter->accountConfigurationCanceled(accountId, "User requested cancellation");
        }
        
        QVariantMap data;
        data["previousState"] = previousState;
        emit m_dbusAdapter->dialogStateChanged("none", "none", data);
    }
    
    // 这里可以添加实际取消对话框的逻辑
    // 例如关闭当前打开的对话框
}

QString KDEOAuth2Plugin::dbusGetCurrentDialogState() const
{
    return m_currentDialogState;
}

QVariantMap KDEOAuth2Plugin::dbusGetDialogInfo() const
{
    QVariantMap info = m_dialogInfo;
    info["currentState"] = m_currentDialogState;
    info["authMethod"] = m_authMethod;
    return info;
}

void KDEOAuth2Plugin::dbusSetOAuth2ServerUrl(const QString &serverUrl)
{
    qDebug() << "KDEOAuth2Plugin::dbusSetOAuth2ServerUrl:" << serverUrl;
    m_serverUrl = serverUrl;
}

void KDEOAuth2Plugin::dbusSetOAuth2ClientId(const QString &clientId)
{
    qDebug() << "KDEOAuth2Plugin::dbusSetOAuth2ClientId:" << clientId;
    m_clientId = clientId;
}

void KDEOAuth2Plugin::dbusSetOAuth2RedirectUri(const QString &redirectUri)
{
    qDebug() << "KDEOAuth2Plugin::dbusSetOAuth2RedirectUri:" << redirectUri;
    m_redirectUri = redirectUri;
}

void KDEOAuth2Plugin::dbusSetOAuth2Scope(const QString &scope)
{
    qDebug() << "KDEOAuth2Plugin::dbusSetOAuth2Scope:" << scope;
    m_scope = scope;
}

QVariantMap KDEOAuth2Plugin::dbusGetOAuth2Configuration() const
{
    QVariantMap config;
    config["serverUrl"] = m_serverUrl;
    config["clientId"] = m_clientId;
    config["authPath"] = m_authPath;
    config["tokenPath"] = m_tokenPath;
    config["userInfoPath"] = m_userInfoPath;
    config["redirectUri"] = m_redirectUri;
    config["scope"] = m_scope;
    config["authMethod"] = m_authMethod;
    return config;
}

bool KDEOAuth2Plugin::dbusTestConnection()
{
    qDebug() << "KDEOAuth2Plugin::dbusTestConnection: testing connection to" << m_serverUrl;
    
    // 简单的连接测试 - 尝试访问服务器
    QUrl testUrl(m_serverUrl);
    if (!testUrl.isValid() || testUrl.scheme().isEmpty() || testUrl.host().isEmpty()) {
        qDebug() << "KDEOAuth2Plugin::dbusTestConnection: invalid server URL";
        return false;
    }
    
    // 这里可以添加更复杂的连接测试逻辑
    // 例如发送HEAD请求检查服务器响应
    return true;
}

QStringList KDEOAuth2Plugin::dbusGetSupportedAuthMethods() const
{
    return QStringList() << "auto" << "manual" << "callback";
}

void KDEOAuth2Plugin::dbusSetAuthMethod(const QString &method)
{
    qDebug() << "KDEOAuth2Plugin::dbusSetAuthMethod:" << method;
    
    QStringList supportedMethods = dbusGetSupportedAuthMethods();
    if (supportedMethods.contains(method)) {
        m_authMethod = method;
    } else {
        qWarning() << "KDEOAuth2Plugin::dbusSetAuthMethod: unsupported method" << method;
    }
}

// 新增的DBus方法实现
bool KDEOAuth2Plugin::dbusSetOAuth2Config(const QString &server, const QString &clientId, const QString &authPath, const QString &tokenPath)
{
    qDebug() << "KDEOAuth2Plugin::dbusSetOAuth2Config:" << server << clientId << authPath << tokenPath;
    
    try {
        // 设置各个配置参数
        dbusSetOAuth2ServerUrl(server);
        dbusSetOAuth2ClientId(clientId);
        // 注意：当前实现中没有直接设置path的方法，这里需要扩展
        
        // 验证URL有效性
        QUrl serverUrl(server);
        if (!serverUrl.isValid() || serverUrl.scheme().isEmpty()) {
            qWarning() << "Invalid server URL:" << server;
            return false;
        }
        
        // 发送配置变化信号
        if (m_dbusAdapter) {
            emit m_dbusAdapter->oauth2ConfigChanged(server, clientId, authPath, tokenPath);
        }
        
        return true;
    } catch (...) {
        qWarning() << "Exception in dbusSetOAuth2Config";
        return false;
    }
}

QString KDEOAuth2Plugin::dbusGetAuthMethod() const
{
    return m_authMethod;
}

int KDEOAuth2Plugin::dbusGetAccountCount() const
{
    qDebug() << "KDEOAuth2Plugin::dbusGetAccountCount";
    
    if (!m_providerName.isEmpty()) {
        return getAccountCountForProvider(m_providerName);
    }
    return 0;
}

QString KDEOAuth2Plugin::dbusGetPluginVersion() const
{
    return "1.0.0";
}

QString KDEOAuth2Plugin::dbusGetPluginInfo() const
{
    return "KDE OAuth2 Plugin with enhanced DBus interface for multi-language support";
}

QString KDEOAuth2Plugin::dbusGetLastError() const
{
    // 这里可以返回最后的错误信息，当前简化实现
    return m_lastError;
}

bool KDEOAuth2Plugin::dbusClearError()
{
    m_lastError.clear();
    return true;
}

QVariantMap KDEOAuth2Plugin::dbusGetCurrentDialogInfo() const
{
    return m_dialogInfo;
}

void KDEOAuth2Plugin::startOAuth2Flow()
{
    qDebug() << "KDEOAuth2Plugin: starting OAuth2 authentication flow";
    
    // 更新状态
    m_currentDialogState = "oauth_in_progress";
    
    QString authUrl = generateAuthUrl();
    QUrl urlCheck(authUrl);
    if (!urlCheck.isValid() || urlCheck.scheme().isEmpty() || urlCheck.host().isEmpty()) {
        QString errorMsg = QString("生成的认证URL无效：%1\n请联系开发人员检查OAuth2配置。").arg(authUrl);
        QMessageBox::critical(nullptr, "OAuth2配置错误", errorMsg);
        qDebug() << "KDEOAuth2Plugin: Invalid authUrl generated:" << authUrl;
        
        // 发送错误信号
        if (m_dbusAdapter) {
            emit m_dbusAdapter->accountCreationError("invalid_auth_url", errorMsg);
        }
        
        m_currentDialogState = "none";
        m_dialogInfo.clear();
        emit canceled();
        return;
    }
    
    qDebug() << "KDEOAuth2Plugin: generated auth URL:" << authUrl;
    
    // 更新对话框信息
    m_dialogInfo["auth_url"] = authUrl;
    m_dialogInfo["redirect_uri"] = m_redirectUri;
    
    // 发送状态变化信号
    if (m_dbusAdapter) {
        emit m_dbusAdapter->dialogStateChanged("new_account", m_currentDialogState, m_dialogInfo);
    }
    
    // 创建OAuth2认证对话框，传递重定向URI
    OAuth2Dialog *dialog = new OAuth2Dialog(authUrl, m_redirectUri);
    
    int result = dialog->exec();
    
    if (result == QDialog::Accepted) {
        QString authCode = dialog->getAuthorizationCode();
        qDebug() << "KDEOAuth2Plugin: received authorization code:" << authCode;
        
        if (!authCode.isEmpty()) {
            // 更新状态到token交换阶段
            m_currentDialogState = "token_exchange";
            m_dialogInfo["auth_code"] = authCode;
            
            if (m_dbusAdapter) {
                emit m_dbusAdapter->dialogStateChanged("new_account", m_currentDialogState, m_dialogInfo);
            }
            
            exchangeCodeForToken(authCode);
        } else {
            qDebug() << "KDEOAuth2Plugin: no authorization code received";
            
            if (m_dbusAdapter) {
                emit m_dbusAdapter->accountCreationCanceled("未收到授权码");
            }
            
            m_currentDialogState = "none";
            m_dialogInfo.clear();
            emit canceled();
        }
    } else {
        qDebug() << "KDEOAuth2Plugin: user canceled authentication";
        
        if (m_dbusAdapter) {
            emit m_dbusAdapter->accountCreationCanceled("用户取消了认证");
        }
        
        m_currentDialogState = "none";
        m_dialogInfo.clear();
        emit canceled();
    }
    
    dialog->deleteLater();
}

QString KDEOAuth2Plugin::generateAuthUrl() const
{
    QUrl url(m_serverUrl + m_authPath);
    QUrlQuery query;
    
    query.addQueryItem("response_type", "code");
    query.addQueryItem("client_id", m_clientId);
    query.addQueryItem("redirect_uri", m_redirectUri);
    query.addQueryItem("scope", "openid");
    query.addQueryItem("state", QUuid::createUuid().toString(QUuid::WithoutBraces));
    
    url.setQuery(query);
    return url.toString();
}

void KDEOAuth2Plugin::exchangeCodeForToken(const QString &authCode)
{
    qDebug() << "KDEOAuth2Plugin: exchanging authorization code for access token";
    
    // 更新状态
    m_currentDialogState = "token_exchange";
    m_dialogInfo["status"] = "requesting_token";
    
    if (m_dbusAdapter) {
        emit m_dbusAdapter->dialogStateChanged("new_account", m_currentDialogState, m_dialogInfo);
    }
    
    QUrl url(m_serverUrl + m_tokenPath);
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");
    
    QUrlQuery postData;
    postData.addQueryItem("grant_type", "authorization_code");
    postData.addQueryItem("client_id", m_clientId);
    postData.addQueryItem("code", authCode);
    postData.addQueryItem("redirect_uri", m_redirectUri);
    
    QNetworkReply *reply = m_networkManager->post(request, postData.toString(QUrl::FullyEncoded).toUtf8());
    connect(reply, &QNetworkReply::finished, this, &KDEOAuth2Plugin::onTokenRequestFinished);
}

void KDEOAuth2Plugin::onTokenRequestFinished()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) {
        qDebug() << "KDEOAuth2Plugin: invalid reply object";
        
        if (m_dbusAdapter) {
            emit m_dbusAdapter->accountCreationError("invalid_reply", "无效的网络响应对象");
        }
        
        m_currentDialogState = "none";
        m_dialogInfo.clear();
        emit canceled();
        return;
    }
    
    if (reply->error() != QNetworkReply::NoError) {
        QString errorMsg = QString("Token请求失败：%1").arg(reply->errorString());
        qDebug() << "KDEOAuth2Plugin: token request failed:" << reply->errorString();
        
        if (m_dbusAdapter) {
            emit m_dbusAdapter->accountCreationError("token_request_failed", errorMsg);
        }
        
        m_currentDialogState = "none";
        m_dialogInfo.clear();
        emit canceled();
        reply->deleteLater();
        return;
    }
    
    QByteArray data = reply->readAll();
    qDebug() << "KDEOAuth2Plugin: token response:" << data;
    
    // 更新状态
    m_currentDialogState = "processing_token";
    m_dialogInfo["status"] = "parsing_token_response";
    
    if (m_dbusAdapter) {
        emit m_dbusAdapter->dialogStateChanged("new_account", m_currentDialogState, m_dialogInfo);
    }
    
    QJsonDocument doc = QJsonDocument::fromJson(data);
    QJsonObject obj = doc.object();
    if (obj.contains("access_token")) {
        m_currentAccessToken = obj["access_token"].toString();
        if (obj.contains("refresh_token")) {
            m_currentRefreshToken = obj["refresh_token"].toString();
        }
        if (obj.contains("expires_in")) {
            m_currentExpiresIn = obj["expires_in"].toInt();
            qDebug() << "KDEOAuth2Plugin: expires_in received:" << m_currentExpiresIn;
        }
        
        // 更新对话框信息
        m_dialogInfo["access_token_received"] = true;
        m_dialogInfo["has_refresh_token"] = !m_currentRefreshToken.isEmpty();
        if (m_currentExpiresIn > 0) {
            m_dialogInfo["expires_in"] = m_currentExpiresIn;
        }
        
        qDebug() << "KDEOAuth2Plugin: successfully obtained access token";
        
        // 更新状态到获取用户信息阶段
        m_currentDialogState = "fetching_user_info";
        m_dialogInfo["status"] = "requesting_user_info";
        
        if (m_dbusAdapter) {
            emit m_dbusAdapter->dialogStateChanged("new_account", m_currentDialogState, m_dialogInfo);
        }
        
        fetchUserInfo(m_currentAccessToken);
    } else {
        qDebug() << "KDEOAuth2Plugin: no access token in response";
        
        if (m_dbusAdapter) {
            emit m_dbusAdapter->accountCreationError("no_access_token", "响应中未包含访问令牌");
        }
        
        m_currentDialogState = "none";
        m_dialogInfo.clear();
        emit canceled();
    }
    
    reply->deleteLater();
}

void KDEOAuth2Plugin::fetchUserInfo(const QString &accessToken)
{
    qDebug() << "KDEOAuth2Plugin: fetching user information";
    
    QUrl url(m_serverUrl + m_userInfoPath);
    QNetworkRequest request(url);
    request.setRawHeader("Authorization", QString("Bearer %1").arg(accessToken).toUtf8());
    
    QNetworkReply *reply = m_networkManager->get(request);
    connect(reply, &QNetworkReply::finished, this, &KDEOAuth2Plugin::onUserInfoRequestFinished);
}

void KDEOAuth2Plugin::onUserInfoRequestFinished()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) {
        qDebug() << "KDEOAuth2Plugin: invalid reply object";
        
        if (m_dbusAdapter) {
            emit m_dbusAdapter->accountCreationError("invalid_reply", "无效的网络响应对象");
        }
        
        m_currentDialogState = "none";
        m_dialogInfo.clear();
        emit canceled();
        return;
    }
    
    qDebug() << "KDEOAuth2Plugin: user info request status code:" << reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    qDebug() << "KDEOAuth2Plugin: user info request error:" << reply->error();
    qDebug() << "KDEOAuth2Plugin: user info request error string:" << reply->errorString();
    
    if (reply->error() != QNetworkReply::NoError) {
        qDebug() << "KDEOAuth2Plugin: user info request failed:" << reply->errorString();
        
        // 更新状态 - 用户信息获取失败，但仍可尝试创建基本账户
        m_currentDialogState = "creating_account";
        m_dialogInfo["status"] = "user_info_failed_creating_basic";
        m_dialogInfo["warning"] = "用户信息获取失败，将创建基本账户";
        
        if (m_dbusAdapter) {
            emit m_dbusAdapter->dialogStateChanged("new_account", m_currentDialogState, m_dialogInfo);
        }
        
        // 即使获取用户信息失败，我们仍然可以创建账户
        createAccountWithBasicInfo();
        reply->deleteLater();
        return;
    }
    
    QByteArray data = reply->readAll();
    qDebug() << "KDEOAuth2Plugin: raw user info response data:" << data;
    qDebug() << "KDEOAuth2Plugin: user info response size:" << data.size() << "bytes";
    
    // 更新状态
    m_currentDialogState = "processing_user_info";
    m_dialogInfo["status"] = "parsing_user_info";
    
    if (m_dbusAdapter) {
        emit m_dbusAdapter->dialogStateChanged("new_account", m_currentDialogState, m_dialogInfo);
    }
    
    // 尝试解析JSON
    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);
    
    if (parseError.error != QJsonParseError::NoError) {
        qDebug() << "KDEOAuth2Plugin: JSON parse error:" << parseError.errorString();
        qDebug() << "KDEOAuth2Plugin: JSON parse error at offset:" << parseError.offset;
        
        // 更新状态并创建基本账户
        m_currentDialogState = "creating_account";
        m_dialogInfo["status"] = "json_parse_failed_creating_basic";
        m_dialogInfo["warning"] = "用户信息解析失败，将创建基本账户";
        
        if (m_dbusAdapter) {
            emit m_dbusAdapter->dialogStateChanged("new_account", m_currentDialogState, m_dialogInfo);
        }
        
        // 即使解析失败，也创建基本账户
        createAccountWithBasicInfo();
        reply->deleteLater();
        return;
    }
    
    QJsonObject userObj = doc.object();
    qDebug() << "KDEOAuth2Plugin: parsed JSON object keys:" << userObj.keys();
    
    // 详细打印每个字段
    for (auto it = userObj.begin(); it != userObj.end(); ++it) {
        qDebug() << "KDEOAuth2Plugin: user info field" << it.key() << "=" << it.value();
    }
    
    // 更新状态为创建账户
    m_currentDialogState = "creating_account";
    m_dialogInfo["status"] = "creating_account_with_user_info";
    m_dialogInfo["user_fields_count"] = userObj.keys().size();
    
    if (m_dbusAdapter) {
        emit m_dbusAdapter->dialogStateChanged("new_account", m_currentDialogState, m_dialogInfo);
    }
    
    // 准备账户数据
    QVariantMap authData;
    authData["server"] = m_serverUrl;
    authData["client_id"] = m_clientId;
    authData["access_token"] = m_currentAccessToken;
    if (!m_currentRefreshToken.isEmpty()) {
        authData["refresh_token"] = m_currentRefreshToken;
    }
    if (m_currentExpiresIn > 0) {
        authData["expires_in"] = m_currentExpiresIn;
    }
    
    // 智能提取用户信息 - 支持多种常见字段名和.NET Claims格式
    QString userId, username, email, displayName, role, portrait;
    
    // 尝试获取用户ID (支持多种字段名)
    if (userObj.contains("sub")) {
        userId = userObj["sub"].toString();
    } else if (userObj.contains("http://schemas.xmlsoap.org/ws/2005/05/identity/claims/nameidentifier")) {
        userId = userObj["http://schemas.xmlsoap.org/ws/2005/05/identity/claims/nameidentifier"].toString();
    } else if (userObj.contains("id")) {
        userId = userObj["id"].toString();
    } else if (userObj.contains("user_id")) {
        userId = userObj["user_id"].toString();
    }
    
    // 尝试获取用户名/显示名 (支持多种字段名)
    if (userObj.contains("http://schemas.xmlsoap.org/ws/2005/05/identity/claims/name")) {
        username = userObj["http://schemas.xmlsoap.org/ws/2005/05/identity/claims/name"].toString();
    } else if (userObj.contains("name")) {
        username = userObj["name"].toString();
    } else if (userObj.contains("username")) {
        username = userObj["username"].toString();
    } else if (userObj.contains("login")) {
        username = userObj["login"].toString();
    } else if (userObj.contains("preferred_username")) {
        username = userObj["preferred_username"].toString();
    }
    
    // 尝试获取邮箱
    if (userObj.contains("email")) {
        email = userObj["email"].toString();
    } else if (userObj.contains("http://schemas.xmlsoap.org/ws/2005/05/identity/claims/emailaddress")) {
        email = userObj["http://schemas.xmlsoap.org/ws/2005/05/identity/claims/emailaddress"].toString();
    } else if (userObj.contains("mail")) {
        email = userObj["mail"].toString();
    }
    
    // 尝试获取角色
    if (userObj.contains("http://schemas.microsoft.com/ws/2008/06/identity/claims/role")) {
        role = userObj["http://schemas.microsoft.com/ws/2008/06/identity/claims/role"].toString();
    } else if (userObj.contains("role")) {
        role = userObj["role"].toString();
    } else if (userObj.contains("roles")) {
        role = userObj["roles"].toString();
    }
    
    // 尝试获取头像
    if (userObj.contains("portrait")) {
        portrait = userObj["portrait"].toString();
    } else if (userObj.contains("picture")) {
        portrait = userObj["picture"].toString();
    } else if (userObj.contains("avatar")) {
        portrait = userObj["avatar"].toString();
    }
    
    // 确定显示名称的优先级
    if (!username.isEmpty()) {
        displayName = username;
    } else if (!email.isEmpty()) {
        displayName = email;
    } else if (!userId.isEmpty()) {
        displayName = "User " + userId;
    } else {
        displayName = "OAuth2 User";
    }
    
    // 添加提取到的用户信息
    if (!userId.isEmpty()) {
        authData["user_id"] = userId;
        qDebug() << "KDEOAuth2Plugin: extracted user_id:" << userId;
    }
    if (!username.isEmpty()) {
        authData["username"] = username;
        qDebug() << "KDEOAuth2Plugin: extracted username:" << username;
    }
    if (!email.isEmpty()) {
        authData["email"] = email;
        qDebug() << "KDEOAuth2Plugin: extracted email:" << email;
    }
    if (!role.isEmpty()) {
        authData["role"] = role;
        qDebug() << "KDEOAuth2Plugin: extracted role:" << role;
    }
    if (!portrait.isEmpty()) {
        authData["portrait"] = portrait;
        // 如果是相对路径，转换为完整URL
        if (portrait.startsWith("/")) {
            authData["portrait_url"] = m_serverUrl + portrait;
        } else {
            authData["portrait_url"] = portrait;
        }
        qDebug() << "KDEOAuth2Plugin: extracted portrait:" << portrait;
    }
    
    // 添加其他标准JWT字段
    QStringList jwtFields = {"iss", "aud", "iat", "exp", "nbf"};
    for (const QString &field : jwtFields) {
        if (userObj.contains(field)) {
            authData[field] = userObj[field].toVariant();
            qDebug() << "KDEOAuth2Plugin: extracted JWT field" << field << ":" << userObj[field];
        }
    }
    
    qDebug() << "KDEOAuth2Plugin: final display name:" << displayName;
    qDebug() << "KDEOAuth2Plugin: final auth data keys:" << authData.keys();
    qDebug() << "KDEOAuth2Plugin: authentication successful, creating account";
    
    // 更新最终状态
    m_currentDialogState = "completed";
    m_dialogInfo["status"] = "account_created_successfully";
    m_dialogInfo["display_name"] = displayName;
    m_dialogInfo["account_data_keys"] = QVariant::fromValue(authData.keys());
    
    if (m_dbusAdapter) {
        emit m_dbusAdapter->dialogStateChanged("new_account", m_currentDialogState, m_dialogInfo);
        emit m_dbusAdapter->accountCreated(0, displayName, authData); // 使用0作为临时账户ID
    }
    
    // 重置状态
    m_currentDialogState = "none";
    m_dialogInfo.clear();
    
    emit success(displayName, "", authData);
    
    reply->deleteLater();
}

void KDEOAuth2Plugin::showConfigureAccountDialog(const quint32 accountId)
{
    qDebug() << "KDEOAuth2Plugin: showing configuration dialog for account" << accountId;
    
    QMessageBox msgBox;
    msgBox.setWindowTitle("Account Configuration");
    msgBox.setText(QString("配置OAuth2账户 ID: %1").arg(accountId));
    msgBox.setStandardButtons(QMessageBox::Ok | QMessageBox::Cancel);
    
    int result = msgBox.exec();
    
    if (result == QMessageBox::Ok) {
        emit configUiReady();
    } else {
        emit canceled();
    }
}

QStringList KDEOAuth2Plugin::supportedServicesForConfig() const
{
    return QStringList() << "oauth2-service";
}

void KDEOAuth2Plugin::createAccountWithBasicInfo()
{
    qDebug() << "KDEOAuth2Plugin: creating account with basic info (no user data available)";
    
    // 更新状态
    m_currentDialogState = "creating_account";
    m_dialogInfo["status"] = "creating_basic_account";
    m_dialogInfo["warning"] = "使用基本信息创建账户";
    
    if (m_dbusAdapter) {
        emit m_dbusAdapter->dialogStateChanged("new_account", m_currentDialogState, m_dialogInfo);
    }
    
    // 准备基本账户数据
    QVariantMap authData;
    authData["server"] = m_serverUrl;
    authData["client_id"] = m_clientId;
    authData["access_token"] = m_currentAccessToken;
    if (!m_currentRefreshToken.isEmpty()) {
        authData["refresh_token"] = m_currentRefreshToken;
    }
    if (m_currentExpiresIn > 0) {
        authData["expires_in"] = m_currentExpiresIn;
    }
    
    // 使用默认显示名称
    QString displayName = "OAuth2 User";
    
    // 更新最终状态
    m_currentDialogState = "completed";
    m_dialogInfo["status"] = "basic_account_created_successfully";
    m_dialogInfo["display_name"] = displayName;
    m_dialogInfo["account_data_keys"] = QVariant::fromValue(authData.keys());
    
    if (m_dbusAdapter) {
        emit m_dbusAdapter->dialogStateChanged("new_account", m_currentDialogState, m_dialogInfo);
        emit m_dbusAdapter->accountCreated(0, displayName, authData); // 使用0作为临时账户ID
    }
    
    // 重置状态
    m_currentDialogState = "none";
    m_dialogInfo.clear();
    
    qDebug() << "KDEOAuth2Plugin: creating basic account with display name:" << displayName;
    emit success(displayName, "", authData);
}

void KDEOAuth2Plugin::loadConfigurationFromEnvironment()
{
    qDebug() << "KDEOAuth2Plugin: loading configuration from environment variables...";
    
    QString configServer = qEnvironmentVariable("OAUTH2_SERVER_URL");
    QString configClientId = qEnvironmentVariable("OAUTH2_CLIENT_ID");
    QString configAuthPath = qEnvironmentVariable("OAUTH2_AUTH_PATH");
    QString configTokenPath = qEnvironmentVariable("OAUTH2_TOKEN_PATH");
    QString configUserInfoPath = qEnvironmentVariable("OAUTH2_USERINFO_PATH");
    QString configRedirectUri = qEnvironmentVariable("OAUTH2_REDIRECT_URI");
    QString configScope = qEnvironmentVariable("OAUTH2_SCOPE");
    
    if (!configServer.isEmpty()) {
        m_serverUrl = configServer;
        qDebug() << "KDEOAuth2Plugin: loaded server URL from config:" << m_serverUrl;
    }
    
    if (!configClientId.isEmpty()) {
        m_clientId = configClientId;
        qDebug() << "KDEOAuth2Plugin: loaded client ID from config:" << m_clientId;
    }
    
    if (!configAuthPath.isEmpty()) {
        m_authPath = configAuthPath;
        qDebug() << "KDEOAuth2Plugin: loaded auth path from config:" << m_authPath;
    }
    
    if (!configTokenPath.isEmpty()) {
        m_tokenPath = configTokenPath;
        qDebug() << "KDEOAuth2Plugin: loaded token path from config:" << m_tokenPath;
    }
    
    if (!configUserInfoPath.isEmpty()) {
        m_userInfoPath = configUserInfoPath;
        qDebug() << "KDEOAuth2Plugin: loaded userinfo path from config:" << m_userInfoPath;
    }
    
    if (!configRedirectUri.isEmpty()) {
        m_redirectUri = configRedirectUri;
        qDebug() << "KDEOAuth2Plugin: loaded redirect URI from config:" << m_redirectUri;
    }
    
    if (!configScope.isEmpty()) {
        m_scope = configScope;
        qDebug() << "KDEOAuth2Plugin: loaded scope from config:" << m_scope;
    }
    
    qDebug() << "KDEOAuth2Plugin: final configuration - Server:" << m_serverUrl 
             << "Client ID:" << m_clientId << "Redirect URI:" << m_redirectUri;
}

void KDEOAuth2Plugin::loadProviderConfiguration()
{
    qDebug() << "KDEOAuth2Plugin: loading provider configuration...";

    // 优先查找系统 provider 路径
    QString providerFile = "/usr/share/accounts/providers/kde/gzweibo-oauth2.provider";
    QFile file(providerFile);
    if (!file.exists()) {
        // 回退到本地工作目录
        providerFile = "gzweibo-oauth2.provider";
        file.setFileName(providerFile);
    }
    if (file.exists() && file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QXmlStreamReader xml(&file);
        // 递归进入 group: auth → oauth2 → user_agent
        while (!xml.atEnd()) {
            xml.readNext();
            if (xml.isStartElement() && xml.name() == "group" && xml.attributes().value("name") == "auth") {
                // 进入 auth 层
                while (!xml.atEnd()) {
                    xml.readNext();
                    if (xml.isStartElement() && xml.name() == "group" && xml.attributes().value("name") == "oauth2") {
                        // 进入 oauth2 层
                        while (!xml.atEnd()) {
                            xml.readNext();
                            if (xml.isStartElement() && xml.name() == "group" && xml.attributes().value("name") == "user_agent") {
                                // 进入 user_agent 层
                                while (!xml.atEnd()) {
                                    xml.readNext();
                                    if (xml.isStartElement() && xml.name() == "setting") {
                                        QString name = xml.attributes().value("name").toString();
                                        QString value = xml.readElementText();
                                        if (name == "Host") {
                                            m_serverUrl = value;
                                            qDebug() << "KDEOAuth2Plugin: loaded Host from provider:" << m_serverUrl;
                                        } else if (name == "AuthPath") {
                                            m_authPath = value;
                                            qDebug() << "KDEOAuth2Plugin: loaded AuthPath from provider:" << m_authPath;
                                        } else if (name == "TokenPath") {
                                            m_tokenPath = value;
                                            qDebug() << "KDEOAuth2Plugin: loaded TokenPath from provider:" << m_tokenPath;
                                        } else if (name == "UserInfoPath") {
                                            m_userInfoPath = value;
                                            qDebug() << "KDEOAuth2Plugin: loaded UserInfoPath from provider:" << m_userInfoPath;
                                        } else if (name == "ClientId") {
                                            m_clientId = value;
                                            qDebug() << "KDEOAuth2Plugin: loaded ClientId from provider:" << m_clientId;
                                        } else if (name == "RedirectUri") {
                                            m_redirectUri = value;
                                            qDebug() << "KDEOAuth2Plugin: loaded RedirectUri from provider:" << m_redirectUri;
                                        } else if (name == "Scope") {
                                            m_scope = value;
                                            qDebug() << "KDEOAuth2Plugin: loaded Scope from provider:" << m_scope;
                                        }
                                    } else if (xml.isEndElement() && xml.name() == "group") {
                                        break; // 退出 user_agent 层
                                    }
                                }
                            } else if (xml.isEndElement() && xml.name() == "group") {
                                break; // 退出 oauth2 层
                            }
                        }
                    } else if (xml.isEndElement() && xml.name() == "group") {
                        break; // 退出 auth 层
                    }
                }
            }
        }
        if (xml.hasError()) {
            qDebug() << "KDEOAuth2Plugin: XML parse error in provider file:" << xml.errorString();
        }
        file.close();
    } else {
        qDebug() << "KDEOAuth2Plugin: provider file not found, fallback to env vars.";
    }

    // 环境变量作为兜底
    QString configServer = qEnvironmentVariable("OAUTH2_SERVER_URL");
    QString configClientId = qEnvironmentVariable("OAUTH2_CLIENT_ID");
    QString configAuthPath = qEnvironmentVariable("OAUTH2_AUTH_PATH");
    QString configTokenPath = qEnvironmentVariable("OAUTH2_TOKEN_PATH");
    QString configUserInfoPath = qEnvironmentVariable("OAUTH2_USERINFO_PATH");
    QString configRedirectUri = qEnvironmentVariable("OAUTH2_REDIRECT_URI");
    QString configScope = qEnvironmentVariable("OAUTH2_SCOPE");

    if (!configServer.isEmpty()) {
        m_serverUrl = configServer;
        qDebug() << "KDEOAuth2Plugin: loaded server URL from env:" << m_serverUrl;
    }
    if (!configClientId.isEmpty()) {
        m_clientId = configClientId;
        qDebug() << "KDEOAuth2Plugin: loaded client ID from env:" << m_clientId;
    }
    if (!configAuthPath.isEmpty()) {
        m_authPath = configAuthPath;
        qDebug() << "KDEOAuth2Plugin: loaded auth path from env:" << m_authPath;
    }
    if (!configTokenPath.isEmpty()) {
        m_tokenPath = configTokenPath;
        qDebug() << "KDEOAuth2Plugin: loaded token path from env:" << m_tokenPath;
    }
    if (!configUserInfoPath.isEmpty()) {
        m_userInfoPath = configUserInfoPath;
        qDebug() << "KDEOAuth2Plugin: loaded userinfo path from env:" << m_userInfoPath;
    }
    if (!configRedirectUri.isEmpty()) {
        m_redirectUri = configRedirectUri;
        qDebug() << "KDEOAuth2Plugin: loaded redirect URI from env:" << m_redirectUri;
    }
    if (!configScope.isEmpty()) {
        m_scope = configScope;
        qDebug() << "KDEOAuth2Plugin: loaded scope from env:" << m_scope;
    }

    qDebug() << "KDEOAuth2Plugin: final configuration - Server:" << m_serverUrl 
             << "Client ID:" << m_clientId << "Redirect URI:" << m_redirectUri;
}

#include "kdeoauth2plugin.moc"

// DBus适配器实现
KDEOAuth2PluginDBusAdapter::KDEOAuth2PluginDBusAdapter(KDEOAuth2Plugin *parent)
    : QDBusAbstractAdaptor(parent)
    , m_plugin(parent)
{
}

void KDEOAuth2PluginDBusAdapter::initNewAccount()
{
    qDebug() << "=== KDEOAuth2PluginDBusAdapter: initNewAccount called via DBus ===";
    qDebug() << "KDEOAuth2PluginDBusAdapter: Checking GUI environment...";

    // 检查是否有GUI环境
    if (!qApp || !qApp->desktop()) {
        qWarning() << "=== KDEOAUTH2 WARNING: No GUI environment available ===";
        qWarning() << "KDEOAuth2PluginDBusAdapter: Cannot display new account dialog in headless environment";
        qWarning() << "KDEOAuth2PluginDBusAdapter: Please run this command in a graphical KDE environment";
        qWarning() << "KDEOAuth2PluginDBusAdapter: Alternative: Use command-line OAuth2 setup or web-based authentication";

        // 在没有GUI的环境中，提供命令行替代方案
        qWarning() << "KDEOAuth2PluginDBusAdapter: For headless setup, you can use the following command-line approach:";
        qWarning() << "KDEOAuth2PluginDBusAdapter: 1. Set DISPLAY environment variable: export DISPLAY=:0";
        qWarning() << "KDEOAuth2PluginDBusAdapter: 2. Or use X11 forwarding if connecting via SSH: ssh -X user@host";
        qWarning() << "KDEOAuth2PluginDBusAdapter: 3. Or run the command directly in the KDE Plasma session";

        // 尝试设置DISPLAY环境变量（如果可能的话）
        if (qgetenv("DISPLAY").isEmpty()) {
            qputenv("DISPLAY", ":0");
            qDebug() << "KDEOAuth2PluginDBusAdapter: Attempted to set DISPLAY=:0 for GUI access";
        }

        // 再次检查GUI环境
        if (qApp && qApp->desktop()) {
            qDebug() << "KDEOAuth2PluginDBusAdapter: GUI environment now available after DISPLAY fix";
            m_plugin->init(KAccountsUiPlugin::NewAccountDialog);
            return;
        }

        // 如果仍然没有GUI，返回错误但不崩溃
        qWarning() << "=== KDEOAUTH2 ERROR: Still no GUI environment available ===";
        return;
    }

    qDebug() << "KDEOAuth2PluginDBusAdapter: GUI environment available, proceeding with dialog";
    m_plugin->init(KAccountsUiPlugin::NewAccountDialog);
}

void KDEOAuth2PluginDBusAdapter::initConfigureAccount(quint32 accountId)
{
    qDebug() << "KDEOAuth2PluginDBusAdapter: initConfigureAccount called via DBus for account" << accountId;

    // 检查是否有GUI环境
    if (!qApp || !qApp->desktop()) {
        qWarning() << "KDEOAuth2PluginDBusAdapter: No GUI environment available for DBus call";
        qWarning() << "KDEOAuth2PluginDBusAdapter: Cannot display configure account dialog in headless environment";
        qWarning() << "KDEOAuth2PluginDBusAdapter: Please run this command in a graphical KDE environment";
        qWarning() << "KDEOAuth2PluginDBusAdapter: Alternative: Use command-line OAuth2 configuration or web-based setup";

        // 在没有GUI的环境中，提供命令行替代方案
        qWarning() << "KDEOAuth2PluginDBusAdapter: For headless setup, you can use the following command-line approach:";
        qWarning() << "KDEOAuth2PluginDBusAdapter: 1. Set DISPLAY environment variable: export DISPLAY=:0";
        qWarning() << "KDEOAuth2PluginDBusAdapter: 2. Or use X11 forwarding if connecting via SSH: ssh -X user@host";
        qWarning() << "KDEOAuth2PluginDBusAdapter: 3. Or run the command directly in the KDE Plasma session";

        // 尝试设置DISPLAY环境变量（如果可能的话）
        if (qgetenv("DISPLAY").isEmpty()) {
            qputenv("DISPLAY", ":0");
            qDebug() << "KDEOAuth2PluginDBusAdapter: Attempted to set DISPLAY=:0 for GUI access";
        }

        // 再次检查GUI环境
        if (qApp && qApp->desktop()) {
            qDebug() << "KDEOAuth2PluginDBusAdapter: GUI environment now available after DISPLAY fix";
            m_plugin->init(KAccountsUiPlugin::ConfigureAccountDialog);
            return;
        }

        // 如果仍然没有GUI，返回错误但不崩溃
        qWarning() << "KDEOAuth2PluginDBusAdapter: Still no GUI environment available";
        return;
    }

    m_plugin->init(KAccountsUiPlugin::ConfigureAccountDialog);
    // 注意：这里我们忽略accountId，因为原始的init方法不支持传递accountId
    // 如果需要，可以修改为直接调用showConfigureAccountDialog
}

QString KDEOAuth2PluginDBusAdapter::getProviderName()
{
    return m_plugin->dbusGetProviderName();
}

void KDEOAuth2PluginDBusAdapter::setProviderName(const QString &providerName)
{
    m_plugin->dbusSetProviderName(providerName);
}

QStringList KDEOAuth2PluginDBusAdapter::getAccountsList()
{
    return m_plugin->dbusGetAccountsList();
}

bool KDEOAuth2PluginDBusAdapter::deleteAccount(quint32 accountId)
{
    qDebug() << "KDEOAuth2PluginDBusAdapter: deleteAccount called via DBus for account" << accountId;
    return m_plugin->dbusDeleteAccount(accountId);
}

bool KDEOAuth2PluginDBusAdapter::enableAccount(quint32 accountId, bool enabled)
{
    qDebug() << "KDEOAuth2PluginDBusAdapter: enableAccount called via DBus for account" << accountId << "enabled:" << enabled;
    return m_plugin->dbusEnableAccount(accountId, enabled);
}

QVariantMap KDEOAuth2PluginDBusAdapter::getAccountDetails(quint32 accountId)
{
    qDebug() << "KDEOAuth2PluginDBusAdapter: getAccountDetails called via DBus for account" << accountId;
    return m_plugin->dbusGetAccountDetails(accountId);
}

bool KDEOAuth2PluginDBusAdapter::refreshToken(quint32 accountId)
{
    qDebug() << "KDEOAuth2PluginDBusAdapter: refreshToken called via DBus for account" << accountId;
    return m_plugin->dbusRefreshToken(accountId);
}

QVariantMap KDEOAuth2PluginDBusAdapter::getPluginStatus()
{
    qDebug() << "KDEOAuth2PluginDBusAdapter: getPluginStatus called via DBus";
    return m_plugin->dbusGetPluginStatus();
}

bool KDEOAuth2PluginDBusAdapter::isHeadlessEnvironment()
{
    qDebug() << "KDEOAuth2PluginDBusAdapter: isHeadlessEnvironment called via DBus";
    // 检查是否有GUI环境
    return (!qApp || !qApp->desktop());
}

// 扩展的DBus接口实现

void KDEOAuth2PluginDBusAdapter::initNewAccountWithConfig(const QVariantMap &config)
{
    qDebug() << "KDEOAuth2PluginDBusAdapter: initNewAccountWithConfig called via DBus with config:" << config;
    m_plugin->dbusInitNewAccountWithConfig(config);
}

void KDEOAuth2PluginDBusAdapter::cancelCurrentDialog()
{
    qDebug() << "KDEOAuth2PluginDBusAdapter: cancelCurrentDialog called via DBus";
    m_plugin->dbusCancelCurrentDialog();
}

QString KDEOAuth2PluginDBusAdapter::getCurrentDialogState()
{
    qDebug() << "KDEOAuth2PluginDBusAdapter: getCurrentDialogState called via DBus";
    return m_plugin->dbusGetCurrentDialogState();
}

QVariantMap KDEOAuth2PluginDBusAdapter::getDialogInfo()
{
    qDebug() << "KDEOAuth2PluginDBusAdapter: getDialogInfo called via DBus";
    return m_plugin->dbusGetDialogInfo();
}

void KDEOAuth2PluginDBusAdapter::setOAuth2ServerUrl(const QString &serverUrl)
{
    qDebug() << "KDEOAuth2PluginDBusAdapter: setOAuth2ServerUrl called via DBus:" << serverUrl;
    m_plugin->dbusSetOAuth2ServerUrl(serverUrl);
}

void KDEOAuth2PluginDBusAdapter::setOAuth2ClientId(const QString &clientId)
{
    qDebug() << "KDEOAuth2PluginDBusAdapter: setOAuth2ClientId called via DBus:" << clientId;
    m_plugin->dbusSetOAuth2ClientId(clientId);
}

void KDEOAuth2PluginDBusAdapter::setOAuth2RedirectUri(const QString &redirectUri)
{
    qDebug() << "KDEOAuth2PluginDBusAdapter: setOAuth2RedirectUri called via DBus:" << redirectUri;
    m_plugin->dbusSetOAuth2RedirectUri(redirectUri);
}

void KDEOAuth2PluginDBusAdapter::setOAuth2Scope(const QString &scope)
{
    qDebug() << "KDEOAuth2PluginDBusAdapter: setOAuth2Scope called via DBus:" << scope;
    m_plugin->dbusSetOAuth2Scope(scope);
}

QVariantMap KDEOAuth2PluginDBusAdapter::getOAuth2Configuration()
{
    qDebug() << "KDEOAuth2PluginDBusAdapter: getOAuth2Configuration called via DBus";
    return m_plugin->dbusGetOAuth2Configuration();
}

bool KDEOAuth2PluginDBusAdapter::testConnection()
{
    qDebug() << "KDEOAuth2PluginDBusAdapter: testConnection called via DBus";
    return m_plugin->dbusTestConnection();
}

QStringList KDEOAuth2PluginDBusAdapter::getSupportedAuthMethods()
{
    qDebug() << "KDEOAuth2PluginDBusAdapter: getSupportedAuthMethods called via DBus";
    return m_plugin->dbusGetSupportedAuthMethods();
}

void KDEOAuth2PluginDBusAdapter::setAuthMethod(const QString &method)
{
    qDebug() << "KDEOAuth2PluginDBusAdapter: setAuthMethod called via DBus:" << method;
    m_plugin->dbusSetAuthMethod(method);
}

// 新的DBus方法实现
void KDEOAuth2PluginDBusAdapter::dbusInitNewAccount()
{
    qDebug() << "KDEOAuth2PluginDBusAdapter: dbusInitNewAccount called via DBus";
    m_plugin->showNewAccountDialog();
}

void KDEOAuth2PluginDBusAdapter::dbusInitNewAccountWithConfig(const QString &server, const QString &clientId, const QString &authPath, const QString &tokenPath)
{
    qDebug() << "KDEOAuth2PluginDBusAdapter: dbusInitNewAccountWithConfig called via DBus";
    qDebug() << "  server:" << server << "clientId:" << clientId;
    qDebug() << "  authPath:" << authPath << "tokenPath:" << tokenPath;
    
    // 设置配置并创建账户
    bool configResult = m_plugin->dbusSetOAuth2Config(server, clientId, authPath, tokenPath);
    if (configResult) {
        m_plugin->showNewAccountDialog();
    } else {
        qWarning() << "Failed to set OAuth2 config, cannot create account";
    }
}

void KDEOAuth2PluginDBusAdapter::dbusCancelCurrentDialog()
{
    qDebug() << "KDEOAuth2PluginDBusAdapter: dbusCancelCurrentDialog called via DBus";
    // 触发取消信号并重置状态
    m_plugin->m_currentDialogState = "none";
    m_plugin->m_dialogInfo.clear();
    
    emit accountCreationCanceled("用户通过DBus请求取消");
    emit dialogStateChanged("new_account", "canceled", m_plugin->m_dialogInfo);
    
    // 触发插件的canceled信号
    emit m_plugin->canceled();
}

QString KDEOAuth2PluginDBusAdapter::dbusGetCurrentDialogState()
{
    qDebug() << "KDEOAuth2PluginDBusAdapter: dbusGetCurrentDialogState called via DBus";
    return m_plugin->m_currentDialogState;
}

QVariantMap KDEOAuth2PluginDBusAdapter::dbusGetCurrentDialogInfo()
{
    qDebug() << "KDEOAuth2PluginDBusAdapter: dbusGetCurrentDialogInfo called via DBus";
    return m_plugin->m_dialogInfo;
}

bool KDEOAuth2PluginDBusAdapter::dbusSetOAuth2Config(const QString &server, const QString &clientId, const QString &authPath, const QString &tokenPath)
{
    qDebug() << "KDEOAuth2PluginDBusAdapter: dbusSetOAuth2Config called via DBus";
    return m_plugin->dbusSetOAuth2Config(server, clientId, authPath, tokenPath);
}

QVariantMap KDEOAuth2PluginDBusAdapter::dbusGetOAuth2Config()
{
    qDebug() << "KDEOAuth2PluginDBusAdapter: dbusGetOAuth2Config called via DBus";
    return m_plugin->dbusGetOAuth2Configuration();
}

bool KDEOAuth2PluginDBusAdapter::dbusSetAuthMethod(const QString &method)
{
    qDebug() << "KDEOAuth2PluginDBusAdapter: dbusSetAuthMethod called via DBus:" << method;
    m_plugin->dbusSetAuthMethod(method);
    return true;
}

QString KDEOAuth2PluginDBusAdapter::dbusGetAuthMethod()
{
    qDebug() << "KDEOAuth2PluginDBusAdapter: dbusGetAuthMethod called via DBus";
    return m_plugin->m_authMethod;
}

int KDEOAuth2PluginDBusAdapter::dbusGetAccountCount()
{
    qDebug() << "KDEOAuth2PluginDBusAdapter: dbusGetAccountCount called via DBus";
    return m_plugin->dbusGetAccountCount();
}

QString KDEOAuth2PluginDBusAdapter::dbusGetPluginVersion()
{
    qDebug() << "KDEOAuth2PluginDBusAdapter: dbusGetPluginVersion called via DBus";
    return m_plugin->dbusGetPluginVersion();
}

QString KDEOAuth2PluginDBusAdapter::dbusGetPluginInfo()
{
    qDebug() << "KDEOAuth2PluginDBusAdapter: dbusGetPluginInfo called via DBus";
    return m_plugin->dbusGetPluginInfo();
}

bool KDEOAuth2PluginDBusAdapter::dbusTestConnection()
{
    qDebug() << "KDEOAuth2PluginDBusAdapter: dbusTestConnection called via DBus";
    return m_plugin->dbusTestConnection();
}

QString KDEOAuth2PluginDBusAdapter::dbusGetLastError()
{
    qDebug() << "KDEOAuth2PluginDBusAdapter: dbusGetLastError called via DBus";
    return m_plugin->dbusGetLastError();
}

bool KDEOAuth2PluginDBusAdapter::dbusClearError()
{
    qDebug() << "KDEOAuth2PluginDBusAdapter: dbusClearError called via DBus";
    return m_plugin->dbusClearError();
}