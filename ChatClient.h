#ifndef CHATCLIENT_H
#define CHATCLIENT_H

#include <QObject>
#include <QtWebSockets/QWebSocket>
#include <QJsonObject>
#include <QJsonDocument>
#include <QJsonArray>
#include <QString>
#include <QUrl>

class FileTransferManager;

class ChatClient : public QObject
{
    Q_OBJECT
public:
    explicit ChatClient(QObject *parent = nullptr);
    ~ChatClient();

    // Connection management
    void connectToServer(const QUrl &url);
    void disconnectFromServer();
    bool isConnected() const;

    // User actions
    void attemptLogin(const QString &username, const QString &password);
    void sendMessage(const QString &to, const QString &content, const QString &type = "text");
    void requestUserList();
    // 新增：发送消息给通义千问
    void sendToTongyi(const QString &content);
    
    // 文件传输相关方法
    void sendFileMessage(const QString &to, const QString &fileName, const QString &fileId, qint64 fileSize);
    FileTransferManager* getFileTransferManager() const;

    // Getter for JWT token
    QString getAuthToken() const;

signals:
    // Connection signals
    void connected();
    void disconnected();
    void socketError(const QString &errorString);

    // Login signals
    void loginSucceeded(const QString &username, const QString &token);
    void loginFailed(const QString &reason);

    // Message signals
    void newMessageReceived(const QString &from, const QString &to, const QString &content, const QString &type, qint64 timestamp);

    // 新增：收到通义千问回复的信号
    void tongyiMessageResponseReceived(const QString &from, const QString &content, qint64 timestamp, bool success);
    
    // 文件传输相关信号
    void fileMessageReceived(const QString &from, const QString &to, const QString &fileName, const QString &fileId, qint64 fileSize, qint64 timestamp);

    // User list signals
    void userListUpdated(const QJsonArray &users);

    // Generic server message or error
    void serverMessage(const QString &message);

private slots:
    void onConnected();
    void onDisconnected();
    void onTextMessageReceived(const QString &message);
    void onError(QAbstractSocket::SocketError error);

private:
    QWebSocket m_webSocket;
    QUrl m_url;
    QString m_authToken;
    QString m_currentUser;
    FileTransferManager *m_fileTransferManager;

    // Helper methods
    QJsonObject parseJson(const QString &jsonString);
    void sendJson(const QJsonObject &jsonObject);

    // Specific message handlers
    void handleLoginResponse(const QJsonObject &data);
    void handleNewMessage(const QJsonObject &data);
    void handleUserListUpdate(const QJsonObject &data);
    void handleServerNotification(const QJsonObject &data);
    // 新增：处理通义千问回复的内部方法
    void handleTongyiMessageResponse(const QJsonObject &data);
    // 新增：处理文件消息的内部方法
    void handleFileMessage(const QJsonObject &data);
};

#endif // CHATCLIENT_H
