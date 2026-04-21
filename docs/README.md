# SafeCrowd 문서 안내

`docs/`는 기준 문서를 모아 두는 위치다. 진행 중 메모, 임시 TODO, 작업 상태 추적은 GitHub Issues와 Project에서 관리한다.

## 문서 구조

- `architecture/`: 계층 구조, 책임, 디렉터리 규칙 같은 오래 유지할 설계 기준
- `process/`: GitHub Project, 이슈/PR 규칙 같은 협업 절차
- `product/`: 개요, 사용자 시나리오, 위험 정의, 백로그 같은 제품 요구와 범위
- `references/`: 외부 조사 자료와 튜토리얼. 기준 문서의 보조 근거로만 사용
- `제출용/`: 학교 제출 산출물. 저장소 운영 기준 문서와 분리해 보관

## 시작점

- [architecture/프로젝트 구조.md](architecture/프로젝트 구조.md)
- [process/GitHub Project.md](process/GitHub Project.md)
- [product/개요서.md](product/개요서.md)
- [product/사용자 시나리오.md](product/사용자 시나리오.md)
- [product/위험 정의.md](product/위험 정의.md)
- [product/고급 위험 모델.md](product/고급 위험 모델.md)
- [product/Product Backlog.md](product/Product Backlog.md)
- [product/Sprint 시연 계획.md](<product/Sprint 시연 계획.md>)

## 유지 규칙

- 한 사실은 한 문서만 기준으로 삼는다.
- 구조 변경은 `architecture/`, 협업 규칙 변경은 `process/`, 요구 변경은 `product/`를 먼저 갱신한다.
- 외부 링크, 조사 메모, 튜토리얼은 `references/`로 보낸다.
- 제출 양식과 산출물은 `제출용/`에 두고 기준 문서처럼 직접 참조하지 않는다.
- 이 저장소는 별도 `adr/` 폴더를 두지 않는다. 구조 결정은 `architecture/프로젝트 구조.md`에, 협업 규칙 결정은 `process/GitHub Project.md`와 `CONTRIBUTING.md`에 반영한다.
