#ifndef PRIVATECHAT_H
#define PRIVATECHAT_H

#include <QDialog>
#include <QString>
#include <QDateTime>
#include <QProgressDialog>

namespace Ui {
class PrivateChat;
}

class ChatClient;
class MainWindow; // 需要包含以便访问静态成员

class PrivateChat : public QDialog
{
    Q_OBJECT

public:
    explicit PrivateChat(ChatClient *client, QWidget *parent = nullptr);
    ~PrivateChat();

    void setChatPartner(const QString &partnerName, const QString &currentUserName);
    QString getChatPartnerName() const;
    void receiveMessage(const QString &sender, const QString &message);

private slots:
    void on_sendButton_clicked();
    void on_emojiButton_clicked();
    void on_fileButton_clicked();
    
    // 文件传输相关槽函数
    void onFileUploadProgress(const QString &fileName, qint64 bytesSent, qint64 bytesTotal);
    void onFileUploadFinished(const QString &fileName, const QString &fileId, bool success, const QString &errorMessage);
    void onFileDownloadProgress(const QString &fileName, qint64 bytesReceived, qint64 bytesTotal);
    void onFileDownloadFinished(const QString &fileName, const QString &localPath, bool success, const QString &errorMessage);

public slots:
    void onFileMessageReceived(const QString &from, const QString &to, const QString &fileName, const QString &fileId, qint64 fileSize, qint64 timestamp);

private:
    Ui::PrivateChat *ui;
    ChatClient *m_chatClient;
    QString m_chatPartnerUsername;
    QString m_currentUsername;
    bool m_isChatWithTongyi; // 新增：标记是否与通义千问聊天
    
    // 文件传输相关成员变量
    QProgressDialog *m_uploadProgressDialog;
    QProgressDialog *m_downloadProgressDialog;

    void appendMessage(const QString &sender, const QString &message, bool isMe);
    void appendFileMessage(const QString &sender, const QString &fileName, const QString &fileId, qint64 fileSize, bool isMe);
    void setupFileTransferConnections();
};

#endif // PRIVATECHAT_H
