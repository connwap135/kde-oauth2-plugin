#include <QCoreApplication>
#include <QDebug>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QUrl>
#include <QUrlQuery>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTimer>
#include <QProcessEnvironment>
#include <QTcpServer>
#include <QTcpSocket>
#include <QHostAddress>

// 模拟OAuth2回调服务器
class TestCallbackServer : public QTcpServer
{
    Q_OBJECT

public:
    TestCallbackServer(QObject *parent = nullptr) : QTcpServer(parent) {}

signals:
    void authorizationCodeReceived(const QString &code);

protected:
    void incomingConnection(qintptr socketDescriptor) override {
        QTcpSocket *socket = new QTcpSocket(this);
        socket->setSocketDescriptor(socketDescriptor);

        connect(socket, &QTcpSocket::readyRead, [this, socket]() {
            QByteArray data = socket->readAll();
            QString request = QString::fromUtf8(data);

            qDebug() << "TestCallbackServer: Received request:" << request;

            // 解析URL获取授权码
            QStringList lines = request.split("\r\n");
            if (!lines.isEmpty()) {
                QString firstLine = lines.first();
                QStringList parts = firstLine.split(" ");
                if (parts.size() >= 2) {
                    QString path = parts[1];
                    QUrl url("http://localhost:8080" + path);
                    QUrlQuery query(url);

                    if (query.hasQueryItem("code")) {
                        QString code = query.queryItemValue("code");
                        qDebug() << "TestCallbackServer: Authorization code received:" << code;
                        emit authorizationCodeReceived(code);
                    }
                }
            }

            // 发送响应
            QString response = "HTTP/1.1 200 OK\r\n"
                             "Content-Type: text/html\r\n"
                             "Content-Length: 0\r\n"
                             "\r\n";
            socket->write(response.toUtf8());
            socket->flush();
            socket->disconnectFromHost();
            socket->deleteLater();
        });
    }
};

// OAuth2测试类
class OAuth2Tester : public QObject
{
    Q_OBJECT

public:
    OAuth2Tester(QObject *parent = nullptr) : QObject(parent) {
        networkManager = new QNetworkAccessManager(this);
        callbackServer = new TestCallbackServer(this);

        // 从环境变量读取配置
        serverUrl = qEnvironmentVariable("OAUTH2_SERVER_URL", "http://192.168.1.12:9007");
        clientId = qEnvironmentVariable("OAUTH2_CLIENT_ID", "10001");
        authPath = qEnvironmentVariable("OAUTH2_AUTH_PATH", "/connect/authorize");
        tokenPath = qEnvironmentVariable("OAUTH2_TOKEN_PATH", "/connect/token");
        userInfoPath = qEnvironmentVariable("OAUTH2_USERINFO_PATH", "/connect/userinfo");
        redirectUri = qEnvironmentVariable("OAUTH2_REDIRECT_URI", "http://localhost:8080/callback");
        scope = qEnvironmentVariable("OAUTH2_SCOPE", "openid profile");

        qDebug() << "OAuth2Tester: Configuration loaded";
        qDebug() << "  Server URL:" << serverUrl;
        qDebug() << "  Client ID:" << clientId;
        qDebug() << "  Redirect URI:" << redirectUri;
    }

    void startTest() {
        qDebug() << "OAuth2Tester: Starting OAuth2 test...";

        // 启动回调服务器
        if (!callbackServer->listen(QHostAddress::LocalHost, 8080)) {
            qCritical() << "OAuth2Tester: Failed to start callback server";
            QCoreApplication::quit();
            return;
        }

        qDebug() << "OAuth2Tester: Callback server started on port 8080";

        // 连接信号
        connect(callbackServer, &TestCallbackServer::authorizationCodeReceived,
                this, &OAuth2Tester::onAuthorizationCodeReceived);

        // 生成授权URL
        QString authUrl = generateAuthUrl();
        qDebug() << "OAuth2Tester: Generated auth URL:" << authUrl;

        // 在浏览器中打开授权URL
        qDebug() << "OAuth2Tester: Please open this URL in your browser to authorize:";
        qDebug() << authUrl;
        qDebug() << "OAuth2Tester: Waiting for authorization callback...";
    }

private slots:
    void onAuthorizationCodeReceived(const QString &code) {
        qDebug() << "OAuth2Tester: Received authorization code:" << code;
        exchangeCodeForToken(code);
    }

    void onTokenRequestFinished() {
        QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());
        if (!reply) return;

        if (reply->error() != QNetworkReply::NoError) {
            qCritical() << "OAuth2Tester: Token request failed:" << reply->errorString();
            QCoreApplication::quit();
            return;
        }

        QByteArray data = reply->readAll();
        qDebug() << "OAuth2Tester: Token response:" << data;

        QJsonDocument doc = QJsonDocument::fromJson(data);
        QJsonObject obj = doc.object();

        if (obj.contains("access_token")) {
            QString accessToken = obj["access_token"].toString();
            QString refreshToken = obj["refresh_token"].toString();

            qDebug() << "OAuth2Tester: ✅ SUCCESS! Access token obtained";
            qDebug() << "  Access Token:" << accessToken.left(20) + "...";
            if (!refreshToken.isEmpty()) {
                qDebug() << "  Refresh Token:" << refreshToken.left(20) + "...";
            }

            // 测试用户信息获取
            fetchUserInfo(accessToken);
        } else {
            qCritical() << "OAuth2Tester: No access token in response";
            QCoreApplication::quit();
        }

        reply->deleteLater();
    }

    void onUserInfoRequestFinished() {
        QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());
        if (!reply) return;

        qDebug() << "OAuth2Tester: User info request finished";

        if (reply->error() != QNetworkReply::NoError) {
            qCritical() << "OAuth2Tester: User info request failed:" << reply->errorString();
        } else {
            QByteArray data = reply->readAll();
            qDebug() << "OAuth2Tester: User info response:" << data;

            QJsonDocument doc = QJsonDocument::fromJson(data);
            if (!doc.isNull()) {
                QJsonObject userObj = doc.object();
                qDebug() << "OAuth2Tester: ✅ User info retrieved successfully";
                qDebug() << "  User fields:" << userObj.keys();
            }
        }

        qDebug() << "OAuth2Tester: 🎉 OAuth2 test completed successfully!";
        QCoreApplication::quit();
        reply->deleteLater();
    }

private:
    QString generateAuthUrl() const {
        QUrl url(serverUrl + authPath);
        QUrlQuery query;
        query.addQueryItem("response_type", "code");
        query.addQueryItem("client_id", clientId);
        query.addQueryItem("redirect_uri", redirectUri);
        query.addQueryItem("scope", scope);
        query.addQueryItem("state", "test_state_123");
        url.setQuery(query);
        return url.toString();
    }

    void exchangeCodeForToken(const QString &code) {
        qDebug() << "OAuth2Tester: Exchanging authorization code for access token...";

        QUrl url(serverUrl + tokenPath);
        QNetworkRequest request(url);
        request.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");

        QUrlQuery postData;
        postData.addQueryItem("grant_type", "authorization_code");
        postData.addQueryItem("client_id", clientId);
        postData.addQueryItem("code", code);
        postData.addQueryItem("redirect_uri", redirectUri);

        QNetworkReply *reply = networkManager->post(request, postData.toString(QUrl::FullyEncoded).toUtf8());
        connect(reply, &QNetworkReply::finished, this, &OAuth2Tester::onTokenRequestFinished);
    }

    void fetchUserInfo(const QString &accessToken) {
        qDebug() << "OAuth2Tester: Fetching user information...";

        QUrl url(serverUrl + userInfoPath);
        QNetworkRequest request(url);
        request.setRawHeader("Authorization", QString("Bearer %1").arg(accessToken).toUtf8());

        QNetworkReply *reply = networkManager->get(request);
        connect(reply, &QNetworkReply::finished, this, &OAuth2Tester::onUserInfoRequestFinished);
    }

private:
    QNetworkAccessManager *networkManager;
    TestCallbackServer *callbackServer;

    QString serverUrl;
    QString clientId;
    QString authPath;
    QString tokenPath;
    QString userInfoPath;
    QString redirectUri;
    QString scope;
};

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    qDebug() << "=== OAuth2 Access Token Test ===";
    qDebug() << "This test will simulate the OAuth2 authorization flow";
    qDebug() << "Make sure your OAuth2 server is running and accessible";

    OAuth2Tester *tester = new OAuth2Tester(&app);
    tester->startTest();

    // 设置超时（5分钟）
    QTimer::singleShot(300000, &app, &QCoreApplication::quit);

    return app.exec();
}