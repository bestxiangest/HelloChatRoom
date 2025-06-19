"""
文件传输功能模块
用于HelloChatRoom的文件传输功能
"""

import os
import json
import uuid
from flask import request, jsonify, send_file
from werkzeug.utils import secure_filename
import logging

# 配置
UPLOAD_FOLDER = os.path.join(os.path.dirname(os.path.dirname(__file__)), 'uploads')
MAX_FILE_SIZE = 50 * 1024 * 1024  # 50MB
ALLOWED_EXTENSIONS = {'txt', 'pdf', 'png', 'jpg', 'jpeg', 'gif', 'doc', 'docx', 'zip', 'rar'}

# 确保上传目录存在
os.makedirs(UPLOAD_FOLDER, exist_ok=True)

# 文件存储字典 {file_id: {filename, original_name, size, uploader, receiver}}
file_storage = {}

# 配置日志
logger = logging.getLogger(__name__)

def allowed_file(filename):
    return '.' in filename and \
           filename.rsplit('.', 1)[1].lower() in ALLOWED_EXTENSIONS

def upload_file():
    """上传文件"""
    try:
        if 'file' not in request.files:
            return jsonify({'success': False, 'error': '没有文件'}), 400
        
        file = request.files['file']
        if file.filename == '':
            return jsonify({'success': False, 'error': '没有选择文件'}), 400
        
        # 获取其他参数
        file_id = request.form.get('fileId')
        uploader = request.form.get('uploader', 'unknown')
        receiver = request.form.get('receiver', 'unknown')
        
        if not file_id:
            file_id = str(uuid.uuid4())
        
        if file and allowed_file(file.filename):
            # 安全的文件名
            original_filename = secure_filename(file.filename)
            # 使用file_id作为存储文件名，保留扩展名
            file_extension = os.path.splitext(original_filename)[1]
            stored_filename = f"{file_id}{file_extension}"
            file_path = os.path.join(UPLOAD_FOLDER, stored_filename)
            
            # 保存文件
            file.save(file_path)
            file_size = os.path.getsize(file_path)
            
            # 存储文件信息
            file_storage[file_id] = {
                'filename': stored_filename,
                'original_name': original_filename,
                'size': file_size,
                'uploader': uploader,
                'receiver': receiver,
                'path': file_path
            }
            
            logger.info(f"文件上传成功: {original_filename} -> {file_id}")
            
            return jsonify({
                'success': True,
                'fileId': file_id,
                'filename': original_filename,
                'size': file_size
            })
        else:
            return jsonify({'success': False, 'error': '不支持的文件类型'}), 400
            
    except Exception as e:
        logger.error(f"上传文件时出错: {str(e)}")
        return jsonify({'success': False, 'error': str(e)}), 500

def download_file(file_id):
    """下载文件"""
    try:
        if file_id not in file_storage:
            return jsonify({'success': False, 'error': '文件不存在'}), 404
        
        file_info = file_storage[file_id]
        file_path = file_info['path']
        
        if not os.path.exists(file_path):
            return jsonify({'success': False, 'error': '文件已被删除'}), 404
        
        logger.info(f"下载文件: {file_info['original_name']} ({file_id})")
        
        return send_file(
            file_path,
            as_attachment=True,
            download_name=file_info['original_name']
        )
        
    except Exception as e:
        logger.error(f"下载文件时出错: {str(e)}")
        return jsonify({'success': False, 'error': str(e)}), 500

def list_files():
    """获取文件列表"""
    try:
        files = []
        for file_id, info in file_storage.items():
            files.append({
                'fileId': file_id,
                'filename': info['original_name'],
                'size': info['size'],
                'uploader': info['uploader'],
                'receiver': info['receiver']
            })
        
        return jsonify({'success': True, 'files': files})
        
    except Exception as e:
        logger.error(f"获取文件列表时出错: {str(e)}")
        return jsonify({'success': False, 'error': str(e)}), 500

def get_file_info(file_id):
    """获取文件信息"""
    try:
        if file_id not in file_storage:
            return jsonify({'success': False, 'error': '文件不存在'}), 404
        
        info = file_storage[file_id]
        return jsonify({
            'success': True,
            'fileId': file_id,
            'filename': info['original_name'],
            'size': info['size'],
            'uploader': info['uploader'],
            'receiver': info['receiver']
        })
        
    except Exception as e:
        logger.error(f"获取文件信息时出错: {str(e)}")
        return jsonify({'success': False, 'error': str(e)}), 500

def health_check():
    """健康检查"""
    return jsonify({'status': 'ok', 'message': '文件传输服务器运行正常'})

def register_file_routes(app):
    """注册文件传输路由"""
    # 配置文件上传相关设置
    app.config['UPLOAD_FOLDER'] = UPLOAD_FOLDER
    app.config['MAX_CONTENT_LENGTH'] = MAX_FILE_SIZE
    
    # 注册路由
    app.add_url_rule('/api/upload', 'upload_file', upload_file, methods=['POST'])
    app.add_url_rule('/api/download/<file_id>', 'download_file', download_file, methods=['GET'])
    app.add_url_rule('/api/files', 'list_files', list_files, methods=['GET'])
    app.add_url_rule('/api/file/<file_id>/info', 'get_file_info', get_file_info, methods=['GET'])
    app.add_url_rule('/health', 'health_check', health_check, methods=['GET'])
    
    logger.info("文件传输路由已注册")
    logger.info(f"上传目录: {os.path.abspath(UPLOAD_FOLDER)}")
    logger.info(f"最大文件大小: {MAX_FILE_SIZE / (1024*1024):.1f}MB")
    logger.info("支持的文件类型: " + ', '.join(ALLOWED_EXTENSIONS))