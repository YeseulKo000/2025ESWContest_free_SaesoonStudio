# yolov8n 모델 불러옴
from ultralytics import YOLO
model = YOLO("yolov8n.pt")


# roboflow에서 다운로드 받은 데이터셋 학습
# data.yaml 파일은 <0 / 1>로 나뉨 (익음 / 익지 않음)
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
    source="/content/image1.jpeg",
    conf=0.25,
    save=True    # 결과 이미지 저장
)

r = results[0] if isinstance(results, list) else results

# 0: 'RIPE', 1: 'UNRIPE'
cls_list = r.boxes.cls.int().tolist() if r.boxes.cls is not None else []
if 0 in cls_list:
    print("익음")
else:
    print("덜익음")
