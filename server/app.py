import os
import time
import logging
import threading
from datetime import timedelta
from flask import Flask, render_template, request, jsonify

# 프로젝트 필수 모듈 임포트
try:
    from config import PORT
    from database import db_manager
    from ai_module.strawberry_analyzer import analyze_ripeness, analyze_flowers
except ImportError as e:
    logging.error(f"필수 모듈 로딩 실패: {e}. 'config.py', 'database/db_manager.py', 'ai_module' 폴더가 올바르게 있는지 확인해주세요.")
    exit()

# DB 정리 유틸리티
try:
    from init_db import clean_old_records
except ImportError:
    clean_old_records = None # 파일이 없어도 서버는 실행됨

# 기본 설정 및 절대 경로 구성
logging.basicConfig(level=logging.INFO, format='%(asctime)s - %(levelname)s - %(message)s')
app = Flask(__name__)

# 경로 설정 — 절대경로 일관 적용
BASE_DIR = os.path.dirname(os.path.abspath(__file__))

# 절대경로 변환 헬퍼
def ensure_abs(path: str):
    return path if os.path.isabs(path) else os.path.abspath(os.path.join(BASE_DIR, path))

# 업로드 폴더 준비
UPLOAD_FOLDER = ensure_abs("temp_images")
os.makedirs(UPLOAD_FOLDER, exist_ok=True)
app.config['UPLOAD_FOLDER'] = UPLOAD_FOLDER
logging.info(f"업로드 폴더(절대경로)가 '{app.config['UPLOAD_FOLDER']}'로 설정되었습니다.")


# 백그라운드 작업
def _cleanup_every_30_days():
    """30일 주기로 오래된 DB 레코드를 정리합니다."""
    if not clean_old_records:
        logging.info("[scheduler] 'clean_old_records' 함수가 없어 DB 정리 작업을 건너뜁니다.")
        return
    
    # 서버 시작 후 바로 실행하지 않고, 30일을 기다린 후 첫 실행
    wait_seconds = int(timedelta(days=30).total_seconds())
    while True:
        time.sleep(wait_seconds)
        try:
            logging.info("[scheduler] 30일이 경과하여 오래된 DB 레코드 정리를 시작합니다.")
            clean_old_records()
        except Exception as e:
            logging.error(f"[scheduler] 주기적 DB 정리 작업 중 오류 발생: {e}")


# 스레드 기동
threading.Thread(target=_cleanup_every_30_days, daemon=True).start()
logging.info("백그라운드 스케줄러(DB 정리, 자동 촬영) 스레드가 시작되었습니다.")



# ANDROID ROUTER (Blueprint) - ADD START
from flask import Blueprint

android_bp = Blueprint("android", __name__, url_prefix="/android")

@android_bp.get("/ping")
def android_ping():
    try:
        return jsonify({"status": "ok"}), 200
    except Exception as e:
        logging.error(f"[android/ping] error: {e}")
        return jsonify({"error": str(e)}), 500


@android_bp.get("/sensor/latest")
def android_sensor_latest():
    """
    최신 센서 데이터 1건을 JSON으로 반환
    - 내부적으로 기존 함수: db_manager.get_latest_sensor_data() 사용
    - 응답 형식: 앱에서 바로 파싱하기 쉬운 flat JSON
    """
    try:
        latest = db_manager.get_latest_sensor_data()
        if not latest:
            return jsonify({"message": "No data"}), 404
        return jsonify(dict(latest)), 200
    except Exception as e:
        logging.error(f"[android/sensor/latest] DB error: {e}")
        return jsonify({"error": str(e)}), 500


@android_bp.post("/sensor")
def android_sensor_post():
    """
    센서 데이터 업로드(ESP32/앱 공용)
    - JSON body 예:
      {
        "soil_moisture": 42.5,
        "air_temperature": 25.3,
        "air_humidity": 61.2,
        "light_intensity": 300.0,
        "water_level": 0.8
      }
    - 내부적으로 기존 함수: db_manager.save_sensor_data(...) 사용
    """
    try:
        data = request.get_json(silent=True) or {}
        required = ["soil_moisture", "air_temperature", "air_humidity", "light_intensity", "water_level"]
        missing = [k for k in required if k not in data]
        if missing:
            return jsonify({"error": "missing fields", "required": required, "missing": missing}), 400

        db_manager.save_sensor_data(
            soil_moisture=data.get("soil_moisture"),
            air_temperature=data.get("air_temperature"),
            air_humidity=data.get("air_humidity"),
            light_intensity=data.get("light_intensity"),
            water_level=data.get("water_level"),
        )
        return jsonify({"status": "ok"}), 200
    except Exception as e:
        logging.error(f"[android/sensor] save error: {e}")
        return jsonify({"error": str(e)}), 500


@android_bp.post("/ai")
def android_ai_analyze():
    """
    이미지 분석 업로드 (Multipart)
    - field name: 'image'
    - 내부적으로 기존 analyze_ripeness / analyze_flowers 호출 후
      db_manager.save_image_analysis_result(...)로 통합 저장
    - 업로드 파일은 기존 app.config['UPLOAD_FOLDER'](절대경로)에 임시 저장 후 삭제
    """
    try:
        f = request.files.get("image")
        if not f:
            return jsonify({"error": "file 'image' required"}), 400

        # 임시 저장 경로
        ts = int(time.time())
        temp_path = os.path.join(app.config['UPLOAD_FOLDER'], f"android_{ts}.jpg")
        f.save(temp_path)
        logging.info(f"[android/ai] image saved: {temp_path}")

        # AI 분석
        ripeness_score, ripeness_text = analyze_ripeness(temp_path)
        flower_count, flower_status = analyze_flowers(temp_path)
        logging.info(f"[android/ai] ripeness={ripeness_text}({ripeness_score}), flowers={flower_count}")

        # 통합 저장
        db_manager.save_image_analysis_result(
            file_path=temp_path,
            ripeness_score=ripeness_score,
            ripeness_text=ripeness_text,
            flower_count=flower_count,
            flower_text=flower_status
        )

        return jsonify({
            "status": "analyzed",
            "ripeness_text": ripeness_text,
            "ripeness_score": ripeness_score,
            "flower_count": flower_count,
            "flower_status": flower_status
        }), 200

    except Exception as e:
        logging.error(f"[android/ai] analyze error: {e}")
        return jsonify({"error": "Image processing failed", "detail": str(e)}), 500
    finally:
        # 임시 파일 제거
        try:
            if 'temp_path' in locals() and os.path.exists(temp_path):
                os.remove(temp_path)
                logging.info(f"[android/ai] temp removed: {temp_path}")
        except Exception as fe:
            logging.warning(f"[android/ai] temp remove failed: {fe}")



# 웹 페이지 및 API 라우트
@app.route('/')
def index():
    try:
        sensor_data_rows = db_manager.get_all_sensor_data()
        return render_template('index.html', sensor_data=sensor_data_rows)
    except Exception as e:
        logging.error(f"Index 페이지 로딩 오류: {e}")
        return "데이터베이스 조회에 실패했습니다.", 500

@app.route('/api/latest_data', methods=['GET'])
def get_latest_data():
    try:
        latest_data = db_manager.get_latest_sensor_data()
        if latest_data:
            return jsonify(dict(latest_data)), 200
        else:
            return jsonify({'message': 'No data available'}), 404
    except Exception as e:
        logging.error(f"최신 데이터 API 오류: {e}")
        return jsonify({'error': str(e)}), 500
    
    
# 안드로이드 호환 엔드포인트
@app.route('/api/latest_sensor_data', methods=['GET'])
def get_latest_sensor_data_for_android():
    try:
        latest_data = db_manager.get_latest_sensor_data()
        if latest_data:
            return jsonify(dict(latest_data)), 200
        else:
            return jsonify({'message': 'No data available'}), 404
    except Exception as e:
        logging.error(f"latest_sensor_data API 오류: {e}")
        return jsonify({'error': str(e)}), 500


# ESP32 통신 라우트
@app.route('/sensor', methods=['POST'])
def receive_sensor_data():
    try:
        data = request.json
        logging.info(f"센서 데이터 수신: {data}")
        db_manager.save_sensor_data(
            soil_moisture=data.get('soil_moisture'),
            air_temperature=data.get('air_temperature'),
            air_humidity=data.get('air_humidity'),
            light_intensity=data.get('light_intensity'),
            water_level=data.get('water_level')
        )
        return jsonify({'status': 'Sensor data saved'}), 200
    except Exception as e:
        logging.error(f"센서 데이터 처리 오류: {e}")
        return jsonify({'error': str(e)}), 500

@app.route('/camera/callback', methods=['POST'])
def camera_callback():
    """이미지 수신, AI 분석, DB에 '기록'을 한 번에 처리합니다."""
    image_data = request.data
    if not image_data:
        return jsonify({'error': 'No image data received'}), 400

    temp_filepath = os.path.join(app.config['UPLOAD_FOLDER'], f"temp_{int(time.time())}.jpg")
    
    try:
        with open(temp_filepath, 'wb') as f:
            f.write(image_data)
        logging.info(f"이미지 임시 저장: {temp_filepath}")

        # AI 분석 모듈 호출
        ripeness_score, ripeness_text = analyze_ripeness(temp_filepath)
        flower_count, flower_status = analyze_flowers(temp_filepath)
        logging.info(f"AI 분석 결과: 딸기='{ripeness_text}'({ripeness_score}), 꽃={flower_count}개")

        db_manager.save_image_analysis_result(
            file_path=temp_filepath,
            ripeness_score=ripeness_score,
            ripeness_text=ripeness_text,
            flower_count=flower_count,
            flower_text=flower_status
        )
        logging.info("DB에 새로운 이미지 및 AI 분석 결과 기록 완료")

        
        return jsonify({'status': 'Image analyzed and result saved'}), 200
    except Exception as e:
        logging.error(f"이미지 처리 및 AI 분석 오류: {e}")
        return jsonify({'error': 'Image processing failed'}), 500
    finally:
        if os.path.exists(temp_filepath):
            os.remove(temp_filepath)
            logging.info(f"임시 이미지 삭제: {temp_filepath}")
            


@app.route('/analysis')
def analysis_page():
    """(새로 추가) AI 분석 결과 확인 페이지"""
    try:
        analysis_data = db_manager.get_all_analysis_data()
        return render_template('analysis.html', analysis_data=analysis_data)
    except Exception as e:
        logging.error(f"Analysis 페이지 로딩 오류: {e}")
        return "AI 분석 기록 조회에 실패했습니다.", 500
    
# 앱 실행
if __name__ == '__main__':
    app.register_blueprint(android_bp)
    app.run(host='0.0.0.0', port=PORT, debug=True)
