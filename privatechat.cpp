#include "privatechat.h"
#include "ui_privatechat.h"
#include "ChatClient.h"
#include "MainWindow.h" // 包含 MainWindow.h 以访问静态成员
#include "FileTransferManager.h"
#include <QMessageBox>
#include <QDebug>
#include <QFileDialog>
#include <QStandardPaths>
#include <QDir>
#include <QUuid>

PrivateChat::PrivateChat(ChatClient *client, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::PrivateChat),
    m_chatClient(client),
    m_isChatWithTongyi(false), // 初始化标志位
    m_uploadProgressDialog(nullptr),
    m_downloadProgressDialog(nullptr)
{
    ui->setupUi(this);
    setAttribute(Qt::WA_DeleteOnClose);

    if (!m_chatClient) {
        qCritical() << "PrivateChat: ChatClient instance is null!";
        QMessageBox::critical(this, "严重错误", "聊天服务不可用。");
        ui->sendButton->setEnabled(false);
        ui->messageInputEdit->setEnabled(false);
    } else {
        setupFileTransferConnections();
    }
}

PrivateChat::~PrivateChat()
{
    qDebug() << "PrivateChat window with" << m_chatPartnerUsername << "destroyed.";
    
    if (m_uploadProgressDialog) {
        m_uploadProgressDialog->deleteLater();
    }
    if (m_downloadProgressDialog) {
        m_downloadProgressDialog->deleteLater();
    }
    
    delete ui;
}

void PrivateChat::setChatPartner(const QString &partnerName, const QString &currentUserName)
{
    this->m_chatPartnerUsername = partnerName;
    this->m_currentUsername = currentUserName;

    // 检查聊天对象是否是通义千问
    m_isChatWithTongyi = (partnerName == MainWindow::TONGYI_QWEN_CONTACT_NAME);

    setWindowTitle(QString("与 %1 的私聊").arg(partnerName));
    ui->chatPartnerNameLabel->setText(partnerName);

    // 根据聊天对象调整UI
    if (m_isChatWithTongyi) {
        ui->chatPartnerStatusLabel->setText("AI 助手"); // 为AI设置特定状态
        ui->fileButton->setEnabled(false); // AI不支持文件传输
        ui->messageDisplayBrowser->clear();
        // 添加一条欢迎语
        appendMessage(MainWindow::TONGYI_QWEN_CONTACT_NAME, QStringLiteral("您好！我是通义千问，有什么可以帮助您的吗？"), false);
    } else {
        ui->chatPartnerStatusLabel->setText("在线"); // 普通用户的状态，可以后续动态更新
        ui->fileButton->setEnabled(true);
        ui->messageDisplayBrowser->clear();
        ui->messageDisplayBrowser->setHtml(QString("<p style='color:grey;'><i>开始与 %1 聊天...</i></p>").arg(partnerName));
    }
}

QString PrivateChat::getChatPartnerName() const
{
    return m_chatPartnerUsername;
}

void PrivateChat::on_sendButton_clicked()
{
    QString message = ui->messageInputEdit->toPlainText().trimmed();
    if (message.isEmpty() || !m_chatClient) {
        return;
    }

    if (!m_chatClient->isConnected()) {
        QMessageBox::warning(this, "发送失败", "未连接到服务器。");
        return;
    }

    // 先在本地显示自己发送的消息
    appendMessage(m_currentUsername, message, true);
    ui->messageInputEdit->clear();

    // ** 关键逻辑：根据聊天对象调用不同的方法 **
    if (m_isChatWithTongyi) {
        m_chatClient->sendToTongyi(message);
    } else {
        m_chatClient->sendMessage(m_chatPartnerUsername, message, "text");
    }
}

void PrivateChat::receiveMessage(const QString &sender, const QString &message)
{
    // 这个方法由 MainWindow 调用，用于显示收到的消息
    // sender 是消息的实际发送方 (例如 "通义千问" 或其他用户的名字)
    bool isMe = (sender == m_currentUsername);
    appendMessage(sender, message, isMe);
}

void PrivateChat::appendMessage(const QString &sender, const QString &message, bool isMe)
{
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
        // 可选：为AI的消息使用不同的背景色以作区分
        QString bgColor = (sender == MainWindow::TONGYI_QWEN_CONTACT_NAME) ? "#e0e0f0" : "#ffffff";
        QString borderColor = (sender == MainWindow::TONGYI_QWEN_CONTACT_NAME) ? "#c0c0e0" : "#e0e0e0";

        formattedMessage = QString(
                               "<div style='text-align: left; margin: 5px;'>"
                               "  <span style='background-color: %3; border: 1px solid %4; padding: 8px 12px; border-radius: 10px; display: inline-block; max-width: 70%;'>"
                               "    <b>%1:</b><br>%2"
                               "  </span>"
                               "</div>"
                               ).arg(displayName, message.toHtmlEscaped(), bgColor, borderColor);
    }

    ui->messageDisplayBrowser->append(formattedMessage);
    ui->messageDisplayBrowser->ensureCursorVisible();
}

void PrivateChat::on_emojiButton_clicked()
{
    QMessageBox::information(this, "提示", "表情功能尚未实现。");
}

void PrivateChat::on_fileButton_clicked()
{
    if (m_isChatWithTongyi) {
        QMessageBox::information(this, "提示", "AI助手不支持文件传输。");
        return;
    }

    if (!m_chatClient || !m_chatClient->isConnected()) {
        QMessageBox::warning(this, "错误", "未连接到服务器。");
        return;
    }

    QString fileName = QFileDialog::getOpenFileName(this, "选择要发送的文件", 
        QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation));
    
    if (fileName.isEmpty()) {
        return;
    }

    QFileInfo fileInfo(fileName);
    if (!fileInfo.exists() || !fileInfo.isFile()) {
        QMessageBox::warning(this, "错误", "选择的文件不存在或不是有效文件。");
        return;
    }

    // 检查文件大小限制（例如50MB）
    const qint64 maxFileSize = 50 * 1024 * 1024; // 50MB
    if (fileInfo.size() > maxFileSize) {
        QMessageBox::warning(this, "错误", "文件大小超过限制（50MB）。");
        return;
    }

    // 生成唯一的文件ID
    QString fileId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    
    // 创建上传进度对话框
    if (m_uploadProgressDialog) {
        m_uploadProgressDialog->deleteLater();
    }
    m_uploadProgressDialog = new QProgressDialog("正在上传文件...", "取消", 0, 100, this);
    m_uploadProgressDialog->setWindowModality(Qt::WindowModal);
    m_uploadProgressDialog->setAutoClose(false);
    m_uploadProgressDialog->setAutoReset(false);
    
    connect(m_uploadProgressDialog, &QProgressDialog::canceled, [this, fileId]() {
        // 这里可以添加取消上传的逻辑
        qDebug() << "用户取消了文件上传:" << fileId;
    });
    
    m_uploadProgressDialog->show();
    
    // 开始上传文件
    FileTransferManager *ftm = m_chatClient->getFileTransferManager();
    if (ftm) {
        ftm->uploadFile(fileName, m_chatPartnerUsername, "");
    } else {
        QMessageBox::critical(this, "错误", "文件传输管理器不可用。");
        if (m_uploadProgressDialog) {
            m_uploadProgressDialog->close();
        }
    }
}

// 设置文件传输连接
void PrivateChat::setupFileTransferConnections()
{
    if (!m_chatClient) return;
    
    FileTransferManager *ftm = m_chatClient->getFileTransferManager();
    if (!ftm) return;
    
    // 连接文件传输信号
    connect(ftm, &FileTransferManager::fileUploadProgress, this, &PrivateChat::onFileUploadProgress);
    connect(ftm, &FileTransferManager::fileUploadFinished, this, &PrivateChat::onFileUploadFinished);
    connect(ftm, &FileTransferManager::fileDownloadProgress, this, &PrivateChat::onFileDownloadProgress);
    connect(ftm, &FileTransferManager::fileDownloadFinished, this, &PrivateChat::onFileDownloadFinished);
    
    // 连接ChatClient的文件消息信号
    connect(m_chatClient, &ChatClient::fileMessageReceived, this, &PrivateChat::onFileMessageReceived);
}

// 文件上传进度
void PrivateChat::onFileUploadProgress(const QString &fileName, qint64 bytesSent, qint64 bytesTotal)
{
    if (m_uploadProgressDialog && bytesTotal > 0) {
        int progress = static_cast<int>((bytesSent * 100) / bytesTotal);
        m_uploadProgressDialog->setValue(progress);
        m_uploadProgressDialog->setLabelText(QString("正在上传文件: %1\n进度: %2/%3")
            .arg(QFileInfo(fileName).fileName())
            .arg(bytesSent)
            .arg(bytesTotal));
    }
}

// 文件上传完成
void PrivateChat::onFileUploadFinished(const QString &fileName, const QString &fileId, bool success, const QString &errorMessage)
{
    if (m_uploadProgressDialog) {
        m_uploadProgressDialog->close();
        m_uploadProgressDialog->deleteLater();
        m_uploadProgressDialog = nullptr;
    }
    
    QFileInfo fileInfo(fileName);
    if (success) {
        // 发送文件消息给对方
        m_chatClient->sendFileMessage(m_chatPartnerUsername, fileInfo.fileName(), fileId, fileInfo.size());
        
        // 在聊天界面显示文件消息
        appendFileMessage(m_currentUsername, fileInfo.fileName(), fileId, fileInfo.size(), true);
        
        QMessageBox::information(this, "成功", "文件上传成功！");
    } else {
        QMessageBox::critical(this, "错误", "文件上传失败。");
    }
}

// 文件下载进度
void PrivateChat::onFileDownloadProgress(const QString &fileName, qint64 bytesReceived, qint64 bytesTotal)
{
    if (m_downloadProgressDialog && bytesTotal > 0) {
        int progress = static_cast<int>((bytesReceived * 100) / bytesTotal);
        m_downloadProgressDialog->setValue(progress);
        m_downloadProgressDialog->setLabelText(QString("正在下载文件: %1\n进度: %2/%3")
            .arg(QFileInfo(fileName).fileName())
            .arg(bytesReceived)
            .arg(bytesTotal));
    }
}

// 文件下载完成
void PrivateChat::onFileDownloadFinished(const QString &fileName, const QString &localPath, bool success, const QString &errorMessage)
{
    if (m_downloadProgressDialog) {
        m_downloadProgressDialog->close();
        m_downloadProgressDialog->deleteLater();
        m_downloadProgressDialog = nullptr;
    }
    
    if (success) {
        QMessageBox::information(this, "成功", QString("文件下载成功！\n保存位置: %1").arg(localPath));
    } else {
        QMessageBox::critical(this, "错误", QString("文件下载失败: %1").arg(errorMessage));
    }
}

// 收到文件消息
void PrivateChat::onFileMessageReceived(const QString &from, const QString &to, const QString &fileName, const QString &fileId, qint64 fileSize, qint64 timestamp)
{
    Q_UNUSED(timestamp)
    
    // 只处理发给当前聊天窗口的文件消息
    if (to != m_currentUsername || from != m_chatPartnerUsername) {
        return;
    }
    
    // 在聊天界面显示文件消息
    appendFileMessage(from, fileName, fileId, fileSize, false);
}

// 添加文件消息到聊天界面
void PrivateChat::appendFileMessage(const QString &sender, const QString &fileName, const QString &fileId, qint64 fileSize, bool isMe)
{
    QString displayName = isMe ? "我" : sender;
    QString sizeText = QString::number(fileSize / 1024.0, 'f', 1) + " KB";
    if (fileSize >= 1024 * 1024) {
        sizeText = QString::number(fileSize / (1024.0 * 1024.0), 'f', 1) + " MB";
    }
    
    QString downloadButton = "";
    // 下载按钮已移除，下载逻辑在接收文件消息时处理
    
    QString formattedMessage;
    if (isMe) {
        formattedMessage = QString(
            "<div style='text-align: right; margin: 5px;'>"
            "  <span style='background-color: #dcf8c6; padding: 8px 12px; border-radius: 10px; display: inline-block; max-width: 70%; text-align:left;'>"
            "    <b>%1:</b><br>📄 %2<br><small>大小: %3</small>%4"
            "  </span>"
            "</div>"
        ).arg(displayName, fileName, sizeText, downloadButton);
    } else {
        formattedMessage = QString(
            "<div style='text-align: left; margin: 5px;'>"
            "  <span style='background-color: #ffffff; border: 1px solid #e0e0e0; padding: 8px 12px; border-radius: 10px; display: inline-block; max-width: 70%;'>"
            "    <b>%1:</b><br>📄 %2<br><small>大小: %3</small>%4"
            "  </span>"
            "</div>"
        ).arg(displayName, fileName, sizeText, downloadButton);
    }
    
    ui->messageDisplayBrowser->append(formattedMessage);
    ui->messageDisplayBrowser->ensureCursorVisible();
    
    // 如果是接收到的文件，询问是否下载
    if (!isMe) {
        int ret = QMessageBox::question(this, "文件接收", 
            QString("收到文件: %1\n大小: %2\n\n是否立即下载？").arg(fileName, sizeText),
            QMessageBox::Yes | QMessageBox::No);
        
        if (ret == QMessageBox::Yes) {
            // 选择保存位置
            QString savePath = QFileDialog::getSaveFileName(this, "保存文件", 
                QStandardPaths::writableLocation(QStandardPaths::DownloadLocation) + "/" + fileName);
            
            if (!savePath.isEmpty()) {
                // 创建下载进度对话框
                if (m_downloadProgressDialog) {
                    m_downloadProgressDialog->deleteLater();
                }
                m_downloadProgressDialog = new QProgressDialog("正在下载文件...", "取消", 0, 100, this);
                m_downloadProgressDialog->setWindowModality(Qt::WindowModal);
                m_downloadProgressDialog->setAutoClose(false);
                m_downloadProgressDialog->setAutoReset(false);
                m_downloadProgressDialog->show();
                
                // 开始下载
                FileTransferManager *ftm = m_chatClient->getFileTransferManager();
                if (ftm) {
                    ftm->downloadFile(fileId, fileName, "", savePath);
                }
            }
        }
    }
}


