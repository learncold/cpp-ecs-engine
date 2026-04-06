# Qt Widgets Designer 초기 세팅 튜토리얼

## 문서 목적
이 문서는 SafeCrowd UI 담당 팀원이 `Qt Widgets Designer`를 이용해 `.ui` 기반으로 화면을 만들 수 있도록, 현재 저장소 상태에 맞춘 최소 초기 세팅 절차를 정리한 문서이다.

이 문서의 목표는 다음과 같다.

- `Qt Widgets Designer`를 사용할 수 있는 상태를 만든다.
- 현재 수기 UI인 `MainWindow`를 `.ui` 기반 구조로 옮길 준비를 한다.
- 화면 배치와 C++ 동작 로직의 역할을 분리한다.

---

## 1. 현재 프로젝트 전제

현재 SafeCrowd는 다음 전제를 가진다.

- UI 기술: `Qt Widgets`
- 앱 타깃: `safecrowd_app`
- 소스 루트: `src/application`
- 현재 메인 윈도우 구현: `src/application/MainWindow.h`, `src/application/MainWindow.cpp`

중요한 점은 현재 루트 `CMakeLists.txt`에서 전역 `AUTOUIC`는 켜져 있지만, `safecrowd_app` 타깃에서 다시 꺼져 있다는 것이다.  
즉, 지금 상태로는 `.ui` 파일을 추가해도 자동으로 `uic`가 돌지 않을 수 있다.

---

## 2. 권장 작업 원칙

초기에는 아래 원칙으로 작업하는 것이 가장 안전하다.

- 레이아웃과 위젯 배치는 `.ui`에서 작업한다.
- 버튼 클릭 이후 동작, 타이머, domain 호출은 `.cpp`에 둔다.
- `domain`과 `engine`은 계속 UI를 몰라야 한다.
- 자동 생성되는 `ui_*.h` 파일은 직접 수정하지 않는다.
- 처음부터 복잡한 화면을 만들지 말고 `MainWindow`부터 `.ui`로 옮긴다.

정리하면 역할은 아래와 같다.

- `.ui`: 화면 구조
- `MainWindow.cpp`: 동작 연결
- `SafeCrowdDomain`: 비즈니스 로직

---

## 3. 준비물

### 3.1. 필수 도구

가장 쉬운 방법은 `Qt Creator`를 설치해서 내장된 `Qt Widgets Designer`를 사용하는 것이다.

권장 준비물:

- `Qt Creator`
- 현재 프로젝트가 빌드 가능한 Qt6 환경
- 기존 CMake preset 환경

### 3.2. 저장소 열기

`Qt Creator`에서 프로젝트 루트 `C:\Project`를 연다.

가능하면 기존 preset을 그대로 사용한다.

- Configure: `cmake --preset windows-debug`
- Build: `cmake --build --preset build-debug`

---

## 4. 1단계: CMake에서 `.ui` 자동 처리 활성화

### 4.1. 현재 상태

루트 `CMakeLists.txt` 상단에서는 아래가 켜져 있다.

```cmake
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)
```

하지만 `safecrowd_app` 타깃 아래에서는 아래처럼 다시 꺼져 있다.

```cmake
set_target_properties(safecrowd_app PROPERTIES
    AUTOMOC OFF
    AUTOUIC OFF
    AUTORCC OFF
)
```

### 4.2. 권장 변경

초기 세팅에서는 위 `set_target_properties(...)` 블록을 제거하는 것이 가장 단순하다.

그리고 `.ui` 파일을 `add_executable(safecrowd_app ...)` 소스 목록에 추가한다.

예시:

```cmake
add_executable(safecrowd_app
    src/application/MainWindow.h
    src/application/main.cpp
    src/application/MainWindow.cpp
    src/application/MainWindow.ui
)
```

핵심은 두 가지다.

- `AUTOUIC`를 끄지 말 것
- `.ui` 파일을 타깃 소스 목록에 포함할 것

---

## 5. 2단계: `MainWindow.ui` 생성

### 5.1. 파일 생성

`Qt Creator`에서:

1. `src/application` 우클릭
2. `Add New...`
3. `Qt Designer Form`
4. 템플릿: `Main Window`
5. 파일명: `MainWindow.ui`

### 5.2. 첫 화면 구성

초기에는 현재 코드와 동일하게 아주 단순하게 시작한다.

필요한 위젯:

- `QLabel` 1개
- `QPushButton` 3개

권장 `objectName`:

- `statusLabel`
- `startButton`
- `pauseButton`
- `stopButton`

### 5.3. 레이아웃 적용

중요: 위젯만 올려놓고 끝내지 말고 반드시 레이아웃을 건다.

권장 방법:

1. 중앙 `centralWidget` 선택
2. `Lay Out Vertically` 적용

이렇게 해야 창 크기가 바뀌어도 UI가 깨지지 않는다.

---

## 6. 3단계: `MainWindow`를 `.ui` 기반 구조로 전환

현재 `MainWindow.cpp`는 다음 요소를 코드로 직접 만들고 있다.

- `QWidget`
- `QVBoxLayout`
- `QLabel`
- `QPushButton`

이를 `.ui`로 옮기면 C++에서는 `setupUi(this)`만 호출하고 동작 연결만 담당하면 된다.

### 6.1. 헤더 구조 예시

```cpp
#pragma once

#include <QMainWindow>
#include <memory>

namespace safecrowd::domain {
class SafeCrowdDomain;
}

class QTimer;

namespace Ui {
class MainWindow;
}

namespace safecrowd::application {

class MainWindow : public QMainWindow {
public:
    explicit MainWindow(safecrowd::domain::SafeCrowdDomain& domain, QWidget* parent = nullptr);
    ~MainWindow() override;

private:
    void startSimulation();
    void pauseSimulation();
    void stopSimulation();
    void tickSimulation();
    void refreshStatusLabel();

    safecrowd::domain::SafeCrowdDomain& domain_;
    std::unique_ptr<Ui::MainWindow> ui_;
    QTimer* tickTimer_{nullptr};
};

}  // namespace safecrowd::application
```

### 6.2. 구현 구조 예시

```cpp
#include "application/MainWindow.h"

#include "ui_MainWindow.h"

#include <QPushButton>
#include <QTimer>

#include "domain/SafeCrowdDomain.h"
#include "engine/EngineState.h"

namespace safecrowd::application {

MainWindow::MainWindow(safecrowd::domain::SafeCrowdDomain& domain, QWidget* parent)
    : QMainWindow(parent),
      domain_(domain),
      ui_(std::make_unique<Ui::MainWindow>()) {
    ui_->setupUi(this);

    tickTimer_ = new QTimer(this);
    tickTimer_->setInterval(16);

    connect(ui_->startButton, &QPushButton::clicked, this, [this]() { startSimulation(); });
    connect(ui_->pauseButton, &QPushButton::clicked, this, [this]() { pauseSimulation(); });
    connect(ui_->stopButton, &QPushButton::clicked, this, [this]() { stopSimulation(); });
    connect(tickTimer_, &QTimer::timeout, this, [this]() { tickSimulation(); });

    refreshStatusLabel();
}

MainWindow::~MainWindow() = default;

}  // namespace safecrowd::application
```

### 6.3. 상태 라벨 접근

기존:

```cpp
statusLabel_->setText(...);
```

변경 후:

```cpp
ui_->statusLabel->setText(...);
```

즉, 위젯 포인터를 직접 멤버로 들고 있지 않고 `ui_` 아래에서 접근한다.

---

## 7. 4단계: 빌드 확인

변경 후 아래 순서로 확인한다.

```powershell
cmake --preset windows-debug
cmake --build --preset build-debug
ctest --preset test-debug
```

초기 확인 포인트:

- `MainWindow.ui`가 실제로 타깃 소스에 포함되어 있는가
- `AUTOUIC`가 앱 타깃에서 꺼져 있지 않은가
- `ui_MainWindow.h` 관련 include 오류가 없는가

---

## 8. 처음에는 하지 않는 것이 좋은 것

초기 세팅 단계에서는 아래 항목을 일부러 미루는 것이 좋다.

- 복잡한 `signals/slots` 자동 연결
- 커스텀 위젯 다수 도입
- 스타일시트 전체 설계
- 리소스 파일 `.qrc` 대량 구성
- 도면/히트맵용 복잡한 렌더링 영역 구현

이유는 첫 목표가 "Designer 기반 작업 흐름이 빌드되는 상태"를 확보하는 것이기 때문이다.

---

## 9. SafeCrowd에서 추천하는 첫 실습 범위

가장 좋은 첫 실습은 현재 `MainWindow`를 그대로 `.ui`로 옮기는 것이다.

추천 순서:

1. `MainWindow.ui` 생성
2. `QLabel + Start/Pause/Stop 버튼` 배치
3. `MainWindow.cpp`에서 `setupUi(this)` 적용
4. 기존 `startSimulation`, `pauseSimulation`, `stopSimulation`, `tickSimulation` 로직 재사용
5. 상태 라벨만 `ui_->statusLabel`로 교체

이 단계까지 성공하면 UI 담당자는 이후 화면도 같은 방식으로 확장할 수 있다.

---

## 10. 이후 확장 방향

초기 세팅이 끝나면 다음 순서로 확장하는 것이 좋다.

### 10.1. 단순 폼 화면 추가

예:

- 시나리오 목록
- 결과 요약 패널
- Import 검토 패널

이런 화면은 Designer와 궁합이 좋다.

### 10.2. 복잡한 시각화는 별도 위젯으로 분리

예:

- 레이아웃 뷰어
- 위험 히트맵
- 시뮬레이션 재생 캔버스

이런 부분은 Designer에서 placeholder를 두고, 나중에 커스텀 위젯으로 붙이는 것이 좋다.

즉:

- 일반 폼 UI: Designer
- 복잡한 시각화: 커스텀 위젯

이 분리가 SafeCrowd 구조에 가장 잘 맞는다.

---

## 11. 체크리스트

작업 전 체크:

- `Qt Creator` 설치 완료
- 프로젝트가 현재 preset으로 빌드 가능
- `safecrowd_app` 타깃에서 `AUTOUIC` 비활성화 여부 확인

작업 중 체크:

- `.ui` 파일이 `src/application` 아래에 있는가
- `.ui` 파일이 CMake 타깃 소스에 들어갔는가
- 위젯 `objectName`이 명확한가
- 레이아웃이 적용되었는가

작업 후 체크:

- 앱이 빌드되는가
- 버튼 클릭이 기존과 동일하게 동작하는가
- 상태 라벨이 정상 갱신되는가

---

## 12. 한 줄 요약

SafeCrowd에서 `Qt Widgets Designer`를 시작하는 가장 안전한 방법은 다음이다.

`MainWindow.ui`를 추가하고, CMake에서 `AUTOUIC`를 정상 활성화한 뒤, 화면 구조는 `.ui`, 동작 로직은 `MainWindow.cpp`에 남긴다.
