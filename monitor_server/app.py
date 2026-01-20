import os
from datetime import datetime
from flask import Flask, request, jsonify, send_from_directory, render_template
from flask_sqlalchemy import SQLAlchemy
from flask_cors import CORS
from celery import Celery
import ollama

app = Flask(__name__)
CORS(app) 

BASE_DIR = os.path.abspath(os.path.dirname(__file__))
UPLOAD_FOLDER = os.path.join(BASE_DIR, 'storage')
app.config['SQLALCHEMY_DATABASE_URI'] = 'sqlite:///' + os.path.join(BASE_DIR, 'monitoring.db')
app.config['UPLOAD_FOLDER'] = UPLOAD_FOLDER
app.config['SQLALCHEMY_TRACK_MODIFICATIONS'] = False

db = SQLAlchemy(app)

CELERY_BROKER_URL = 'redis://localhost:6379/0'
celery_app = Celery(app.name, broker=CELERY_BROKER_URL)
celery_app.conf.update(app.config)

class ActivityLog(db.Model):
    id = db.Column(db.Integer, primary_key=True)
    username = db.Column(db.String(80))
    timestamp = db.Column(db.DateTime, default=datetime.now)
    image_path = db.Column(db.String(255))
    ai_analysis = db.Column(db.Text, default="В очереди на анализ...")

    def to_dict(self):
        return {
            "id": self.id,
            "username": self.username,
            "timestamp": self.timestamp.strftime('%Y-%m-%d %H:%M:%S'),
            "image_url": f"/storage/{self.username}/{os.path.basename(self.image_path)}",
            "ai_analysis": self.ai_analysis
        }

if not os.path.exists(UPLOAD_FOLDER): os.makedirs(UPLOAD_FOLDER)
with app.app_context(): db.create_all()


@app.route('/api/users')
def get_users():
    users = db.session.query(ActivityLog.username).distinct().all()
    return jsonify([u[0] for u in users])


@app.route('/admin')
def admin_page():
    return render_template('admin.html')

@app.route('/api/admin/overview')
def admin_overview():
    logs = ActivityLog.query.order_by(ActivityLog.timestamp.desc()).limit(40).all()
    return jsonify([log.to_dict() for log in logs])




@celery_app.task
def analyze_screenshot_task(log_id, filepath):
    with app.app_context():
        log = db.session.get(ActivityLog, log_id)
        if not log: return
        try:
            prompt = (
                "Analyze this desktop screenshot. "
    "Identify the main activity. "
    "Output instructions: "
    "1. You must respond in RUSSIAN. "
    "2. Choose one category: [Работа], [Соцсети], [Развлечения], [Обучение]. "
    "3. Write a one-sentence description in Russian. "
    "4. STRICT FORMAT: [Category] Description. "
    "Example: [Работа] Пользователь пишет код в редакторе VS Code."
            )
            
            res = ollama.chat(model='llava', messages=[{
                'role': 'user',
                'content': prompt,
                'images': [filepath]
            }])
            log.ai_analysis = res['message']['content']
        except Exception as e:
            log.ai_analysis = f"Ошибка анализа: {str(e)}"
        db.session.commit()




@app.route('/upload', methods=['POST'])
def upload():
    file = request.files.get('file')
    user = request.form.get('user', 'unknown')
    if not file: return jsonify({"error": "No file"}), 400
    user_dir = os.path.join(app.config['UPLOAD_FOLDER'], user)
    os.makedirs(user_dir, exist_ok=True)
    filename = f"{datetime.now().strftime('%H%M%S')}.png"
    filepath = os.path.join(user_dir, filename)
    file.save(filepath)
    
    new_log = ActivityLog(username=user, image_path=filepath)
    db.session.add(new_log)
    db.session.commit()
    
    analyze_screenshot_task.delay(new_log.id, filepath) 
    
    return jsonify({"status": "ok", "id": new_log.id}), 200

@app.route('/api/logs/<username>', methods=['GET'])
def get_logs(username):
    logs = ActivityLog.query.filter_by(username=username).order_by(ActivityLog.timestamp.desc()).all()
    return jsonify([log.to_dict() for log in logs])

@app.route('/storage/<path:filename>')
def serve_image(filename):
    return send_from_directory(app.config['UPLOAD_FOLDER'], filename)

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