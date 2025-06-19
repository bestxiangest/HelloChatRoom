# HelloChatRoom

一个基于Qt和Flask-SocketIO的现代化聊天室应用，支持实时消息传递、文件传输和AI智能对话功能。

## 🌟 功能特性

### 💬 实时聊天
- **多用户在线聊天**：支持多用户同时在线交流
- **私聊功能**：一对一私密聊天
- **群聊功能**：广播消息给所有在线用户
- **用户状态管理**：实时显示在线用户列表
- **消息时间戳**：精确到毫秒的消息时间记录

### 🤖 AI智能助手
- **通义千问集成**：内置阿里云通义千问AI助手
- **上下文对话**：支持多轮对话，保持对话上下文
- **智能回复**：AI助手提供智能化的回复和建议

### 📁 文件传输
- **多格式支持**：支持txt, pdf, png, jpg, jpeg, gif, doc, docx, zip, rar等格式
- **大文件传输**：支持最大50MB文件传输
- **实时进度**：上传下载进度实时显示
- **安全传输**：基于HTTP协议的安全文件传输
- **自动消息**：文件传输完成后自动发送文件消息

### 🎨 用户界面
- **现代化UI**：基于Qt Widgets的现代化界面设计
- **响应式布局**：适配不同屏幕尺寸
- **直观操作**：简洁明了的用户交互体验
- **多窗口支持**：登录窗口、主窗口、私聊窗口

## 🛠️ 技术栈

### 客户端 (Qt C++)
- **Qt 6.9.0**：跨平台GUI框架
- **Qt WebSockets**：WebSocket通信
- **Qt Network**：网络请求处理
- **C++17**：现代C++标准
- **MinGW 64-bit**：编译器

### 服务端 (Python)
- **Flask**：轻量级Web框架
- **Flask-SocketIO**：WebSocket支持
- **Flask-CORS**：跨域资源共享
- **OpenAI API**：通义千问AI集成
- **Python 3.12**：Python运行环境

## 📦 项目结构

```
HelloChatRoom/
├── app/                    # 服务端代码
│   ├── __init__.py        # Flask应用初始化
│   ├── file_transfer.py   # 文件传输API
│   └── sockets/
│       └── chat.py        # SocketIO事件处理
├── ChatClient.cpp/.h      # 聊天客户端核心类
├── FileTransferManager.cpp/.h  # 文件传输管理器
├── loginwindow.cpp/.h/.ui # 登录窗口
├── mainwindow.cpp/.h/.ui  # 主窗口
├── privatechat.cpp/.h/.ui # 私聊窗口
├── main.cpp               # 程序入口
├── HelloChatRoom.pro      # Qt项目文件
├── run.py                 # 服务端启动脚本
└── uploads/               # 文件上传目录
```

## 🚀 快速开始

### 环境要求

**客户端**
- Qt 6.9.0 或更高版本
- MinGW 64-bit 编译器
- Windows 操作系统

**服务端**
- Python 3.8 或更高版本
- pip 包管理器

### 安装步骤

#### 1. 克隆项目
```bash
git clone https://github.com/yourusername/HelloChatRoom.git
cd HelloChatRoom
```

#### 2. 设置服务端
```bash
# 创建虚拟环境（推荐）
python -m venv venv
venv\Scripts\activate  # Windows

# 安装依赖
pip install flask flask-socketio flask-cors openai
```

#### 3. 启动服务端
```bash
python run.py
```
服务器将在 `http://localhost:5000` 启动

#### 4. 编译客户端
```bash
# 使用Qt Creator打开HelloChatRoom.pro
# 或使用命令行编译
qmake HelloChatRoom.pro
mingw32-make
```

#### 5. 运行客户端
编译完成后运行生成的可执行文件，或在Qt Creator中直接运行。

## 📖 使用说明

### 登录
1. 启动客户端应用
2. 使用预设账户登录：
   - 用户名：`admin`、`alice`、`bob`
   - 密码：`1`

### 聊天功能
1. **群聊**：在主窗口中发送消息给所有在线用户
2. **私聊**：双击用户列表中的用户名开启私聊窗口
3. **AI对话**：在聊天界面中与通义千问AI助手对话

### 文件传输
1. 在私聊窗口中点击文件按钮（📄）
2. 选择要发送的文件（支持多种格式，最大50MB）
3. 等待上传完成，文件消息自动发送
4. 接收方点击下载按钮保存文件

## 🔧 配置说明

### 服务器配置
- **端口**：默认5000，可通过环境变量`PORT`修改
- **AI API**：在`app/sockets/chat.py`中配置通义千问API密钥
- **文件存储**：上传文件默认存储在`uploads/`目录

### 客户端配置
- **服务器地址**：在`main.cpp`中修改服务器URL
- **连接协议**：使用WebSocket协议连接

## 🤝 贡献指南

1. Fork 本项目
2. 创建特性分支 (`git checkout -b feature/AmazingFeature`)
3. 提交更改 (`git commit -m 'Add some AmazingFeature'`)
4. 推送到分支 (`git push origin feature/AmazingFeature`)
5. 开启 Pull Request

## 📝 开发计划

- [ ] 添加文件传输加密
- [ ] 实现断点续传
- [ ] 添加文件预览功能
- [ ] 支持批量文件传输
- [ ] 添加传输历史记录
- [ ] 实现文件过期自动删除
- [ ] 添加用户头像功能
- [ ] 支持表情包发送
- [ ] 实现消息撤回功能

## 📄 许可证

本项目采用 MIT 许可证 - 查看 [LICENSE](LICENSE) 文件了解详情

## 👥 作者

- **开发者** - [Sharpcaterpillar](https://github.com/bestxiangest)

## 🙏 致谢

- Qt Framework 提供的强大GUI框架
- Flask-SocketIO 提供的WebSocket支持
- 阿里云通义千问提供的AI服务
- 所有贡献者和测试用户

## 📞 联系方式

如有问题或建议，请通过以下方式联系：
- 提交 [Issue](https://github.com/yourusername/HelloChatRoom/issues)
- 发送邮件至：zzningg@qq.com

---

⭐ 如果这个项目对你有帮助，请给它一个星标！
