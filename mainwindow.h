#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QListWidgetItem>
#include <QDateTime>
#include <QMap>
#include <QJsonArray> // 确保包含

namespace Ui {
class MainWindow;
}

class ChatClient;
class PrivateChat;
class LoginWindow;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(ChatClient *client, QWidget *parent = nullptr);
    ~MainWindow();

    void setCurrentUser(const QString &username);
    // 新增：通义千问的固定联系人名称
    static const QString TONGYI_QWEN_CONTACT_NAME;


private slots:
    void on_sendButton_clicked();
    void on_contactsListWidget_itemDoubleClicked(QListWidgetItem *item);
    void on_searchLineEdit_textChanged(const QString &searchText);
    void on_actionLogout_triggered();
    void on_actionExit_triggered();
    void on_actionAbout_triggered();
    void on_emojiButton_clicked();
    void on_fileButton_clicked();

    void handleNewMessageReceived(const QString &from, const QString &to, const QString &content, const QString &type, qint64 timestamp);
    void handleUserListUpdated(const QJsonArray &users);
    void handleSocketError(const QString &errorString);
    void handleServerMessage(const QString &message);
    void handleClientDisconnected();

    // 新增：处理通义千问回复的槽函数
    void handleTongyiMessageResponse(const QString &from, const QString &content, qint64 timestamp, bool success);
    // 新增：处理文件消息的槽函数
    void handleFileMessageReceived(const QString &from, const QString &to, const QString &fileName, const QString &fileId, qint64 fileSize, qint64 timestamp);
    // void handleSocketIoConnected(); // 从您上传的文件看，这个槽并未在.h中声明，我们将保持一致

private:
    Ui::MainWindow *ui;
    ChatClient *m_chatClient;
    QString m_currentUsername;
    QMap<QString, PrivateChat*> m_activePrivateChats;

    void appendMessageToDisplay(const QString &sender, const QString &message, bool isMe, const QString &chatContext = "main");
    void addTongyiQwenToList(); // 新增：辅助函数
    void openChatWith(const QString &contactName); // 修改了 openPrivateChat 的名称，使其更通用
    void updateStatusBar(const QString &message, bool isError = false);
};

#endif // MAINWINDOW_H
