# Test DXF Fixtures

DXF 임포트 단위 테스트가 직접 참조하는 픽스처 모음입니다.
시연용 평면 자산은 `assets/demo-layouts/` 에서 별도로 관리합니다.

## Included Files

| File | Notes | Used by |
| --- | --- | --- |
| `office_suite.dxf` | 소형 사무실 평면 (로비/회의실/오픈오피스/주방/창고/출구). `SPACE_*`/`WALLS`/`DOORS`/`EXIT`/`OBSTACLE`/`WINDOWS` 레이어 | `tests/DxfImportServiceTests.cpp` (정상 임포트 경로 검증) |
| `blocking_review_demo.dxf` | 의도적으로 차단 이슈를 발생시키는 작은 평면 | `tests/DxfImportServiceTests.cpp` (차단 이슈 검출 검증) |

## Provenance

- 로컬 생성기: [generate_building_samples.py](./generate_building_samples.py)
- 생성본은 `ezdxf` 로 다시 열어 검증되었습니다.

## Notes

- 이 폴더의 DXF 는 임포트 동작/이슈 검증을 위한 테스트 픽스처입니다.
- 시연 자산이 필요하면 `assets/demo-layouts/` 를 참조하세요.
