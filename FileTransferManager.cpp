#include "FileTransferManager.h"
#include <QDebug>
#include <QStandardPaths>
#include <QDir>
#include <QMessageBox>
#include <QApplication>
#include <QMimeDatabase>
#include <QMimeType>
#include <QUuid>

FileTransferManager::FileTransferManager(QObject *parent)
    : QObject(parent)
    , m_networkManager(new QNetworkAccessManager(this))
    , m_serverBaseUrl("http://localhost:8080") // 指向我们的文件传输服务器
{
    // 连接网络管理器的信号
    connect(m_networkManager, &QNetworkAccessManager::finished, this, [this](QNetworkReply *reply) {
        reply->deleteLater();
    });
}

FileTransferManager::~FileTransferManager()
{
    // 清理未完成的下载文件
    for (auto it = m_downloadFiles.begin(); it != m_downloadFiles.end(); ++it) {
        if (it.value()) {
            it.value()->close();
            delete it.value();
        }
    }
}

void FileTransferManager::setServerBaseUrl(const QString &baseUrl)
{
    m_serverBaseUrl = baseUrl;
    qDebug() << "FileTransferManager: Server base URL set to" << m_serverBaseUrl;
}

void FileTransferManager::uploadFile(const QString &filePath, const QString &recipient, const QString &authToken)
{
    QFileInfo fileInfo(filePath);
    if (!fileInfo.exists() || !fileInfo.isFile()) {
        emit fileUploadFinished(fileInfo.fileName(), "", false, "文件不存在或不是有效文件");
        return;
    }

    QFile *file = new QFile(filePath);
    if (!file->open(QIODevice::ReadOnly)) {
        emit fileUploadFinished(fileInfo.fileName(), "", false, "无法打开文件进行读取");
        delete file;
        return;
    }

    // 创建多部分表单数据
    QHttpMultiPart *multiPart = new QHttpMultiPart(QHttpMultiPart::FormDataType);

    // 添加文件部分
    QHttpPart filePart;
    filePart.setHeader(QNetworkRequest::ContentTypeHeader, QVariant("application/octet-stream"));
    filePart.setHeader(QNetworkRequest::ContentDispositionHeader, 
                      QVariant(QString("form-data; name=\"file\"; filename=\"%1\"")
                              .arg(fileInfo.fileName())));
    filePart.setBodyDevice(file);
    file->setParent(multiPart); // 确保文件在multiPart删除时也被删除
    multiPart->append(filePart);

    // 添加接收者信息
    QHttpPart recipientPart;
    recipientPart.setHeader(QNetworkRequest::ContentDispositionHeader, 
                           QVariant("form-data; name=\"recipient\""));
    recipientPart.setBody(recipient.toUtf8());
    multiPart->append(recipientPart);

    // 创建请求
    QNetworkRequest request = createAuthenticatedRequest("/api/upload", authToken);
    
    // 发送请求
    QNetworkReply *reply = m_networkManager->post(request, multiPart);
    multiPart->setParent(reply); // 确保multiPart在reply删除时也被删除
    
    // 存储文件名用于跟踪
    m_uploadReplies[reply] = fileInfo.fileName();
    
    // 连接信号
    connect(reply, &QNetworkReply::finished, this, &FileTransferManager::onUploadFinished);
    connect(reply, &QNetworkReply::uploadProgress, this, &FileTransferManager::onUploadProgress);
    connect(reply, QOverload<QNetworkReply::NetworkError>::of(&QNetworkReply::errorOccurred),
            this, &FileTransferManager::onNetworkError);
    
    emit fileUploadStarted(fileInfo.fileName());
    qDebug() << "FileTransferManager: Started uploading file" << fileInfo.fileName() << "to" << recipient;
}

void FileTransferManager::downloadFile(const QString &fileId, const QString &fileName, const QString &authToken)
{
    // 创建下载目录
    QString downloadDir = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
    if (downloadDir.isEmpty()) {
        downloadDir = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    }
    downloadDir += "/ChatRoomDownloads";
    
    QDir dir;
    if (!dir.exists(downloadDir)) {
        dir.mkpath(downloadDir);
    }
    
    QString localFilePath = downloadDir + "/" + fileName;
    
    // 如果文件已存在，添加数字后缀
    int counter = 1;
    QString baseName = QFileInfo(fileName).baseName();
    QString extension = QFileInfo(fileName).suffix();
    while (QFile::exists(localFilePath)) {
        if (extension.isEmpty()) {
            localFilePath = QString("%1/%2(%3)").arg(downloadDir, baseName, QString::number(counter));
        } else {
            localFilePath = QString("%1/%2(%3).%4").arg(downloadDir, baseName, QString::number(counter), extension);
        }
        counter++;
    }
    
    QFile *file = new QFile(localFilePath);
    if (!file->open(QIODevice::WriteOnly)) {
        emit fileDownloadFinished(fileName, "", false, "无法创建本地文件");
        delete file;
        return;
    }
    
    // 创建下载请求
    QNetworkRequest request = createAuthenticatedRequest(QString("/api/download/%1").arg(fileId), authToken);
    
    QNetworkReply *reply = m_networkManager->get(request);
    
    // 存储信息用于跟踪
    m_downloadReplies[reply] = fileName;
    m_downloadFiles[reply] = file;
    
    // 连接信号
    connect(reply, &QNetworkReply::finished, this, &FileTransferManager::onDownloadFinished);
    connect(reply, &QNetworkReply::downloadProgress, this, &FileTransferManager::onDownloadProgress);
    connect(reply, &QNetworkReply::readyRead, this, [this, reply]() {
        if (m_downloadFiles.contains(reply)) {
            QFile *file = m_downloadFiles[reply];
            if (file && file->isOpen()) {
                file->write(reply->readAll());
            }
        }
    });
    connect(reply, QOverload<QNetworkReply::NetworkError>::of(&QNetworkReply::errorOccurred),
            this, &FileTransferManager::onNetworkError);
    
    emit fileDownloadStarted(fileName);
    qDebug() << "FileTransferManager: Started downloading file" << fileName << "(ID:" << fileId << ")";
}

void FileTransferManager::getFileList(const QString &authToken)
{
    QNetworkRequest request = createAuthenticatedRequest("/api/files", authToken);
    
    QNetworkReply *reply = m_networkManager->get(request);
    
    connect(reply, &QNetworkReply::finished, this, &FileTransferManager::onFileListFinished);
    connect(reply, QOverload<QNetworkReply::NetworkError>::of(&QNetworkReply::errorOccurred),
            this, &FileTransferManager::onNetworkError);
    
    qDebug() << "FileTransferManager: Requesting file list";
}

QNetworkRequest FileTransferManager::createAuthenticatedRequest(const QString &endpoint, const QString &authToken)
{
    QNetworkRequest request;
    request.setUrl(QUrl(m_serverBaseUrl + endpoint));
    request.setHeader(QNetworkRequest::UserAgentHeader, "ChatRoom-Qt-Client/1.0");
    
    if (!authToken.isEmpty()) {
        request.setRawHeader("Authorization", ("Bearer " + authToken).toUtf8());
    }
    
    return request;
}

QString FileTransferManager::generateBoundary()
{
    return QUuid::createUuid().toString().remove('{').remove('}').remove('-');
}

void FileTransferManager::onUploadFinished()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) return;
    
    QString fileName = m_uploadReplies.value(reply, "Unknown");
    m_uploadReplies.remove(reply);
    
    if (reply->error() == QNetworkReply::NoError) {
        // 解析服务器响应
        QByteArray responseData = reply->readAll();
        QJsonDocument doc = QJsonDocument::fromJson(responseData);
        
        if (doc.isObject()) {
            QJsonObject response = doc.object();
            bool success = response.value("success").toBool();
            QString fileId = response.value("fileId").toString();
            QString message = response.value("message").toString();
            
            if (success && !fileId.isEmpty()) {
                emit fileUploadFinished(fileName, fileId, true, "");
                qDebug() << "FileTransferManager: File" << fileName << "uploaded successfully with ID" << fileId;
            } else {
                emit fileUploadFinished(fileName, "", false, message.isEmpty() ? "服务器返回错误" : message);
            }
        } else {
            emit fileUploadFinished(fileName, "", false, "服务器响应格式错误");
        }
    } else {
        QString errorMsg = QString("网络错误: %1").arg(reply->errorString());
        emit fileUploadFinished(fileName, "", false, errorMsg);
        qDebug() << "FileTransferManager: Upload failed for" << fileName << ":" << errorMsg;
    }
}

void FileTransferManager::onUploadProgress(qint64 bytesSent, qint64 bytesTotal)
{
    QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) return;
    
    QString fileName = m_uploadReplies.value(reply, "Unknown");
    emit fileUploadProgress(fileName, bytesSent, bytesTotal);
}

void FileTransferManager::onDownloadFinished()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) return;
    
    QString fileName = m_downloadReplies.value(reply, "Unknown");
    QFile *file = m_downloadFiles.value(reply, nullptr);
    
    m_downloadReplies.remove(reply);
    m_downloadFiles.remove(reply);
    
    QString localPath;
    if (file) {
        localPath = file->fileName();
        file->close();
        delete file;
    }
    
    if (reply->error() == QNetworkReply::NoError) {
        emit fileDownloadFinished(fileName, localPath, true, "");
        qDebug() << "FileTransferManager: File" << fileName << "downloaded successfully to" << localPath;
    } else {
        // 删除不完整的文件
        if (!localPath.isEmpty()) {
            QFile::remove(localPath);
        }
        
        QString errorMsg = QString("下载失败: %1").arg(reply->errorString());
        emit fileDownloadFinished(fileName, "", false, errorMsg);
        qDebug() << "FileTransferManager: Download failed for" << fileName << ":" << errorMsg;
    }
}

void FileTransferManager::onDownloadProgress(qint64 bytesReceived, qint64 bytesTotal)
{
    QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) return;
    
    QString fileName = m_downloadReplies.value(reply, "Unknown");
    emit fileDownloadProgress(fileName, bytesReceived, bytesTotal);
}

void FileTransferManager::onFileListFinished()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) return;
    
    if (reply->error() == QNetworkReply::NoError) {
        QByteArray responseData = reply->readAll();
        QJsonDocument doc = QJsonDocument::fromJson(responseData);
        
        if (doc.isObject()) {
            QJsonObject response = doc.object();
            emit fileListReceived(response);
            qDebug() << "FileTransferManager: File list received successfully";
        } else {
            emit networkError("文件列表响应格式错误");
        }
    } else {
        QString errorMsg = QString("获取文件列表失败: %1").arg(reply->errorString());
        emit networkError(errorMsg);
        qDebug() << "FileTransferManager: Get file list failed:" << errorMsg;
    }
}

void FileTransferManager::onNetworkError(QNetworkReply::NetworkError error)
{
    QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) return;
    
    QString errorMsg = QString("网络错误 (%1): %2").arg(QString::number(error), reply->errorString());
    emit networkError(errorMsg);
    qDebug() << "FileTransferManager: Network error:" << errorMsg;
}