# Social Force 보행 모델 (Helbing-Molnár, Helbing-Farkas-Vicsek)

본 문서는 SafeCrowd `domain` 레이어의 군중 이동/회피 물리 모델로 채택한 Social Force 모델의 출처와 채택 근거를 정리한다. 코드 상의 매개변수는 본 문서를 통해 추적한다.

## 1. 채택 모델

군중 시뮬레이션의 보행 미시 모델로 **Helbing-Molnár Social Force 모델 (1995)** 을 채택하고, 전방 비대칭성(anisotropy) 가중치는 **Helbing-Farkas-Vicsek (2000)** 의 정식을 따른다.

## 2. 1차 출처

### 2.1. Helbing & Molnár (1995)
- D. Helbing and P. Molnár, "Social force model for pedestrian dynamics," *Physical Review E* **51**(5), 4282–4286 (1995).
- DOI: <https://doi.org/10.1103/PhysRevE.51.4282>
- 사전 인쇄본: <https://arxiv.org/abs/cond-mat/9805244>

핵심 식 (코드와 일대일 대응)
- Eq. (3) 추진력 (driving force):
  - `f_i^drv = (v_i^0 - v_i) / τ_i`
  - 코드: [`socialForceDriving`](../../src/domain/ScenarioSimulationInternal.cpp)
  - 매개변수: `kSocialForceRelaxationTime` = τ
- Eq. (4) 보행자-보행자 척력 (isotropic exponential repulsion):
  - `f_ij = A_i · exp((r_ij - d_ij) / B_i) · n_ij`
  - 코드: [`socialForceAgentRepulsion`](../../src/domain/ScenarioSimulationInternal.cpp)
  - 매개변수: `kSocialForceAgentStrength` = A_i, `kSocialForceAgentRange` = B_i
- Eq. (5) 보행자-벽 척력:
  - `f_iW = A_W · exp((r_i - d_iW) / B_W) · n_iW`
  - 코드: [`socialForceWallRepulsion`](../../src/domain/ScenarioSimulationInternal.cpp)
  - 매개변수: `kSocialForceWallStrength` = A_W, `kSocialForceWallRange` = B_W

### 2.2. Helbing, Farkas & Vicsek (2000)
- D. Helbing, I. Farkas, and T. Vicsek, "Simulating dynamical features of escape panic," *Nature* **407**, 487–490 (2000).
- DOI: <https://doi.org/10.1038/35035023>

본 PR에서 차용한 부분은 보행자-보행자 척력의 비대칭 가중치 `w(φ)` 한 항이다.

- 가중치 식 (Eq. (3)):
  - `w(φ) = λ + (1 - λ) · (1 + cos φ) / 2`
  - `φ`는 자기 진행 방향과 상대편 방향 사이 각.
  - 매개변수: `kSocialForceAgentAnisotropy` = λ
- 비고: 본 PR에는 같은 논문이 도입한 신체 접촉력(`body force k`)과 마찰력(`κ`)은 도입하지 않았다. 두 항은 패닉 상황을 표현하기 위한 항으로, 본 단계의 정상 보행 시뮬레이션 범위를 벗어난다. 도입 시 별도 설계 문서가 필요하다.

## 3. 채택 매개변수와 근거

| 상수 | 값 | 단위 | 근거 |
|------|----|------|------|
| `kSocialForceRelaxationTime` (τ) | 0.5 | s | Helbing-Molnár (1995) 본문에서 사용한 대표 값 |
| `kSocialForceAgentStrength` (A_i / m_i) | 2.1 | m/s² | Helbing-Molnár 원문은 힘(N) 단위. 본 코드는 가속도 단위로 정규화하기 위해 평균 체중 m ≈ 80 kg로 나눈 통상 사용 값을 채택 |
| `kSocialForceAgentRange` (B_i) | 0.30 | m | 원문은 0.08 m. 본 시뮬레이터의 고정 시간 간격(약 1/30 s)에서 안정적인 적분과 매끈한 회피를 위해 확장. 후속 검증에서 fundamental diagram 비교 후 재조정 대상 |
| `kSocialForceAgentAnisotropy` (λ) | 0.5 | – | Helbing-Farkas-Vicsek (2000) 본문 사용 값 |
| `kSocialForceAgentInteractionRadius` | 2.0 | m | 척력 컷오프. 5·B_i 이상에서 기여가 1% 미만이 되는 거리 기준 |
| `kSocialForceWallStrength` (A_W / m_i) | 5.0 | m/s² | 일반적으로 보행자-벽 척력은 보행자-보행자보다 강하게 둔다 (Helbing-Molnár §IV) |
| `kSocialForceWallRange` (B_W) | 0.20 | m | 위와 같은 이유로 보행자 척력보다 짧게 설정 |
| `kSocialForceWallInteractionRadius` | 1.0 | m | 척력 컷오프 |
| `kSocialForceMaxAcceleration` (a_max) | 5.0 | m/s² | 인간이 단시간 발휘 가능한 보행 가속도의 통상 상한 |
| `kSocialForceHeadOnTangentBias` | 0.4 | – | 완벽 정대칭 head-on 교착을 푸는 결정론적 좌측 편향. 학술 모델에는 없는 SafeCrowd 추가 항으로, 결정론 보장과 데드락 방지 목적임을 명시 |
| `kSocialForceHeadOnAlignmentThreshold` | 0.985 | cos | 위 편향의 적용 범위. cos φ ≥ 0.985 (≈ φ ≤ 10°) 인 거의 정면 충돌 상황에만 작동 |

## 4. SafeCrowd 분류와 사용 범위

- 위치: `domain` 레이어 (`src/domain/ScenarioSimulationInternal.{h,cpp}`).
- Pathfinder 정합성: Pathfinder 2026.1은 자체 보행 알고리즘을 비공개로 두기 때문에 본 모델은 [`고급 위험 모델.md`](../product/고급%20위험%20모델.md)의 `Pathfinder 정합 확장` 항목으로는 분류하지 않는다. `연구 후보`에서 출발해 구현으로 승격된 항목으로 본다.
- 본 모델은 보행 미시 모델(이동/회피 물리)만을 담당한다. 압사·압박 판정, 낙상, 역방향/교차류 정밀 지표, 추천 최적화는 [`고급 위험 모델.md` §3, §5](../product/고급%20위험%20모델.md)에 따라 별도 설계 문서가 선행되어야 한다.

## 5. 검증 권고 (후속 트랙)

- 1대1 head-on 분리 회귀 테스트는 `tests/ScenarioSimulationSystemsTests.cpp`에 포함되어 있다.
- 다음 단계 검증으로 권고:
  - **Fundamental diagram (밀도-속도)**: Helbing 등 후속 연구가 표준으로 사용. SafeCrowd `domain` 단위 테스트로 도입 가능.
  - **단일 출구 통과시간 회귀**: 동일 시드·동일 인구로 본 모델 도입 전후 시뮬레이션 결과 비교.
- 검증 트랙은 별도 분석(Lightweight Task) 이슈로 분리한다.

## 6. 향후 확장 시 추가 인용 대상

- Helbing-Farkas-Vicsek (2000)의 신체 접촉력/마찰력 항을 도입할 때.
- Moussaïd et al. (2009/2011)의 시야 기반(cognitive heuristic) 보행 모델을 검토할 때.
- Johansson, Helbing, Shukla (2007)의 elliptic specification을 검토할 때.

각 확장은 별도 연구 후보로 [`고급 위험 모델.md` §3](../product/고급%20위험%20모델.md)에 추가하고 본 문서를 갱신한다.
