# config.py
import os

# 현재 파일의 절대 경로를 가져와 프로젝트의 루트 디렉터리로 설정
BASE_DIR = os.path.dirname(os.path.abspath(__file__))

# SQLite 데이터베이스 파일 경로 (절대 경로)
DB_PATH = os.path.join(BASE_DIR, 'database.db')

# 이미지 저장 폴더 경로 (절대 경로)
# Flask의 static 폴더 안에 uploads 폴더를 만들어 이미지를 저장
IMAGE_UPLOAD_FOLDER = os.path.join(BASE_DIR, 'static/uploads')

# 웹 서버 포트
PORT = 8080

# ESP32의 IP 주소 (스마트팜 ESP32의 실제 IP 주소로 변경 필요)
ESP32_IP = '192.168.4.1'


SOIL_OPT = 45.0
SOIL_HALF = 10.0
SOIL_MIN = 0.0
SOIL_MAX = 100.0

AIR_OPT = 60.0
AIR_HALF = 10.0
AIR_MIN = 0.0
AIR_MAX = 100.0

TEMP_OPT = 24.0
TEMP_HALF = 6.0
TEMP_MIN = 5.0
TEMP_MAX = 40.0

def _linear_peak_score(x, center, half_width, min_x, max_x):
    if x is None:
        return 0.0
    x = float(x)
    if x <= min_x or x >= max_x:
        return 0.0
    d = abs(x - center)
    if d >= half_width:
        return 0.0
    s = 100.0 * (1.0 - d / half_width)
    if s < 0.0:
        s = 0.0
    if s > 100.0:
        s = 100.0
    return round(s, 2)

def soil_score_from_raw(soil_pct):
    return _linear_peak_score(soil_pct, SOIL_OPT, SOIL_HALF, SOIL_MIN, SOIL_MAX)

def air_score_from_raw(air_pct):
    return _linear_peak_score(air_pct, AIR_OPT, AIR_HALF, AIR_MIN, AIR_MAX)

def temp_score_from_raw(temp_c):
    return _linear_peak_score(temp_c, TEMP_OPT, TEMP_HALF, TEMP_MIN, TEMP_MAX)

def composite_from_raw(soil_pct, air_pct, temp_c):
    s1 = soil_score_from_raw(soil_pct)
    s2 = air_score_from_raw(air_pct)
    s3 = temp_score_from_raw(temp_c)
    return round((s1 + s2 + s3) / 3.0, 2)

COMPOSITE_GOOD_THRESHOLD = 80
COMPOSITE_WARN_THRESHOLD = 50
