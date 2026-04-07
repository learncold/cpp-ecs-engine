# Pathfinder 2026.1 군중 대피 시뮬레이션 기능 조사

## 문서 목적
- Pathfinder 2026.1 공식 문서에서 SafeCrowd와 직접 연결되는 기능을 1차로 정리한다.
- 범위는 `Pathfinder 2026.1` 본문과, Pathfinder 결과 해석에 필요한 `Results 2026.1` 일부 페이지까지 포함한다.
- 이 문서는 기준 설계 문서가 아니라 외부 조사 메모다. 제품 요구나 아키텍처 결정은 `docs/product/`, `docs/architecture/` 문서를 기준으로 유지한다.

## 조사 기준
- SafeCrowd의 핵심 요구와 직접 연결되는가:
  - 공간 구조를 입력하고 수정할 수 있는가
  - 인원 특성과 행동을 시나리오별로 다르게 줄 수 있는가
  - 출구 폐쇄, 통제, 대기, 우회 같은 운영 이벤트를 줄 수 있는가
  - 결과를 수치와 시각화로 비교할 수 있는가

## 조사 진행 트리
- 아래 트리는 공식 Pathfinder 2026.1 navigator를 leaf까지 최대한 그대로 옮긴 full mirror 구조다.
- `The Results Viewer` 아래에는 Pathfinder navigator가 연결하는 Results 2026.1 navigator를 그대로 이어 붙였다.
- 체크는 현재까지 실제로 조사한 페이지와, 그 조사 범위를 포함하는 상위 섹션에만 표시했다.
- 현재 기준으로 조사 대상 leaf는 모두 확인을 마쳤고, 미체크 `[ ]` 항목은 남아 있지 않다.
- `x` 표시는 현재 SafeCrowd의 군중 대피 기능 조사 범위에서 제외한 항목이다. 상위 항목이 `x`면 하위 항목도 동일 기준으로 본다.

- x [Introduction](https://www.thunderheadeng.com/docs/2026-1/pathfinder/introduction/)
- x [Changelog](https://www.thunderheadeng.com/docs/2026-1/pathfinder/changelog/)
- x [Known Issues](https://www.thunderheadeng.com/docs/2026-1/pathfinder/known-issues/)
- x [System Requirements](https://www.thunderheadeng.com/docs/2026-1/pathfinder/system-requirements/)
- x [Examples](https://www.thunderheadeng.com/docs/2026-1/pathfinder/examples/)
  - x [Fundamentals](https://www.thunderheadeng.com/docs/2026-1/pathfinder/examples/fundamentals/)
  - x [Applications](https://www.thunderheadeng.com/docs/2026-1/pathfinder/examples/applications/)
    - x [Circulation Movement Using Queues](https://www.thunderheadeng.com/docs/2026-1/pathfinder/examples/applications/cafeteria-queues/)
    - x [Comparing NFPA 130, SFPE, and Pathfinder](https://www.thunderheadeng.com/docs/2026-1/pathfinder/examples/applications/comparing-nfpa-130/)
    - x [Coupling PyroSim fire results and Pathfinder movement](https://www.thunderheadeng.com/docs/2026-1/pathfinder/examples/applications/coupling-pyrosim-pathfinder/)
    - x [Display Experimental Trajectory Data in Pathfinder Results Viewer](https://www.thunderheadeng.com/docs/2026-1/pathfinder/examples/applications/display-experimental-trajectory/)
    - x [Evacuation of Theatres and Stadiums](https://www.thunderheadeng.com/docs/2026-1/pathfinder/examples/applications/evacuation-theatres-stadiums/)
    - x [Fixing Geometry in Imported CAD or BIM Models](https://www.thunderheadeng.com/docs/2026-1/pathfinder/examples/applications/fix-imported-cad-geometry/)
    - x [Hospital Evacuation using Pathfinder](https://www.thunderheadeng.com/docs/2026-1/pathfinder/examples/applications/hospital-evacuation/)
    - x [Refuge Floors, Firefighters, and Elevators](https://www.thunderheadeng.com/docs/2026-1/pathfinder/examples/applications/refuge-floor/)
    - x [Shopping using Triggers to change behavior](https://www.thunderheadeng.com/docs/2026-1/pathfinder/examples/applications/shopping-using-triggers/)
    - x [Subway Circulation and Emergency Evacuation using Triggers](https://www.thunderheadeng.com/docs/2026-1/pathfinder/examples/applications/subway-triggers/)
    - x [Subway Station Passenger Movement](https://www.thunderheadeng.com/docs/2026-1/pathfinder/examples/applications/subway-waiting-rooms/)
  - x [Feature Demos](https://www.thunderheadeng.com/docs/2026-1/pathfinder/examples/how-to/)
    - x [Aircraft Evacuation](https://www.thunderheadeng.com/docs/2026-1/pathfinder/examples/how-to/aircraft-evacuation/)
    - x [Avoiding Unwanted Gaps in Navigation Geometry During Object Movement](https://www.thunderheadeng.com/docs/2026-1/pathfinder/examples/how-to/avoid-unwanted-gaps/)
    - x [Balancing Occupant Flow Through Multiple Doors](https://www.thunderheadeng.com/docs/2026-1/pathfinder/examples/how-to/balance-flow/)
    - x [Use a Blueprint to Sketch a Model](https://www.thunderheadeng.com/docs/2026-1/pathfinder/examples/how-to/blueprint-to-sketch-model/)
    - x [Circulation Movement](https://www.thunderheadeng.com/docs/2026-1/pathfinder/examples/how-to/circulation-movement-cafeteria/)
    - x [Creating Stairs](https://www.thunderheadeng.com/docs/2026-1/pathfinder/examples/how-to/create-stairs/)
    - x [Distribute Exit Goals](https://www.thunderheadeng.com/docs/2026-1/pathfinder/examples/how-to/distribute-occupant-exit/)
    - x [Distributed Use of Stairs and Escalators](https://www.thunderheadeng.com/docs/2026-1/pathfinder/examples/how-to/distributed-use-of-stairs/)
    - x [Staged Evacuation using Triggers](https://www.thunderheadeng.com/docs/2026-1/pathfinder/examples/how-to/evacuation-using-triggers/)
    - x [Evacuation Using Familiar Paths](https://www.thunderheadeng.com/docs/2026-1/pathfinder/examples/how-to/familiar-routes/)
    - x [Fractional Effective Dose Integration with Evacuation Results](https://www.thunderheadeng.com/docs/2026-1/pathfinder/examples/how-to/fed-integration/)
    - x [Fix Geometry Created from DWG Files](https://www.thunderheadeng.com/docs/2026-1/pathfinder/examples/how-to/fix-dwg-geometry/)
    - x [Group Movement in Pathfinder](https://www.thunderheadeng.com/docs/2026-1/pathfinder/examples/how-to/group-movement/)
    - x [IFC Import to Pathfinder (and PyroSim)](https://www.thunderheadeng.com/docs/2026-1/pathfinder/examples/how-to/ifc-import/)
    - x [Manually Coupling PyroSim (FDS) and Pathfinder to Respond to Smoke](https://www.thunderheadeng.com/docs/2026-1/pathfinder/examples/how-to/manually-coupling-fds/)
    - x [Optimize Navigation Geometry](https://www.thunderheadeng.com/docs/2026-1/pathfinder/examples/how-to/optimize-nav-geom/)
    - x [Profile Switching](https://www.thunderheadeng.com/docs/2026-1/pathfinder/examples/how-to/profile-switching/)
    - x [Ramps in Pathfinder](https://www.thunderheadeng.com/docs/2026-1/pathfinder/examples/how-to/ramps/)
    - x [Reduce Occupant Diamter to Navigate Narrow Geometry](https://www.thunderheadeng.com/docs/2026-1/pathfinder/examples/how-to/reduce-occupant-diameter/)
    - x [How to remove a handle](https://www.thunderheadeng.com/docs/2026-1/pathfinder/examples/how-to/remove-handle/)
    - x [Results Scripting](https://www.thunderheadeng.com/docs/2026-1/pathfinder/examples/how-to/results-scripting/)
    - x [Simple Queues](https://www.thunderheadeng.com/docs/2026-1/pathfinder/examples/how-to/simple-queues/)
    - x [Triggers](https://www.thunderheadeng.com/docs/2026-1/pathfinder/examples/how-to/triggers/)
    - x [VR in Pathfinder and PyroSim](https://www.thunderheadeng.com/docs/2026-1/pathfinder/examples/how-to/vr-in-pathfinder-pyrosim/)
- x [User Interface](https://www.thunderheadeng.com/docs/2026-1/pathfinder/user-interface/)
  - x [Navigation View](https://www.thunderheadeng.com/docs/2026-1/pathfinder/user-interface/navigation-view/)
  - x [2D/3D Workspace](https://www.thunderheadeng.com/docs/2026-1/pathfinder/user-interface/2d-3d-workspace/)
  - x [Selecting Objects](https://www.thunderheadeng.com/docs/2026-1/pathfinder/user-interface/selecting-objects/)
  - x [Editing Objects](https://www.thunderheadeng.com/docs/2026-1/pathfinder/user-interface/editing-objects/)
  - x [Tools](https://www.thunderheadeng.com/docs/2026-1/pathfinder/user-interface/tools/)
  - x [View Options](https://www.thunderheadeng.com/docs/2026-1/pathfinder/user-interface/view-options/)
  - x [Views](https://www.thunderheadeng.com/docs/2026-1/pathfinder/user-interface/views/)
  - x [Bulk Renaming](https://www.thunderheadeng.com/docs/2026-1/pathfinder/user-interface/bulk-rename/)
  - x [Object Tags](https://www.thunderheadeng.com/docs/2026-1/pathfinder/user-interface/object-tags/)
  - x [Preferences](https://www.thunderheadeng.com/docs/2026-1/pathfinder/user-interface/preferences/)
  - x [Undo/Redo](https://www.thunderheadeng.com/docs/2026-1/pathfinder/user-interface/undo-redo/)
  - x [Copy/Paste](https://www.thunderheadeng.com/docs/2026-1/pathfinder/user-interface/copy-paste/)
  - x [Object Sets](https://www.thunderheadeng.com/docs/2026-1/pathfinder/user-interface/object-sets/)
  - x [Discrete Distributions](https://www.thunderheadeng.com/docs/2026-1/pathfinder/user-interface/distributions/)
  - x [Scenarios](https://www.thunderheadeng.com/docs/2026-1/pathfinder/user-interface/scenarios/)
- [x] [Geometry](https://www.thunderheadeng.com/docs/2026-1/pathfinder/geometry/)
  - [x] [Floors](https://www.thunderheadeng.com/docs/2026-1/pathfinder/geometry/floors/)
  - [x] [Materials](https://www.thunderheadeng.com/docs/2026-1/pathfinder/geometry/materials/)
  - [x] [Rooms](https://www.thunderheadeng.com/docs/2026-1/pathfinder/geometry/rooms/)
  - [x] [Obstructions](https://www.thunderheadeng.com/docs/2026-1/pathfinder/geometry/obstructions/)
  - [x] [Walls](https://www.thunderheadeng.com/docs/2026-1/pathfinder/geometry/walls/)
  - [x] [Obstacles](https://www.thunderheadeng.com/docs/2026-1/pathfinder/geometry/obstacles/)
  - [x] [Doors](https://www.thunderheadeng.com/docs/2026-1/pathfinder/geometry/doors/)
  - [x] [Stairs](https://www.thunderheadeng.com/docs/2026-1/pathfinder/geometry/stairs/)
  - [x] [Ramps](https://www.thunderheadeng.com/docs/2026-1/pathfinder/geometry/ramps/)
  - [x] [Escalators](https://www.thunderheadeng.com/docs/2026-1/pathfinder/geometry/escalators/)
  - [x] [Moving Walkways](https://www.thunderheadeng.com/docs/2026-1/pathfinder/geometry/moving-walkways/)
  - [x] [Elevators](https://www.thunderheadeng.com/docs/2026-1/pathfinder/geometry/elevators/)
    - [x] [Introduction](https://www.thunderheadeng.com/docs/2026-1/pathfinder/geometry/elevators/introduction/)
    - [x] [Configuration](https://www.thunderheadeng.com/docs/2026-1/pathfinder/geometry/elevators/configuration/)
    - [x] [Creating an Elevator from a Single Room](https://www.thunderheadeng.com/docs/2026-1/pathfinder/geometry/elevators/creating/)
    - [x] [Creating Multiple Elevators from Different Rooms](https://www.thunderheadeng.com/docs/2026-1/pathfinder/geometry/elevators/creating-multiple/)
    - [x] [Creating an Elevator Bank](https://www.thunderheadeng.com/docs/2026-1/pathfinder/geometry/elevators/creating-banks/)
    - [x] [Connecting / Disconnecting Floors from an Elevator](https://www.thunderheadeng.com/docs/2026-1/pathfinder/geometry/elevators/connecting-disconnecting/)
    - [x] [Technical Reference](https://www.thunderheadeng.com/docs/2026-1/pathfinder/geometry/elevators/tech-ref/)
    - [x] [Verification](https://www.thunderheadeng.com/docs/2026-1/pathfinder/geometry/elevators/verification/)
  - x [Measuring Length/Distance](https://www.thunderheadeng.com/docs/2026-1/pathfinder/geometry/measuring-length/)
  - x [Generating Geometry from Images](https://www.thunderheadeng.com/docs/2026-1/pathfinder/geometry/generating-from-images/)
  - x [Generating Geometry from PDF Files](https://www.thunderheadeng.com/docs/2026-1/pathfinder/geometry/generating-from-pdf/)
  - [x] [Generating Geometry from CAD](https://www.thunderheadeng.com/docs/2026-1/pathfinder/geometry/generating-from-cad/)
- [x] [Profiles](https://www.thunderheadeng.com/docs/2026-1/pathfinder/profiles/)
  - [x] [Characteristics Tab](https://www.thunderheadeng.com/docs/2026-1/pathfinder/profiles/characteristics/)
  - [x] [Movement Tab](https://www.thunderheadeng.com/docs/2026-1/pathfinder/profiles/movement/)
  - [x] [Restrictions Tab](https://www.thunderheadeng.com/docs/2026-1/pathfinder/profiles/restrictions/)
  - [x] [Door Choice Tab](https://www.thunderheadeng.com/docs/2026-1/pathfinder/profiles/door-choice/)
  - x [Animation Tab](https://www.thunderheadeng.com/docs/2026-1/pathfinder/profiles/animation/)
  - [x] [Output Tab](https://www.thunderheadeng.com/docs/2026-1/pathfinder/profiles/output/)
  - [x] [Advanced Tab](https://www.thunderheadeng.com/docs/2026-1/pathfinder/profiles/advanced/)
- [x] [Behaviors](https://www.thunderheadeng.com/docs/2026-1/pathfinder/behaviors/)
  - [x] [Creating a New Behavior](https://www.thunderheadeng.com/docs/2026-1/pathfinder/behaviors/creating-new/)
- [x] [Occupants](https://www.thunderheadeng.com/docs/2026-1/pathfinder/occupants/)
  - [x] [Generating Occupants](https://www.thunderheadeng.com/docs/2026-1/pathfinder/occupants/generating/)
  - [x] [Redistributing Profiles and Behaviors](https://www.thunderheadeng.com/docs/2026-1/pathfinder/occupants/redistribute-profiles/)
  - [x] [Randomizing Occupant Positions](https://www.thunderheadeng.com/docs/2026-1/pathfinder/occupants/randomize-position/)
  - [x] [Reducing Population](https://www.thunderheadeng.com/docs/2026-1/pathfinder/occupants/reduce-population/)
  - [x] [Occupant Sources](https://www.thunderheadeng.com/docs/2026-1/pathfinder/occupants/occupant-source/)
  - [x] [Occupant Tags](https://www.thunderheadeng.com/docs/2026-1/pathfinder/occupants/occupant-tags/)
- [x] [Movement Groups](https://www.thunderheadeng.com/docs/2026-1/pathfinder/groups/)
  - [x] [Configuration](https://www.thunderheadeng.com/docs/2026-1/pathfinder/groups/configuration/)
  - [x] [Template Configuration](https://www.thunderheadeng.com/docs/2026-1/pathfinder/groups/template-configuration/)
  - [x] [Creating a Movement Group](https://www.thunderheadeng.com/docs/2026-1/pathfinder/groups/movement-groups/)
  - [x] [Creating a Movement Group Template](https://www.thunderheadeng.com/docs/2026-1/pathfinder/groups/movement-group-templates/)
  - [x] [Creating Movement Groups from a Template before a simulation](https://www.thunderheadeng.com/docs/2026-1/pathfinder/groups/groups-from-occs/)
  - [x] [Creating Movement Groups from a Template during a simulation](https://www.thunderheadeng.com/docs/2026-1/pathfinder/groups/groups-from-sources/)
  - [x] [Output](https://www.thunderheadeng.com/docs/2026-1/pathfinder/groups/output/)
- [x] [Queues](https://www.thunderheadeng.com/docs/2026-1/pathfinder/queues/)
  - [x] [Service Points](https://www.thunderheadeng.com/docs/2026-1/pathfinder/queues/services/)
  - [x] [Queue Paths](https://www.thunderheadeng.com/docs/2026-1/pathfinder/queues/paths/)
  - [x] [Path Nodes](https://www.thunderheadeng.com/docs/2026-1/pathfinder/queues/path-nodes/)
- [x] [Targets](https://www.thunderheadeng.com/docs/2026-1/pathfinder/targets/)
  - [x] [Creating Occupant Targets](https://www.thunderheadeng.com/docs/2026-1/pathfinder/targets/creating/)
  - [x] [Occupant Target Properties](https://www.thunderheadeng.com/docs/2026-1/pathfinder/targets/properties/)
  - [x] [Orienting Occupant Targets](https://www.thunderheadeng.com/docs/2026-1/pathfinder/targets/orientation/)
  - [x] [Prioritizing Occupant Targets](https://www.thunderheadeng.com/docs/2026-1/pathfinder/targets/priority/)
  - [x] [Using Occupant Targets](https://www.thunderheadeng.com/docs/2026-1/pathfinder/targets/using-targets/)
  - [x] [Occupant Target Reservation System](https://www.thunderheadeng.com/docs/2026-1/pathfinder/targets/reservation/)
  - [x] [Occupant Target Limitations](https://www.thunderheadeng.com/docs/2026-1/pathfinder/targets/limitations/)
  - [x] [Occupant Target Output](https://www.thunderheadeng.com/docs/2026-1/pathfinder/targets/output/)
- [x] [Triggers](https://www.thunderheadeng.com/docs/2026-1/pathfinder/triggers/)
  - [x] [Creating Triggers](https://www.thunderheadeng.com/docs/2026-1/pathfinder/triggers/creating/)
  - [x] [Trigger Behavior](https://www.thunderheadeng.com/docs/2026-1/pathfinder/triggers/behavior/)
  - [x] [Occupant Response to a Trigger](https://www.thunderheadeng.com/docs/2026-1/pathfinder/triggers/response/)
  - [x] [Trigger Memory](https://www.thunderheadeng.com/docs/2026-1/pathfinder/triggers/memory/)
  - [x] [Trigger Rank](https://www.thunderheadeng.com/docs/2026-1/pathfinder/triggers/rank/)
  - [x] [Trigger Properties](https://www.thunderheadeng.com/docs/2026-1/pathfinder/triggers/properties/)
  - [x] [Trigger Limitations](https://www.thunderheadeng.com/docs/2026-1/pathfinder/triggers/limitations/)
  - [x] [Trigger Output](https://www.thunderheadeng.com/docs/2026-1/pathfinder/triggers/output/)
- [x] [Simulation](https://www.thunderheadeng.com/docs/2026-1/pathfinder/simulation/)
  - [x] [Parameters](https://www.thunderheadeng.com/docs/2026-1/pathfinder/simulation/parameters/)
  - [x] [Starting and Managing Simulations](https://www.thunderheadeng.com/docs/2026-1/pathfinder/simulation/starting/)
  - [x] [Stopping and Resuming Simulations](https://www.thunderheadeng.com/docs/2026-1/pathfinder/simulation/stopping/)
- [x] [Output](https://www.thunderheadeng.com/docs/2026-1/pathfinder/output/)
  - [x] [Summary Report](https://www.thunderheadeng.com/docs/2026-1/pathfinder/output/summary/)
  - [x] [Monte Carlo Results](https://www.thunderheadeng.com/docs/2026-1/pathfinder/output/monte-carlo/)
  - [x] [Cumulative Output](https://www.thunderheadeng.com/docs/2026-1/pathfinder/output/cumulative/)
  - [x] [Door History](https://www.thunderheadeng.com/docs/2026-1/pathfinder/output/doors/)
  - [x] [Room History](https://www.thunderheadeng.com/docs/2026-1/pathfinder/output/rooms/)
  - [x] [Measurement Regions](https://www.thunderheadeng.com/docs/2026-1/pathfinder/output/measurement-regions/)
  - [x] [Occupant Parameters](https://www.thunderheadeng.com/docs/2026-1/pathfinder/output/occupant-params/)
  - [x] [Interpersonal Distance](https://www.thunderheadeng.com/docs/2026-1/pathfinder/output/interpersonal-distance/)
  - [x] [Occupant Summary](https://www.thunderheadeng.com/docs/2026-1/pathfinder/output/occupant-summary/)
  - [x] [Occupant History](https://www.thunderheadeng.com/docs/2026-1/pathfinder/output/occupant-history/)
  - [x] [Groups Output](https://www.thunderheadeng.com/docs/2026-1/pathfinder/output/groups/)
  - [x] [Triggers History](https://www.thunderheadeng.com/docs/2026-1/pathfinder/output/triggers/)
  - [x] [Occupant Target History](https://www.thunderheadeng.com/docs/2026-1/pathfinder/output/targets/)
  - [x] [Simulation Variation Output](https://www.thunderheadeng.com/docs/2026-1/pathfinder/output/variations/)
  - [x] [3D Results](https://www.thunderheadeng.com/docs/2026-1/pathfinder/output/3d-results/)
- [x] [Advanced](https://www.thunderheadeng.com/docs/2026-1/pathfinder/advanced/)
  - [x] [Assisted Evacuation](https://www.thunderheadeng.com/docs/2026-1/pathfinder/advanced/assisted-evacuation/)
    - [x] [Introduction](https://www.thunderheadeng.com/docs/2026-1/pathfinder/advanced/assisted-evacuation/introduction/)
    - [x] [Basic Usage](https://www.thunderheadeng.com/docs/2026-1/pathfinder/advanced/assisted-evacuation/basic-usage/)
    - [x] [Configuration](https://www.thunderheadeng.com/docs/2026-1/pathfinder/advanced/assisted-evacuation/configuration/)
    - [x] [Creating an Assisted Evacuation Team](https://www.thunderheadeng.com/docs/2026-1/pathfinder/advanced/assisted-evacuation/creating-teams/)
    - [x] [Assigning a Client to a Team](https://www.thunderheadeng.com/docs/2026-1/pathfinder/advanced/assisted-evacuation/assigning-clients/)
    - [x] [Assigning an Assistant to a Team](https://www.thunderheadeng.com/docs/2026-1/pathfinder/advanced/assisted-evacuation/adding-assistants/)
    - [x] [Technical Reference](https://www.thunderheadeng.com/docs/2026-1/pathfinder/advanced/assisted-evacuation/tech-ref/)
    - [x] [Verification Tests](https://www.thunderheadeng.com/docs/2026-1/pathfinder/advanced/assisted-evacuation/verification/)
  - [x] [Measurement Regions](https://www.thunderheadeng.com/docs/2026-1/pathfinder/advanced/measurement-regions/)
  - x [Coupling with PyroSim (FDS) Simulations](https://www.thunderheadeng.com/docs/2026-1/pathfinder/advanced/coupling-fds/)
  - [x] [Monte Carlo](https://www.thunderheadeng.com/docs/2026-1/pathfinder/advanced/monte-carlo/)
  - x [Custom Avatars](https://www.thunderheadeng.com/docs/2026-1/pathfinder/advanced/avatars/)
  - x [Custom Animations](https://www.thunderheadeng.com/docs/2026-1/pathfinder/advanced/animations/)
  - x [Scripting API](https://www.thunderheadeng.com/docs/2026-1/pathfinder/advanced/scripting/)
- x [Troubleshooting](https://www.thunderheadeng.com/docs/2026-1/pathfinder/troubleshooting/)
  - x [Licensing](https://www.thunderheadeng.com/docs/2026-1/pathfinder/troubleshooting/licensing/)
  - x [Crashes on Startup and Video Display Problems](https://www.thunderheadeng.com/docs/2026-1/pathfinder/troubleshooting/display-problems/)
  - x [System Memory Issues](https://www.thunderheadeng.com/docs/2026-1/pathfinder/troubleshooting/memory-issues/)
  - x [Occupants appear to be stuck](https://www.thunderheadeng.com/docs/2026-1/pathfinder/troubleshooting/occupants-stuck/)
  - x [Warnings or Errors in the Navigation Tree](https://www.thunderheadeng.com/docs/2026-1/pathfinder/troubleshooting/tree-errors/)
  - x [Reducing Computation Time](https://www.thunderheadeng.com/docs/2026-1/pathfinder/troubleshooting/run-time/)
  - x [Use Software Compatibility Driver?](https://www.thunderheadeng.com/docs/2026-1/pathfinder/troubleshooting/software-compatibility-driver/)
- [x] [The Results Viewer](https://www.thunderheadeng.com/docs/2026-1/pathfinder/results/)
  - x [Introduction](https://www.thunderheadeng.com/docs/2026-1/results/introduction/)
  - x [Changelog](https://www.thunderheadeng.com/docs/2026-1/results/changelog/)
  - x [Known Issues](https://www.thunderheadeng.com/docs/2026-1/results/known-issues/)
  - x [System Requirements](https://www.thunderheadeng.com/docs/2026-1/results/system-requirements/)
  - x [Licensing](https://www.thunderheadeng.com/docs/2026-1/results/licensing/)
  - x [User Interface](https://www.thunderheadeng.com/docs/2026-1/results/user-interface/)
    - x [Preferences](https://www.thunderheadeng.com/docs/2026-1/results/user-interface/preferences/)
    - x [Results Visualization File](https://www.thunderheadeng.com/docs/2026-1/results/user-interface/visualization-file/)
    - x [Results Timelines](https://www.thunderheadeng.com/docs/2026-1/results/user-interface/timelines/)
    - x [Navigating Through a Model](https://www.thunderheadeng.com/docs/2026-1/results/user-interface/navigation/)
    - x [Scene Camera](https://www.thunderheadeng.com/docs/2026-1/results/user-interface/camera/)
    - x [Views, Tours, and Clipping](https://www.thunderheadeng.com/docs/2026-1/results/user-interface/views/)
    - x [Displaying Geometry Input](https://www.thunderheadeng.com/docs/2026-1/results/user-interface/displaying-geometry/)
    - x [Selection](https://www.thunderheadeng.com/docs/2026-1/results/user-interface/selection/)
    - x [Viewing Multi-floor Problems](https://www.thunderheadeng.com/docs/2026-1/results/user-interface/multi-floor/)
    - x [Animation Playback](https://www.thunderheadeng.com/docs/2026-1/results/user-interface/animation-playback/)
    - x [Unit Systems](https://www.thunderheadeng.com/docs/2026-1/results/user-interface/units/)
    - x [Refreshing Results](https://www.thunderheadeng.com/docs/2026-1/results/user-interface/refreshing/)
    - x [File Streaming](https://www.thunderheadeng.com/docs/2026-1/results/user-interface/file-streaming/)
    - x [Rendering Options](https://www.thunderheadeng.com/docs/2026-1/results/user-interface/rendering-options/)
    - x [Motion Blur](https://www.thunderheadeng.com/docs/2026-1/results/user-interface/motion-blur/)
    - x [Skyboxes](https://www.thunderheadeng.com/docs/2026-1/results/user-interface/skyboxes/)
    - x [Screenshots](https://www.thunderheadeng.com/docs/2026-1/results/user-interface/screenshots/)
    - x [Creating Movies](https://www.thunderheadeng.com/docs/2026-1/results/user-interface/movies/)
    - x [Annotations](https://www.thunderheadeng.com/docs/2026-1/results/user-interface/annotations/)
  - x [Viewing PyroSim (FDS) Output](https://www.thunderheadeng.com/docs/2026-1/results/viewing-pyrosim-output/)
    - x [Navigation View Grouping](https://www.thunderheadeng.com/docs/2026-1/results/viewing-pyrosim-output/organization/)
    - x [Activating Visualizations](https://www.thunderheadeng.com/docs/2026-1/results/viewing-pyrosim-output/activating-results/)
    - x [Colorbars](https://www.thunderheadeng.com/docs/2026-1/results/viewing-pyrosim-output/colorbars/)
    - x [Global FDS Properties](https://www.thunderheadeng.com/docs/2026-1/results/viewing-pyrosim-output/global-properties/)
    - x [3D Smoke](https://www.thunderheadeng.com/docs/2026-1/results/viewing-pyrosim-output/3d-smoke/)
    - x [3D Slices](https://www.thunderheadeng.com/docs/2026-1/results/viewing-pyrosim-output/3d-slices/)
    - x [Plot3D](https://www.thunderheadeng.com/docs/2026-1/results/viewing-pyrosim-output/plot3d/)
    - x [Data3D](https://www.thunderheadeng.com/docs/2026-1/results/viewing-pyrosim-output/data3d/)
    - x [Isosurfaces](https://www.thunderheadeng.com/docs/2026-1/results/viewing-pyrosim-output/isosurfaces/)
    - x [2D Slices](https://www.thunderheadeng.com/docs/2026-1/results/viewing-pyrosim-output/2d-slices/)
    - x [2D Slice Vectors](https://www.thunderheadeng.com/docs/2026-1/results/viewing-pyrosim-output/2d-slice-vectors/)
    - x [Boundaries](https://www.thunderheadeng.com/docs/2026-1/results/viewing-pyrosim-output/boundaries/)
    - x [Particles](https://www.thunderheadeng.com/docs/2026-1/results/viewing-pyrosim-output/particles/)
    - x [Maximum Errors](https://www.thunderheadeng.com/docs/2026-1/results/viewing-pyrosim-output/max-errors/)
    - x [Devices and HRR Data](https://www.thunderheadeng.com/docs/2026-1/results/viewing-pyrosim-output/devices/)
  - [x] [Viewing Pathfinder Output](https://www.thunderheadeng.com/docs/2026-1/results/viewing-pathfinder-output/)
    - [x] [Displaying Occupants](https://www.thunderheadeng.com/docs/2026-1/results/viewing-pathfinder-output/displaying-occupants/)
    - [x] [Occupant Coloring](https://www.thunderheadeng.com/docs/2026-1/results/viewing-pathfinder-output/occupant-coloring/)
    - [x] [Occupant Spacing Disks](https://www.thunderheadeng.com/docs/2026-1/results/viewing-pathfinder-output/occupant-spacing-disks/)
    - [x] [Selecting Occupants](https://www.thunderheadeng.com/docs/2026-1/results/viewing-pathfinder-output/selecting-occupants/)
    - [x] [Viewing Occupant Paths](https://www.thunderheadeng.com/docs/2026-1/results/viewing-pathfinder-output/occupant-paths/)
    - [x] [Occupant Contours / Heat Maps](https://www.thunderheadeng.com/docs/2026-1/results/viewing-pathfinder-output/occupant-contours/)
    - [x] [Occupant Proximity Analysis](https://www.thunderheadeng.com/docs/2026-1/results/viewing-pathfinder-output/occupant-proximity/)
    - [x] [2D Object Plots](https://www.thunderheadeng.com/docs/2026-1/results/viewing-pathfinder-output/2d-object-plots/)
    - [x] [Calculating Fractional Effective Dose (FED)](https://www.thunderheadeng.com/docs/2026-1/results/viewing-pathfinder-output/fed/)
    - [x] [Calculating Occupant Visibility through Smoke](https://www.thunderheadeng.com/docs/2026-1/results/viewing-pathfinder-output/visibility/)
    - [x] [Occupant Vision Contours](https://www.thunderheadeng.com/docs/2026-1/results/viewing-pathfinder-output/vision/)
  - [x] [Viewing CSV Output](https://www.thunderheadeng.com/docs/2026-1/results/viewing-csv-output/)
    - [x] [Supported CSV Formats](https://www.thunderheadeng.com/docs/2026-1/results/viewing-csv-output/supported-formats/)
    - [x] [Loading CSV Files](https://www.thunderheadeng.com/docs/2026-1/results/viewing-csv-output/loading/)
    - [x] [Managing CSV Files](https://www.thunderheadeng.com/docs/2026-1/results/viewing-csv-output/managing/)
    - [x] [Plotting and Exporting CSV Files](https://www.thunderheadeng.com/docs/2026-1/results/viewing-csv-output/plotting-and-export/)
  - [x] [Viewing 2D Plots](https://www.thunderheadeng.com/docs/2026-1/results/viewing-2d-plots/)
  - [x] [Exporting Data](https://www.thunderheadeng.com/docs/2026-1/results/exporting-data/)
  - x [Advanced](https://www.thunderheadeng.com/docs/2026-1/results/advanced/)
    - x [VR Mode](https://www.thunderheadeng.com/docs/2026-1/results/advanced/vr-mode/)
    - x [User Scripts](https://www.thunderheadeng.com/docs/2026-1/results/advanced/user-scripts/)
  - x [Technical Reference](https://www.thunderheadeng.com/docs/2026-1/results/technical-reference/)
    - x [Occupant Contours](https://www.thunderheadeng.com/docs/2026-1/results/technical-reference/contours/)
    - x [Occupant Visibility through Smoke](https://www.thunderheadeng.com/docs/2026-1/results/technical-reference/visibility/)
  - x [Troubleshooting](https://www.thunderheadeng.com/docs/2026-1/results/troubleshooting/)
    - x [Licensing](https://www.thunderheadeng.com/docs/2026-1/results/troubleshooting/licensing/)
    - x [Crashes on Startup and Display Problems](https://www.thunderheadeng.com/docs/2026-1/results/troubleshooting/crash-on-startup/)
    - x [Stuttering FDS Results](https://www.thunderheadeng.com/docs/2026-1/results/troubleshooting/stuttering/)
    - x [Unresponsive Results](https://www.thunderheadeng.com/docs/2026-1/results/troubleshooting/unresponsive/)
    - x [Use Software Compatibility Driver?](https://www.thunderheadeng.com/docs/2026-1/results/troubleshooting/software-compatibility-driver/)
- x [Appendices](https://www.thunderheadeng.com/docs/2026-1/pathfinder/appendices/)
  - x [Appendix A: Pre-defined Speed Profiles](https://www.thunderheadeng.com/docs/2026-1/pathfinder/appendices/speed-profiles/)
  - x [Appendix B: Avatar and Animation File Format](https://www.thunderheadeng.com/docs/2026-1/pathfinder/appendices/avatars-and-animation/)
  - x [Technical Reference](https://www.thunderheadeng.com/docs/2026-1/pathfinder/appendices/technical-reference/)
    - x [Geometry](https://www.thunderheadeng.com/docs/2026-1/pathfinder/appendices/technical-reference/geometry/)
    - x [Behaviors and Goals](https://www.thunderheadeng.com/docs/2026-1/pathfinder/appendices/technical-reference/behaviors/)
    - x [Pathfinding](https://www.thunderheadeng.com/docs/2026-1/pathfinder/appendices/technical-reference/pathfinding/)
    - x [SFPE Mode](https://www.thunderheadeng.com/docs/2026-1/pathfinder/appendices/technical-reference/sfpe/)
    - x [Steering Mode](https://www.thunderheadeng.com/docs/2026-1/pathfinder/appendices/technical-reference/steering/)
    - x [Assisted Evacuation](https://www.thunderheadeng.com/docs/2026-1/pathfinder/appendices/technical-reference/assisted-evac/)
    - x [Vehicle Agents](https://www.thunderheadeng.com/docs/2026-1/pathfinder/appendices/technical-reference/vehicles/)
    - x [Elevators](https://www.thunderheadeng.com/docs/2026-1/pathfinder/appendices/technical-reference/elevators/)
    - x [Solution Procedure](https://www.thunderheadeng.com/docs/2026-1/pathfinder/appendices/technical-reference/solution/)
    - x [Input File Format](https://www.thunderheadeng.com/docs/2026-1/pathfinder/appendices/technical-reference/input-file/)
    - x [View File Format](https://www.thunderheadeng.com/docs/2026-1/pathfinder/appendices/technical-reference/view-file/)
    - x [Fractional Effective Dose (FED)](https://www.thunderheadeng.com/docs/2026-1/pathfinder/appendices/technical-reference/fed/)
    - x [Measurement Regions](https://www.thunderheadeng.com/docs/2026-1/pathfinder/appendices/technical-reference/measurement-regions/)
  - x [Verification and Validation](https://www.thunderheadeng.com/docs/2026-1/pathfinder/appendices/verification-validation/)
    - x [Fundamental Diagram Tests](https://www.thunderheadeng.com/docs/2026-1/pathfinder/appendices/verification-validation/fundamental-diagrams/)
      - x [Fundamental Diagram for Unidirectional Flow](https://www.thunderheadeng.com/docs/2026-1/pathfinder/appendices/verification-validation/fundamental-diagrams/unidirectional/)
      - x [Fundamental Diagram for Bidirectional Flow](https://www.thunderheadeng.com/docs/2026-1/pathfinder/appendices/verification-validation/fundamental-diagrams/bidirectional/)
      - x [Fundmanetal Diagram for Merging of Pedestrian Streams in T-Junction](https://www.thunderheadeng.com/docs/2026-1/pathfinder/appendices/verification-validation/fundamental-diagrams/t-junction/)
      - x [Fundamental Diagram Customization for Stairs and Ramps](https://www.thunderheadeng.com/docs/2026-1/pathfinder/appendices/verification-validation/fundamental-diagrams/stairs-ramps/)
    - x [Flow Rate Tests](https://www.thunderheadeng.com/docs/2026-1/pathfinder/appendices/verification-validation/flow-rate/)
      - x [Door Flow Rates](https://www.thunderheadeng.com/docs/2026-1/pathfinder/appendices/verification-validation/flow-rate/doors/)
      - x [Stair Flow Rates](https://www.thunderheadeng.com/docs/2026-1/pathfinder/appendices/verification-validation/flow-rate/stairs/)
      - x [Corridor Flow Rates](https://www.thunderheadeng.com/docs/2026-1/pathfinder/appendices/verification-validation/flow-rate/corridors/)
    - x [Behavior Tests](https://www.thunderheadeng.com/docs/2026-1/pathfinder/appendices/verification-validation/behavior/)
      - x [Refuge Room as Final Destination](https://www.thunderheadeng.com/docs/2026-1/pathfinder/appendices/verification-validation/behavior/refuge/)
      - x [Grouping Behavior](https://www.thunderheadeng.com/docs/2026-1/pathfinder/appendices/verification-validation/behavior/grouping/)
      - x [Corridor Merging](https://www.thunderheadeng.com/docs/2026-1/pathfinder/appendices/verification-validation/behavior/corridor-merging/)
      - x [Stairway Merging](https://www.thunderheadeng.com/docs/2026-1/pathfinder/appendices/verification-validation/behavior/stairway-merging/)
      - x [Passing Slow Occupants on Stairs](https://www.thunderheadeng.com/docs/2026-1/pathfinder/appendices/verification-validation/behavior/passing-on-stairs/)
      - x [Elevator Loading](https://www.thunderheadeng.com/docs/2026-1/pathfinder/appendices/verification-validation/behavior/elevator-loading/)
      - x [Use of Corridor during Cornering](https://www.thunderheadeng.com/docs/2026-1/pathfinder/appendices/verification-validation/behavior/corridor-cornering/)
    - x [Special Program Features](https://www.thunderheadeng.com/docs/2026-1/pathfinder/appendices/verification-validation/special-features/)
      - x [Assisted Evacuation](https://www.thunderheadeng.com/docs/2026-1/pathfinder/appendices/verification-validation/special-features/assisted-evac/)
      - x [Source Flow Rates](https://www.thunderheadeng.com/docs/2026-1/pathfinder/appendices/verification-validation/special-features/flow-rates/)
      - x [Fractional Effective Dose (FED) Calculation](https://www.thunderheadeng.com/docs/2026-1/pathfinder/appendices/verification-validation/special-features/fed/)
      - x [Walking Speed Reduction Due to Smoke](https://www.thunderheadeng.com/docs/2026-1/pathfinder/appendices/verification-validation/special-features/walk-speed-smoke/)
      - x [Social Distancing](https://www.thunderheadeng.com/docs/2026-1/pathfinder/appendices/verification-validation/special-features/social-distancing/)
    - x [IMO Tests](https://www.thunderheadeng.com/docs/2026-1/pathfinder/appendices/verification-validation/imo/)
      - x [IMO 1](https://www.thunderheadeng.com/docs/2026-1/pathfinder/appendices/verification-validation/imo/imo-1/)
      - x [IMO 2](https://www.thunderheadeng.com/docs/2026-1/pathfinder/appendices/verification-validation/imo/imo-2/)
      - x [IMO 3](https://www.thunderheadeng.com/docs/2026-1/pathfinder/appendices/verification-validation/imo/imo-3/)
      - x [IMO 4](https://www.thunderheadeng.com/docs/2026-1/pathfinder/appendices/verification-validation/imo/imo-4/)
      - x [IMO 5](https://www.thunderheadeng.com/docs/2026-1/pathfinder/appendices/verification-validation/imo/imo-5/)
      - x [IMO 6](https://www.thunderheadeng.com/docs/2026-1/pathfinder/appendices/verification-validation/imo/imo-6/)
      - x [IMO 7](https://www.thunderheadeng.com/docs/2026-1/pathfinder/appendices/verification-validation/imo/imo-7/)
      - x [IMO 8](https://www.thunderheadeng.com/docs/2026-1/pathfinder/appendices/verification-validation/imo/imo-8/)
      - x [IMO 9](https://www.thunderheadeng.com/docs/2026-1/pathfinder/appendices/verification-validation/imo/imo-9/)
      - x [IMO 10](https://www.thunderheadeng.com/docs/2026-1/pathfinder/appendices/verification-validation/imo/imo-10/)
      - x [IMO 11](https://www.thunderheadeng.com/docs/2026-1/pathfinder/appendices/verification-validation/imo/imo-11/)
    - x [NIST Evacuation Tests](https://www.thunderheadeng.com/docs/2026-1/pathfinder/appendices/verification-validation/nist/)
      - x [NIST Verif 1.1](https://www.thunderheadeng.com/docs/2026-1/pathfinder/appendices/verification-validation/nist/nist-1-1/)
      - x [NIST Verif 2.1](https://www.thunderheadeng.com/docs/2026-1/pathfinder/appendices/verification-validation/nist/nist-2-1/)
      - x [NIST Verif 2.2](https://www.thunderheadeng.com/docs/2026-1/pathfinder/appendices/verification-validation/nist/nist-2-2/)
      - x [NIST Verif 2.3](https://www.thunderheadeng.com/docs/2026-1/pathfinder/appendices/verification-validation/nist/nist-2-3/)
      - x [NIST Verif 2.4](https://www.thunderheadeng.com/docs/2026-1/pathfinder/appendices/verification-validation/nist/nist-2-4/)
      - x [NIST Verif 2.5](https://www.thunderheadeng.com/docs/2026-1/pathfinder/appendices/verification-validation/nist/nist-2-5/)
      - x [NIST Verif 2.6](https://www.thunderheadeng.com/docs/2026-1/pathfinder/appendices/verification-validation/nist/nist-2-6/)
      - x [NIST Verif 2.7](https://www.thunderheadeng.com/docs/2026-1/pathfinder/appendices/verification-validation/nist/nist-2-7/)
      - x [NIST Verif 2.8](https://www.thunderheadeng.com/docs/2026-1/pathfinder/appendices/verification-validation/nist/nist-2-8/)
      - x [NIST Verif 2.9](https://www.thunderheadeng.com/docs/2026-1/pathfinder/appendices/verification-validation/nist/nist-2-9/)
      - x [NIST Verif 2.10](https://www.thunderheadeng.com/docs/2026-1/pathfinder/appendices/verification-validation/nist/nist-2-10/)
      - x [NIST Verif 3.1](https://www.thunderheadeng.com/docs/2026-1/pathfinder/appendices/verification-validation/nist/nist-3-1/)
      - x [NIST Verif 3.2](https://www.thunderheadeng.com/docs/2026-1/pathfinder/appendices/verification-validation/nist/nist-3-2/)
      - x [NIST Verif 3.3](https://www.thunderheadeng.com/docs/2026-1/pathfinder/appendices/verification-validation/nist/nist-3-3/)
      - x [NIST Verif 4.1](https://www.thunderheadeng.com/docs/2026-1/pathfinder/appendices/verification-validation/nist/nist-4-1/)
      - x [NIST Verif 5.1](https://www.thunderheadeng.com/docs/2026-1/pathfinder/appendices/verification-validation/nist/nist-5-1/)
      - x [NIST Verif 5.2](https://www.thunderheadeng.com/docs/2026-1/pathfinder/appendices/verification-validation/nist/nist-5-2/)
    - x [SFPE Example Problems](https://www.thunderheadeng.com/docs/2026-1/pathfinder/appendices/verification-validation/sfpe/)
      - x [SFPE Example 1](https://www.thunderheadeng.com/docs/2026-1/pathfinder/appendices/verification-validation/sfpe/sfpe-1/)
      - x [SFPE Example 2](https://www.thunderheadeng.com/docs/2026-1/pathfinder/appendices/verification-validation/sfpe/sfpe-2/)
    - x [Archive](https://www.thunderheadeng.com/docs/2026-1/pathfinder/appendices/verification-validation/archive/)
  - x [Additional Resources](https://www.thunderheadeng.com/docs/2026-1/pathfinder/appendices/resources/)
## 한눈에 보는 결론
- Pathfinder는 단순한 출구 탐색 도구보다 넓다. 공간 컴포넌트, 인원 프로필, 행동 스크립트, 동적 유입, 그룹 이동, 출력 CSV, 후처리 시각화까지 한 모델 안에서 연결한다.
- SafeCrowd 관점에서 가장 먼저 참고할 축은 `Geometry`, `Profiles`, `Behaviors`, `Triggers`, `Simulation`, `Output/Results`다.
- 현재 우선순위 기준으로는 `groups/queues/targets/results viewer 고도화`가 1차 확장이고, `elevators/assisted evacuation/vision/FED`는 중기 확장으로 두는 편이 더 타당하다.

## 1. 공간 모델링 기능

### 1.1. 방-문 중심 연결 구조
- 관련 문서: [Floors](https://www.thunderheadeng.com/docs/2026-1/pathfinder/geometry/floors/), [Rooms](https://www.thunderheadeng.com/docs/2026-1/pathfinder/geometry/rooms/), [Walls](https://www.thunderheadeng.com/docs/2026-1/pathfinder/geometry/walls/), [Obstructions](https://www.thunderheadeng.com/docs/2026-1/pathfinder/geometry/obstructions/), [Obstacles](https://www.thunderheadeng.com/docs/2026-1/pathfinder/geometry/obstacles/), [Doors](https://www.thunderheadeng.com/docs/2026-1/pathfinder/geometry/doors/)
- [Floors](https://www.thunderheadeng.com/docs/2026-1/pathfinder/geometry/floors/)는 각 층이 2D 작업면이 아니라 `Elevation`, `Default Floor Height`, `Map Type`을 가진 독립 객체라고 설명한다. 즉, 수직 좌표와 CAD 배경 기준을 층 레벨에서 먼저 잡는 구조다.
- [Rooms](https://www.thunderheadeng.com/docs/2026-1/pathfinder/geometry/rooms/)는 실제 이동 가능한 navigation 면을 만든다. room마다 `Speed Modifier`, `Color/Opacity`, `Refuge Area`, `Accepted Profiles`를 둘 수 있어, 단순 공간 구획이 아니라 속도·시각화·대피 목적지·프로필 접근 제약까지 함께 가진다.
- [Walls](https://www.thunderheadeng.com/docs/2026-1/pathfinder/geometry/walls/)는 room 경계를 정의하는 기본 기하 요소다. 문서상 wall을 기준으로 room을 그리고, wall 높이와 연결 관계가 navigation 추출과 문 배치 가능성에 직접 연결된다.
- [Obstructions](https://www.thunderheadeng.com/docs/2026-1/pathfinder/geometry/obstructions/)은 영구적인 통과 불가 고체로 취급된다. CAD/BIM import 후 room extraction에서 제외하거나 포함하는 기준이 되고, navigation mesh를 깎아내는 정적 장애물에 가깝다.
- Pathfinder에서는 두 방 사이를 사람이 지나가려면 반드시 [Door](https://www.thunderheadeng.com/docs/2026-1/pathfinder/geometry/doors/)가 필요하다. 문은 단순 연결점이 아니라 결과 분석에서 유량 측정 지점으로도 쓰이고, `SFPE` 모드에서는 핵심 유량 제어 지점으로 동작한다.
- Door 생성 시 `Min Width`, `Max Width`, `Max Depth` 같은 파라미터로 문을 놓을 수 있는 후보 엣지를 제한한다.
- [Obstacles](https://www.thunderheadeng.com/docs/2026-1/pathfinder/geometry/obstacles/)는 obstruction과 달리 동적 환경 변화를 위한 transient patch다. `Speed Modifier`나 `Speed Limit`을 시간 가변값으로 주어 감속 또는 완전 회피를 만들고, 필요하면 `Prevent Occupant Placement`로 초기 배치/재배치도 막을 수 있다. 문서 예시도 연기, 화재, 바리케이드, 수하물 같은 가변 위험을 obstacle로 다룬다.
- SafeCrowd 시사점:
  - 우리도 `Floor -> Room/Zone -> Wall/Boundary -> Doorway` 계층을 분리해야 한다.
  - 문은 단순 통과 가능 여부만이 아니라 병목, 유량, 대기열의 측정 지점이 되어야 한다.
  - 또 정적 장애물과 동적 위험 구역을 하나로 합치면 안 된다. Pathfinder처럼 `obstruction(정적)`과 `obstacle(동적)`를 분리하는 편이 모델과 결과 해석 모두에 유리하다.

### 1.2. 수직 이동 컴포넌트
- 관련 문서: [Elevators](https://www.thunderheadeng.com/docs/2026-1/pathfinder/geometry/elevators/), [Introduction](https://www.thunderheadeng.com/docs/2026-1/pathfinder/geometry/elevators/introduction/), [Configuration](https://www.thunderheadeng.com/docs/2026-1/pathfinder/geometry/elevators/configuration/), [Creating an Elevator from a Single Room](https://www.thunderheadeng.com/docs/2026-1/pathfinder/geometry/elevators/creating/), [Creating Multiple Elevators from Different Rooms](https://www.thunderheadeng.com/docs/2026-1/pathfinder/geometry/elevators/creating-multiple/), [Creating an Elevator Bank](https://www.thunderheadeng.com/docs/2026-1/pathfinder/geometry/elevators/creating-banks/), [Connecting / Disconnecting Floors from an Elevator](https://www.thunderheadeng.com/docs/2026-1/pathfinder/geometry/elevators/connecting-disconnecting/), [Technical Reference - Elevators](https://www.thunderheadeng.com/docs/2026-1/pathfinder/appendices/technical-reference/elevators/), [Verification - Elevator Loading](https://www.thunderheadeng.com/docs/2026-1/pathfinder/appendices/verification-validation/behavior/elevator-loading/)
- Geometry 섹션은 `Stairs`, `Ramps`, `Escalators`, `Moving Walkways`, [Elevators](https://www.thunderheadeng.com/docs/2026-1/pathfinder/geometry/elevators/)를 별도 주제로 둔다.
- [Introduction](https://www.thunderheadeng.com/docs/2026-1/pathfinder/geometry/elevators/introduction/)은 elevator를 여러 floor에 퍼진 room 집합으로 정의하고, `EVAC`과 `SCAN` 두 운행 타입을 구분한다. `EVAC`은 단일 discharge floor와 prioritized pickup floor 목록을 가지며, 승객을 태운 뒤에는 discharge만 수행한다. `SCAN`은 일반 엘리베이터처럼 호출 방향을 따라 층을 sweep한다.
- 같은 문서는 `Double Deck Elevators`도 지원한다고 설명한다. 다만 짝수/홀수 floor parity 제약, level 간 vertical spacing 제약이 있고, 잘못 설정하면 occupant가 stuck 될 수 있다고 적고 있다.
- [Configuration](https://www.thunderheadeng.com/docs/2026-1/pathfinder/geometry/elevators/configuration/)은 `Nominal Load`, `Open Delay`, `Close Delay`, `Discharge Floor`, `Floor Priority`, `Level Data`, `Initial Floor`, `Call Distance`를 둔다. 특히 nominal load는 steering mode에서 elevator 내부 occupant 크기 스케일링과 연결되고, floor별 `Open+Close Time`과 `Delay`를 별도로 줄 수 있다.
- [Creating an Elevator from a Single Room](https://www.thunderheadeng.com/docs/2026-1/pathfinder/geometry/elevators/creating/)와 [Creating Multiple Elevators from Different Rooms](https://www.thunderheadeng.com/docs/2026-1/pathfinder/geometry/elevators/creating-multiple/)는 base room을 기준으로 elevator shaft를 자동 생성한다. 여러 elevator를 한 번에 만들 수 있지만, 서로 다른 층의 base room이 2D 평면에서 겹치면 선택 순서에 따라 기존 shaft를 침범해 형상이 망가질 수 있다고 경고한다.
- [Creating an Elevator Bank](https://www.thunderheadeng.com/docs/2026-1/pathfinder/geometry/elevators/creating-banks/)는 같은 call signal을 공유하는 elevator 묶음을 만든다. 즉, 단일 shaft보다 `bank-level dispatch`를 먼저 모델에 올려둔 구조다.
- [Connecting / Disconnecting Floors from an Elevator](https://www.thunderheadeng.com/docs/2026-1/pathfinder/geometry/elevators/connecting-disconnecting/)은 shaft가 닿는 층에 기본 연결되지만, 특정 floor의 door 또는 level 전체를 disable해서 pickup/egress를 막을 수 있다고 설명한다.
- 2026.1 tree의 `Technical Reference`와 `Verification`은 도구에서 본문을 직접 열 수 없었지만, 공식 appendices의 대응 문서인 [Technical Reference - Elevators](https://www.thunderheadeng.com/docs/2026-1/pathfinder/appendices/technical-reference/elevators/)와 [Verification - Elevator Loading](https://www.thunderheadeng.com/docs/2026-1/pathfinder/appendices/verification-validation/behavior/elevator-loading/)로 동작 근거를 확인했다.
- [Technical Reference - Elevators](https://www.thunderheadeng.com/docs/2026-1/pathfinder/appendices/technical-reference/elevators/)는 Pathfinder가 moving platform 대신 `discharge room + pickup rooms` 구조로 elevator를 모델링한다고 설명한다. 운행은 `Idling -> Pickup -> Discharge` 3단계로 나뉘고, 호출은 목표 elevator가 부여된 occupant가 문 0.5m 이내에 왔을 때 발생한다.
- 같은 기술 문서는 elevator 선택이 `Locally Quickest` 변형으로 이뤄지며, global travel time에 elevator 도착 추정 시간과 discharge 후 nearest exit까지의 걷기 시간을 포함한다고 설명한다. 또 steering mode에서는 nominal load를 맞추기 위해 elevator 내부 collision을 꺼서 loading을 달성한다.
- [Verification - Elevator Loading](https://www.thunderheadeng.com/docs/2026-1/pathfinder/appendices/verification-validation/behavior/elevator-loading/)은 nominal load가 5, 10, 20, 50일 때 observed loading이 기대값과 맞는지 검증하고, steering/steering+SFPE/SFPE 모드에서 모두 기대 loading과 일치했다고 보고한다.
- SafeCrowd 시사점:
  - 초기 버전은 계단과 문 중심으로 시작하되, 데이터 모델은 엘리베이터 같은 특수 이동 수단을 나중에 추가할 수 있게 열어 두는 편이 좋다.
  - 재난 대응 시나리오를 생각하면 승강기 사용 제한, 우선 호출, 구조 인력 전용 동작, 층별 enable/disable 같은 상태 전이는 별도 이벤트 모델로 빼는 편이 맞다.
  - 특히 `EVAC/SCAN`, `floor priority`, `door timing`, `bank dispatch`는 단순 path edge 하나로는 표현이 안 된다. SafeCrowd가 elevator를 넣을 때는 connector가 아니라 `stateful transport system`으로 보는 편이 정확하다.
  - 기술 문서가 보여 주듯 elevator는 `호출 로직`, `door timer`, `loading model`, `discharge policy`가 핵심이다. SafeCrowd가 나중에 승강기를 넣을 때도 geometry보다 state machine을 먼저 설계하는 편이 맞다.

### 1.3. 외부 도면/모델 입력과 시각 자료 활용
- 관련 문서: [Generating Geometry from CAD](https://www.thunderheadeng.com/docs/2026-1/pathfinder/geometry/generating-from-cad/), [Materials](https://www.thunderheadeng.com/docs/2026-1/pathfinder/geometry/materials/)
- Geometry 문서 구조상 `Raster Images`, `Creating Geometry from Images`, `Generating Geometry from CAD`, `IFC` 계열 워크플로가 모두 독립 기능으로 존재한다.
- [Materials](https://www.thunderheadeng.com/docs/2026-1/pathfinder/geometry/materials/) 문서는 DWG/FBX/DAE/OBJ/glTF/GLB/FDS/PSM에서 재질 또는 시각 속성을 가져와 표시용으로 활용한다고 설명한다.
- [Generating Geometry from CAD](https://www.thunderheadeng.com/docs/2026-1/pathfinder/geometry/generating-from-cad/) 문서는 IFC, DXF, DWG, FBX, DAE, OBJ, glTF/GLB 등 여러 포맷을 가져온 뒤, `Generate Model from BIM`으로 방·문·계단을 자동 생성할 수 있다고 설명한다.
- 특히 IFC는 문서상 건물 요소 타입 정보가 풍부해 `doors`, `stairs`, `slabs`를 Pathfinder 요소로 바꾸는 흐름이 가장 매끄러운 포맷으로 소개된다.
- 비 IFC 포맷은 기본적으로 많은 객체가 `Obstruction`으로 들어오므로, `Import Type`을 수동 조정한 뒤 자동 생성 또는 수동 보정을 하는 흐름이 권장된다.
- 2D CAD와 3D CAD 모두에서 `Extract Room` 도구를 통해 방을 뽑아낼 수 있고, 빈틈이 있으면 `Gap Tolerance`, `Thin Wall`, `Set Z` 같은 보정 절차를 거친다.
- SafeCrowd 시사점:
  - 우리도 장기적으로는 `이미지 기반 스케치 -> 구조 추출`, `CAD/IFC 가져오기`, `수동 보정`의 3단계를 염두에 두는 편이 좋다.
  - 다만 MVP는 완전한 CAD 파이프라인보다 `간단한 평면 구조 입력 + DXF/IFC 확장 여지`가 현실적이다.

### 1.4. 계단 모델링의 세부 조건
- 관련 문서: [Stairs](https://www.thunderheadeng.com/docs/2026-1/pathfinder/geometry/stairs/)
- [Stairs](https://www.thunderheadeng.com/docs/2026-1/pathfinder/geometry/stairs/) 문서는 계단을 하나의 straight-run으로 보고, 방 경계 사이에 직접 만들거나 특정 기준을 만족할 때까지 뻗는 방식으로 생성할 수 있다고 설명한다.
- 중요한 점은 계단 양 끝이 방 경계와 연결되어야 하고, 위아래 끝단에 가장 큰 점유자 반경 이상 빈 공간이 있어야 정상적으로 시뮬레이션된다는 것이다.
- 또한 문서상 계단의 실제 표시 기울기는 시각화용이고, 이동 속도는 `tread`와 `run` 값으로 계산된다.
- SafeCrowd 시사점:
  - 계단은 단순 높이 차 연결선이 아니라, 끝단 연결 조건과 유효 이동 파라미터를 가진 특수 커넥터로 다뤄야 한다.
  - 속도 모델에서 계단 기하 형상과 이동 성능 파라미터를 분리하는 편이 구현상 유리하다.

### 1.5. Ramp, Escalator, Moving Walkway의 공통 구조
- 관련 문서: [Ramps](https://www.thunderheadeng.com/docs/2026-1/pathfinder/geometry/ramps/), [Escalators](https://www.thunderheadeng.com/docs/2026-1/pathfinder/geometry/escalators/), [Moving Walkways](https://www.thunderheadeng.com/docs/2026-1/pathfinder/geometry/moving-walkways/)
- [Ramps](https://www.thunderheadeng.com/docs/2026-1/pathfinder/geometry/ramps/)는 생성 방식과 표현이 계단과 거의 같고, 양 끝의 implicit door와 직사각형 형상을 그대로 가진다. 차이는 기본적으로 occupant 속도를 바꾸지 않는다는 점이다.
- [Escalators](https://www.thunderheadeng.com/docs/2026-1/pathfinder/geometry/escalators/)는 별도 객체라기보다 계단에 `one-way direction`과 `Speed Constant`를 부여한 형태다. 이 speed constant는 시간 가변값이 될 수 있어, 시뮬레이션 중 에스컬레이터를 켜고 끄는 용도로도 쓸 수 있다.
- 문서상 escalator는 Results에서 시각적으로 stairs와 구분되지 않으며, occupant profile의 `Escalator Preference`에 따라 걷거나 특정 측에 서는 동작이 바뀐다. 에스컬레이터가 꺼지면 일반 stairs처럼 사용된다.
- [Moving Walkways](https://www.thunderheadeng.com/docs/2026-1/pathfinder/geometry/moving-walkways/)는 ramp에 speed constant를 주는 방식으로 설명된다. 즉, 평평한 ramp를 walkway처럼 사용한다.
- SafeCrowd 시사점:
  - `stairs`, `ramp`, `escalator`, `moving walkway`를 전부 별도 시스템으로 쪼개기보다, 공통 connector 타입 위에 `경사`, `방향`, `속도 modifier`, `운영 on/off`를 얹는 구조가 더 실용적이다.
  - ramp의 속도 영향은 커넥터 자체보다 프로필 속도 모델과 결합해 결정하고, escalator/walkway는 스케줄 가능한 이동 보조장치로 보는 편이 맞다.

### 1.6. 공간 모델링에서 바로 참고할 점
- 문을 별도 객체로 두고, 방/구역 연결 관계를 명시한다.
- 수직 이동은 `stairs`, `ramp`, `elevator`처럼 타입이 다른 연결 컴포넌트로 다룬다.
- 결과 분석용 지점은 문과 계단처럼 의미 있는 경계 단위에 둔다.

## 2. 인원 프로필과 이동 제약 기능

### 2.1. 프로필 단위 고급 파라미터
- 관련 문서: [Profiles](https://www.thunderheadeng.com/docs/2026-1/pathfinder/profiles/), [Advanced Tab](https://www.thunderheadeng.com/docs/2026-1/pathfinder/profiles/advanced/)
- [Advanced Tab](https://www.thunderheadeng.com/docs/2026-1/pathfinder/profiles/advanced/)에는 가속 시간, 충돌 응답 시간, 벽 경계 거리, 대기열에서의 개인 간격, 사회적 거리두기 적용 여부와 거리, 연기 속 최대 속도 제한 같은 고급 파라미터가 있다.
- 이 파라미터들은 상수뿐 아니라 `uniform`, `normal`, `log-normal` 같은 분포로도 줄 수 있다.
- SafeCrowd 시사점:
  - 고정 속도 하나만으로는 부족하다.
  - `desired speed`, `acceleration`, `body clearance`, `personal distance`, `smoke/visibility penalty` 같은 값을 프로필 또는 분포 수준에서 관리할 필요가 있다.

### 2.2. 사회적 거리와 대기열 간격의 구분
- 관련 문서: [Advanced Tab](https://www.thunderheadeng.com/docs/2026-1/pathfinder/profiles/advanced/)
- Pathfinder는 [Advanced Tab](https://www.thunderheadeng.com/docs/2026-1/pathfinder/profiles/advanced/)에서 `Personal Distance`와 `Social Distance`를 구분한다.
- `Personal Distance`는 대기열에서 사람 모양 사이의 간격에 가깝고, `Social Distance`는 더 엄격한 중심 간 거리 제약으로 설명된다.
- SafeCrowd 시사점:
  - 우리도 모든 회피를 하나의 반발력으로 합치기보다, `일상 보행 간격`, `대기열 간격`, `강제 분리 규칙`을 구분하는 편이 모델 확장에 유리하다.

### 2.3. 연기/가시성 연동
- 관련 문서: [Advanced Tab](https://www.thunderheadeng.com/docs/2026-1/pathfinder/profiles/advanced/)
- FDS 출력과 연결되면 [Advanced Tab](https://www.thunderheadeng.com/docs/2026-1/pathfinder/profiles/advanced/)의 `Speed in Smoke` 옵션으로 가시성이 낮을수록 최대 이동 속도를 제한할 수 있다.
- SafeCrowd 시사점:
  - 현재 위험 정의 문서의 `VisibilityField`, `SmokeDensityField` 구상과 잘 맞는다.
  - 향후 연기/시야 상태는 경로 선택뿐 아니라 속도 상한에도 직접 연결하는 편이 적절하다.

### 2.4. 컴포넌트 사용 제약
- 관련 문서: [Restrictions Tab](https://www.thunderheadeng.com/docs/2026-1/pathfinder/profiles/restrictions/)
- [Restrictions Tab](https://www.thunderheadeng.com/docs/2026-1/pathfinder/profiles/restrictions/)은 프로필별로 어떤 컴포넌트를 쓸 수 있는지 정한다.
- 예를 들어 특정 프로필은 모든 계단/문/에스컬레이터/엘리베이터를 사용할 수 있게 하거나, 일부만 허용하거나, 에스컬레이터는 `Up Only`/`Down Only`, 엘리베이터는 `Based on Behavior`로 제한할 수 있다.
- SafeCrowd 시사점:
  - 보행 약자, 운영 요원, 일반 방문객, 구조대원을 같은 길찾기 규칙으로 처리하면 안 된다.
  - `profile -> usable connectors` 제약은 초기부터 넣어두는 것이 좋다.

### 2.5. Characteristics Tab의 분포형 속도와 재현성
- 관련 문서: [Characteristics Tab](https://www.thunderheadeng.com/docs/2026-1/pathfinder/profiles/characteristics/)
- [Characteristics Tab](https://www.thunderheadeng.com/docs/2026-1/pathfinder/profiles/characteristics/)은 프로필 수준에서 `Priority Level`, 최대 속도, 신체 특성, 확률 분포 기반 파라미터를 다룬다.
- 속도는 `constant`, `uniform`, `normal`, `lognormal` 분포를 사용할 수 있고, 각 Occupant는 고유 random seed를 가져 같은 입력 모델이면 같은 개체 속성이 반복 실행마다 유지되도록 한다.
- 문서상 고급 속도 속성에서는 `Level Terrain`, `Stairs`, `Ramps`별 속도 모델을 따로 둘 수 있고, 기본 `SFPE` fundamental diagram 대신 테이블 기반 speed-density profile을 줄 수도 있다.
- SafeCrowd 시사점:
  - 단순히 “프로필마다 평균 속도”만 두기보다, 분포와 seed를 함께 저장해야 반복 실험과 재현성이 성립한다.
  - 지형별 속도 함수와 density-speed 관계를 프로필 레벨에서 교체 가능하게 두면 SafeCrowd의 실험 범위가 넓어진다.

### 2.6. Movement Tab의 보조 대피와 trigger 반응도
- 관련 문서: [Movement Tab](https://www.thunderheadeng.com/docs/2026-1/pathfinder/profiles/movement/)
- [Movement Tab](https://www.thunderheadeng.com/docs/2026-1/pathfinder/profiles/movement/)은 초기 방향, `Requires Assistance to Move`, 일방향 문 무시 여부, 에스컬레이터 선호, trigger susceptibility를 다룬다.
- 특히 `Requires Assistance to Move`는 스스로 이동할 수 없는 점유자를 보조 대피 시나리오와 직접 연결한다.
- 또 `Trigger Susceptibility (Seeking/Waiting)`와 `Allowed Triggers`를 통해, 같은 trigger라도 점유자 유형에 따라 반응 확률과 사용 가능 여부를 다르게 줄 수 있다.
- SafeCrowd 시사점:
  - 보행 약자와 구조대/운영 요원 모델은 단순 속도 저하만으로는 부족하고, 보조 이동 의존성을 별도 상태로 가져야 한다.
  - 운영 이벤트는 전원에게 동일하게 먹는 전역 스위치보다, 점유자 유형별 반응 확률까지 포함한 모델이 더 현실적이다.

### 2.7. Door Choice Tab의 출구 선택 비용 모델
- 관련 문서: [Door Choice Tab](https://www.thunderheadeng.com/docs/2026-1/pathfinder/profiles/door-choice/)
- [Door Choice Tab](https://www.thunderheadeng.com/docs/2026-1/pathfinder/profiles/door-choice/)은 출구 선택을 단순 최단거리 하나가 아니라 `Current Room Travel Time`, `Current Room Queue Time`, `Global Travel Time`, `Elevator Wait Time`, `Current Door Preference`, `Current Room Distance Penalty`의 조합으로 설명한다.
- 이 모델은 같은 방 안에서 가까운 문까지의 비용, 그 문 앞 대기열 비용, 문을 지난 뒤 전역 경로 비용, 엘리베이터를 얼마나 기다릴지, 이미 고른 문을 얼마나 유지할지를 분리한다.
- SafeCrowd 시사점:
  - 출구 선택은 “nearest exit” 규칙 하나로 두면 병목과 우회가 과하게 단순화된다.
  - 최소한 `국소 이동 시간`, `문 앞 대기열`, `전역 잔여 경로`, `문 고착도(door stickiness)` 정도는 분리된 비용 항으로 고려할 가치가 있다.

## 3. 행동 스크립트와 시나리오 제어 기능

### 3.1. 행동은 순차 액션 스크립트
- 관련 문서: [Behaviors](https://www.thunderheadeng.com/docs/2026-1/pathfinder/behaviors/), [Creating a New Behavior](https://www.thunderheadeng.com/docs/2026-1/pathfinder/behaviors/creating-new/)
- [Creating a New Behavior](https://www.thunderheadeng.com/docs/2026-1/pathfinder/behaviors/creating-new/) 문서는 행동을 생성한 뒤 액션을 순서대로 추가하는 구조를 설명한다.
- 행동에는 `Initial Delay`가 있고, 액션은 방/웨이포인트/엘리베이터로 이동하거나, 기다리거나, 다른 행동으로 바꾸는 식으로 쌓는다.
- `Resume Prior Behavior`도 지원해, 일시적으로 다른 행동으로 전환했다가 원래 행동 흐름으로 복귀할 수 있다.
- SafeCrowd 시사점:
  - 우리도 시나리오 이벤트를 단순 플래그 모음으로 두기보다, `행동 상태기계` 또는 `행동 시퀀스`로 다루는 편이 확장성이 좋다.
  - 특히 “평상시 이동 -> 경보 발생 -> 대피 행동 전환 -> 상황 종료 후 복귀” 같은 패턴은 명시적 행동 전환 모델이 있어야 자연스럽다.

### 3.2. 트리거 기반 행동 전환
- 관련 문서: [Triggers](https://www.thunderheadeng.com/docs/2026-1/pathfinder/triggers/), [Creating Triggers](https://www.thunderheadeng.com/docs/2026-1/pathfinder/triggers/creating/), [Trigger Behavior](https://www.thunderheadeng.com/docs/2026-1/pathfinder/triggers/behavior/), [Occupant Response to a Trigger](https://www.thunderheadeng.com/docs/2026-1/pathfinder/triggers/response/), [Trigger Memory](https://www.thunderheadeng.com/docs/2026-1/pathfinder/triggers/memory/), [Trigger Rank](https://www.thunderheadeng.com/docs/2026-1/pathfinder/triggers/rank/), [Trigger Properties](https://www.thunderheadeng.com/docs/2026-1/pathfinder/triggers/properties/), [Trigger Limitations](https://www.thunderheadeng.com/docs/2026-1/pathfinder/triggers/limitations/), [Trigger Output](https://www.thunderheadeng.com/docs/2026-1/pathfinder/triggers/output/)
- [Trigger Properties](https://www.thunderheadeng.com/docs/2026-1/pathfinder/triggers/properties/) 문서는 트리거마다 `Rank`, `Behavior`, `Resume if interrupted`, `Wait Area Radius` 같은 속성을 둔다.
- [Creating Triggers](https://www.thunderheadeng.com/docs/2026-1/pathfinder/triggers/creating/)는 트리거를 사전에 수동 배치하거나, 시뮬레이션 중 `Trigger Template`에서 런타임 생성하는 두 흐름을 분리한다. 전자는 전역 경보나 고정 표지처럼 쓰고, 후자는 occupant의 행동 안에서 동적으로 만들어진다.
- [Trigger Behavior](https://www.thunderheadeng.com/docs/2026-1/pathfinder/triggers/behavior/)는 트리거가 단순 `<Wait at Trigger>` 내장 행동을 줄 수도 있고, 임의의 custom behavior로 갈아탈 수도 있다고 설명한다. 행동 끝을 `Resume Prior Behavior`로 두면 일시 중단형, `Goto Exits` 같은 terminal action으로 끝내면 영구 전환형이 된다.
- [Occupant Response to a Trigger](https://www.thunderheadeng.com/docs/2026-1/pathfinder/triggers/response/)는 반응 절차를 `filter -> awareness -> decision scheduling -> decision -> use`의 단계로 설명한다. 사용 가능 여부는 occupant의 `Allowed Triggers`와 trigger의 `Allowed Occupants`를 동시에 통과해야 하고, 사용 확률은 기본적으로 `Influence x Trigger Susceptibility`다.
- 같은 문서에서 `Decision Time=Automatic`이면 `seeking` 상태에서는 즉시, `idling` 상태에서는 대기 기간 안의 임의 시점에 결정한다고 설명한다. 즉, 동일한 이벤트라도 이동 중인지 대기 중인지에 따라 반응 타이밍이 달라진다.
- [Trigger Memory](https://www.thunderheadeng.com/docs/2026-1/pathfinder/triggers/memory/)는 한 occupant가 같은 placed trigger 또는 trigger instance를 다시 사용하지 않는다고 설명한다. 거절한 trigger도 기억하지만, 사용 확률이 바뀌거나 `Remain aware` 조건이 달라지면 다시 고려할 수 있다.
- [Trigger Rank](https://www.thunderheadeng.com/docs/2026-1/pathfinder/triggers/rank/)는 높은 랭크의 트리거가 낮은 랭크 트리거를 끊고 개입할 수 있다고 설명한다. 동률이면 임의 선택이고, 끊긴 trigger가 재개될지는 `Resume if interrupted`와 trigger behavior 종료 방식에 달려 있다.
- [Trigger Limitations](https://www.thunderheadeng.com/docs/2026-1/pathfinder/triggers/limitations/)은 `Movement Groups`가 trigger를 사용하지 않는다고 명시한다. 또 queue 대기 중에는 인터럽트 가능하지만 service point에서는 불가, elevator 내부에서는 인터럽트 불가 같은 예외가 있다.
- [Trigger Output](https://www.thunderheadeng.com/docs/2026-1/pathfinder/triggers/output/)은 실제 이력 조회를 Output의 [Triggers History](https://www.thunderheadeng.com/docs/2026-1/pathfinder/output/triggers/)로 연결한다.
- SafeCrowd 시사점:
  - 우리 문서의 운영 이벤트 모델과 거의 직접 대응된다.
  - 출구 폐쇄, 특정 구역 통제, 유도 신호 변경, 순차 퇴장 개시를 모두 동일한 이벤트 큐로 넣기보다, `우선순위`, `중단 가능 여부`, `복귀 정책`까지 가진 이벤트/행동 전환 체계가 필요하다.
  - 이벤트 반응 확률과 반응 지연을 분리해야 한다. 즉 `보이는가`, `언제 판단하는가`, `실제로 따르는가`를 하나의 플래그로 뭉개면 Pathfinder 수준의 표현력을 잃는다.
  - 그룹 이동, queue, elevator 같은 특수 상태에서는 이벤트 인터럽트 규칙이 따로 필요하다.

### 3.3. 초기 occupant 생성과 배치
- 관련 문서: [Occupants](https://www.thunderheadeng.com/docs/2026-1/pathfinder/occupants/), [Generating Occupants](https://www.thunderheadeng.com/docs/2026-1/pathfinder/occupants/generating/)
- [Generating Occupants](https://www.thunderheadeng.com/docs/2026-1/pathfinder/occupants/generating/)는 occupant를 여러 방식으로 넣는 절차를 분리해 둔다. 개별 배치, 영역 배치, 방 전체 배치, occupant target에서 생성, CSV 위치 import가 모두 가능하다.
- 영역 배치는 `Random`과 `Uniform`을 모두 지원하고, 인원 수 기준뿐 아니라 `density` 기준 생성도 지원한다. 또한 profile과 behavior를 퍼센트 분포로 섞어 넣을 수 있다.
- 개별 occupant를 선택하면 이름, profile, behavior뿐 아니라 실제로 샘플링된 profile 파라미터 값을 확인하고 override할 수 있다.
- SafeCrowd 시사점:
  - SafeCrowd도 초기 인원 배치를 단일 방식으로 제한하지 말고 `개별 배치`, `영역 분포`, `CSV import` 정도는 최소한 별도 흐름으로 두는 편이 좋다.
  - 생성 단계에서부터 `count/density`, `profile distribution`, `behavior distribution`을 함께 지정하는 구조가 반복 실험에 유리하다.

### 3.4. 기존 occupant 집단의 재분배와 축소
- 관련 문서: [Redistributing Profiles and Behaviors](https://www.thunderheadeng.com/docs/2026-1/pathfinder/occupants/redistribute-profiles/), [Randomizing Occupant Positions](https://www.thunderheadeng.com/docs/2026-1/pathfinder/occupants/randomize-position/), [Reducing Population](https://www.thunderheadeng.com/docs/2026-1/pathfinder/occupants/reduce-population/)
- [Redistributing Profiles and Behaviors](https://www.thunderheadeng.com/docs/2026-1/pathfinder/occupants/redistribute-profiles/)는 이미 생성된 occupant 수를 바꾸지 않고, profile/behavior 배정만 다시 섞는다.
- [Randomizing Occupant Positions](https://www.thunderheadeng.com/docs/2026-1/pathfinder/occupants/randomize-position/)은 선택한 방들 안에서 occupant 위치를 다시 생성한다. 현재 방 안에만 재배치할지, 선택된 방들 전체로 이동시킬지 고를 수 있고 `Random/Uniform` 분포도 다시 선택한다.
- [Reducing Population](https://www.thunderheadeng.com/docs/2026-1/pathfinder/occupants/reduce-population/)은 room, stair, ramp, occupant group 단위로 인원을 줄이는 기능이다. 삭제 방식도 `random`, `first`, `last` 중에서 고를 수 있다.
- SafeCrowd 시사점:
  - 우리도 시나리오 파라미터를 바꿀 때 전체를 다시 만드는 것만 고집할 필요는 없다. 기존 배치에서 `profile reshuffle`, `position reshuffle`, `population thinning`을 빠르게 돌릴 수 있어야 실험 회전 속도가 빨라진다.
  - 특히 population reduction은 “부분 통제”, “입장 제한”, “유효 수용 인원 감소” 실험에 직접 대응된다.

### 3.5. Occupant Tags와 상태 라벨링
- 관련 문서: [Occupant Tags](https://www.thunderheadeng.com/docs/2026-1/pathfinder/occupants/occupant-tags/)
- [Occupant Tags](https://www.thunderheadeng.com/docs/2026-1/pathfinder/occupants/occupant-tags/)는 occupant에 여러 label을 붙여 시뮬레이션 중 관련 집단을 식별하게 한다.
- 문서상 tags는 `Goto Occupant`, `Look At` 행동의 대상 지정, social distancing 대상 지정, trigger 사용 가능 집단 지정, 결과 파일 보고에 사용된다.
- tag는 시작 시 profile/occupant property로 부여할 수 있고, room/stair/ramp 진입 시 자동 부여, door 통과 시 부여, `Change Tags` behavior action으로 실행 중 추가/제거/교체도 가능하다.
- SafeCrowd 시사점:
  - SafeCrowd도 단순 profile만으로는 부족하고, 실행 중 바뀌는 `집단 라벨` 계층이 필요하다.
  - `운영 요원`, `도움 필요`, `유도 완료`, `통제 대상`, `안전구역 진입` 같은 상태를 태그로 관리하면 trigger/behavior/output 연결이 쉬워진다.

### 3.6. 동적 인원 유입
- 관련 문서: [Occupants](https://www.thunderheadeng.com/docs/2026-1/pathfinder/occupants/), [Occupant Sources](https://www.thunderheadeng.com/docs/2026-1/pathfinder/occupants/occupant-source/)
- [Occupant Sources](https://www.thunderheadeng.com/docs/2026-1/pathfinder/occupants/occupant-source/)는 사전에 정한 유량 표를 바탕으로 시뮬레이션 도중 새로운 인원을 계속 생성한다.
- 소스는 직사각형 영역으로 두거나 문/방 같은 컴포넌트에 붙일 수 있다.
- SafeCrowd 시사점:
  - 행사장이나 역 같은 장소는 “초기 배치 후 대피”만으로 부족하다.
  - `입장 흐름`, `외부에서 계속 유입되는 인원`, `구역별 유입률 변화`를 표현할 수 있어야 한다.

### 3.7. 그룹 이동
- 관련 문서: [Movement Group Configuration](https://www.thunderheadeng.com/docs/2026-1/pathfinder/groups/configuration/), [Template Configuration](https://www.thunderheadeng.com/docs/2026-1/pathfinder/groups/template-configuration/), [Creating a Movement Group](https://www.thunderheadeng.com/docs/2026-1/pathfinder/groups/movement-groups/), [Creating a Movement Group Template](https://www.thunderheadeng.com/docs/2026-1/pathfinder/groups/movement-group-templates/), [Creating Movement Groups from a Template before a simulation](https://www.thunderheadeng.com/docs/2026-1/pathfinder/groups/groups-from-occs/), [Creating Movement Groups from a Template during a simulation](https://www.thunderheadeng.com/docs/2026-1/pathfinder/groups/groups-from-sources/), [Groups Output](https://www.thunderheadeng.com/docs/2026-1/pathfinder/output/groups/)
- [Movement Group Configuration](https://www.thunderheadeng.com/docs/2026-1/pathfinder/groups/configuration/)은 그룹 자체와 그룹 멤버 참조를 분리해 관리한다. 그룹 멤버는 occupant 자체가 아니라 기존 occupant를 가리키는 reference다.
- 같은 문서는 leader를 수동 지정할 수 있고, 비워 두면 현재 goal에 가장 가까운 멤버가 자동 leader가 된다고 설명한다. `Maximum Distance`는 group을 잇는 minimum spanning tree의 longest edge 기준으로 연결 여부를 판단하고, 끊기면 leader는 `Slowdown Time` 동안 감속 후 정지한다.
- `Enforce Social Distancing Between Group Members`를 통해 같은 그룹 내부에도 거리두기를 강제할 수 있고, 문서상 “그룹 구성원이 가진 컴포넌트 제약은 서로 공유된다”고 명시한다.
- [Template Configuration](https://www.thunderheadeng.com/docs/2026-1/pathfinder/groups/template-configuration/)은 템플릿에 `Number of Members`, `Numbers of Members per Profile`, `Leader's Profile`을 둘 수 있게 한다. 즉, 단순 동행뿐 아니라 “보호자 1 + 피보호자 1”, “인솔자 1 + 참가자 n” 같은 조합형 그룹도 만든다.
- [Creating a Movement Group](https://www.thunderheadeng.com/docs/2026-1/pathfinder/groups/movement-groups/)는 시뮬레이션 시작 전에 선택한 occupant들로 정적 group을 만드는 흐름이다. [Creating a Movement Group Template](https://www.thunderheadeng.com/docs/2026-1/pathfinder/groups/movement-group-templates/)는 이런 group type을 재사용 가능한 template로 저장한다.
- [Creating Movement Groups from a Template before a simulation](https://www.thunderheadeng.com/docs/2026-1/pathfinder/groups/groups-from-occs/)은 이미 배치된 occupant 집합에서 template 기반 group을 만들고, [Creating Movement Groups from a Template during a simulation](https://www.thunderheadeng.com/docs/2026-1/pathfinder/groups/groups-from-sources/)은 occupant source가 사람을 넣을 때 group template distribution으로 동적 group을 함께 생성한다.
- [Groups Output](https://www.thunderheadeng.com/docs/2026-1/pathfinder/output/groups/)은 CSV에 `Group ID`, `Member IDs`, `Group Name`, `Template`를 남기고, JSON에는 time step별 membership 변화를 넣는다. 즉, group은 단순 초기 태그가 아니라 런타임 상태 객체다.
- SafeCrowd 시사점:
  - 가족/동행자/인솔 그룹을 독립 개체의 우연한 근접으로만 처리하면 부족하다.
  - 최소한 `leader-follower`, `group cohesion`, `group-level connector restriction`, `template-based group composition`, `runtime group logging` 정도는 중기 기능으로 고려할 가치가 있다.

### 3.8. 예약형 목표 지점
- 관련 문서: [Targets](https://www.thunderheadeng.com/docs/2026-1/pathfinder/targets/), [Creating Occupant Targets](https://www.thunderheadeng.com/docs/2026-1/pathfinder/targets/creating/), [Occupant Target Properties](https://www.thunderheadeng.com/docs/2026-1/pathfinder/targets/properties/), [Orienting Occupant Targets](https://www.thunderheadeng.com/docs/2026-1/pathfinder/targets/orientation/), [Prioritizing Occupant Targets](https://www.thunderheadeng.com/docs/2026-1/pathfinder/targets/priority/), [Using Occupant Targets](https://www.thunderheadeng.com/docs/2026-1/pathfinder/targets/using-targets/), [Occupant Target Reservation System](https://www.thunderheadeng.com/docs/2026-1/pathfinder/targets/reservation/), [Occupant Target Limitations](https://www.thunderheadeng.com/docs/2026-1/pathfinder/targets/limitations/), [Occupant Target Output](https://www.thunderheadeng.com/docs/2026-1/pathfinder/targets/output/)
- Pathfinder는 별도 `Queues`와 [Occupant Targets](https://www.thunderheadeng.com/docs/2026-1/pathfinder/targets/properties/) 섹션을 갖는다.
- [Occupant Target Properties](https://www.thunderheadeng.com/docs/2026-1/pathfinder/targets/properties/)는 최소한 `orientation`과 `priority`를 갖고, 예약 시스템과 함께 사용된다.
- [Creating Occupant Targets](https://www.thunderheadeng.com/docs/2026-1/pathfinder/targets/creating/)는 target을 직접 배치하거나 기존 occupant 위치에서 생성하는 흐름을 지원한다. 좌석, 집결 지점, 개별 대기 위치처럼 “점 단위 수용 슬롯”을 명시적으로 모델링하는 방식이다.
- [Prioritizing Occupant Targets](https://www.thunderheadeng.com/docs/2026-1/pathfinder/targets/priority/)는 각 target priority를 수동으로 줄 수도 있고, 기준점으로부터의 거리 gradient로 일괄 생성할 수도 있다고 설명한다. 문서상 binning을 쓰면 reservation conflict를 줄이고 성능을 개선할 수 있다.
- [Using Occupant Targets](https://www.thunderheadeng.com/docs/2026-1/pathfinder/targets/using-targets/)는 behavior에 `Goto Occupant Targets` action이 있어야 target을 예약하고 이동할 수 있다고 설명한다. 예약을 못 잡으면 occupant는 현재 room에서 기다리고, 예약을 잡은 뒤에는 `Abandon Occupant Targets`를 만나기 전까지 그 예약을 유지한다.
- [Occupant Target Reservation System](https://www.thunderheadeng.com/docs/2026-1/pathfinder/targets/reservation/)은 모든 occupant의 요청을 한 번에 해석하고, priority preference와 travel distance를 함께 써서 누가 target을 차지할지 정한다고 설명한다. 문서 표현대로 “가장 원하고 가장 가까운 occupant”가 이기는 구조다.
- [Occupant Target Limitations](https://www.thunderheadeng.com/docs/2026-1/pathfinder/targets/limitations/)은 movement group에서 occupant targets가 현재 지원되지 않는다고 명시한다. 또 좌석열 ingress처럼 좁은 seating row에서는 sitting 모델이 없어서 과도한 혼잡이 생길 수 있다고 경고한다.
- [Occupant Target Output](https://www.thunderheadeng.com/docs/2026-1/pathfinder/targets/output/)은 실제 상태 이력을 [Occupant Target History](https://www.thunderheadeng.com/docs/2026-1/pathfinder/output/targets/) 출력으로 연결한다.
- SafeCrowd 시사점:
  - 좌석, 집결 지점, 일시 대기 위치, 순차 퇴장 대기선 같은 기능이 필요해지면 일반 waypoint만으로는 부족하다.
  - 다만 SafeCrowd 초반에는 `queue/target`을 독립 시스템으로 만들기보다, `대기 지점 + 수용량 + 우선순위` 정도의 단순 형태로 출발해도 된다.
  - target 선택은 “최단 거리 하나”가 아니라 `우선순위 + 거리 + 예약 경쟁`의 조합이어야 현실적이다.
  - 특히 좌석/대기 슬롯 문제는 navigation target과 occupant 상태 모델이 함께 가야 한다. Pathfinder도 sitting을 따로 못 모델링하면 한계가 드러난다고 적고 있다.

### 3.9. Queues와 서비스 지점 모델링
- 관련 문서: [Queues](https://www.thunderheadeng.com/docs/2026-1/pathfinder/queues/), [Service Points](https://www.thunderheadeng.com/docs/2026-1/pathfinder/queues/services/), [Queue Paths](https://www.thunderheadeng.com/docs/2026-1/pathfinder/queues/paths/), [Path Nodes](https://www.thunderheadeng.com/docs/2026-1/pathfinder/queues/path-nodes/)
- [Queues](https://www.thunderheadeng.com/docs/2026-1/pathfinder/queues/) 문서는 queue를 “목표를 위해 줄 서서 기다리게 만드는 occupant-organizing structure”로 설명하고, 구성 요소를 `Service Points`, `Queue Paths`, `Path Nodes`로 나눈다.
- [Service Points](https://www.thunderheadeng.com/docs/2026-1/pathfinder/queues/services/)는 navigation mesh 위의 목적 지점으로 정의되며, 도착한 occupant는 여기서 대기한 뒤 다음 behavior action으로 풀려난다. `Service Time`은 상수값이나 사용자 정의 분포로 줄 수 있다.
- [Queue Paths](https://www.thunderheadeng.com/docs/2026-1/pathfinder/queues/paths/)는 path node들의 선형 집합이며, occupant는 이 경로를 따라 줄을 만든다. `Occupant Spacing`, `Force Follow Path`, `Accepted Profiles`로 줄 간격, 경로 강제 여부, 허용 프로필을 제어한다.
- [Path Nodes](https://www.thunderheadeng.com/docs/2026-1/pathfinder/queues/path-nodes/)는 service까지의 경로를 정의하는 점 집합으로, 생성 후에도 순서를 바꾸거나 추가/삭제하며 줄 형태를 수정할 수 있다.
- SafeCrowd 시사점:
  - 보안 검색대, 게이트 통제선, 매표/정산 구역, 화장실 앞 대기처럼 “서비스 처리 시간”이 병목의 본질인 상황은 문 유량 모델만으로 충분하지 않다.
  - 따라서 SafeCrowd도 장기적으로는 `서비스 지점`, `대기선 형상`, `대기 간격`, `허용 사용자군`을 갖는 queue 시스템을 별도 컴포넌트로 두는 편이 맞다.
  - Pathfinder 문서도 `Queues`와 `Movement Groups`의 완전한 동시 지원은 아직 부족해 그룹이 queue에서 멈출 수 있다고 경고한다. 즉, queue와 group cohesion의 상호작용은 구현 난도가 높은 축이다.

### 3.10. 보조 대피 팀/클라이언트/어시스턴트 모델
- 관련 문서: [Assisted Evacuation](https://www.thunderheadeng.com/docs/2026-1/pathfinder/advanced/assisted-evacuation/), [Introduction](https://www.thunderheadeng.com/docs/2026-1/pathfinder/advanced/assisted-evacuation/introduction/), [Basic Usage](https://www.thunderheadeng.com/docs/2026-1/pathfinder/advanced/assisted-evacuation/basic-usage/), [Configuration](https://www.thunderheadeng.com/docs/2026-1/pathfinder/advanced/assisted-evacuation/configuration/), [Creating an Assisted Evacuation Team](https://www.thunderheadeng.com/docs/2026-1/pathfinder/advanced/assisted-evacuation/creating-teams/), [Assigning a Client to a Team](https://www.thunderheadeng.com/docs/2026-1/pathfinder/advanced/assisted-evacuation/assigning-clients/), [Assigning an Assistant to a Team](https://www.thunderheadeng.com/docs/2026-1/pathfinder/advanced/assisted-evacuation/adding-assistants/), [Technical Reference - Assisted Evacuation](https://www.thunderheadeng.com/docs/2026-1/pathfinder/appendices/technical-reference/assisted-evac/), [Verification - Assisted Evacuation](https://www.thunderheadeng.com/docs/2026-1/pathfinder/appendices/verification-validation/special-features/assisted-evac/)
- [Introduction](https://www.thunderheadeng.com/docs/2026-1/pathfinder/advanced/assisted-evacuation/introduction/)은 용어를 `Assistant`, `Client`, `Team`으로 나누고, 이 기능이 별도 AI가 아니라 behavior system 위에 얹힌다고 설명한다. 즉, 보조 대피도 결국 행동 액션 조합으로 모델링된다.
- 같은 문서는 assistant가 `Assist Occupants` action으로 팀에 들어간 뒤 현재/미래 client 수요를 보고 도움 제안을 보내고, client는 `Wait for Assistance` action으로 대기하다가 가장 가까운 assistant들로 vehicle의 빈 slot을 채운다고 설명한다.
- client는 assistant가 붙은 상태로 이후 행동을 계속 수행하되, `Goto Exits`, `Goto Refuge Rooms`, `Wait Until Simulation End`, `Detach from Assistants` 같은 terminating action에서 detach 규칙이 달라진다. 특히 `Goto Exits`에서는 exit 직전에 detach되고, assistant는 다른 client를 도울 수 있게 된다.
- [Basic Usage](https://www.thunderheadeng.com/docs/2026-1/pathfinder/advanced/assisted-evacuation/basic-usage/) 튜토리얼은 실제 설정 절차를 단순하게 보여 준다. assistant behavior에는 `Assist Occupants`, client behavior에는 `Wait for Assistance`를 추가하고, client profile에는 `Default Wheelchair` 같은 vehicle과 `Requires Assistance to Move`를 켠다.
- [Creating an Assisted Evacuation Team](https://www.thunderheadeng.com/docs/2026-1/pathfinder/advanced/assisted-evacuation/creating-teams/)은 기본 `Default Team` 외에 custom team을 만들고 기존 team을 복사해 설정을 재사용할 수 있다고 설명한다. 팀은 “누가 누구를 돕는가”를 behavior와 분리하는 배정 단위다.
- [Configuration](https://www.thunderheadeng.com/docs/2026-1/pathfinder/advanced/assisted-evacuation/configuration/)은 client priority를 `Distance to Assistants` 또는 명시 리스트로 설정하게 한다. assistants/client 목록은 read-only이며 behavior action으로만 채워진다. 관련 profile 속성으로는 `Vehicle Shape`와 `Requires Assistance to Move`가 직접 연결된다.
- [Assigning a Client to a Team](https://www.thunderheadeng.com/docs/2026-1/pathfinder/advanced/assisted-evacuation/assigning-clients/)은 client가 하나의 team이 아니라 여러 assisting teams를 받을 수 있게 하고, [Assigning an Assistant to a Team](https://www.thunderheadeng.com/docs/2026-1/pathfinder/advanced/assisted-evacuation/adding-assistants/)은 assistant가 한 시점에는 하나의 team에만 속하지만 behavior 안의 추가 `Assist Occupants` action으로 simulation 중 team을 바꿀 수 있다고 설명한다.
- 2026.1 tree의 `Technical Reference`와 `Verification Tests` 역시 도구에서 본문을 직접 열 수 없어, 공식 appendices 대응 문서인 [Technical Reference - Assisted Evacuation](https://www.thunderheadeng.com/docs/2026-1/pathfinder/appendices/technical-reference/assisted-evac/)와 [Verification - Assisted Evacuation](https://www.thunderheadeng.com/docs/2026-1/pathfinder/appendices/verification-validation/special-features/assisted-evac/)를 함께 확인했다.
- [Technical Reference - Assisted Evacuation](https://www.thunderheadeng.com/docs/2026-1/pathfinder/appendices/technical-reference/assisted-evac/)는 assisted model을 vehicle agent와 attachment slot 관점에서 설명한다. assistant는 client vehicle의 slot에 attach되어 이동하고, 이후 경로는 client behavior가 주도한다.
- [Verification - Assisted Evacuation](https://www.thunderheadeng.com/docs/2026-1/pathfinder/appendices/verification-validation/special-features/assisted-evac/)은 evacuation chair와 stretcher 시나리오, 계단 하강, assistant 수(1인 chair, 4인 stretcher), 병원 대피 사례를 비교한다. 문서상 Pathfinder 결과는 hand calculation보다 door opening, cornering, additional movement를 포함해 다소 느리지만 합리적인 범위로 일치한다고 설명한다.
- SafeCrowd 시사점:
  - 보행 약자 지원은 단순 속도 감소나 group movement로 대체하기 어렵다. `vehicle/assistance slot`, `assistant-client matching`, `detach rule`, `team assignment`가 별도 상태로 필요하다.
  - 특히 wheelchair, hospital bed, 구조대 escort 같은 시나리오를 다루려면 `occupant capability`와 `assist workflow`를 behavior/action 계층에 같이 둬야 한다.
  - 초기 버전은 전 범위를 다 넣기보다 `escort behavior + requires assistance + detach at exit` 정도의 축소형 모델부터 시작하는 편이 현실적이다.
  - 검증 문서가 계단 chair/stretcher와 assistant 수까지 다루는 만큼, SafeCrowd도 장기적으로는 단순 escort 하나보다 `vehicle class`, `required assistants`, `terrain-dependent speed`를 분리해야 한다.

## 4. 시뮬레이션 엔진과 실행 파라미터

### 4.1. SFPE 모드와 Steering 모드
- 관련 문서: [Simulation](https://www.thunderheadeng.com/docs/2026-1/pathfinder/simulation/), [Parameters](https://www.thunderheadeng.com/docs/2026-1/pathfinder/simulation/parameters/)
- Pathfinder는 [Parameters](https://www.thunderheadeng.com/docs/2026-1/pathfinder/simulation/parameters/)에서 `SFPE`와 `Steering` 두 흐름을 구분해 설명한다.
- `Steering` 모드는 충돌 회피와 상호작용을 중심으로 사람 움직임을 더 직접적으로 모사하려는 모드이며, 명시적 문 대기열 없이도 자연스럽게 대기열이 형성될 수 있다고 설명한다.
- `Limit Door Flow Rate` 같은 옵션으로 Steering 모드에서도 문 유량 상한을 줄 수 있다.
- SafeCrowd 시사점:
  - 우리도 장기적으로는 “거시적 근사 모드”와 “미시적 상호작용 모드”를 분리할 수 있다.
  - 단기적으로는 미시 시뮬레이션 하나로 시작하더라도, 문 유량을 별도 제어할 수 있는 장치는 남겨 두는 편이 좋다.

### 4.2. 밀도와 문 유량 파라미터
- 관련 문서: [Parameters](https://www.thunderheadeng.com/docs/2026-1/pathfinder/simulation/parameters/)
- [Simulation Parameters](https://www.thunderheadeng.com/docs/2026-1/pathfinder/simulation/parameters/)에는 `Max Room Density`, `Door Flow Rate -> Boundary Layer`, `Specific Flow` 계산 방식 같은 항목이 있다.
- 문 유량은 유효 폭과 특정 유량을 이용해 계산된다.
- SafeCrowd 시사점:
  - 병목 모델링에서 문은 단순 충돌 결과가 아니라, 별도 측정식이나 제약식으로도 다뤄볼 수 있다.
  - 우리 위험 정의 문서의 `FlowRate`, `SpecificFlow`, `Headway` 지표와 직접 연결된다.

### 4.3. 경로 메시와 성능 조절
- 관련 문서: [Parameters](https://www.thunderheadeng.com/docs/2026-1/pathfinder/simulation/parameters/)
- [Simulation Parameters](https://www.thunderheadeng.com/docs/2026-1/pathfinder/simulation/parameters/)의 `Paths Parameters`에는 `Max Agent Radius Trim Error`, `Navigation Mesh Refinement`, `Max Edge Length`, `Min Angle` 같은 옵션이 있다.
- 더 작은 메시가 긴 우회 경로 문제를 줄일 수 있지만, 계산 성능은 떨어진다고 설명한다.
- SafeCrowd 시사점:
  - 정확도와 성능의 교환관계를 UI나 설정으로 열어 둘 필요가 있다.
  - 경로 그래프/내비게이션 메시의 정밀도는 결과 재현성과 대규모 실행 성능 모두에 영향을 준다.

### 4.4. 디버그/재시작
- 관련 문서: [Parameters](https://www.thunderheadeng.com/docs/2026-1/pathfinder/simulation/parameters/), [Starting and Managing Simulations](https://www.thunderheadeng.com/docs/2026-1/pathfinder/simulation/starting/), [Stopping and Resuming Simulations](https://www.thunderheadeng.com/docs/2026-1/pathfinder/simulation/stopping/)
- [Simulation Parameters](https://www.thunderheadeng.com/docs/2026-1/pathfinder/simulation/parameters/)에는 `Save Restart Files`, `Snapshot Interval`, `Resume Simulation` 같은 디버그 및 복구 기능이 있다.
- [Starting and Managing Simulations](https://www.thunderheadeng.com/docs/2026-1/pathfinder/simulation/starting/)은 여러 scenario와 Monte Carlo variation을 한 번에 선택해 실행하는 흐름을 설명한다. 실행 dialog는 현재 scenario/variation, batch 진행 상황, `distance to goal` 지표를 보여 주고, `Debug` 버튼으로 런타임 시각화를 연다.
- 같은 문서는 scenario/variation을 개별 PTH 파일로 export해 `_run.bat`와 `_make_plots.bat`로 batch 실행하는 흐름도 제공한다. 또 설치 폴더의 `testsim.bat`로 command-line 실행을 지원한다고 명시한다.
- [Stopping and Resuming Simulations](https://www.thunderheadeng.com/docs/2026-1/pathfinder/simulation/stopping/)은 실행 취소 시 snapshot 파일을 만들고, 나중에 같은 모델을 열어 `Simulation > Resume Simulation`으로 특정 시점부터 재개하는 절차를 설명한다.
- SafeCrowd 시사점:
  - 반복 실험이 중요한 프로젝트이므로, 장시간 시뮬레이션 중간 저장과 재개 기능은 나중에 큰 가치가 있다.
  - GUI 실행과 별도로 `headless batch run` 경로가 있어야 한다. 시나리오 조합 실험, CI 검증, 자동 리포트 생성은 이 경로가 없으면 막힌다.
  - snapshot 기반 재개는 단순 편의 기능이 아니라, 긴 시뮬레이션과 Monte Carlo 반복에서 계산 자산을 보존하는 운영 기능에 가깝다.

## 5. 출력 파일과 정량 지표

### 5.1. 시나리오/변형별 폴더 구조
- 관련 문서: [Output](https://www.thunderheadeng.com/docs/2026-1/pathfinder/output/), [Simulation Variation Output](https://www.thunderheadeng.com/docs/2026-1/pathfinder/output/variations/)
- 시뮬레이션을 돌리면 저장한 PTH 이름의 새 폴더 아래에 출력이 생성된다.
- 시나리오가 여러 개면 시나리오별 폴더가 나뉘고, Monte Carlo variation이 있으면 각 시나리오 폴더 아래 variation 폴더가 추가된다. 이 구조는 [Simulation Variation Output](https://www.thunderheadeng.com/docs/2026-1/pathfinder/output/variations/) 문서와도 연결된다.
- SafeCrowd 시사점:
  - 우리도 `scenario -> run/variation` 구조를 기본 저장 단위로 삼는 편이 결과 비교와 재현성 관리에 좋다.

### 5.2. 요약/집계 출력
- 관련 문서: [Output](https://www.thunderheadeng.com/docs/2026-1/pathfinder/output/)
- [Output](https://www.thunderheadeng.com/docs/2026-1/pathfinder/output/) 문서는 최소/최대/평균 출구 시간, 방/문의 FIFO 시간, 개별 인원 이동, jam time, 샘플 포인트 데이터, 방 점유 차트, 문 유량 차트를 제공한다고 설명한다.
- 관련 하위 문서로 `Summary Report`, `Door History`, `Room History`, `Measurement Regions`, `Occupant Summary`, `Groups Output`, `Triggers History`, `Occupant Target History`, `Monte Carlo Results`, `Simulation Variation Output`가 존재한다.
- [Summary Report](https://www.thunderheadeng.com/docs/2026-1/pathfinder/output/summary/)는 `filename_summary.txt`로 저장되며, 시뮬레이션 모드, 총 인원 수, 완료 시간 통계, 메시 복잡도, 문 개수, 이동 거리, trigger 사용 시간, target 사용 시간을 한 파일에 묶어 보여준다.
- [Door History](https://www.thunderheadeng.com/docs/2026-1/pathfinder/output/doors/)는 시간별 문 통과량, 방향별 사용량, 폭, boundary layer, 문 앞 queue 수를 남기며, Results에서 flow rate나 specific flow 차트로 바로 이어진다.
- [Room History](https://www.thunderheadeng.com/docs/2026-1/pathfinder/output/rooms/)는 시간별 room/stair occupant count를 제공한다.
- SafeCrowd 시사점:
  - 결과는 최종 총대피 시간 하나로 끝나면 안 된다.
  - 최소한 `door history`, `room history`, `measurement regions`, `variation summary`에 해당하는 계층형 결과 구조가 필요하다.

### 5.3. 인원 단위 상세 이력
- 관련 문서: [Occupant History](https://www.thunderheadeng.com/docs/2026-1/pathfinder/output/occupant-history/), [Output Tab](https://www.thunderheadeng.com/docs/2026-1/pathfinder/profiles/output/)
- [Occupant History](https://www.thunderheadeng.com/docs/2026-1/pathfinder/output/occupant-history/)는 상세 출력이 활성화된 인원에 대해 시간 스텝마다 데이터를 남긴다.
- [Output Tab](https://www.thunderheadeng.com/docs/2026-1/pathfinder/profiles/output/)은 profile별 `Output Detailed Data` 옵션으로 이 상세 로그 생성을 켜게 한다. 공식 문서도 CPU와 disk 사용량이 크게 늘 수 있으니 특정 occupant/profile에만 선택적으로 켜는 편이 낫다고 경고한다.
- 문서에서 예시로 든 필드는 시간, ID, 이름, active 여부, 3D 위치, 속도, 누적 이동 거리, 현재 room, terrain type 등이다.
- SafeCrowd 시사점:
  - 위험 재현과 사후 분석을 하려면 전역 집계만으로는 부족하다.
  - 최소한 선택된 인원 또는 샘플 집단에 대해 `trajectory + speed + zone + state` 수준의 시계열을 남겨야 한다.
  - 또 상세 로그는 전체 인구에 상시 적용하기보다 `디버그 대상 cohort`, `취약군`, `표본 집단` 단위로 선택적으로 켜는 구조가 실용적이다.

### 5.4. 대인 거리 보고
- 관련 문서: [Parameters](https://www.thunderheadeng.com/docs/2026-1/pathfinder/simulation/parameters/)
- [Simulation Parameters](https://www.thunderheadeng.com/docs/2026-1/pathfinder/simulation/parameters/)에서 `Enable Interpersonal Distance Reporting`을 켜면 추가 거리 출력 파일이 생기며, 문서상 계산 비용 때문에 런타임이 약 `10%` 증가한다고 설명한다.
- [Interpersonal Distance](https://www.thunderheadeng.com/docs/2026-1/pathfinder/output/interpersonal-distance/) 출력은 center-to-center 기준으로 가장 가까운 점유자 거리와 1m, 2m, 3m 내 점유자 수와 ID를 기록한다.
- 또한 reference distance 계산은 벽과 분리선은 막고, 문은 비차단으로 처리하는 shortest path 기반 거리로 설명된다.
- SafeCrowd 시사점:
  - 사회적 거리, 압박 위험, 국소 밀도 분석은 값이 크다.
  - 다만 이런 지표는 항상 켜기보다 선택적 분석 모드로 두는 편이 현실적이다.

### 5.5. Measurement Regions의 속도-밀도 관측
- 관련 문서: [Measurement Regions](https://www.thunderheadeng.com/docs/2026-1/pathfinder/output/measurement-regions/), [Advanced - Measurement Regions](https://www.thunderheadeng.com/docs/2026-1/pathfinder/advanced/measurement-regions/)
- [Measurement Regions](https://www.thunderheadeng.com/docs/2026-1/pathfinder/output/measurement-regions/)은 시간별 `Density`, `Velocity`, `SeekVelocity`, `Count`를 별도 관측 영역에 대해 기록한다.
- [Advanced - Measurement Regions](https://www.thunderheadeng.com/docs/2026-1/pathfinder/advanced/measurement-regions/)은 이 관측 영역이 setup 기능임을 분명히 한다. region은 하나의 room/stair/terrain 안에 완전히 들어가야 하고, interior walls를 가로지르면 안 되며, steady flow가 형성되는 구간에 두는 것이 권장된다.
- 문서상 Results에서는 이 데이터를 시간 이력과 speed-vs-density plot으로 볼 수 있다.
- SafeCrowd 시사점:
  - 전역 평균만으로는 부족하고, 사용자가 지정한 관측 단면이나 구역에서 밀도-속도 관계를 뽑을 수 있어야 한다.
  - 우리 위험 정의 문서의 `FlowMeasurementSystem`과 직접 연결되는 지점이다.

### 5.6. Monte Carlo 결과의 반복 실험 요약
- 관련 문서: [Monte Carlo Results](https://www.thunderheadeng.com/docs/2026-1/pathfinder/output/monte-carlo/), [Advanced - Monte Carlo](https://www.thunderheadeng.com/docs/2026-1/pathfinder/advanced/monte-carlo/)
- [Monte Carlo Results](https://www.thunderheadeng.com/docs/2026-1/pathfinder/output/monte-carlo/)는 분포형 위치, 프로필, door delay를 여러 variation으로 반복 실행한 뒤 `completion times`와 `travel distances`의 `MIN`, `MAX`, `LOW`, `HIGH`, `AVG`, `STDDEV`를 variation별로 저장한다.
- [Advanced - Monte Carlo](https://www.thunderheadeng.com/docs/2026-1/pathfinder/advanced/monte-carlo/)는 variation 생성 규칙을 설명한다. `Variation Count`, 별도 `Randomization Seed`, `Randomize Positions/Profile Properties/Profiles`, occupant filter의 top-to-bottom override, `Room Rule`, `Position Distribution(Random/Uniform)`을 조합한다.
- 같은 문서는 Monte Carlo가 geometry 자체를 바꾸지는 못하고, door/stair width 같은 geometry 계열 비교는 scenario로 해야 한다고 명시한다. 또 occupant source의 output도 variation마다 함께 randomize된다고 설명한다.
- 또한 completion time과 travel distance plot을 별도 HTML로 제공한다.
- SafeCrowd 시사점:
  - SafeCrowd도 확률적 초기 배치와 분포형 속성을 쓰려면 단일 실행 결과 대신 반복 실행 요약을 1급 결과로 다뤄야 한다.
  - 현재 `위험 정의.md`의 반복 실행 요약 규칙과 정합성이 높다.
  - 구현 시에도 `variation generator`는 geometry 변경기와 분리하는 편이 맞다. Pathfinder도 그 경계를 분명히 둔다.

### 5.7. Cumulative Output의 프로그램 연동용 JSON
- 관련 문서: [Cumulative Output](https://www.thunderheadeng.com/docs/2026-1/pathfinder/output/cumulative/)
- [Cumulative Output](https://www.thunderheadeng.com/docs/2026-1/pathfinder/output/cumulative/)은 시뮬레이션 완료 시 `filename_out.json` 하나에 JSON 출력 전부를 합친다. 이 파일은 `Write Json Output Files`가 켜져 있을 때만 생성되며, 공식 문서도 외부 애플리케이션의 custom post processing에 적합하다고 설명한다.
- 문서상 루트 키는 `triggers`, `doors`, `groups`, `measurementRegions`, `targets`, `rooms`, `occupants`다.
- 특히 `occupants`는 occupant ID 기준으로 `Occupant Summary`, `Occupant Parameters`, `Occupant History`, `Interpersonal Distance`를 합친 뒤 `socialDistancing`, `initialParams`, `detailed`를 한 객체 안에 붙여 제공한다.
- SafeCrowd 시사점:
  - 결과 저장 포맷은 CSV 여러 개만 두기보다, 대시보드·분석 파이프라인·자동 비교에 바로 쓰는 단일 JSON 아티팩트도 함께 두는 편이 좋다.
  - 장기적으로는 `run.json` 같은 canonical 결과 묶음을 만들고, 세부 CSV/Parquet은 파생 산출물로 두는 구조가 적절하다.

### 5.8. Occupant Parameters와 Summary로 입력/결과를 분리 검증
- 관련 문서: [Occupant Parameters](https://www.thunderheadeng.com/docs/2026-1/pathfinder/output/occupant-params/), [Occupant Summary](https://www.thunderheadeng.com/docs/2026-1/pathfinder/output/occupant-summary/)
- [Occupant Parameters](https://www.thunderheadeng.com/docs/2026-1/pathfinder/output/occupant-params/)는 각 occupant의 초기 상태를 거의 전부 요약한다. 프로필, behavior, 최대 속도, 형상 반경, 시작 room, 초기 x/y/z 등이 기록되어, 프로필 분포가 실제 샘플링 결과와 맞는지 검증하는 용도로 쓰인다.
- [Occupant Summary](https://www.thunderheadeng.com/docs/2026-1/pathfinder/output/occupant-summary/)는 실행 후 occupant별 `exit time`, `active time`, `congestion time`, `distance`, `num triggers used`, `num occ targets used`, refuge room 체류 시간, tag 부여/해제 시각까지 누적 결과를 남긴다.
- 문서상 `Occupant Summary` JSON은 occupant ID별로 결과를 정리하고, 태그별 `usageTime`, `lastAdded`, `lastRemoved`도 함께 제공한다.
- SafeCrowd 시사점:
  - 입력에서 샘플링된 값과 실행 후 관측된 값을 한 파일에 섞기보다, `initial parameters`와 `outcome summary`를 분리해야 검증과 디버깅이 쉬워진다.
  - 태그 기반 시계열 요약은 `안전구역 진입`, `통제 상태`, `보조 대피 중` 같은 상태 추적 설계와 직접 연결된다.

### 5.9. Groups, Triggers, Targets의 상태 이력
- 관련 문서: [Groups Output](https://www.thunderheadeng.com/docs/2026-1/pathfinder/output/groups/), [Triggers History](https://www.thunderheadeng.com/docs/2026-1/pathfinder/output/triggers/), [Occupant Target History](https://www.thunderheadeng.com/docs/2026-1/pathfinder/output/targets/)
- [Groups Output](https://www.thunderheadeng.com/docs/2026-1/pathfinder/output/groups/)은 시뮬레이션 전/중에 생성된 그룹을 모두 기록한다. CSV는 `Group ID`, `Member IDs`, `Group Name`, `Template` 등을 남기고, JSON은 시간 스텝별 group membership까지 제공한다.
- [Triggers History](https://www.thunderheadeng.com/docs/2026-1/pathfinder/output/triggers/)는 `Data Output Freq.` 기준 시간 스텝마다 각 trigger의 사용 인원 수를 CSV/JSON으로 남긴다.
- [Occupant Target History](https://www.thunderheadeng.com/docs/2026-1/pathfinder/output/targets/)는 시간 스텝마다 target의 `reserved_by`, `reserved_by_id`, `in_use`를 기록한다.
- SafeCrowd 시사점:
  - 그룹 형성, 운영 이벤트 사용량, 목표 지점 예약 상태는 “현재 상태”만 있어서는 사후 분석이 어렵다.
  - 따라서 SafeCrowd도 `entity summary`와 별개로 `trigger usage history`, `target reservation history`, `group membership history` 같은 time series 레이어를 두는 편이 좋다.

### 5.10. 3D Results와 The Results Viewer
- 관련 문서: [3D Results](https://www.thunderheadeng.com/docs/2026-1/pathfinder/output/3d-results/), [Results 2026.1](https://www.thunderheadeng.com/docs/2026-1/results/)
- [3D Results](https://www.thunderheadeng.com/docs/2026-1/pathfinder/output/3d-results/)은 Pathfinder에 3D 결과 시각화를 위한 별도 `Pathfinder Results Viewer`가 포함된다고 설명한다. 문서상 Results는 `custom camera tracking`, `data visualization`, `configuration management`, `video export` 도구를 제공한다.
- 기본 설정에서는 시뮬레이션이 끝나면 Results Viewer가 자동 실행되고, 필요하면 Pathfinder 툴바나 Results 메뉴에서 수동으로 열 수 있다.
- [Results 2026.1](https://www.thunderheadeng.com/docs/2026-1/results/) 루트 문서는 Pathfinder 결과 후처리 전용 문서 체계가 별도로 존재함을 보여준다. 즉, Pathfinder 본체와 Results Viewer가 역할을 분담하는 구조다.
- SafeCrowd 시사점:
  - 모델 작성 UI와 후처리 UI를 완전히 같은 화면에 우겨 넣기보다, 장기적으로는 `작성/실행`과 `재생/분석/내보내기`를 분리하는 설계가 더 확장성이 높다.
  - 최소한 결과 아티팩트는 별도 viewer나 웹 리포트에서 재생 가능한 형태로 남기는 편이 맞다.

## 6. 결과 시각화 기능

### 6.1. Occupant Contours / Heat Maps
- 관련 문서: [Occupant Contours / Heat Maps](https://www.thunderheadeng.com/docs/2026-1/results/viewing-pathfinder-output/occupant-contours/)
- Results 2026.1의 [Occupant Contours / Heat Maps](https://www.thunderheadeng.com/docs/2026-1/results/viewing-pathfinder-output/occupant-contours/)는 바닥, 이동 궤적, 인원 모델 위에 동적 데이터를 겹쳐 보여준다.
- 제공되는 contour 예시는 다음과 같다.
  - `Congestion`
  - `Congestion (Maximum)`
  - `Density`
  - `Level of Service`
  - `Social Distance`
  - `Social Linkage`
  - `Social Usage`
  - `Speed`
  - `Normalized Speed`
  - `Time to Exit`
  - `Travel Distance`
  - `Travel Distance Remaining`
  - `Usage (Instantaneous)`
  - `Usage (Accumulated)`
- 또한 contour는 `Average`, `Maximum` 같은 필터를 얹어 trailing interval 기준으로 부드럽게 하거나 최댓값을 볼 수 있다.
- SafeCrowd 시사점:
  - 우리도 처음부터 모든 heat map을 구현할 필요는 없지만, `Density`, `Congestion`, `Time to Exit`, `Travel Distance Remaining`은 우선순위가 높다.
  - 위험 정의 문서의 `고밀도 지속시간`, `정체 상태`, `탈출 지연`, `delta heatmap`과 직접 연결된다.

### 6.2. 시야 기반 시각화
- 관련 문서: [Pathfinder 2026.1 메인](https://www.thunderheadeng.com/docs/2026-1/pathfinder/), [Occupant Vision Contours](https://www.thunderheadeng.com/docs/2026-1/results/viewing-pathfinder-output/vision/)
- [Pathfinder 2026.1 메인](https://www.thunderheadeng.com/docs/2026-1/pathfinder/) 소개 페이지는 새 `Occupant Vision Contours`를 강조한다.
- 제공 값은 `Current Viewer Count`, `Unique Viewer Count`, `Vision Time`이다.
- [Occupant Vision Contours](https://www.thunderheadeng.com/docs/2026-1/results/viewing-pathfinder-output/vision/) 문서는 기본 시야 원뿔을 `30m` 거리와 `60°` 시야각으로 설명하고, geometry obstruction을 반영해 어떤 표면이 실제로 보였는지 누적한다.
- 또한 `Time Step`, `Time Range`, `Vision Geometry`, `Sample Density`, `Slope Filter`, `Name Filter`로 계산 범위를 조절할 수 있다.
- 설명상 이 기능은 표지, wayfinding, 안내 요소의 가시성과 배치 평가에 유용하다.
- SafeCrowd 시사점:
  - 현재 SafeCrowd의 핵심은 대피 성능이지만, 향후 `표지 배치`, `유도 전략`, `안내 요소 가시성`을 검토하려면 매우 참고할 만하다.
  - 우리가 정의한 `시야 제한`과 `유도 신호` 모델을 결과 시각화로 확장할 때 직접 벤치마크할 수 있다.

### 6.3. 이동 거리 기반 시각화
- 관련 문서: [Pathfinder 2026.1 메인](https://www.thunderheadeng.com/docs/2026-1/pathfinder/), [Occupant Contours / Heat Maps](https://www.thunderheadeng.com/docs/2026-1/results/viewing-pathfinder-output/occupant-contours/), [Occupant Coloring](https://www.thunderheadeng.com/docs/2026-1/results/viewing-pathfinder-output/occupant-coloring/)
- [Pathfinder 2026.1 메인](https://www.thunderheadeng.com/docs/2026-1/pathfinder/) 소개 페이지는 `Travel Distance`와 `Travel Distance Remaining` contour를 새 기능으로 설명한다.
- 설명상 긴 우회 경로, 비효율적 동선, 출구 배치 문제를 후처리에서 파악하는 데 유용하다.
- SafeCrowd 시사점:
  - 우리도 대피 시간만이 아니라 “얼마나 비효율적으로 이동했는가”를 따로 봐야 한다.
  - 운영 대안 추천 시 `distance remaining hotspot`은 출구 추가 개방이나 유도 변경 추천 근거가 될 수 있다.

### 6.4. Occupant Coloring과 Paths의 분석 가치
- 관련 문서: [Occupant Coloring](https://www.thunderheadeng.com/docs/2026-1/results/viewing-pathfinder-output/occupant-coloring/), [Viewing Occupant Paths](https://www.thunderheadeng.com/docs/2026-1/results/viewing-pathfinder-output/occupant-paths/)
- [Occupant Coloring](https://www.thunderheadeng.com/docs/2026-1/results/viewing-pathfinder-output/occupant-coloring/)은 점유자를 `movement group`, `behavior`, `profile`, `view direction`, `speed`, `normalized speed`, `time to exit`, `travel distance`, `travel distance remaining`, `congestion`, `visibility`, `FED`, `occupant contour` 값으로 색칠할 수 있다.
- [Viewing Occupant Paths](https://www.thunderheadeng.com/docs/2026-1/results/viewing-pathfinder-output/occupant-paths/)는 점유자별 경로를 시뮬레이션 시작 시점부터 현재 재생 시점까지 그려 주며, 경로 색도 현재 Occupant Coloring 규칙을 그대로 따른다.
- SafeCrowd 시사점:
  - 개체 궤적과 상태 변화를 한 화면에 합쳐 보는 기능은 병목 원인 추적과 시나리오 디버깅에 매우 유용하다.
  - 특히 `By View Direction`, `By Speed`, `By Occupant Contour` 색상 규칙은 역방향 흐름, 정체, 경로 비효율을 빠르게 드러내는 좋은 패턴이다.

### 6.5. Occupant 표시, 선택, spacing disks
- 관련 문서: [Displaying Occupants](https://www.thunderheadeng.com/docs/2026-1/results/viewing-pathfinder-output/displaying-occupants/), [Selecting Occupants](https://www.thunderheadeng.com/docs/2026-1/results/viewing-pathfinder-output/selecting-occupants/), [Occupant Spacing Disks](https://www.thunderheadeng.com/docs/2026-1/results/viewing-pathfinder-output/occupant-spacing-disks/)
- [Displaying Occupants](https://www.thunderheadeng.com/docs/2026-1/results/viewing-pathfinder-output/displaying-occupants/)는 occupant를 disk, cylinder, mannequin, person avatar 같은 여러 표시 레벨로 보여 주는 흐름을 설명한다. 화면 복잡도와 분석 목적에 따라 표현 수준을 바꾸는 사용 패턴이 전제돼 있다.
- [Selecting Occupants](https://www.thunderheadeng.com/docs/2026-1/results/viewing-pathfinder-output/selecting-occupants/)는 클릭 선택과 `Find Occupant by ID`를 모두 지원한다. 선택된 occupant와 경로는 노란색으로 강조되고, 다중 선택도 가능하다.
- [Occupant Spacing Disks](https://www.thunderheadeng.com/docs/2026-1/results/viewing-pathfinder-output/occupant-spacing-disks/)는 지정 반경 기준 overlap을 원반으로 시각화한다. 전체 occupant 또는 선택 occupant만 표시할 수 있고, social distancing 반경을 그대로 쓰거나 half-sized display로 겹침을 줄일 수 있다.
- SafeCrowd 시사점:
  - 결과 viewer는 단순 재생기가 아니라 `표시 레벨`, `개체 선택`, `상세 분석`이 이어지는 탐색 도구여야 한다.
  - 개별 occupant 선택이 plot/export와 연결되면 병목 원인, 이상 개체, 특정 운영 요원의 행동을 빠르게 추적할 수 있다.

### 6.6. Occupant 2D plots와 FDS 연계 지표
- 관련 문서: [2D Object Plots](https://www.thunderheadeng.com/docs/2026-1/results/viewing-pathfinder-output/2d-object-plots/), [Calculating Fractional Effective Dose (FED)](https://www.thunderheadeng.com/docs/2026-1/results/viewing-pathfinder-output/fed/), [Calculating Occupant Visibility through Smoke](https://www.thunderheadeng.com/docs/2026-1/results/viewing-pathfinder-output/visibility/)
- [2D Object Plots](https://www.thunderheadeng.com/docs/2026-1/results/viewing-pathfinder-output/2d-object-plots/)는 occupant, door, room 객체를 우클릭해 시간 그래프로 바로 넘긴다. occupant는 `congestion`, `normalized speed`, `time to exit`, `travel distance remaining`, `view direction`, `visibility`, `FED`, `x/y/z`까지, door는 step usage/width/queue, room은 occupant count를 plot할 수 있다.
- 같은 문서는 occupant plot이 animation file과 detailed occupant CSV를 함께 사용한다고 설명한다. 즉 후처리 plot은 viewer 내부 재생 정보와 원본 로그를 결합한 계층이다.
- [Calculating Fractional Effective Dose (FED)](https://www.thunderheadeng.com/docs/2026-1/results/viewing-pathfinder-output/fed/)는 Pathfinder와 PyroSim 결과를 함께 불러왔을 때 CO2, CO, O2 종 농도 데이터를 사용해 FED를 계산한다. Results는 Plot3D, 3D Slice, 2D Slice를 모두 입력으로 쓸 수 있고, 계산된 FED는 coloring과 2D plot으로 접근한다.
- [Calculating Occupant Visibility through Smoke](https://www.thunderheadeng.com/docs/2026-1/results/viewing-pathfinder-output/visibility/)는 occupant 위치와 시선 방향, FDS 연기 obscuration을 결합해 standing/crawling 두 높이의 visibility를 계산한다. standing eye level은 occupant height의 `94%`, crawling은 `29%`로 둔다.
- SafeCrowd 시사점:
  - 결과 viewer는 전역 heat map만이 아니라 `entity time series`를 직접 뽑을 수 있어야 한다.
  - 연기/독성/가시성 같은 환경장과 보행 결과를 후처리에서 결합하는 구조가 중요하다. SafeCrowd도 위험 노출 계산을 엔진에만 고정하지 말고 분석 계층에서 재계산할 여지를 두는 편이 낫다.

### 6.7. Occupant Proximity Analysis
- 관련 문서: [Occupant Proximity Analysis](https://www.thunderheadeng.com/docs/2026-1/results/viewing-pathfinder-output/occupant-proximity/)
- [Occupant Proximity Analysis](https://www.thunderheadeng.com/docs/2026-1/results/viewing-pathfinder-output/occupant-proximity/)는 Results에서 별도 분석 작업으로 수행되며, EXPOSED 모델을 바탕으로 occupant가 서로에게 얼마나 노출되는지 계산한다.
- 주요 파라미터는 `Proximity Radius`, `Analysis Timestep`, `Report Mode(Simple/Advanced)`, `Occupant counts affect global assessment`다.
- 문서상 전역 평가지표 `G`와 `k=0` 노출 시간을 보고, `Advanced` 모드에서는 각 `k` 값에 대한 노출 시간을 더 자세히 본다. 플롯은 `Occ Counts vs Time`, `Exposure Time vs. Occ Counts`, `[k=0] Exposure Time`, `Maximum/Average Exposure Time vs. Time`까지 제공한다.
- 다만 이 분석은 navigation mesh와 room boundary를 기준으로 노출을 판단하므로, 경기장 좌석열처럼 열린 공간을 방 단위로 많이 쪼갠 모델에서는 과소평가가 생길 수 있다고 문서가 명시한다.
- SafeCrowd 시사점:
  - 감염 노출, 접촉 밀집, 국소 혼잡도를 후처리로 평가하는 기능은 일반 대피 시간 지표와 별도로 가치가 있다.
  - 대신 모델링 단위가 노출 계산에 직접 영향을 주므로, SafeCrowd에서 근접도 분석을 넣는다면 공간 분할 방식과 계산 기준을 함께 설계해야 한다.

### 6.8. The Results Viewer의 CSV 수용 구조
- 관련 문서: [Viewing CSV Output](https://www.thunderheadeng.com/docs/2026-1/results/viewing-csv-output/), [Supported CSV Formats](https://www.thunderheadeng.com/docs/2026-1/results/viewing-csv-output/supported-formats/), [Loading CSV Files](https://www.thunderheadeng.com/docs/2026-1/results/viewing-csv-output/loading/), [Managing CSV Files](https://www.thunderheadeng.com/docs/2026-1/results/viewing-csv-output/managing/)
- [Viewing CSV Output](https://www.thunderheadeng.com/docs/2026-1/results/viewing-csv-output/)은 Results가 Pathfinder/FDS의 다수 CSV를 직접 읽고, visualization file과 함께 CSV 링크를 저장한다고 설명한다.
- [Supported CSV Formats](https://www.thunderheadeng.com/docs/2026-1/results/viewing-csv-output/supported-formats/) 기준으로, generic CSV도 `맨 앞 header rows`, `초 단위의 증가하는 time column`, `최소 하나의 numeric column`만 있으면 읽을 수 있다. quantity/unit 행은 필수는 아니지만 강하게 권장된다.
- [Loading CSV Files](https://www.thunderheadeng.com/docs/2026-1/results/viewing-csv-output/loading/)은 파일명으로 알려진 CSV 타입을 자동 추정하고, 알 수 없는 경우 `<custom>`으로 time column, header row 수, description/quantity/unit row를 수동 지정하게 한다.
- [Managing CSV Files](https://www.thunderheadeng.com/docs/2026-1/results/viewing-csv-output/managing/)에 따르면 로드된 CSV는 Navigation View의 `XY Plot Data` 아래 `CSV File` 객체로 생기고, time 외 컬럼이 각각 `Time Series` 객체가 된다. visualization 저장 시 CSV는 상대 경로로 기억되며, refresh 시 컬럼 추가/삭제/순서 변경도 이름·quantity·unit이 유지되면 추적한다.
- SafeCrowd 시사점:
  - 결과 분석 UI는 시뮬레이터 전용 바이너리에만 기대지 말고, `generic time series schema`를 받아들이는 독립 viewer로 설계할 가치가 있다.
  - 경로 저장, 컬럼 메타데이터, refresh 내구성까지 고려하면 결과 파일 포맷은 값뿐 아니라 `description/quantity/unit` 계층을 같이 가져야 한다.

### 6.9. 2D Plot과 Data Export의 후처리 워크플로
- 관련 문서: [Plotting and Exporting CSV Files](https://www.thunderheadeng.com/docs/2026-1/results/viewing-csv-output/plotting-and-export/), [Viewing 2D Plots](https://www.thunderheadeng.com/docs/2026-1/results/viewing-2d-plots/), [Exporting Data](https://www.thunderheadeng.com/docs/2026-1/results/exporting-data/)
- [Viewing 2D Plots](https://www.thunderheadeng.com/docs/2026-1/results/viewing-2d-plots/)은 `Data3D Points`, `Sample Points`, `Time Series Objects`를 같은 플롯 체계에서 다룬다. quantity와 unit이 같은 여러 객체는 한 plot에 겹쳐 볼 수 있고, 표시 방식은 `Line`, `Stem`, `Step`, axis range, grid, marker까지 조절된다.
- [Exporting Data](https://www.thunderheadeng.com/docs/2026-1/results/exporting-data/)는 선택한 시계열 객체를 CSV/TSV로 다시 내보내게 해 준다. 여러 source를 섞을 때는 각 source의 keyframe 합집합 기준으로 시간이 만들어져 일부 값은 보간될 수 있다.
- 같은 문서의 `Exporting Occupant History CSV Data`는 시뮬레이션 당시 상세 occupant output을 켜지 않았더라도, Results에서 visualization 파일을 기반으로 occupant history 성격의 CSV를 다시 뽑을 수 있다고 설명한다. 다만 이 경로는 CSV만 지원하고 JSON은 지원하지 않는다.
- export 옵션은 전체 occupant 대상 혹은 선택 occupant만 대상으로 할 수 있고, `Merge into one file`, `Create one file per occupant`, `Min. Output Interval`, `Time Range`를 조절한다.
- SafeCrowd 시사점:
  - 실행 중 출력만으로 모든 분석 수요를 해결하려고 하면 비용이 크다. Results처럼 `재생 파일 -> 선택적 재수출` 흐름을 두면 상세 로그를 항상 저장하지 않아도 된다.
  - 또한 비교 그래프와 데이터 내보내기를 후처리 계층에 두면, 시뮬레이터는 계산에 집중하고 viewer는 분석과 보고서 생성을 담당하는 분리가 가능하다.

## 7. SafeCrowd 우선순위로 다시 정리

1차 조사 내용을 다시 보면, 현재 우선순위는 "모델링 범위의 넓이"보다 "운영 의사결정에 바로 연결되는 최소 코어"를 먼저 확보하는 쪽이 더 타당하다.

### 7.1. MVP 코어
- `Floor/Room/Wall/Door/Obstruction/Obstacle`를 분리한 공간 계층과 문 유량 측정 구조
- `stairs/ramp/escalator/walkway`를 공통 connector + modifier로 다루는 방식
- 프로필별 이동 제약, usable connector restriction, 분포형 속도/가속/간격 파라미터
- 단순 최단거리보다 `국소 이동 시간 + 대기 + 잔여 경로`를 반영하는 기본 출구 선택 비용
- 행동 시퀀스, 트리거 기반 행동 전환, occupant tags
- 초기 배치 + 동적 인원 유입 + scenario/run/variation 결과 폴더 구조
- `door history`, `room history`, `measurement regions`, `cumulative JSON`, 선택적 per-occupant history
- `Density`, `Congestion`, `Time to Exit`, `Travel Distance Remaining` 중심의 결과 레이어

### 7.2. 1차 확장
- 그룹 이동
- 대기열과 예약형 목표 지점
- generic results viewer, 2D plot, CSV/TSV 재내보내기
- Monte Carlo variation 생성 규칙과 반복 실행 요약 고도화
- 근접도/대인 거리 분석

### 7.3. 중기 확장 후보
- 승강기 시나리오
- 구조 인력/보조 대피 시나리오
- 시야/표지/유도 가시화
- 연기/FDS/FED 연동
- 더 정교한 위험 노출 후처리

### 7.4. Pathfinder와의 차별화 후보
- Pathfinder의 강점은 입력/실행/시각화의 폭이다.
- SafeCrowd는 여기에 더해 `위험 징후 탐지 -> 운영 대안 추천 -> 대안 시나리오 자동 생성 -> 비교 근거 제시` 흐름을 강화하는 쪽이 차별화에 더 직접적이다.
- 즉, SafeCrowd는 "시뮬레이터 자체"보다 `의사결정 지원 레이어`를 제품의 중심 가치로 두는 편이 맞다.

## 8. 심화 조사 후보

현재 조사 대상 트리의 1차 조사는 완료했다. 아래 항목은 "아직 조사 안 됨"이라기보다, SafeCrowd 설계 깊이를 더 올리고 싶을 때 추가로 파고들 가치가 큰 주제다.

### 8.1. 추가로 파고들 가치가 큰 섹션
1. `Geometry`
   - `Generating Geometry from Images`, `PDF Files`, `Manual IFC Mapping`, `FDS Obstacles`의 입력 자동화 수준
   - import 후 수동 보정 워크플로와 obstruction/room/door 매핑 한계를 어디까지 제어하는지
2. `Profiles + Occupants`
   - `Animation Tab`과 일부 runtime profile switching/familiar path 계열이 실제 동선 의사결정에 얼마나 개입하는지
   - familiar path, profile switching, occupant tagging 같은 세부 기능이 어느 정도까지 열려 있는지
3. `Results Viewer`
   - `Advanced`, `Technical Reference`, `Troubleshooting`을 조사해 후처리 한계와 extension 포인트를 정리할지
   - Pathfinder 본체와 Results 분리 구조를 우리 제품에 어떻게 대응시킬지

### 8.2. SafeCrowd 설계로 바로 옮길 핵심 질문
- 문/계단/구역 단위 지표를 어떤 데이터 구조로 저장할 것인가
- 행동 전환을 ECS에서 상태 컴포넌트로 둘 것인가, 이벤트 스크립트 레이어로 둘 것인가
- 결과 후처리를 실시간과 배치 분석으로 어떻게 나눌 것인가

## 출처
- Pathfinder 2026.1 메인: <https://www.thunderheadeng.com/docs/2026-1/pathfinder/>
- Geometry - Floors: <https://www.thunderheadeng.com/docs/2026-1/pathfinder/geometry/floors/>
- Geometry - Rooms: <https://www.thunderheadeng.com/docs/2026-1/pathfinder/geometry/rooms/>
- Geometry - Obstructions: <https://www.thunderheadeng.com/docs/2026-1/pathfinder/geometry/obstructions/>
- Geometry - Walls: <https://www.thunderheadeng.com/docs/2026-1/pathfinder/geometry/walls/>
- Geometry - Obstacles: <https://www.thunderheadeng.com/docs/2026-1/pathfinder/geometry/obstacles/>
- Geometry - Doors: <https://www.thunderheadeng.com/docs/2026-1/pathfinder/geometry/doors/>
- Geometry - Elevators: <https://www.thunderheadeng.com/docs/2026-1/pathfinder/geometry/elevators/>
- Geometry - Elevators - Introduction: <https://www.thunderheadeng.com/docs/2026-1/pathfinder/geometry/elevators/introduction/>
- Geometry - Elevators - Configuration: <https://www.thunderheadeng.com/docs/2026-1/pathfinder/geometry/elevators/configuration/>
- Geometry - Elevators - Creating an Elevator from a Single Room: <https://www.thunderheadeng.com/docs/2026-1/pathfinder/geometry/elevators/creating/>
- Geometry - Elevators - Creating Multiple Elevators from Different Rooms: <https://www.thunderheadeng.com/docs/2026-1/pathfinder/geometry/elevators/creating-multiple/>
- Geometry - Elevators - Creating an Elevator Bank: <https://www.thunderheadeng.com/docs/2026-1/pathfinder/geometry/elevators/creating-banks/>
- Geometry - Elevators - Connecting / Disconnecting Floors from an Elevator: <https://www.thunderheadeng.com/docs/2026-1/pathfinder/geometry/elevators/connecting-disconnecting/>
- Geometry - Materials: <https://www.thunderheadeng.com/docs/2026-1/pathfinder/geometry/materials/>
- Geometry - Stairs: <https://www.thunderheadeng.com/docs/2026-1/pathfinder/geometry/stairs/>
- Geometry - Ramps: <https://www.thunderheadeng.com/docs/2026-1/pathfinder/geometry/ramps/>
- Geometry - Escalators: <https://www.thunderheadeng.com/docs/2026-1/pathfinder/geometry/escalators/>
- Geometry - Moving Walkways: <https://www.thunderheadeng.com/docs/2026-1/pathfinder/geometry/moving-walkways/>
- Geometry - Generating Geometry from CAD: <https://www.thunderheadeng.com/docs/2026-1/pathfinder/geometry/generating-from-cad/>
- Profiles - Characteristics Tab: <https://www.thunderheadeng.com/docs/2026-1/pathfinder/profiles/characteristics/>
- Profiles - Movement Tab: <https://www.thunderheadeng.com/docs/2026-1/pathfinder/profiles/movement/>
- Profiles - Restrictions Tab: <https://www.thunderheadeng.com/docs/2026-1/pathfinder/profiles/restrictions/>
- Profiles - Door Choice Tab: <https://www.thunderheadeng.com/docs/2026-1/pathfinder/profiles/door-choice/>
- Profiles - Output Tab: <https://www.thunderheadeng.com/docs/2026-1/pathfinder/profiles/output/>
- Profiles - Advanced Tab: <https://www.thunderheadeng.com/docs/2026-1/pathfinder/profiles/advanced/>
- Behaviors - Creating a New Behavior: <https://www.thunderheadeng.com/docs/2026-1/pathfinder/behaviors/creating-new/>
- Occupants - Generating Occupants: <https://www.thunderheadeng.com/docs/2026-1/pathfinder/occupants/generating/>
- Occupants - Redistributing Profiles and Behaviors: <https://www.thunderheadeng.com/docs/2026-1/pathfinder/occupants/redistribute-profiles/>
- Occupants - Randomizing Occupant Positions: <https://www.thunderheadeng.com/docs/2026-1/pathfinder/occupants/randomize-position/>
- Occupants - Reducing Population: <https://www.thunderheadeng.com/docs/2026-1/pathfinder/occupants/reduce-population/>
- Occupants - Occupant Sources: <https://www.thunderheadeng.com/docs/2026-1/pathfinder/occupants/occupant-source/>
- Occupants - Occupant Tags: <https://www.thunderheadeng.com/docs/2026-1/pathfinder/occupants/occupant-tags/>
- Movement Groups - Configuration: <https://www.thunderheadeng.com/docs/2026-1/pathfinder/groups/configuration/>
- Movement Groups - Template Configuration: <https://www.thunderheadeng.com/docs/2026-1/pathfinder/groups/template-configuration/>
- Movement Groups - Creating a Movement Group: <https://www.thunderheadeng.com/docs/2026-1/pathfinder/groups/movement-groups/>
- Movement Groups - Creating a Movement Group Template: <https://www.thunderheadeng.com/docs/2026-1/pathfinder/groups/movement-group-templates/>
- Movement Groups - Creating Movement Groups from a Template before a simulation: <https://www.thunderheadeng.com/docs/2026-1/pathfinder/groups/groups-from-occs/>
- Movement Groups - Creating Movement Groups from a Template during a simulation: <https://www.thunderheadeng.com/docs/2026-1/pathfinder/groups/groups-from-sources/>
- Queues: <https://www.thunderheadeng.com/docs/2026-1/pathfinder/queues/>
- Queues - Service Points: <https://www.thunderheadeng.com/docs/2026-1/pathfinder/queues/services/>
- Queues - Queue Paths: <https://www.thunderheadeng.com/docs/2026-1/pathfinder/queues/paths/>
- Queues - Path Nodes: <https://www.thunderheadeng.com/docs/2026-1/pathfinder/queues/path-nodes/>
- Targets - Creating Occupant Targets: <https://www.thunderheadeng.com/docs/2026-1/pathfinder/targets/creating/>
- Targets - Occupant Target Properties: <https://www.thunderheadeng.com/docs/2026-1/pathfinder/targets/properties/>
- Targets - Orienting Occupant Targets: <https://www.thunderheadeng.com/docs/2026-1/pathfinder/targets/orientation/>
- Targets - Prioritizing Occupant Targets: <https://www.thunderheadeng.com/docs/2026-1/pathfinder/targets/priority/>
- Targets - Using Occupant Targets: <https://www.thunderheadeng.com/docs/2026-1/pathfinder/targets/using-targets/>
- Targets - Occupant Target Reservation System: <https://www.thunderheadeng.com/docs/2026-1/pathfinder/targets/reservation/>
- Targets - Occupant Target Limitations: <https://www.thunderheadeng.com/docs/2026-1/pathfinder/targets/limitations/>
- Targets - Occupant Target Output: <https://www.thunderheadeng.com/docs/2026-1/pathfinder/targets/output/>
- Triggers - Creating Triggers: <https://www.thunderheadeng.com/docs/2026-1/pathfinder/triggers/creating/>
- Triggers - Trigger Behavior: <https://www.thunderheadeng.com/docs/2026-1/pathfinder/triggers/behavior/>
- Triggers - Occupant Response to a Trigger: <https://www.thunderheadeng.com/docs/2026-1/pathfinder/triggers/response/>
- Triggers - Trigger Memory: <https://www.thunderheadeng.com/docs/2026-1/pathfinder/triggers/memory/>
- Triggers - Trigger Rank: <https://www.thunderheadeng.com/docs/2026-1/pathfinder/triggers/rank/>
- Triggers - Trigger Properties: <https://www.thunderheadeng.com/docs/2026-1/pathfinder/triggers/properties/>
- Triggers - Trigger Limitations: <https://www.thunderheadeng.com/docs/2026-1/pathfinder/triggers/limitations/>
- Triggers - Trigger Output: <https://www.thunderheadeng.com/docs/2026-1/pathfinder/triggers/output/>
- Simulation - Parameters: <https://www.thunderheadeng.com/docs/2026-1/pathfinder/simulation/parameters/>
- Simulation - Starting and Managing Simulations: <https://www.thunderheadeng.com/docs/2026-1/pathfinder/simulation/starting/>
- Simulation - Stopping and Resuming Simulations: <https://www.thunderheadeng.com/docs/2026-1/pathfinder/simulation/stopping/>
- Output: <https://www.thunderheadeng.com/docs/2026-1/pathfinder/output/>
- Advanced - Assisted Evacuation: <https://www.thunderheadeng.com/docs/2026-1/pathfinder/advanced/assisted-evacuation/>
- Advanced - Assisted Evacuation - Introduction: <https://www.thunderheadeng.com/docs/2026-1/pathfinder/advanced/assisted-evacuation/introduction/>
- Advanced - Assisted Evacuation - Basic Usage: <https://www.thunderheadeng.com/docs/2026-1/pathfinder/advanced/assisted-evacuation/basic-usage/>
- Advanced - Assisted Evacuation - Configuration: <https://www.thunderheadeng.com/docs/2026-1/pathfinder/advanced/assisted-evacuation/configuration/>
- Advanced - Assisted Evacuation - Creating an Assisted Evacuation Team: <https://www.thunderheadeng.com/docs/2026-1/pathfinder/advanced/assisted-evacuation/creating-teams/>
- Advanced - Assisted Evacuation - Assigning a Client to a Team: <https://www.thunderheadeng.com/docs/2026-1/pathfinder/advanced/assisted-evacuation/assigning-clients/>
- Advanced - Assisted Evacuation - Assigning an Assistant to a Team: <https://www.thunderheadeng.com/docs/2026-1/pathfinder/advanced/assisted-evacuation/adding-assistants/>
- Advanced - Measurement Regions: <https://www.thunderheadeng.com/docs/2026-1/pathfinder/advanced/measurement-regions/>
- Advanced - Monte Carlo: <https://www.thunderheadeng.com/docs/2026-1/pathfinder/advanced/monte-carlo/>
- Appendices - Technical Reference - Elevators: <https://www.thunderheadeng.com/docs/2026-1/pathfinder/appendices/technical-reference/elevators/>
- Appendices - Technical Reference - Assisted Evacuation: <https://www.thunderheadeng.com/docs/2026-1/pathfinder/appendices/technical-reference/assisted-evac/>
- Appendices - Verification - Elevator Loading: <https://www.thunderheadeng.com/docs/2026-1/pathfinder/appendices/verification-validation/behavior/elevator-loading/>
- Appendices - Verification - Assisted Evacuation: <https://www.thunderheadeng.com/docs/2026-1/pathfinder/appendices/verification-validation/special-features/assisted-evac/>
- Output - Summary Report: <https://www.thunderheadeng.com/docs/2026-1/pathfinder/output/summary/>
- Output - Monte Carlo Results: <https://www.thunderheadeng.com/docs/2026-1/pathfinder/output/monte-carlo/>
- Output - Cumulative Output: <https://www.thunderheadeng.com/docs/2026-1/pathfinder/output/cumulative/>
- Output - Door History: <https://www.thunderheadeng.com/docs/2026-1/pathfinder/output/doors/>
- Output - Room History: <https://www.thunderheadeng.com/docs/2026-1/pathfinder/output/rooms/>
- Output - Measurement Regions: <https://www.thunderheadeng.com/docs/2026-1/pathfinder/output/measurement-regions/>
- Output - Occupant Parameters: <https://www.thunderheadeng.com/docs/2026-1/pathfinder/output/occupant-params/>
- Output - Interpersonal Distance: <https://www.thunderheadeng.com/docs/2026-1/pathfinder/output/interpersonal-distance/>
- Output - Occupant Summary: <https://www.thunderheadeng.com/docs/2026-1/pathfinder/output/occupant-summary/>
- Output - Occupant History: <https://www.thunderheadeng.com/docs/2026-1/pathfinder/output/occupant-history/>
- Output - Groups Output: <https://www.thunderheadeng.com/docs/2026-1/pathfinder/output/groups/>
- Output - Triggers History: <https://www.thunderheadeng.com/docs/2026-1/pathfinder/output/triggers/>
- Output - Occupant Target History: <https://www.thunderheadeng.com/docs/2026-1/pathfinder/output/targets/>
- Output - Simulation Variation Output: <https://www.thunderheadeng.com/docs/2026-1/pathfinder/output/variations/>
- Output - 3D Results: <https://www.thunderheadeng.com/docs/2026-1/pathfinder/output/3d-results/>
- Results 2026.1: <https://www.thunderheadeng.com/docs/2026-1/results/>
- Results 2026.1 - Occupant Contours / Heat Maps: <https://www.thunderheadeng.com/docs/2026-1/results/viewing-pathfinder-output/occupant-contours/>
- Results 2026.1 - Occupant Vision Contours: <https://www.thunderheadeng.com/docs/2026-1/results/viewing-pathfinder-output/vision/>
- Results 2026.1 - Displaying Occupants: <https://www.thunderheadeng.com/docs/2026-1/results/viewing-pathfinder-output/displaying-occupants/>
- Results 2026.1 - Occupant Coloring: <https://www.thunderheadeng.com/docs/2026-1/results/viewing-pathfinder-output/occupant-coloring/>
- Results 2026.1 - Occupant Spacing Disks: <https://www.thunderheadeng.com/docs/2026-1/results/viewing-pathfinder-output/occupant-spacing-disks/>
- Results 2026.1 - Selecting Occupants: <https://www.thunderheadeng.com/docs/2026-1/results/viewing-pathfinder-output/selecting-occupants/>
- Results 2026.1 - Viewing Occupant Paths: <https://www.thunderheadeng.com/docs/2026-1/results/viewing-pathfinder-output/occupant-paths/>
- Results 2026.1 - Occupant Proximity Analysis: <https://www.thunderheadeng.com/docs/2026-1/results/viewing-pathfinder-output/occupant-proximity/>
- Results 2026.1 - 2D Object Plots: <https://www.thunderheadeng.com/docs/2026-1/results/viewing-pathfinder-output/2d-object-plots/>
- Results 2026.1 - Calculating Fractional Effective Dose (FED): <https://www.thunderheadeng.com/docs/2026-1/results/viewing-pathfinder-output/fed/>
- Results 2026.1 - Calculating Occupant Visibility through Smoke: <https://www.thunderheadeng.com/docs/2026-1/results/viewing-pathfinder-output/visibility/>
- Results 2026.1 - Viewing CSV Output: <https://www.thunderheadeng.com/docs/2026-1/results/viewing-csv-output/>
- Results 2026.1 - Supported CSV Formats: <https://www.thunderheadeng.com/docs/2026-1/results/viewing-csv-output/supported-formats/>
- Results 2026.1 - Loading CSV Files: <https://www.thunderheadeng.com/docs/2026-1/results/viewing-csv-output/loading/>
- Results 2026.1 - Managing CSV Files: <https://www.thunderheadeng.com/docs/2026-1/results/viewing-csv-output/managing/>
- Results 2026.1 - Plotting and Exporting CSV Files: <https://www.thunderheadeng.com/docs/2026-1/results/viewing-csv-output/plotting-and-export/>
- Results 2026.1 - Viewing 2D Plots: <https://www.thunderheadeng.com/docs/2026-1/results/viewing-2d-plots/>
- Results 2026.1 - Exporting Data: <https://www.thunderheadeng.com/docs/2026-1/results/exporting-data/>
