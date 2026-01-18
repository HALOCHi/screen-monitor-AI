import os
import threading
from datetime import datetime
from flask import Flask, request, jsonify, send_from_directory
from flask_sqlalchemy import SQLAlchemy
import ollama

app = Flask(__name__)

# Настройки папок и БД
BASE_DIR = os.path.abspath(os.path.dirname(__file__))
UPLOAD_FOLDER = os.path.join(BASE_DIR, 'storage')
app.config['SQLALCHEMY_DATABASE_URI'] = 'sqlite:///' + os.path.join(BASE_DIR, 'monitoring.db')
app.config['UPLOAD_FOLDER'] = UPLOAD_FOLDER

db = SQLAlchemy(app)

# Модель данных
class ActivityLog(db.Model):
    id = db.Column(db.Integer, primary_key=True)
    username = db.Column(db.String(80))
    timestamp = db.Column(db.DateTime, default=datetime.now)
    image_path = db.Column(db.String(255))
    ai_analysis = db.Column(db.Text, default="В очереди на анализ...")

# Создание БД и папок
if not os.path.exists(UPLOAD_FOLDER): os.makedirs(UPLOAD_FOLDER)
with app.app_context(): db.create_all()

# Функция анализа (запускается в фоне, чтобы сервер не тормозил)
def analyze_screenshot(log_id, filepath):
    with app.app_context():
        log = ActivityLog.query.get(log_id)
        try:
            # Запрос к нейросети
            res = ollama.chat(model='llava', messages=[{
                'role': 'user',
                'content': 'Что делает человек на скриншоте? Опиши кратко программы и сайты. Пиши на русском.',
                'images': [filepath]
            }])
            log.ai_analysis = res['message']['content']
        except Exception as e:
            log.ai_analysis = f"Ошибка ИИ: {str(e)}"
        db.session.commit()

@app.route('/upload', methods=['POST'])
def upload():
    file = request.files.get('file')
    user = request.form.get('user', 'unknown')
    
    if not file: return "No file", 400

    user_dir = os.path.join(app.config['UPLOAD_FOLDER'], user)
    os.makedirs(user_dir, exist_ok=True)
    
    filename = f"{datetime.now().strftime('%H%M%S')}.png"
    filepath = os.path.join(user_dir, filename)
    file.save(filepath)
    
    # Пишем в БД
    new_log = ActivityLog(username=user, image_path=filepath)
    db.session.add(new_log)
    db.session.commit()
    
    # Запускаем ИИ в отдельном потоке
    threading.Thread(target=analyze_screenshot, args=(new_log.id, filepath)).start()
    
    return jsonify({"status": "ok"}), 200

# Раздача картинок для сайта
@app.route('/storage/<path:filename>')
def serve_image(filename):
    return send_from_directory(app.config['UPLOAD_FOLDER'], filename)

# Красивый просмотрщик
@app.route('/dashboard/<username>')
def dashboard(username):
    logs = ActivityLog.query.filter_by(username=username).order_by(ActivityLog.timestamp.desc()).all()
    html = f"<h1>Логи: {username}</h1><table border='1' style='width:100%; border-collapse:collapse;'>"
    html += "<tr><th>Время</th><th>Что делает (ИИ)</th><th>Скриншот</th></tr>"
    for log in logs:
        img_url = f"/storage/{username}/{os.path.basename(log.image_path)}"
        html += f"<tr><td style='padding:10px'>{log.timestamp.strftime('%H:%M:%S')}</td>"
        html += f"<td style='padding:10px'>{log.ai_analysis}</td>"
        html += f"<td><img src='{img_url}' width='300'></td></tr>"
    return html + "</table>"

if __name__ == '__main__':
    app.run(host='0.0.0.0', port=5000)
