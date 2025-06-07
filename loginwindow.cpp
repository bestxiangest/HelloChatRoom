#include "LoginWindow.h"
#include "ui_LoginWindow.h"
#include "MainWindow.h" // 引入 MainWindow
#include "ChatClient.h"  // 引入 ChatClient
#include <QMessageBox>
#include <QDebug>

// 修改构造函数
LoginWindow::LoginWindow(ChatClient *client, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::LoginWindow),
    m_chatClient(client), // 保存 ChatClient 指针
    m_mainWindow(nullptr)
{
    ui->setupUi(this);
    setWindowTitle("用户登录");

    if (!m_chatClient) {
        qCritical() << "LoginWindow: ChatClient instance is null!";
        // 可能需要禁用登录按钮或显示错误
        QMessageBox::critical(this, "严重错误", "聊天服务不可用。");
        ui->loginButton->setEnabled(false);
        return;
    }

    // 连接 ChatClient 的信号到 LoginWindow 的槽
    connect(m_chatClient, &ChatClient::loginSucceeded, this, &LoginWindow::handleLoginSucceeded);
    connect(m_chatClient, &ChatClient::loginFailed, this, &LoginWindow::handleLoginFailed);
    connect(m_chatClient, &ChatClient::socketError, this, &LoginWindow::handleSocketError);
    connect(m_chatClient, &ChatClient::disconnected, this, &LoginWindow::handleDisconnected);

    // 可选：检查初始连接状态
    if (!m_chatClient->isConnected()) {
        // 可以显示一个“正在连接到服务器...”的提示，或者在 handleSocketError/handleDisconnected 中处理
        qDebug() << "LoginWindow: Not initially connected to server.";
        // QMessageBox::information(this, "连接提示", "正在尝试连接到服务器...");
    }
}

LoginWindow::~LoginWindow()
{
    delete ui;
    // m_mainWindow is handled by its own lifecycle or WA_DeleteOnClose
}

void LoginWindow::on_loginButton_clicked()
{
    QString username = ui->usernameLineEdit->text().trimmed();
    QString password = ui->passwordLineEdit->text();

    if (username.isEmpty() || password.isEmpty()) {
        QMessageBox::warning(this, "登录错误", "用户名和密码不能为空。");
        return;
    }

    if (!m_chatClient->isConnected()) {
        QMessageBox::warning(this, "登录错误", "未连接到服务器，请稍后再试。");
        // 尝试重新连接，或者提示用户检查网络
        // m_chatClient->connectToServer(serverUrl); // 需要服务器URL
        return;
    }

    // 调用 ChatClient 的 attemptLogin 方法
    ui->loginButton->setEnabled(false); // 禁用按钮，防止重复点击
    // ui->statusbar->showMessage("正在登录..."); // 移除: 'statusbar' 不存在于 LoginWindow.ui
    m_chatClient->attemptLogin(username, password);
}

void LoginWindow::on_registerButton_clicked()
{
    QMessageBox::information(this, "注册", "注册功能尚未实现。");
}

void LoginWindow::handleLoginSucceeded(const QString &username, const QString &token)
{
    Q_UNUSED(token); // Token 可能暂时不用在客户端UI层面，但ChatClient内部会保存
    qDebug() << "LoginWindow: Login successful for user:" << username;
    // ui->statusbar->showMessage("登录成功!"); // 移除: 'statusbar' 不存在于 LoginWindow.ui

    // 创建并显示主窗口，传递 ChatClient 实例
    // 确保 m_mainWindow 只被创建一次，或者正确管理其生命周期
    if (!m_mainWindow) {
        m_mainWindow = new MainWindow(m_chatClient); // 将 ChatClient 指针传递给 MainWindow
        // m_mainWindow->setAttribute(Qt::WA_DeleteOnClose); // 确保关闭时删除
    }
    m_mainWindow->setCurrentUser(username); // 设置当前用户名
    m_mainWindow->show();

    accept(); // 关闭登录对话框
}

void LoginWindow::handleLoginFailed(const QString &reason)
{
    qDebug() << "LoginWindow: Login failed. Reason:" << reason;
    QMessageBox::warning(this, "登录失败", reason);
    ui->loginButton->setEnabled(true); // 重新启用登录按钮
    // ui->statusbar->clearMessage(); // 移除: 'statusbar' 不存在于 LoginWindow.ui
    ui->passwordLineEdit->clear();
}

void LoginWindow::handleSocketError(const QString &errorString)
{
    // 这个槽可能会在任何时候被调用，如果连接断开或失败
    if (this->isVisible()) { // 只在登录窗口可见时显示错误，避免打扰主窗口
        QMessageBox::critical(this, "网络错误", "无法连接到服务器: " + errorString);
        ui->loginButton->setEnabled(true); // 确保按钮可用
        // ui->statusbar->showMessage("连接错误!"); // 移除: 'statusbar' 不存在于 LoginWindow.ui
    }
}

void LoginWindow::handleDisconnected()
{
    if (this->isVisible()) {
        QMessageBox::warning(this, "连接断开", "与服务器的连接已断开。");
        ui->loginButton->setEnabled(true);
        // ui->statusbar->showMessage("已断开连接"); // 移除: 'statusbar' 不存在于 LoginWindow.ui
    }
}

// 这个函数不再需要，因为登录逻辑移至 ChatClient
// void LoginWindow::attemptServerLogin(const QString &username, const QString &password) { ... }

