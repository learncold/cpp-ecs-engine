# MassMotion

예. MassMotion은 공식적으로 대피 시뮬레이션을 수행할 수 있습니다. Oasys/Arup의 2024 검증 문서는 MassMotion을 “pedestrian movement and evacuation simulation program”으로 설명하고, 제품 페이지도 `Evacuate`를 기본 스케줄링 옵션 중 하나로 제시합니다.  
근거: [MassMotion 제품 페이지](https://www.oasys-software.com/products/massmotion/), [2024 Fire Evacuation Verification & Validation](https://www.oasys-software.com/wp-content/uploads/2024/07/Fire-Evacuation-VV.pdf)

다만 범위를 정확히 보면, 공식 문서는 MassMotion이 사람의 대피 행동, 출구 선택, 혼잡, 이동시간 같은 `egress` 분석을 하는 도구라고 설명합니다. 반면 화재나 연기 자체를 직접 모델링하지는 않는다고 명시합니다.  
근거: [2024 Fire Evacuation Verification & Validation](https://www.oasys-software.com/wp-content/uploads/2024/07/Fire-Evacuation-VV.pdf)

대피 시뮬레이션 절차는 보통 이렇게 이해하면 됩니다.

1. 공간 모델을 만든다.  
CAD/BIM을 가져오거나 직접 3D 환경을 만들고, 바닥, 문/링크, 계단, 램프, 에스컬레이터, 출입 포털, 장애물 등을 보행 네트워크로 분류합니다.  
근거: [MassMotion 제품 페이지](https://www.oasys-software.com/products/massmotion/), [2024 Fire Evacuation Verification & Validation](https://www.oasys-software.com/wp-content/uploads/2024/07/Fire-Evacuation-VV.pdf)

2. 대피 인원과 출구 조건을 정의한다.  
Entry portal로 에이전트를 생성하고, Exit portal을 목표 출구로 지정합니다. 인원수, 생성 시간/속도, origin-destination, exit zone 등을 설정할 수 있습니다.  
근거: [MassMotion 제품 페이지](https://www.oasys-software.com/products/massmotion/), [2024 Fire Evacuation Verification & Validation](https://www.oasys-software.com/wp-content/uploads/2024/07/Fire-Evacuation-VV.pdf)

3. `Evacuate` 이벤트를 설정한다.  
공식 User Guide에 따르면 `Evacuate` 이벤트는 에이전트에게 일정 대기시간을 준 뒤 출구 쪽으로 이동시키며, 필요하면 zone을 순차적으로 비우도록 설정할 수 있습니다.  
근거: [MassMotion User Guide 11.8 PDF](https://www.oasys-software.com/wp-content/uploads/2024/07/Oasys-MassMotion-User-Guide-11-8.pdf)

4. 경로 선택과 행동 규칙을 준다.  
에이전트는 `Least Cost` 방식으로 가장 유리한 출구를 찾거나, 특정 출구를 지정받아 그쪽으로 이동할 수 있습니다. 공식 검증 문서는 혼잡과 출구 가용성 변화에 따라 경로를 재평가할 수 있다고 설명합니다.  
근거: [2024 Fire Evacuation Verification & Validation](https://www.oasys-software.com/wp-content/uploads/2024/07/Fire-Evacuation-VV.pdf)

5. 여러 시나리오를 돌리고 결과를 비교한다.  
MassMotion은 그래프, 맵, 필터 기반 분석으로 혼잡 구간, 대기시간, 이동시간, 문/계단 유량 등을 비교할 수 있습니다. 실제 공식 사례에서도 여러 대피 전략 시나리오를 비교해 최적안을 찾는 방식으로 사용됐습니다.  
근거: [MassMotion 제품 페이지](https://www.oasys-software.com/products/massmotion/), [Cendana 대피 사례](https://www.oasys-software.com/case-studies/evacuation-modelling-of-cendana-building-using-oasys-massmotion/)

참고용 공식 링크 모음:
- [MassMotion Documentation 허브](https://www.oasys-software.com/support/massmotion/massmotion-documentation/)
- [MassMotion 제품 페이지](https://www.oasys-software.com/products/massmotion/)
- [2024 Fire Evacuation Verification & Validation](https://www.oasys-software.com/wp-content/uploads/2024/07/Fire-Evacuation-VV.pdf)
- [MassMotion User Guide 11.8 PDF](https://www.oasys-software.com/wp-content/uploads/2024/07/Oasys-MassMotion-User-Guide-11-8.pdf)

원하시면 다음 답변에서 `대피성능평가 관점에서 MassMotion으로 뽑아볼 수 있는 주요 지표`만 따로 정리해 드리겠습니다.

# Pathfinder

예. `Pathfinder`는 공식적으로 대피(egress) 시뮬레이션을 수행할 수 있습니다. Thunderhead는 Pathfinder를 `crowd movement and egress software`로 소개하고, `Emergency Preparedness` 문서에서는 기본 시뮬레이션이 `Egress`이며 각 agent가 가장 빠른 경로로 출구로 이동한다고 설명합니다. 또한 `assisted evacuation`, `threat awareness`, `delayed/probabilistic reactions`도 지원합니다.  
근거: [Pathfinder 제품 페이지](https://www.thunderheadeng.com/pathfinder/), [Emergency Preparedness](https://www.thunderheadeng.com/pathfinder/emergency-preparedness/)

다만 범위를 정확히 보면, Pathfinder의 기본 축은 `사람 이동/대피 모델링`입니다. 화재·연기 자체의 물리 해석은 `PyroSim(FDS)`와 연동해 `FED`(독성 노출량), `visibility`(가시성), 연기 속 보행속도 저하 등을 평가하는 구조입니다. 공식 튜토리얼은 `full coupling`이 아직 완료되지 않았고, 연기 반응을 반영하는 일부 워크플로는 현재도 수동 coupling 방식이라고 설명합니다.  
근거: [Coupling with PyroSim (FDS)](https://www.thunderheadeng.com/docs/2025-1/pathfinder/advanced/coupling-fds/), [Manual smoke coupling tutorial](https://www.thunderheadeng.com/docs/2025-1/pathfinder/examples/how-to/manually-coupling-fds/)

대피 시뮬레이션 절차는 공식 문서 기준으로 보통 이렇게 진행됩니다.

1. 공간 모델 작성  
CAD/BIM을 가져오거나 직접 모델링한 뒤, `floor extraction`으로 navigation mesh를 만들고 `room`, `door`, `stair`, `exit`를 정의합니다. Pathfinder에서 `exit`는 모델 경계에 놓인 door입니다.  
근거: [Fundamentals 튜토리얼](https://www.thunderheadeng.com/docs/2025-1/pathfinder/examples/fundamentals/), [Generating Geometry from CAD](https://www.thunderheadeng.com/docs/2025-1/pathfinder/geometry/generating-from-cad/), [Doors / Exits](https://www.thunderheadeng.com/docs/2025-1/pathfinder/geometry/doors/)

2. 인원 배치  
방 안에 occupant를 직접 배치하거나, `occupant source`로 시간대별 유입 인원을 생성할 수 있습니다.  
근거: [Fundamentals 튜토리얼](https://www.thunderheadeng.com/docs/2025-1/pathfinder/examples/fundamentals/), [Occupant Sources](https://www.thunderheadeng.com/docs/2026-1/pathfinder/occupants/occupant-source/)

3. 보행 특성과 대피 규칙 설정  
`Profile`로 속도, 체격, 분포 등을 정의하고, `Behavior`로 대피 로직을 줍니다. 기본 behavior인 `Goto Any Exit`는 가장 빠른 경로로 아무 출구나 향하게 합니다. 필요하면 특정 출구, refuge room, elevator, assisted evacuation 동선도 구성할 수 있습니다.  
근거: [Profiles](https://www.thunderheadeng.com/docs/2025-1/pathfinder/profiles/), [Behaviors](https://www.thunderheadeng.com/docs/2026-1/pathfinder/behaviors/), [Assisted Evacuation](https://www.thunderheadeng.com/docs/2025-1/pathfinder/advanced/assisted-evacuation/)

4. 비상상황 반응 모델링  
단순 최단대피 외에도 `familiar routes`, `triggers`, `delayed/probabilistic reactions`로 익숙한 출구 선호, 경보 전파, 일부 인원의 지연 반응 등을 반영할 수 있습니다.  
근거: [Evacuation Using Familiar Paths](https://www.thunderheadeng.com/docs/2025-1/pathfinder/examples/how-to/familiar-routes/), [Triggers](https://www.thunderheadeng.com/docs/2025-1/pathfinder/examples/how-to/evacuation-using-triggers/)

5. 시뮬레이션 실행  
`Run Simulation`으로 시나리오를 실행하고, 필요하면 Monte Carlo variation으로 여러 랜덤 케이스를 반복 실행할 수 있습니다.  
근거: [Fundamentals 튜토리얼](https://www.thunderheadeng.com/docs/2025-1/pathfinder/examples/fundamentals/), [Simulation](https://www.thunderheadeng.com/docs/2026-1/pathfinder/simulation/), [Pathfinder 2025.1](https://www.thunderheadeng.com/docs/2025-1/pathfinder/)

6. 결과 분석  
결과 뷰어와 CSV에서 `exit time`, `congestion time`, `distance`, `safe time` 등을 확인합니다. PyroSim/FDS 결과를 연동하면 `FED`와 `visibility`도 함께 평가할 수 있습니다.  
근거: [Occupant Summary output](https://www.thunderheadeng.com/docs/2025-1/pathfinder/output/occupant-summary/), [Coupling with PyroSim (FDS)](https://www.thunderheadeng.com/docs/2025-1/pathfinder/advanced/coupling-fds/), [Results 2025.1](https://www.thunderheadeng.com/docs/2025-1/results/)

원하시면 다음 답변에서 `MassMotion vs LEGION vs Pathfinder`를 `대피성능평가 관점`으로 비교해드리겠습니다.
