#include "MainWindow.h"
#include "ui_MainWindow.h"
#include "PrivateChat.h"
#include "LoginWindow.h"
#include "ChatClient.h"
#include <QMessageBox>
#include <QDebug>
#include <QApplication>
#include <QJsonArray>
#include <QJsonObject>
#include <QIcon>

// 定义静态常量成员
const QString MainWindow::TONGYI_QWEN_CONTACT_NAME = QStringLiteral("通义千问");

MainWindow::MainWindow(ChatClient *client, QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow),
    m_chatClient(client),
    m_currentUsername("")
{
    ui->setupUi(this);
    setWindowTitle("实时聊天");

    if (!m_chatClient) {
        qCritical() << "MainWindow: ChatClient instance is null!";
        QMessageBox::critical(this, "严重错误", "聊天服务不可用。");
        ui->sendButton->setEnabled(false);
        ui->messageInputEdit->setEnabled(false);
        ui->contactsListWidget->setEnabled(false);
        return;
    }

    connect(m_chatClient, &ChatClient::newMessageReceived, this, &MainWindow::handleNewMessageReceived);
    connect(m_chatClient, &ChatClient::userListUpdated, this, &MainWindow::handleUserListUpdated);
    connect(m_chatClient, &ChatClient::socketError, this, &MainWindow::handleSocketError);
    connect(m_chatClient, &ChatClient::serverMessage, this, &MainWindow::handleServerMessage);
    connect(m_chatClient, &ChatClient::disconnected, this, &MainWindow::handleClientDisconnected);

    // 新增：连接通义千问的回复信号
    connect(m_chatClient, &ChatClient::tongyiMessageResponseReceived, this, &MainWindow::handleTongyiMessageResponse);

    ui->chatDisplayBrowser->setHtml("<p style='color:grey;'><i>选择一个联系人或群组开始聊天。</i></p>");
    ui->usernameLabel->setText("未登录");
    ui->statusLabel->setText("离线");
}

MainWindow::~MainWindow()
{
    // Disconnect signals to prevent issues during destruction if ChatClient outlives this window
    if (m_chatClient) {
        disconnect(m_chatClient, &ChatClient::newMessageReceived, this, &MainWindow::handleNewMessageReceived);
        disconnect(m_chatClient, &ChatClient::userListUpdated, this, &MainWindow::handleUserListUpdated);
        disconnect(m_chatClient, &ChatClient::socketError, this, &MainWindow::handleSocketError);
        disconnect(m_chatClient, &ChatClient::serverMessage, this, &MainWindow::handleServerMessage);
        disconnect(m_chatClient, &ChatClient::disconnected, this, &MainWindow::handleClientDisconnected);
        disconnect(m_chatClient, &ChatClient::tongyiMessageResponseReceived, this, &MainWindow::handleTongyiMessageResponse);
    }

    // Close and delete all active private chat windows
    qDeleteAll(m_activePrivateChats);
    m_activePrivateChats.clear();
    delete ui;
    qDebug() << "MainWindow destroyed";
}

void MainWindow::setCurrentUser(const QString &username)
{
    m_currentUsername = username;
    ui->usernameLabel->setText(username);
    ui->statusLabel->setText(m_chatClient && m_chatClient->isConnected() ? "在线" : "离线");
    setWindowTitle(QString("实时聊天 - %1").arg(username));

    if (m_chatClient && m_chatClient->isConnected()) {
        m_chatClient->requestUserList();
    } else {
        updateStatusBar("未连接到服务器，部分功能可能不可用。", true);
    }
}


void MainWindow::on_sendButton_clicked()
{
    QString message = ui->messageInputEdit->toPlainText().trimmed();
    if (message.isEmpty() || !m_chatClient) return;

    // 主窗口发送按钮默认发送到公共聊天室
    m_chatClient->sendMessage("all", message, "text");
    ui->messageInputEdit->clear();
}

void MainWindow::on_contactsListWidget_itemDoubleClicked(QListWidgetItem *item)
{
    if (!item) return;
    QString contactName = item->text();
    openChatWith(contactName); // 使用新的通用函数
}

void MainWindow::openChatWith(const QString &contactName)
{
    if (contactName == m_currentUsername) {
        return; // 不允许和自己聊天
    }

    if (m_activePrivateChats.contains(contactName)) {
        m_activePrivateChats[contactName]->activateWindow();
        m_activePrivateChats[contactName]->raise();
    } else {
        if (!m_chatClient) {
            QMessageBox::critical(this, "错误", "聊天服务不可用。");
            return;
        }
        PrivateChat *privateChatWindow = new PrivateChat(m_chatClient, this);
        privateChatWindow->setChatPartner(contactName, m_currentUsername); // 传递联系人名称

        connect(privateChatWindow, &QObject::destroyed, this, [this, contactName]() {
            m_activePrivateChats.remove(contactName);
            qDebug() << "Chat with" << contactName << "closed and removed from map.";
        });

        m_activePrivateChats.insert(contactName, privateChatWindow);
        privateChatWindow->show();
    }
}
void MainWindow::on_searchLineEdit_textChanged(const QString &searchText)
{
    for (int i = 0; i < ui->contactsListWidget->count(); ++i) {
        QListWidgetItem *item = ui->contactsListWidget->item(i);
        if (item) {
            item->setHidden(!item->text().contains(searchText, Qt::CaseInsensitive));
        }
    }
}

void MainWindow::appendMessageToDisplay(const QString &sender, const QString &message, bool isMe, const QString &chatContext)
{
    // This function now primarily serves the main chat window.
    // Private chats will have their own appendMessage.
    // 'chatContext' can be used if main window handles multiple group chats, not used yet.
    Q_UNUSED(chatContext);

    QString displayName = isMe ? "我" : sender;
    QString formattedMessage;

    if (isMe) {
        formattedMessage = QString(
                               "<div style='text-align: right; margin: 5px;'>"
                               "  <span style='background-color: #dcf8c6; padding: 8px 12px; border-radius: 10px; display: inline-block; max-width: 70%; text-align:left;'>"
                               "    <b>%1:</b><br>%2"
                               "  </span>"
                               "</div>"
                               ).arg(displayName, message.toHtmlEscaped());
    } else {
        formattedMessage = QString(
                               "<div style='text-align: left; margin: 5px;'>"
                               "  <span style='background-color: #ffffff; border: 1px solid #e0e0e0; padding: 8px 12px; border-radius: 10px; display: inline-block; max-width: 70%;'>"
                               "    <b>%1:</b><br>%2"
                               "  </span>"
                               "</div>"
                               ).arg(displayName, message.toHtmlEscaped());
    }
    ui->chatDisplayBrowser->append(formattedMessage);
    ui->chatDisplayBrowser->ensureCursorVisible();
}

// 新增：辅助函数，将通义千问添加到联系人列表顶部
void MainWindow::addTongyiQwenToList()
{
    // 检查是否已存在
    for (int i = 0; i < ui->contactsListWidget->count(); ++i) {
        if (ui->contactsListWidget->item(i)->text() == TONGYI_QWEN_CONTACT_NAME) {
            return;
        }
    }
    QListWidgetItem *tongyiItem = new QListWidgetItem(TONGYI_QWEN_CONTACT_NAME);
    tongyiItem->setForeground(Qt::blue); // 用不同颜色标记
    // tongyiItem->setIcon(QIcon(":/icons/ai_icon.png")); // 可选：设置一个专属图标
    ui->contactsListWidget->insertItem(0, tongyiItem); // 插入到列表顶部
}

// --- Slots for ChatClient Signals ---
void MainWindow::handleNewMessageReceived(const QString &from, const QString &to, const QString &content, const QString &type, qint64 timestamp)
{
    Q_UNUSED(type); Q_UNUSED(timestamp);
    if (to == m_currentUsername) { // 私聊消息
        openChatWith(from); // 自动打开或激活聊天窗口
        if (m_activePrivateChats.contains(from)) {
            m_activePrivateChats[from]->receiveMessage(from, content);
            m_activePrivateChats[from]->activateWindow();
        }
    } else if (to == "all") { // 公共消息
        appendMessageToDisplay(from, content, (from == m_currentUsername), "main");
    } else {
        // Message might be for another user or a group this client is part of but not "all"
        // Or it's a message this client sent, echoed by the server.
        if (from == m_currentUsername && to != "all") {
            // This is likely an echo of a private message I sent.
            // The private chat window should handle displaying its own sent messages.
            // Or, if server echoes all PMs, the target PM window needs to get it.
            if (m_activePrivateChats.contains(to)) {
                // m_activePrivateChats[to]->receiveMessage(from, content); // Already handled by PM window sending it
            }
        } else {
            qDebug() << "MainWindow: Received message not for 'all' or directly to me:" << to;
            // Potentially display in a specific group chat window if 'to' is a group ID
        }
    }
}

// 新增：处理通义千问回复的槽函数实现
void MainWindow::handleTongyiMessageResponse(const QString &from, const QString &content, qint64 timestamp, bool success)
{
    Q_UNUSED(timestamp);
    if (from != TONGYI_QWEN_CONTACT_NAME) return;

    // 确保与通义千问的聊天窗口已打开
    openChatWith(TONGYI_QWEN_CONTACT_NAME);

    if (m_activePrivateChats.contains(TONGYI_QWEN_CONTACT_NAME)) {
        PrivateChat *tongyiChat = m_activePrivateChats[TONGYI_QWEN_CONTACT_NAME];
        if (!success) {
            // 如果失败，在消息前加上错误标记
            tongyiChat->receiveMessage(from, "[错误] " + content);
        } else {
            tongyiChat->receiveMessage(from, content);
        }
        tongyiChat->activateWindow(); // 将窗口置前
    } else {
        // 作为后备，在状态栏显示
        updateStatusBar("收到通义千问回复，但聊天窗口未打开。", !success);
    }
}

void MainWindow::handleUserListUpdated(const QJsonArray &users)
{
    ui->contactsListWidget->clear();
    addTongyiQwenToList(); // **首先添加通义千问**

    for (const QJsonValue &userValue : users) {
        QJsonObject userObject = userValue.toObject();
        QString username = userObject.value("username").toString();

        if (username == m_currentUsername || username == TONGYI_QWEN_CONTACT_NAME) continue; // 不列出自己或重复的通义千问

        QListWidgetItem *item = new QListWidgetItem(username, ui->contactsListWidget);
        if (userObject.value("status").toString("offline") == "online") {
            item->setForeground(Qt::darkGreen);
        } else {
            item->setForeground(Qt::gray);
        }
    }
}

void MainWindow::handleSocketError(const QString &errorString)
{
    updateStatusBar("网络错误: " + errorString, true);
    ui->statusLabel->setText("离线");
}

void MainWindow::handleServerMessage(const QString &message)
{
    updateStatusBar(message, false); // Display general server messages
}

void MainWindow::handleClientDisconnected()
{
    updateStatusBar("已从服务器断开连接。", true);
    ui->statusLabel->setText("离线");
    // Optionally, disable chat functionality or attempt to reconnect
    // QMessageBox::warning(this, "连接断开", "与服务器的连接已丢失。请尝试重新登录。");
    // on_actionLogout_triggered(); // Force logout
}


void MainWindow::updateStatusBar(const QString &message, bool isError)
{
    if (isError) {
        ui->statusbar->setStyleSheet("color: red;");
    } else {
        ui->statusbar->setStyleSheet(""); // Reset to default color
    }
    ui->statusbar->showMessage(message, 5000); // Show for 5 seconds
    qDebug() << "StatusBar:" << message;
}


// --- Menu Action Slots ---
void MainWindow::on_actionLogout_triggered()
{
    qDebug() << "Logout action triggered.";
    if (m_chatClient) {
        m_chatClient->disconnectFromServer(); // Politely disconnect
    }

    // Close all private chat windows gracefully
    // Create a copy of keys because closing window modifies m_activePrivateChats via destroyed signal
    QStringList openChats = m_activePrivateChats.keys();
    for(const QString& contact : openChats) {
        if(m_activePrivateChats.contains(contact)) {
            m_activePrivateChats.value(contact)->close(); // This should trigger its deletion via WA_DeleteOnClose
        }
    }
    m_activePrivateChats.clear();


    close(); // Close MainWindow

    // Re-create and show LoginWindow
    // This is where the first error you reported (no default constructor for LoginWindow)
    // will be fixed because m_chatClient is available here.
    if (m_chatClient) { // Ensure chatClient is still valid (though it should be app-lifetime)
        LoginWindow *loginWindow = new LoginWindow(m_chatClient);
        loginWindow->show();
    } else {
        // Fallback or error handling if chatClient is somehow null
        QMessageBox::critical(nullptr, "严重错误", "无法重新加载登录界面，聊天服务丢失。");
        QApplication::quit(); // Or some other recovery/exit
    }
}

void MainWindow::on_actionExit_triggered()
{
    QApplication::quit();
}

void MainWindow::on_actionAbout_triggered()
{
    QMessageBox::about(this, "关于实时聊天室",
                       "现代化实时聊天室客户端\n版本 0.1\n基于 Qt C++");
}

// --- Message Toolbar Button Slots (Placeholders) ---
void MainWindow::on_emojiButton_clicked()
{
    QMessageBox::information(this, "提示", "表情功能尚未实现。");
}

void MainWindow::on_fileButton_clicked()
{
    QMessageBox::information(this, "提示", "文件发送功能尚未实现。");
}
