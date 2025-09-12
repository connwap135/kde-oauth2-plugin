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

// KDEOAuth2Plugin 实现 (支持从环境变量读取配置)
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
{
    qDebug() << "KDEOAuth2Plugin: Constructor called";
    m_networkManager = new QNetworkAccessManager(this);
    
    // 优先从provider文件加载配置
    loadProviderConfiguration();
    // 再从环境变量加载配置（可覆盖provider配置）
    loadConfigurationFromEnvironment();
}

KDEOAuth2Plugin::~KDEOAuth2Plugin()
{
    qDebug() << "KDEOAuth2Plugin: Destructor called";
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
    // 检查是否已存在账户（单账户限制）
    if (!m_providerName.isEmpty()) {
        int accountCount = getAccountCountForProvider(m_providerName);
        qDebug() << "KDEOAuth2Plugin: existing account count for provider" << m_providerName << ":" << accountCount;
        if (accountCount > 0) {
            QMessageBox::warning(nullptr, "账户限制", QString("Provider '%1' 已存在账户，无法重复添加。\n如需更换请先删除原账户。").arg(m_providerName));
            emit canceled();
            return;
        }
    }
    startOAuth2Flow();
}

int KDEOAuth2Plugin::getAccountCountForProvider(const QString &providerId) const
{
    // 使用 Accounts-Qt 查询指定 provider 下的账户数
    // 注意：KAccountsUiPlugin 运行于进程内，直接构造 Manager 即可
    Accounts::Manager manager;
    Accounts::AccountIdList ids = manager.accountList(providerId);
    int count = 0;
    for (Accounts::AccountId id : ids) {
        std::unique_ptr<Accounts::Account> account(manager.account(id));
        if (account && account->enabled()) {
            ++count;
        }
    }
    return count;
}

void KDEOAuth2Plugin::startOAuth2Flow()
{
    qDebug() << "KDEOAuth2Plugin: starting OAuth2 authentication flow";
    
    QString authUrl = generateAuthUrl();
    QUrl urlCheck(authUrl);
    if (!urlCheck.isValid() || urlCheck.scheme().isEmpty() || urlCheck.host().isEmpty()) {
        QMessageBox::critical(nullptr, "OAuth2配置错误", QString("生成的认证URL无效：%1\n请联系开发人员检查OAuth2配置。").arg(authUrl));
        qDebug() << "KDEOAuth2Plugin: Invalid authUrl generated:" << authUrl;
        emit canceled();
        return;
    }
    
    qDebug() << "KDEOAuth2Plugin: generated auth URL:" << authUrl;
    
    // 创建OAuth2认证对话框，传递重定向URI
    OAuth2Dialog *dialog = new OAuth2Dialog(authUrl, m_redirectUri);
    
    int result = dialog->exec();
    
    if (result == QDialog::Accepted) {
        QString authCode = dialog->getAuthorizationCode();
        qDebug() << "KDEOAuth2Plugin: received authorization code:" << authCode;
        
        if (!authCode.isEmpty()) {
            exchangeCodeForToken(authCode);
        } else {
            qDebug() << "KDEOAuth2Plugin: no authorization code received";
            emit canceled();
        }
    } else {
        qDebug() << "KDEOAuth2Plugin: user canceled authentication";
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
        emit canceled();
        return;
    }
    
    if (reply->error() != QNetworkReply::NoError) {
        qDebug() << "KDEOAuth2Plugin: token request failed:" << reply->errorString();
        emit canceled();
        reply->deleteLater();
        return;
    }
    
    QByteArray data = reply->readAll();
    qDebug() << "KDEOAuth2Plugin: token response:" << data;
    
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
        qDebug() << "KDEOAuth2Plugin: successfully obtained access token";
        fetchUserInfo(m_currentAccessToken);
    } else {
        qDebug() << "KDEOAuth2Plugin: no access token in response";
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
        emit canceled();
        return;
    }
    
    qDebug() << "KDEOAuth2Plugin: user info request status code:" << reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    qDebug() << "KDEOAuth2Plugin: user info request error:" << reply->error();
    qDebug() << "KDEOAuth2Plugin: user info request error string:" << reply->errorString();
    
    if (reply->error() != QNetworkReply::NoError) {
        qDebug() << "KDEOAuth2Plugin: user info request failed:" << reply->errorString();
        // 即使获取用户信息失败，我们仍然可以创建账户
    }
    
    QByteArray data = reply->readAll();
    qDebug() << "KDEOAuth2Plugin: raw user info response data:" << data;
    qDebug() << "KDEOAuth2Plugin: user info response size:" << data.size() << "bytes";
    
    // 尝试解析JSON
    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);
    
    if (parseError.error != QJsonParseError::NoError) {
        qDebug() << "KDEOAuth2Plugin: JSON parse error:" << parseError.errorString();
        qDebug() << "KDEOAuth2Plugin: JSON parse error at offset:" << parseError.offset;
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
    
    // 准备基本账户数据
    QVariantMap authData;
    authData["server"] = m_serverUrl;
    authData["client_id"] = m_clientId;
    authData["access_token"] = m_currentAccessToken;
    if (!m_currentRefreshToken.isEmpty()) {
        authData["refresh_token"] = m_currentRefreshToken;
    }
    
    // 使用默认显示名称
    QString displayName = "OAuth2 User";
    
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