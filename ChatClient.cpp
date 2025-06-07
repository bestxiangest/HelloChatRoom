#include "ChatClient.h"
#include "FileTransferManager.h"
#include <QDebug>
#include <QJsonArray>

ChatClient::ChatClient(QObject *parent) : QObject(parent), m_fileTransferManager(nullptr)
{
    // Connect signals from QWebSocket to our slots
    connect(&m_webSocket, &QWebSocket::connected, this, &ChatClient::onConnected);
    connect(&m_webSocket, &QWebSocket::disconnected, this, &ChatClient::onDisconnected);
    connect(&m_webSocket, &QWebSocket::textMessageReceived, this, &ChatClient::onTextMessageReceived);
    connect(&m_webSocket, QOverload<QAbstractSocket::SocketError>::of(&QWebSocket::errorOccurred), this, &ChatClient::onError);
    
    // 创建文件传输管理器
    m_fileTransferManager = new FileTransferManager(this);
}

ChatClient::~ChatClient()
{
    if (m_webSocket.isValid()) {
        m_webSocket.close();
    }
    // m_fileTransferManager 会被自动删除，因为它是这个对象的子对象
}

void ChatClient::connectToServer(const QUrl &url)
{
    m_url = url;
    qDebug() << "ChatClient: Attempting to connect to" << url.toString();
    m_webSocket.open(m_url);
}

void ChatClient::disconnectFromServer()
{
    if (m_webSocket.isValid()) {
        qDebug() << "ChatClient: Disconnecting from server.";
        m_webSocket.close();
    }
}

bool ChatClient::isConnected() const
{
    // This reflects WebSocket state. True Socket.IO connection requires more.
    return m_webSocket.state() == QAbstractSocket::ConnectedState;
}

void ChatClient::onConnected()
{
    qDebug() << "ChatClient: WebSocket transport connected to" << m_url.toString();
    // The QWebSocket is connected. Server will now send Engine.IO OPEN ("0{...}").
    // We will send Socket.IO CONNECT ("40") after receiving the Engine.IO OPEN packet.
    emit connected(); // Signal that the underlying transport is ready.
}

void ChatClient::onDisconnected()
{
    qDebug() << "ChatClient: WebSocket disconnected.";
    m_authToken.clear();
    m_currentUser.clear();
    emit disconnected();
}

void ChatClient::onError(QAbstractSocket::SocketError error)
{
    QString errorString = m_webSocket.errorString();
    qDebug() << "ChatClient: WebSocket error:" << error << "-" << errorString;
    emit socketError(errorString);
}

void ChatClient::onTextMessageReceived(const QString &rawMessage)
{
    qDebug() << "ChatClient: Raw message received:" << rawMessage;
    if (rawMessage.startsWith(QStringLiteral("0"))) {
        qDebug() << "ChatClient: Engine.IO OPEN packet received:" << rawMessage.mid(1);
        if (m_webSocket.isValid() && m_webSocket.state() == QAbstractSocket::ConnectedState) {
            QString socketIoConnectPacket = QStringLiteral("40");
            m_webSocket.sendTextMessage(socketIoConnectPacket);
            qDebug() << "ChatClient: Sent Socket.IO CONNECT packet (40) to default namespace.";
        }
    } else if (rawMessage.startsWith(QStringLiteral("40"))) {
        qDebug() << "ChatClient: Received Socket.IO CONNECT ACK from server (or server connected to namespace):" << rawMessage;
    } else if (rawMessage.startsWith(QStringLiteral("41"))) {
        qDebug() << "ChatClient: Received Socket.IO packet type 1 (possibly CONNECT_ERROR or non-default ACK):" << rawMessage;
    } else if (rawMessage.startsWith(QStringLiteral("42"))) { // Socket.IO EVENT packet
        QString jsonPart = rawMessage.mid(2);
        QJsonDocument doc = QJsonDocument::fromJson(jsonPart.toUtf8());

        if (doc.isNull() || !doc.isArray()) { qWarning() << "ChatClient: Failed to parse Socket.IO event data or not an array:" << jsonPart; return; }
        QJsonArray eventArray = doc.array();
        if (eventArray.isEmpty()) { qWarning() << "ChatClient: Socket.IO event array is empty:" << jsonPart; return; }

        QString eventType = eventArray.at(0).toString();
        QJsonObject data;
        if (eventArray.size() > 1 && eventArray.at(1).isObject()) {
            data = eventArray.at(1).toObject();
        }

        if (eventType == "login_response") {
            handleLoginResponse(data);
        } else if (eventType == "new_message") {
            handleNewMessage(data);
        } else if (eventType == "user_list_update") {
            handleUserListUpdate(data);
        } else if (eventType == "server_notification" || eventType == "error") {
            handleServerNotification(data);
        }
        // 新增：处理通义千问的回复事件
        else if (eventType == "tongyi_message_response") {
            handleTongyiMessageResponse(data);
        }
        // 新增：处理文件消息事件
        else if (eventType == "file_message") {
            handleFileMessage(data);
        }
        else {
            qDebug() << "ChatClient: Unknown or unhandled Socket.IO event type received:" << eventType;
        }
    } else if (rawMessage == QStringLiteral("2")) { // Engine.IO PING packet from server
        qDebug() << "ChatClient: Engine.IO PING packet (heartbeat) received from server. Sending PONG.";
        if (m_webSocket.isValid() && m_webSocket.state() == QAbstractSocket::ConnectedState) {
            m_webSocket.sendTextMessage(QStringLiteral("3")); // Send PONG
        }
    } else if (rawMessage.startsWith(QStringLiteral("3"))) { // Engine.IO PONG packet
        qDebug() << "ChatClient: Engine.IO PONG packet received from server:" << rawMessage.mid(1);
    } else {
        qDebug() << "ChatClient: Received unhandled raw message:" << rawMessage;
    }
}


// 新增：实现发送消息给通义千问的方法
void ChatClient::sendToTongyi(const QString &content)
{
    if (!isConnected()) {
        qDebug() << "ChatClient: Cannot send to Tongyi. Not connected.";
        emit tongyiMessageResponseReceived(QStringLiteral("通义千问"), QStringLiteral("错误：未连接到服务。"), QDateTime::currentMSecsSinceEpoch(), false);
        return;
    }
    QJsonObject tongyiData;
    tongyiData["content"] = content;

    QJsonObject payload;
    payload["event"] = "send_to_tongyi";
    payload["data"] = tongyiData;

    sendJson(payload);
    qDebug() << "ChatClient: Sent message to Tongyi:" << content;
}



// --- User Actions (attemptLogin, sendMessage, requestUserList) remain the same ---
// ... (它们调用下面的 sendJson)

void ChatClient::attemptLogin(const QString &username, const QString &password)
{
    if (!isConnected()) { // Relies on WebSocket connection state
        qDebug() << "ChatClient: Not connected to server at WebSocket level. Cannot attempt login.";
        emit loginFailed("未连接到服务器");
        return;
    }
    // It might be better to have a state like "socketIoConnected" after "40" is processed.
    // For now, proceeding if WebSocket is up.

    QJsonObject loginData;
    loginData["username"] = username;
    loginData["password"] = password;

    QJsonObject payload;
    payload["event"] = "login";
    payload["data"] = loginData;

    sendJson(payload);
    qDebug() << "ChatClient: Queued login request for" << username; // Changed from "Sent" as it's now more clearly async
}

void ChatClient::sendMessage(const QString &to, const QString &content, const QString &type)
{
    if (!isConnected() || m_authToken.isEmpty()) {
        qDebug() << "ChatClient: Cannot send message. Not connected or not authenticated.";
        return;
    }

    QJsonObject messageData;
    messageData["to"] = to;
    messageData["content"] = content;
    messageData["type"] = type;

    QJsonObject payload;
    payload["event"] = "message";
    payload["data"] = messageData;

    sendJson(payload);
    qDebug() << "ChatClient: Queued chat message to" << to;
}

void ChatClient::requestUserList() {
    if (!isConnected() || m_authToken.isEmpty()) {
        qDebug() << "ChatClient: Cannot request user list. Not connected or not authenticated.";
        return;
    }
    QJsonObject payload;
    payload["event"] = "get_user_list";
    payload["data"] = QJsonObject();

    sendJson(payload);
    qDebug() << "ChatClient: Queued request for user list.";
}


QString ChatClient::getAuthToken() const
{
    return m_authToken;
}

// --- Private Helper Methods ---
QJsonObject ChatClient::parseJson(const QString &jsonString)
{
    QJsonDocument doc = QJsonDocument::fromJson(jsonString.toUtf8());
    if (doc.isNull() || !doc.isObject()) {
        qDebug() << "ChatClient (helper): Failed to parse JSON or not a JSON object:" << jsonString;
        return QJsonObject();
    }
    return doc.object();
}

void ChatClient::sendJson(const QJsonObject &payloadObject) // This is the private helper method
{
    if (m_webSocket.isValid() && m_webSocket.state() == QAbstractSocket::ConnectedState) {
        QString eventName = payloadObject.value("event").toString();
        QJsonValue eventDataValue = payloadObject.value("data");

        if (eventName.isEmpty()) {
            qWarning() << "ChatClient::sendJson: Event name is empty in payload.";
            return;
        }

        QJsonArray socketIoPacketArray;
        socketIoPacketArray.append(eventName);
        if (!eventDataValue.isNull() && !eventDataValue.isUndefined()) {
            socketIoPacketArray.append(eventDataValue);
        }

        QString messageToSend = QStringLiteral("42") + QJsonDocument(socketIoPacketArray).toJson(QJsonDocument::Compact);

        m_webSocket.sendTextMessage(messageToSend);
        qDebug() << "ChatClient: Sent Socket.IO formatted message:" << messageToSend;
    } else {
        qDebug() << "ChatClient: WebSocket not valid or not connected. Cannot send message.";
    }
}

// --- Specific Message Handlers (handleLoginResponse, etc.) remain the same ---
// ... (handleLoginResponse, handleNewMessage, handleUserListUpdate, handleServerNotification)
void ChatClient::handleLoginResponse(const QJsonObject &data)
{
    bool success = data.value("success").toBool();
    if (success) {
        m_currentUser = data.value("username").toString();
        m_authToken = data.value("token").toString("placeholder_token_if_not_sent");
        qDebug() << "ChatClient: Login successful for" << m_currentUser;
        emit loginSucceeded(m_currentUser, m_authToken);
    } else {
        QString reason = data.value("message").toString("登录失败，请检查您的凭据。");
        qDebug() << "ChatClient: Login failed. Reason:" << reason;
        emit loginFailed(reason);
    }
}

void ChatClient::handleNewMessage(const QJsonObject &data)
{
    QString from = data.value("from").toString();
    QString to = data.value("to").toString();
    QString content = data.value("content").toString();
    QString type = data.value("type").toString("text");
    qint64 timestamp = data.value("timestamp").toVariant().toLongLong();

    if (from.isEmpty() || content.isEmpty()) {
        qDebug() << "ChatClient: Received incomplete new_message data.";
        return;
    }
    emit newMessageReceived(from, to, content, type, timestamp);
}

void ChatClient::handleUserListUpdate(const QJsonObject &data)
{
    QJsonArray users = data.value("users").toArray();
    emit userListUpdated(users);
    qDebug() << "ChatClient: User list updated.";
}

void ChatClient::handleServerNotification(const QJsonObject &data)
{
    QString message = data.value("message").toString();
    if (!message.isEmpty()) {
        qDebug() << "ChatClient: Server notification/error:" << message;
        emit serverMessage(message);
    }
}

// 新增：实现处理通义千问回复的内部方法
void ChatClient::handleTongyiMessageResponse(const QJsonObject &data)
{
    bool success = data.value("success").toBool(false);
    QString from = data.value("from").toString(); // 应该总是 "通义千问"
    QString content = data.value("content").toString();
    qint64 timestamp = data.value("timestamp").toVariant().toLongLong();

    if (from.isEmpty() || content.isEmpty()) {
        qWarning() << "ChatClient: Received incomplete tongyi_message_response data.";
        // 即使内容为空，如果'from'存在，也可能是一个错误提示
        if (!from.isEmpty()) {
            emit tongyiMessageResponseReceived(from, content.isEmpty() ? "收到空回复" : content, timestamp, false);
        }
        return;
    }
    // 发出新信号，由 UI 层面处理
    emit tongyiMessageResponseReceived(from, content, timestamp, success);
}

// 新增：发送文件消息
void ChatClient::sendFileMessage(const QString &to, const QString &fileName, const QString &fileId, qint64 fileSize)
{
    if (!isConnected() || m_authToken.isEmpty()) {
        qDebug() << "ChatClient: Cannot send file message. Not connected or not authenticated.";
        return;
    }

    QJsonObject fileData;
    fileData["to"] = to;
    fileData["file_name"] = fileName;
    fileData["file_id"] = fileId;
    fileData["file_size"] = fileSize;
    fileData["type"] = "file";

    QJsonObject payload;
    payload["event"] = "file_message";
    payload["data"] = fileData;

    sendJson(payload);
    qDebug() << "ChatClient: Sent file message to" << to << "- File:" << fileName << "ID:" << fileId;
}

// 新增：获取文件传输管理器
FileTransferManager* ChatClient::getFileTransferManager() const
{
    return m_fileTransferManager;
}

// 新增：处理文件消息
void ChatClient::handleFileMessage(const QJsonObject &data)
{
    QString from = data.value("from").toString();
    QString to = data.value("to").toString();
    QString fileName = data.value("file_name").toString();
    QString fileId = data.value("file_id").toString();
    qint64 fileSize = data.value("file_size").toVariant().toLongLong();
    qint64 timestamp = data.value("timestamp").toVariant().toLongLong();

    if (from.isEmpty() || fileName.isEmpty() || fileId.isEmpty()) {
        qWarning() << "ChatClient: Received incomplete file_message data.";
        return;
    }

    emit fileMessageReceived(from, to, fileName, fileId, fileSize, timestamp);
    qDebug() << "ChatClient: Received file message from" << from << "- File:" << fileName << "ID:" << fileId;
}
