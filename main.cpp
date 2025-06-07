#include "LoginWindow.h" // 调整路径
#include "ChatClient.h"  // 引入 ChatClient
#include <QApplication>
#include <QLocale>
#include <QTranslator>
#include <QUrl> // 用于服务器URL

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    // 1. 创建 ChatClient 实例
    ChatClient chatClient;

    // 2. 设置服务器地址并尝试连接
    //    请将 "ws://localhost:5000/ws" 替换为您的实际Flask-SocketIO服务器地址
    //    通常 Flask-SocketIO 的默认路径是 /socket.io/，但您的文档中是 /ws，请确认
    //    如果您的 Flask 服务器运行在本地的 5000 端口，并且 WebSocket 路径是 /ws
    // QUrl serverUrl(QStringLiteral("ws://127.0.0.1:5000/ws")); // <<---- 就是这一行！请修改为您的服务器URL
    QUrl serverUrl(QStringLiteral("ws://127.0.0.1:5000/socket.io/?EIO=4&transport=websocket")); // <<---- 就是这一行！请修改为您的服务器URL
    chatClient.connectToServer(serverUrl);

    // 3. 创建登录窗口并传递 ChatClient 实例
    LoginWindow w(&chatClient); // 将 chatClient 指针传递给 LoginWindow
    w.show();

    // 进入事件循环
    return a.exec();
}
