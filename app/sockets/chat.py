from flask_socketio import emit, join_room, leave_room, disconnect
from flask import request
import time
import os
import sys
from openai import OpenAI

# 这些变量将在 app/__init__.py 中初始化并传入
# socketio_instance = None
# online_users_ref = None
# mock_users_db_ref = None

# 存储每个 SID 对应的用户名
# { 'sid': 'username', ... }
sid_to_user = {}
# 用于存储每个用户与通义千问的对话历史
# 结构: { 'sid': [{'role': 'system', 'content': '...'}, ...], ... }
tongyi_conversations = {}

# 初始化通义千问API客户端
try:
    tongyi_client = OpenAI(
        api_key="sk-dcbb7246ac3f402994454b91120b95ab",
        base_url="https://dashscope.aliyuncs.com/compatible-mode/v1",
    )
    print("通义千问客户端初始化成功。")
except Exception as e:
    print(f"错误: 初始化通义千问客户端失败 - {e}")
    tongyi_client = None


def init_chat_events(sio, users_online_dict, users_db_dict):
    # global socketio_instance, online_users_ref, mock_users_db_ref
    # socketio_instance = sio
    # online_users_ref = users_online_dict
    # mock_users_db_ref = users_db_dict

    # 使用传入的引用
    online_users = users_online_dict
    mock_users_db = users_db_dict


    print("Initializing chat events...")

    @sio.on('connect')
    def handle_connect(auth_data=None):
        # auth_data 是可选的，客户端可以在连接时发送认证信息
        # Qt 客户端的 QWebSocket.open() 可以传递 headers，但 SocketIO Python 客户端通常在 connect 事件后发送认证
        sid = request.sid
        print(f"Client connected: {sid}")  # 这是您已有的打印语句
        # 为新连接的客户端初始化通义千问的对话历史
        tongyi_conversations[sid] = [
            {
                "role": "system",
                "content": "你是一个乐于助人的人工智能助手。",  # 您可以自定义系统提示
            }
        ]
        print(f"为 SID {sid} 初始化了通义千问的对话历史。")

    @sio.on('disconnect')
    def handle_disconnect():
        sid = request.sid
        username = sid_to_user.pop(sid, None) # 从 sid_to_user 移除
        if username and username in online_users:
            del online_users[username] # 从 online_users 移除
            print(f"User {username} (SID: {sid}) disconnected.")
            # 广播用户列表更新
            broadcast_user_list(sio, online_users)
        else:
            print(f"Client disconnected: {sid} (was not logged in or already removed)")
        # 清理该客户端的对话历史
        if sid in tongyi_conversations:
            del tongyi_conversations[sid]
            print(f"清理了 SID {sid} 的通义千问对话历史。")


    @sio.on('login')
    def handle_login(data):
        """
        处理登录请求
        期望数据: {'username': 'user', 'password': 'pass'}
        """
        print(f"SERVER: handle_login CALLED! Data: {data}, SID: {request.sid}")

        sid = request.sid
        username = data.get('username')
        password = data.get('password')
        print(f"Login attempt from SID {sid}: User '{username}'")

        if not username or not password:
            emit('login_response', {'success': False, 'message': '用户名和密码不能为空'}, room=sid)
            return

        # 简单的基于 mock_users_db 的认证
        if username in mock_users_db and mock_users_db[username] == password:
            if username in online_users: # 如果用户已在线 (可能从其他地方登录)
                # emit('login_response', {'success': False, 'message': '用户已登录'}, room=sid)
                # 或者允许重复登录，踢掉旧的会话 (更复杂)
                # 这里我们简单地允许，但要注意 SID 管理
                print(f"User {username} is already marked as online. Re-associating SID.")
                # 如果之前的 SID 存在，需要清理
                old_sid = online_users.get(username)
                if old_sid and old_sid != sid:
                    # 从 sid_to_user 移除旧的 SID 映射
                    if old_sid in sid_to_user:
                        del sid_to_user[old_sid]
                    # 清理旧SID的对话历史
                    if old_sid in tongyi_conversations:
                        del tongyi_conversations[old_sid]
                        print(f"清理了旧 SID {old_sid} 的通义千问对话历史。")
                    # 可以选择性地通知旧的 SID 断开连接
                    # sio.disconnect(old_sid) # 这会触发旧 SID 的 disconnect 事件

            online_users[username] = sid
            sid_to_user[sid] = username
            emit('login_response', {
                'success': True,
                'username': username,
                'message': '登录成功',
                # 'token': token # 如果您在客户端处理 token
            }, room=sid)
            print(f"User {username} logged in successfully with SID {sid}.")
            # 广播用户列表更新
            broadcast_user_list(sio, online_users)
        else:
            emit('login_response', {'success': False, 'message': '用户名或密码错误'}, room=sid)
            print(f"Login failed for user '{username}'.")

    @sio.on('message')
    def handle_message(data):
        """
        处理收到的消息
        期望数据: {'to': 'user2/all', 'content': 'Hello', 'type': 'text'}
        """
        sid = request.sid
        sender_username = sid_to_user.get(sid)

        if not sender_username:
            emit('server_notification', {'message': '错误：您尚未登录，无法发送消息。'}, room=sid)
            # disconnect(sid) # 可以选择断开未登录用户的连接
            return

        recipient = data.get('to')
        content = data.get('content')
        msg_type = data.get('type', 'text') # 默认为文本类型
        timestamp = int(time.time() * 1000) # 毫秒级时间戳

        if not recipient or not content:
            emit('server_notification', {'message': '错误：消息格式不正确（缺少接收者或内容）。'}, room=sid)
            return

        print(f"Message from '{sender_username}' (SID: {sid}) to '{recipient}': {content}")

        response_data = {
            'from': sender_username,
            'to': recipient, # 让客户端知道原始的 'to'
            'content': content,
            'type': msg_type,
            'timestamp': timestamp
        }

        if recipient == "all":
            # 广播给所有已连接（且已登录）的用户
            # 注意：sio.emit 会发给所有连接的客户端，包括发送者
            # 如果不想发给发送者，可以使用 broadcast=True, include_self=False (或 sio.send with skip_sid)
            # 或者，让客户端自己判断是否是自己的消息
            sio.emit('new_message', response_data) # room 参数不指定，则为广播
            print(f"Broadcasting message from {sender_username} to all.")
        else:
            # 私聊
            recipient_sid = online_users.get(recipient)
            if recipient_sid:
                # 发送给特定用户
                sio.emit('new_message', response_data, room=recipient_sid)
                print(f"Sending private message from {sender_username} to {recipient} (SID: {recipient_sid}).")
                # 也可以同时发一份给发送者，让其确认发送 (如果客户端不立即显示自己的消息)
                # sio.emit('new_message', response_data, room=sid)
            else:
                # 用户不在线或不存在
                emit('server_notification', {'message': f"错误：用户 '{recipient}' 不在线或不存在。"}, room=sid)
                print(f"User {recipient} not found or not online for PM from {sender_username}.")

    @sio.on('send_to_tongyi')
    def handle_send_to_tongyi(data):
        sid = request.sid
        user_username = sid_to_user.get(sid)

        if not user_username:
            emit('tongyi_message_response', {
                'success': False,
                'from': '通义千问',
                'content': '错误：您尚未登录，无法与AI对话。'
            }, room=sid)
            return

        user_input = data.get('content')
        if not user_input:
            emit('tongyi_message_response', {
                'success': False,
                'from': '通义千问',
                'content': '错误：发送的内容不能为空。'
            }, room=sid)
            return

        print(f"用户 '{user_username}' (SID: {sid}) 发送给通义千问: {user_input}")

        # 获取该用户的对话历史
        current_conversation = tongyi_conversations.get(sid)
        if not current_conversation:
            # 如果由于某种原因历史不存在，重新初始化
            current_conversation = [{"role": "system", "content": "你是一个乐于助人的AI助手。"}]
            tongyi_conversations[sid] = current_conversation
            print(f"警告: 为 SID {sid} 重新初始化了通义千问对话历史。")

        current_conversation.append({"role": "user", "content": user_input})

        # 调用通义千问API
        assistant_output = get_tongyi_response(current_conversation)

        # 将模型的回复添加到对话历史
        current_conversation.append({"role": "assistant", "content": assistant_output})

        # (可选) 限制对话历史长度，防止无限增长
        MAX_HISTORY_LEN = 20  # 系统消息 + 19轮对话
        if len(current_conversation) > MAX_HISTORY_LEN + 1:
            tongyi_conversations[sid] = [current_conversation[0]] + current_conversation[-(MAX_HISTORY_LEN):]

        # 打印到控制台时处理编码错误
        try:
            print(f"通义千问回复给 '{user_username}' (SID: {sid}): {assistant_output}")
        except UnicodeEncodeError:
            encoded_output = assistant_output.encode(sys.stdout.encoding or 'utf-8', errors='replace').decode(
                sys.stdout.encoding or 'utf-8')
            print(f"通义千问回复给 '{user_username}' (SID: {sid}) (部分字符被替换): {encoded_output}")

        # 将回复发送回客户端
        emit('tongyi_message_response', {
            'success': True,
            'from': '通义千问',
            'content': assistant_output,
            'timestamp': int(time.time() * 1000)
        }, room=sid)

    @sio.on('file_message')
    def handle_file_message(data):
        """
        处理文件消息转发
        期望数据: {'to': 'target_user', 'file_name': 'filename', 'file_id': 'id', 'file_size': size}
        """
        sid = request.sid
        sender_username = sid_to_user.get(sid)
        if not sender_username:
            emit('server_notification', {'message': '错误：您尚未登录，无法发送文件消息。'}, room=sid)
            return

        target_username = data.get('to')
        file_name = data.get('file_name')
        file_id = data.get('file_id')
        file_size = data.get('file_size')
        
        if not all([target_username, file_name, file_id]):
            emit('server_notification', {'message': '错误：文件消息数据不完整。'}, room=sid)
            return

        # 检查目标用户是否在线
        target_sid = online_users.get(target_username)
        if not target_sid:
            emit('server_notification', {'message': f'错误：用户 {target_username} 不在线。'}, room=sid)
            return

        # 转发文件消息给目标用户
        file_message_data = {
            'from': sender_username,
            'to': target_username,
            'file_name': file_name,
            'file_id': file_id,
            'file_size': file_size,
            'timestamp': int(time.time() * 1000)
        }
        
        emit('file_message', file_message_data, room=target_sid)
        print(f"File message forwarded from {sender_username} to {target_username}: {file_name} (ID: {file_id})")
        
        # 向发送者确认消息已转发
        emit('server_notification', {'message': f'文件消息已发送给 {target_username}'}, room=sid)

    @sio.on('get_user_list')
    def handle_get_user_list(data=None): # data 参数是可选的
        sid = request.sid
        sender_username = sid_to_user.get(sid)
        if not sender_username:
            emit('server_notification', {'message': '错误：您尚未登录，无法获取用户列表。'}, room=sid)
            return

        users_list_payload = []
        for user, user_sid in online_users.items():
            users_list_payload.append({'username': user, 'status': 'online'}) # 简单状态

        emit('user_list_update', {'users': users_list_payload}, room=sid)
        print(f"Sent user list to {sender_username} (SID: {sid}).")


def broadcast_user_list(sio_instance, online_users_dict):
    """辅助函数，向所有在线用户广播当前用户列表"""
    users_list_payload = []
    for user, sid in online_users_dict.items():
        users_list_payload.append({'username': user, 'status': 'online'})

    # 广播给所有已登录的用户
    # 我们不能直接 room=online_users_dict.values() 因为这会尝试发送给 SID 列表
    # 而是应该迭代并发送，或者让客户端在连接后请求一次，并在有变化时广播
    sio_instance.emit('user_list_update', {'users': users_list_payload}) # 广播给所有人
    print("Broadcasted user list update to all clients.")

def get_tongyi_response(messages_history, model="qwen-turbo-latest"):
    """调用通义千问API并返回回复内容。"""
    if not tongyi_client:
        return "抱歉，AI助手服务当前不可用（客户端未初始化）。"
    try:
        completion = tongyi_client.chat.completions.create(
            model=model,
            messages=messages_history,
            stream=False # 确保使用非流式响应
        )
        return completion.choices[0].message.content
    except Exception as e:
        print(f"错误: 调用通义千问API时发生错误: {e}")
        # 根据错误类型返回更友好的提示
        error_str = str(e).lower()
        if "invalid api key" in error_str or "authentication" in error_str:
            return "抱歉，AI服务认证失败，请检查API Key配置。"
        elif "balance" in error_str or "quota" in error_str:
            return "抱歉，您的AI服务账户余额不足或已超出配额。"
        return f"抱歉，与AI助手交互时发生了一个错误。"