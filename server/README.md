# Smartfarm 2025

ESP32 + Raspberry Pi 기반 스마트팜 시스템  
Flask 서버 + SQLite DB + YOLOv8 인공지능 연동

# 주요 기능
- 센서 데이터 수집
- 이미지 업로드 및 분석
- 장치 제어 (펌프, 팬, LED 등)
- 웹 대시보드 및 안드로이드 앱 연동

# 명령어
// 필요한 패키지 다운 (터미널에서 아래의 명령어 실행)  
pip install -r requirements.txt

// 필요한 패키지 업데이트 (마찬가지로 터미널에서)  
pip freeze > requirements.txt

## wheelhouse_requirements.txt 갱신 방법

wheelhouse/ 폴더의 .whl 파일들을 읽어 `wheelhouse_requirements.txt`를 자동 생성/갱신한다.
기존 내용을 통째로 덮어쓴다. 한번에 복사 후 붙여넣는다.

# Windows (Bash)

for f in wheelhouse/*.whl; do  
>base="$(basename "$f")"  
>name="${base%%-*}"  
>rest="${base#*-}"  
>ver="${rest%%-*}"  
>echo "${name//_/-}==${ver}  # wheel: ${base}"  
done | sort > wheelhouse_requirements.txt
