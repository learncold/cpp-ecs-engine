# DWG Import Fixtures

DWG samples for manual and automated import testing.

These files are kept separate from `tests/dxf/` because they require DWG-capable tooling or conversion before the current DXF import path can consume them directly.

## Included Files

| Path | Source | Notes |
| --- | --- | --- |
| `autodesk_acad_samples/*.dwg` | Autodesk AutoCAD LT sample files | Official AutoCAD LT 2010+ sample set. Useful for broad DWG parser/conversion smoke tests. |
| `cadforum/sample_floor_plan.dwg` | CADForum block catalog | Small residential sample floor plan. |
| `dwgshare/sample_apartment_floor_plans.dwg` | DWGShare | Apartment floor plan sample extracted from the downloaded ZIP. |
| `dwgshare/shopping_center_free_autocad_drawings.dwg` | DWGShare | Shopping center plan sample extracted from the downloaded ZIP; useful for evacuation-scale import trials. |

## Provenance

- Autodesk source: https://www.autodesk.com/support/technical/article/caas/tsarticles/ts/01em4r6LLJgnQQVBlk5GqD.html
- CADForum source: https://www.cadforum.cz/catalog_en/block.asp?blk=12286
- DWGShare apartment source: https://dwgshare.com/256-free-download-of-cad-drawings-for-sample-apartment-floor-plans/
- DWGShare shopping center source: https://dwgshare.com/shopping-center-free-autocad-drawings/

## Notes

- The original ZIP archives downloaded from DWGShare are not committed; only the extracted DWG files are kept.
- AutoCAD can export these DWG files to DXF for the existing import module test path.
- Keep additional large or third-party CAD files in this folder with provenance notes.
