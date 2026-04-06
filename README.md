# SafeCrowd

SafeCrowd는 ECS 기반 군중 시뮬레이션과 안전 의사결정 지원을 목표로 하는 Qt 데스크톱 애플리케이션입니다.  
건물, 행사장, 대피 시나리오 같은 실제 운영 환경을 가정해 군중 흐름, 병목, 위험 징후를 비교 가능한 형태로 분석하는 것을 목표로 합니다.

## 핵심 목표

- 실제 공간 구조를 바탕으로 시뮬레이션 가능한 레이아웃을 구성한다.
- 여러 운영 대안을 시나리오로 비교해 위험 차이를 확인한다.
- Qt 기반 UI와 분리된 ECS 엔진 구조를 유지한다.

## 아키텍처

프로젝트는 `application -> domain -> engine` 계층을 유지합니다.

- `src/application`: Qt UI와 애플리케이션 조립
- `src/domain`: SafeCrowd 도메인 로직
- `src/engine`: 재사용 가능한 ECS 엔진

상세 구조는 [docs/architecture/프로젝트 구조.md](docs/architecture/프로젝트 구조.md)를 기준 문서로 사용합니다.

## 빠른 시작

전체 앱 빌드:

```powershell
cmake --preset windows-debug
cmake --build --preset build-debug
ctest --preset test-debug
```

Qt 앱 없이 빠르게 검증:

```powershell
cmake --preset windows-debug-no-app
cmake --build --preset build-engine-debug
cmake --build --preset build-engine-domain-debug
cmake --build --preset build-no-app-debug
ctest --preset test-no-app-debug
```

앱 빌드에는 `vcpkg.json`의 `qtbase` 의존성이 필요합니다.

## 문서

- [docs/README.md](docs/README.md): 문서 전체 안내
- [docs/product/개요서.md](docs/product/개요서.md): 프로젝트 개요와 이해당사자
- [docs/product/사용자 시나리오.md](docs/product/사용자 시나리오.md): 대표 사용자 흐름
- [docs/product/위험 정의.md](docs/product/위험 정의.md): 위험 및 사고 모델 정의
- [docs/process/GitHub Project.md](docs/process/GitHub Project.md): 이슈, 프로젝트, PR 운영 규칙

## 기여 방식

기본 흐름은 `issue -> branch -> PR -> merge`입니다.  
문서와 저장소 운영 규칙은 [CONTRIBUTING.md](CONTRIBUTING.md)를 따릅니다.
