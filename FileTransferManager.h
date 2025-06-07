#ifndef FILETRANSFERMANAGER_H
#define FILETRANSFERMANAGER_H

#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QHttpMultiPart>
#include <QHttpPart>
#include <QFile>
#include <QFileInfo>
#include <QUrl>
#include <QProgressDialog>
#include <QJsonObject>
#include <QJsonDocument>

class FileTransferManager : public QObject
{
    Q_OBJECT

public:
    explicit FileTransferManager(QObject *parent = nullptr);
    ~FileTransferManager();

    // 上传文件到服务器
    void uploadFile(const QString &filePath, const QString &recipient, const QString &authToken);
    
    // 下载文件从服务器
    void downloadFile(const QString &fileId, const QString &fileName, const QString &authToken, const QString &savePath = QString());
    
    // 获取文件列表
    void getFileList(const QString &authToken);
    
    // 设置服务器基础URL
    void setServerBaseUrl(const QString &baseUrl);

signals:
    // 文件上传相关信号
    void fileUploadStarted(const QString &fileName);
    void fileUploadProgress(const QString &fileName, qint64 bytesSent, qint64 bytesTotal);
    void fileUploadFinished(const QString &fileName, const QString &fileId, bool success, const QString &errorMessage);
    
    // 文件下载相关信号
    void fileDownloadStarted(const QString &fileName);
    void fileDownloadProgress(const QString &fileName, qint64 bytesReceived, qint64 bytesTotal);
    void fileDownloadFinished(const QString &fileName, const QString &localPath, bool success, const QString &errorMessage);
    
    // 文件列表相关信号
    void fileListReceived(const QJsonObject &fileList);
    
    // 通用错误信号
    void networkError(const QString &errorMessage);

private slots:
    void onUploadFinished();
    void onUploadProgress(qint64 bytesSent, qint64 bytesTotal);
    void onDownloadFinished();
    void onDownloadProgress(qint64 bytesReceived, qint64 bytesTotal);
    void onFileListFinished();
    void onNetworkError(QNetworkReply::NetworkError error);

private:
    QNetworkAccessManager *m_networkManager;
    QString m_serverBaseUrl;
    QMap<QNetworkReply*, QString> m_uploadReplies;  // 跟踪上传请求
    QMap<QNetworkReply*, QString> m_downloadReplies; // 跟踪下载请求
    QMap<QNetworkReply*, QFile*> m_downloadFiles;   // 跟踪下载文件
    
    // 辅助方法
    QNetworkRequest createAuthenticatedRequest(const QString &endpoint, const QString &authToken);
    QString generateBoundary();
};

#endif // FILETRANSFERMANAGER_H