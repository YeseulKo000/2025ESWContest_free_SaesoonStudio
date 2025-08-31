# yolov8n 모델 불러옴
from ultralytics import YOLO
model = YOLO("yolov8n.pt")


# roboflow에서 다운로드 받은 데이터셋 학습
# data.yaml 파일은 <0 / 1>로 나뉨 (꽃 탐지X / 탐지O)
results = model.train(
    data="/content/dataset/data.yaml",
    epochs=50,
    imgsz=640,
    batch=8
)


# 학습 끝난 모델 불러옴
model = YOLO('runs/detect/train/weights/best.pt')

# 이미지 예측
results = model.predict(
    source="/content/dataset/test/images/20221025_121026_jpg.rf.adb8469895cd017bf0e3a26ad499023d.jpg",
    conf=0.25,
    save=True    # 결과 이미지 저장
)

flower_detected = False

for r in results:
  if len(r.boxes.cls) > 0:    # 결과 박스 인덱스 존재하면 꽃 탐지O
    flower_detected = True
    break

if flower_detected:
  print("꽃 있음")
else:
  print("꽃 없음")
