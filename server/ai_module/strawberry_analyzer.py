# ai_module/strawberry_analyzer.py

import os
from config import BASE_DIR
from ultralytics import YOLO
import logging


# 1. 두 개의 모델 경로를 각각 지정
RIPE_MODEL_PATH = os.path.join(BASE_DIR, 'ai_module', 'weights', 'ripe.pt')
FLOWER_MODEL_PATH = os.path.join(BASE_DIR, 'ai_module', 'weights', 'flower.pt')


# 2. 두 모델을 각각 로드
try:
    ripe_model = YOLO(RIPE_MODEL_PATH)
    logging.info(f"YOLOv8 ripe model loaded successfully from {RIPE_MODEL_PATH}")
except Exception as e:
    logging.error(f"Error loading ripe model: {e}")
    ripe_model = None

try:
    flower_model = YOLO(FLOWER_MODEL_PATH)
    logging.info(f"YOLOv8 flower model loaded successfully from {FLOWER_MODEL_PATH}")
except Exception as e:
    logging.error(f"Error loading flower model: {e}")
    flower_model = None


# 3. 딸기 ripeness 분석 함수
def analyze_ripeness(image_path):
    """
    주어진 이미지에서 딸기를 감지하고 가장 높은 신뢰도의 익음 상태와 점수를 반환합니다.
    """
    if ripe_model is None:
        return 0.0, "딸기 모델 로딩 실패"

    try:
        results = ripe_model(image_path)
        best_confidence = 0.0
        best_ripeness_text = "딸기 미검출"

        for result in results:
            for box in result.boxes:
                confidence = box.conf[0].item()
                if confidence > best_confidence:
                    best_confidence = confidence
                    class_id = int(box.cls[0].item())
                    best_ripeness_text = ripe_model.names[class_id]
        
        final_score = round(best_confidence, 2)
        logging.info(f"Ripeness analysis: Best guess is '{best_ripeness_text}' with score {final_score}")
        return final_score, best_ripeness_text

    except Exception as e:
        logging.error(f"Error during ripeness analysis: {e}")
        return 0.0, "분석 중 오류 발생"
    
    
# 4. 꽃 개화 여부 분석 함수
def analyze_flowers(image_path):
    """
    주어진 이미지에서 꽃을 감지하고, 감지된 꽃의 개수를 반환합니다.
    """
    if flower_model is None:
        return 0, "꽃 모델 로딩 실패"

    try:
        results = flower_model(image_path)
        flower_count = 0

        for result in results:
            # result.boxes가 존재하고, 객체가 하나 이상 감지되었는지 확인
            if result.boxes and len(result.boxes) > 0:
                flower_count += len(result.boxes)
        
        logging.info(f"Flower analysis: Found {flower_count} flowers.")
        # 꽃의 개수만 반환합니다. (필요하다면 신뢰도 점수 등 추가 정보 반환 가능)
        return flower_count, "분석 완료"

    except Exception as e:
        logging.error(f"Error during flower analysis: {e}")
        return 0, "분석 중 오류 발생"


# --- 테스트 코드 ---
if __name__ == '__main__':
    test_image = 'images/test_strawberry.jpg'
    
    import os
    if os.path.exists(test_image):
        print("--- Testing Ripeness Analysis ---")
        ripe_score, ripe_text = analyze_ripeness(test_image)
        print(f"Test Result -> Score: {ripe_score}, Ripeness: {ripe_text}")

        print("\n--- Testing Flower Analysis ---")
        flowers, flower_status = analyze_flowers(test_image)
        print(f"Test Result -> Flower Count: {flowers}, Status: {flower_status}")
    else:
        print(f"Test image not found at: {test_image}")