from flask import Flask
from flask_socketio import SocketIO
from flask_cors import CORS
import os

socketio = SocketIO()

# --- 用户和会话存储 (来自您项目现有的上下文) ---
online_users = {}
mock_users_db = {
    "admin": "1",
    "alice": "1",
    "bob": "1"
}


# ----------------------------------------------------

def create_app(debug=False):
    """应用工厂函数"""
    app = Flask(__name__)
    app.debug = debug
    app.config['SECRET_KEY'] = os.environ.get('SECRET_KEY', 'a_very_secret_and_random_key_that_you_should_change')

    CORS(app, resources={r"/*": {"origins": "*"}})

    # 注册现有的 SocketIO 事件处理器
    from .sockets import chat as chat_sockets
    chat_sockets.init_chat_events(socketio, online_users, mock_users_db)

    # 注册文件传输功能
    from .file_transfer import register_file_routes
    register_file_routes(app)

    socketio.init_app(app, cors_allowed_origins="*")

    @app.route('/')
    def index():
        # 一个简单的响应来检查服务器是否正在运行
        return "<h1>聊天服务器正在运行！(包含文件传输接口)</h1>"

    return app
