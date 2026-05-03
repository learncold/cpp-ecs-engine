# Test DXF Fixtures

DXF 임포트 테스트용 건물/층 평면 샘플 모음입니다.
시연용 평면 자산은 `assets/demo-layouts/` 에서 별도로 관리합니다.

이 폴더에는 성격이 다른 두 종류의 샘플이 있습니다.

1. 외부 공개 저장소에서 가져온 실제 건물 평면 DXF
2. 테스트 범위를 보강하기 위해 로컬에서 생성한 건물 평면 DXF

## Included Files

| File | Type | Notes | Source |
| --- | --- | --- | --- |
| `apartment_floor_plan.dxf` | downloaded | 2-bedroom apartment floor plan with walls, doors, windows, labels, and dimensions | `ylevental/cad-portfolio` `librecad/floor_plan.dxf` |
| `home_plan.dxf` | generated | compact residential floor plan with `SPACE_*`, `WALLS`, `DOORS`, `EXIT`, `WINDOWS`, `OBSTACLE` layers | local generator |
| `office_suite.dxf` | generated | small office suite floor plan with lobby, meeting room, open office, kitchenette, storage, and exits | local generator |
| `evacuation_complex_large.dxf` | generated | large multi-wing building for evacuation simulation with concourse, assembly spaces, service wings, obstacles, and 8 perimeter exits | local generator |
| `blocking_review_demo.dxf` | generated | 의도적으로 차단 이슈를 발생시키는 작은 평면 (`tests/DxfImportServiceTests.cpp` 차단 이슈 검출 검증) | local generator |

## Provenance

- Downloaded sample license: [LICENSE.cad-portfolio.txt](./LICENSE.cad-portfolio.txt)
- Downloaded sample upstream repository: https://github.com/ylevental/cad-portfolio
- Downloaded sample upstream file: https://github.com/ylevental/cad-portfolio/blob/main/librecad/floor_plan.dxf
- Local generator: [generate_building_samples.py](./generate_building_samples.py)

## Notes

- `apartment_floor_plan.dxf`는 외부 공개 MIT 저장소에서 가져온 실제 건물 평면입니다.
- `home_plan.dxf`, `office_suite.dxf`, `evacuation_complex_large.dxf`는 SafeCrowd import 테스트에 맞게 로컬에서 생성한 건물 평면 샘플입니다.
- 생성본도 `ezdxf`로 다시 열어 검증했습니다.
- 시연 자산이 필요하면 `assets/demo-layouts/` 를 참조하세요.
