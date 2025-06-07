from app import create_app, socketio
import os

# 从 .env 文件加载环境变量 (如果使用 python-dotenv)
# from dotenv import load_dotenv
# load_dotenv()

app = create_app(debug=True)

if __name__ == '__main__':
    # 获取端口号，默认为 5000
    port = int(os.environ.get("PORT", 5000))
    # 允许来自任何源的连接，这对于开发非常重要，因为 Qt 客户端可能从不同的 "源" 连接
    # 在生产环境中，您可能希望更严格地控制允许的源
    socketio.run(app, host='0.0.0.0', port=port, allow_unsafe_werkzeug=True)
    # allow_unsafe_werkzeug=True 是为了兼容新版 Werkzeug 和 Flask-SocketIO 的开发服务器
    # 在生产环境中，您应该使用更健壮的 WSGI 服务器，如 Gunicorn 或 uWSGI
