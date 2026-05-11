import sys
import os
import traceback

if len(sys.argv) > 1:
    BASE_DIR = sys.argv[1]
else:
    BASE_DIR = os.getcwd()

ERROR_LOG_PATH = os.path.join(BASE_DIR, "PythonError.log")

if os.path.exists(ERROR_LOG_PATH):
    try:
        os.remove(ERROR_LOG_PATH)
    except:
        pass

try:
    # 🌟 제미나이 패키지 임포트
    import google.generativeai as genai

    MAP_DATA_PATH = os.path.join(BASE_DIR, "3D_MapData.txt")
    PROMPT_DATA_PATH = os.path.join(BASE_DIR, "PromptData.txt")
    JSON_OUTPUT_PATH = os.path.join(BASE_DIR, "SpawnData_3D.json")
    CURRENT_STATE_PATH = os.path.join(BASE_DIR, "CurrentState.txt")

    # 🌟 팀장님의 실제 API 키 입력
    API_KEY = "AIzaSyAc4ggIIdssIrgHricd4TDnTYMbtl1qrio"
    genai.configure(api_key=API_KEY)

    def read_text_file(file_path):
        if not os.path.exists(file_path): return ""
        try:
            with open(file_path, "r", encoding="utf-8") as f: return f.read()
        except UnicodeDecodeError:
            with open(file_path, "r", encoding="utf-16") as f: return f.read()

    # 파일에서 데이터 읽어오기
    map_data = read_text_file(MAP_DATA_PATH)
    user_custom_prompt = read_text_file(PROMPT_DATA_PATH)
    current_state = read_text_file(CURRENT_STATE_PATH)

    # 🌟 프롬프트에 {current_state}와 {user_custom_prompt}를 제대로 삽입!
    prompt = f"""
너는 'Silent Recall'의 수석 레벨 디자이너야. 

[현재 배치 데이터]
{current_state}

[사용자 요청]
{user_custom_prompt}

[🔥 핵심 시스템 1: 직전 기믹 인식 및 중복 생성 절대 금지]
[현재 배치 데이터]의 제일 마지막 줄(최종 꼬리 노드)의 "Class(에셋 종류)"를 확인해라.
- 만약 직전 객체가 발판(BasicCube, Pitch 0)이라면, 그 바로 뒤에 또 발판을 생성하지 마라!
- 방향을 꺾을 때도 '새로운 코너용 발판'을 만들 필요 없이, 이미 존재하는 '마지막 발판의 실제 끝점(Edge)'에서부터 곧바로 다음 기믹(슬라이딩, 벽 등)의 방향(Yaw)을 틀어서 이어 붙여라!

[🔥 핵심 시스템 2: 진행 방향(Forward) 추적 및 동적 끝점 연산]
1. 기본 진행 방향은 '+X축'.
2. 사용자가 방향 전환(우회전 90도 등)을 지시하면 진행 축을 +Y 또는 -Y로 전환.
3. 진행 축에 따라 추출할 '실제 끝점(Edge)'을 [현재 배치 데이터]에서 찾아라. 
   - +X 진행 중: PrevEdge = MaxX
   - +Y 진행 중: PrevEdge = MaxY
   - -Y 진행 중: PrevEdge = MinY
4. 방향을 꺾은 직후에 배치되는 첫 번째 객체는 반드시 Rotation의 Yaw 값을 90(우측) 또는 -90(좌측)으로 틀어야 한다.

[🛠️ 템플릿 공식 (진행 방향 기준)]
1. 슬라이딩 (피치 -30): 
   - asset_name: "BasicCube"
   - 진행 좌표 = PrevEdge + (새 큐브 스케일 * 50 * 0.866)
   - Z 좌표 = 이전 객체 MaxZ - (새 큐브 스케일 * 50 * 0.5)
2. 그래플링 훅: 
   - asset_name: "GrapplePoint"
   - 진행 좌표 = PrevEdge + 1300
   - Z 좌표 = 이전 객체 MaxZ + 500
3. 벽타기: 
   - asset_name: "WalkableWall"
   - 진행 방향의 측면(수직 축)으로 400만큼 띄워서 배치. 진행 좌표는 PrevEdge + 800.
4. 일반 착지 발판:
   - 직전이 발판이 아닐 때(그래플링/벽타기 직후)만 생성할 것.

[🔥 JSON 출력 규격 엄수 (모두 소문자 키)]
{{ "action": "Create", "actor_id": "", "short_title": "", "reasoning": "...", "asset_name": "...", "location": {{"x":0,"y":0,"z":0}}, "rotation": {{"pitch":0,"yaw":0,"roll":0}}, "scale": {{"x":1,"y":1,"z":1}} }}
오직 JSON 배열만 반환해라.
"""

    # 🌟 정확한 최신 모델명으로 수정
    model = genai.GenerativeModel('gemini-2.0-flash')
    
    # 🌟 JSON 출력 강제화 (이 기능 덕분에 제미나이가 헛소리를 안 합니다)
    response = model.generate_content(
        prompt, 
        generation_config={"response_mime_type": "application/json"}
    )
    
    with open(JSON_OUTPUT_PATH, "w", encoding="utf-8") as f:
        f.write(response.text)

except Exception as e:
    with open(ERROR_LOG_PATH, "w", encoding="utf-8") as f:
        f.write("=== Python Execution Error ===\n")
        f.write(str(e) + "\n")
        f.write(traceback.format_exc())