# HelloChatRoom 文件传输功能说明

## 概述

本项目已成功集成了文件传输功能，允许用户在私聊中发送和接收文件。

## 功能特性

### 1. 文件上传
- 支持多种文件格式：txt, pdf, png, jpg, jpeg, gif, doc, docx, zip, rar
- 文件大小限制：50MB
- 实时上传进度显示
- 上传完成后自动发送文件消息给对方

### 2. 文件下载
- 接收到文件消息时自动弹出下载确认框
- 可选择保存位置
- 实时下载进度显示
- 下载完成提示

### 3. 文件消息显示
- 在聊天界面中以特殊格式显示文件消息
- 显示文件名、大小和下载按钮
- 区分发送和接收的文件消息

## 技术实现

### 客户端组件

1. **FileTransferManager类**
   - 负责文件的上传和下载
   - 使用HTTP协议与服务器通信
   - 支持进度回调和错误处理

2. **ChatClient类扩展**
   - 新增文件消息发送功能
   - 集成FileTransferManager
   - 处理文件消息接收

3. **PrivateChat界面扩展**
   - 文件按钮功能实现
   - 进度对话框显示
   - 文件消息显示和交互

### 服务端组件

**file_server.py** - 基于Flask的文件传输服务器
- 提供文件上传API：`POST /api/upload`
- 提供文件下载API：`GET /api/download/<file_id>`
- 提供文件列表API：`GET /api/files`
- 提供文件信息API：`GET /api/file/<file_id>/info`
- 健康检查API：`GET /health`

## 使用方法

### 1. 启动服务器

```bash
# 安装依赖（如果尚未安装）
python -m pip install flask flask-cors werkzeug

# 启动文件传输服务器
python file_server.py
```

服务器将在 http://localhost:8080 启动

### 2. 编译客户端

```bash
# 使用提供的编译脚本
build.bat
```

或者手动编译：
```bash
qmake HelloChatRoom.pro
mingw32-make
```

### 3. 使用文件传输

1. 启动HelloChatRoom客户端
2. 登录并开始私聊
3. 点击文件按钮（📄）选择要发送的文件
4. 等待上传完成，文件消息将自动发送给对方
5. 接收方会看到文件消息和下载提示
6. 点击下载按钮或在弹出框中选择下载位置

## 注意事项

1. **AI聊天限制**：与通义千问的聊天不支持文件传输
2. **文件大小限制**：单个文件不能超过50MB
3. **网络连接**：需要确保客户端能够访问文件传输服务器
4. **文件安全**：服务器会验证文件类型，但建议不要传输敏感文件

## 文件存储

- 上传的文件存储在服务器的 `uploads/` 目录中
- 文件使用UUID重命名，确保唯一性
- 服务器内存中维护文件ID到文件信息的映射

## API接口说明

### 上传文件
```
POST /api/upload
Content-Type: multipart/form-data

参数：
- file: 文件数据
- fileId: 文件ID（可选）
- uploader: 上传者用户名
- receiver: 接收者用户名

返回：
{
  "success": true,
  "fileId": "uuid",
  "filename": "原始文件名",
  "size": 文件大小
}
```

### 下载文件
```
GET /api/download/<file_id>

返回：文件数据流
```

## 故障排除

1. **编译错误**：确保Qt环境正确配置，检查PATH环境变量
2. **服务器连接失败**：确认file_server.py正在运行且端口8080可访问
3. **文件上传失败**：检查文件大小和格式是否符合要求
4. **下载失败**：确认文件ID有效且文件仍存在于服务器

## 扩展功能建议

1. 添加文件传输加密
2. 实现断点续传
3. 添加文件预览功能
4. 支持批量文件传输
5. 添加传输历史记录
6. 实现文件过期自动删除