# AI 콘솔 연애 시뮬레이터

## 프로젝트 개요
이 프로젝트는 로컬 Ollama API와 연동하여 AI 여자친구 "사야"와의 대화를 콘솔에서 시뮬레이션하는 C++ 애플리케이션입니다. CMake를 통해 빌드되며, `libcurl`과 `nlohmann::json`을 사용해 Ollama와 통신하고 상태를 관리합니다.

## 주요 기능
- `data/persona.txt`에 정의된 사야의 페르소나를 로드하여 대화 맥락에 반영
- 플레이어 입력을 기반으로 Ollama `generate` API 호출 및 응답 출력
- 특정 키워드(예: "좋아", "싫어")에 따라 호감도 가중치 조정
- 최근 대화 내용과 호감도를 `data/save.json`에 저장하여 다음 실행 시 복원
- 환경 변수로 Ollama 엔드포인트와 모델을 손쉽게 구성

## 폴더 구조
```
ai-dating-sim/
├─ CMakeLists.txt
├─ include/
│  ├─ character.h
│  ├─ ollama_client.h
│  └─ utils.h
├─ src/
│  ├─ character.cpp
│  ├─ main.cpp
│  ├─ ollama_client.cpp
│  └─ utils.cpp
└─ data/
   ├─ persona.txt
   └─ save.json
```

## 사전 준비물
- C++17 호환 컴파일러 (예: GCC 11+, Clang 13+, MSVC 19.3+)
- CMake 3.16 이상
- `libcurl` 개발 헤더 및 라이브러리
- 인터넷에서 제공하는 `nlohmann/json.hpp`는 `include/` 경로에 포함되어 있습니다.
- 로컬에서 실행 중인 Ollama 서버 (`ollama serve`)

## 빌드 방법
```bash
cmake -S . -B build
cmake --build build
```

## 실행 방법
빌드가 끝나면 다음과 같이 실행할 수 있습니다.
```bash
./build/ai_dating_sim
```

실행 후 콘솔에서 "오빠:" 프롬프트가 표시되며 자유롭게 메시지를 입력하면 됩니다. "exit"를 입력하면 프로그램이 종료됩니다.

## 환경 변수 설정
프로그램은 기본적으로 `http://localhost:11434/api/generate` 엔드포인트와 `llama3` 모델을 사용합니다. 다음 환경 변수를 통해 값을 덮어쓸 수 있습니다.

- `OLLAMA_ENDPOINT`: Ollama API 기본 URL (예: `http://localhost:11434/api/generate`)
- `OLLAMA_MODEL`: 사용할 모델 이름 (예: `llama3`)

예시:
```bash
export OLLAMA_ENDPOINT="http://localhost:11434/api/generate"
export OLLAMA_MODEL="llama3"
./build/ai_dating_sim
```

## 데이터 파일
- `data/persona.txt`: 사야의 페르소나 및 인사말을 정의합니다. 필요에 따라 내용을 수정하면 AI의 말투와 성격을 바꿀 수 있습니다.
- `data/save.json`: 호감도와 마지막 대화를 저장합니다. 파일이 없을 경우 프로그램이 기본 값을 생성합니다.

## 문제 해결
- Ollama 서버가 실행 중인지 확인하세요. (`ollama serve`)
- 네트워크 오류 또는 응답이 없을 경우, 프로그램은 오류 메시지를 출력하고 다음 입력을 기다립니다.
- `libcurl` 라이브러리나 인증서 설정이 필요할 수 있으니 운영체제에 맞는 패키지를 설치하세요.

행복한 대화 되세요! 💕
