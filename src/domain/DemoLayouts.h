#pragma once

#include "domain/FacilityLayout2D.h"

namespace safecrowd::domain::DemoLayouts {

struct Sprint1FacilityIds {
    static constexpr const char* LayoutId = "demo-fixture-01";
    static constexpr const char* FloorId = "L1";
    static constexpr const char* MainRoomZoneId = "zone-room-1";
    static constexpr const char* SideRoomZoneId = "zone-room-2";
    static constexpr const char* ExitPassageZoneId = "zone-passage-1";
    static constexpr const char* ExitZoneId = "zone-exit-1";
    static constexpr const char* OpeningConnectionId = "conn-opening-1";
    static constexpr const char* DoorwayConnectionId = "conn-doorway-1";
    static constexpr const char* ExitConnectionId = "conn-exit-1";
    static constexpr const char* BarrierId = "barrier-1";
    static constexpr const char* WestWallId = "barrier-wall-west";
    static constexpr const char* MainRoomNorthWallId = "barrier-wall-main-north";
    static constexpr const char* MainRoomSouthWallId = "barrier-wall-main-south";
    static constexpr const char* SideRoomNorthWallId = "barrier-wall-side-north";
    static constexpr const char* SideRoomSouthWallId = "barrier-wall-side-south";
    static constexpr const char* PassageNorthWallId = "barrier-wall-passage-north";
    static constexpr const char* PassageSouthWallId = "barrier-wall-passage-south";
    static constexpr const char* NorthWallId = MainRoomNorthWallId;
    static constexpr const char* SouthWallId = MainRoomSouthWallId;
    static constexpr const char* PassageExitWallUpperId = "barrier-wall-passage-exit-upper";
    static constexpr const char* PassageExitWallLowerId = "barrier-wall-passage-exit-lower";
    static constexpr const char* MainSideWallLowerId = "barrier-wall-main-side-lower";
    static constexpr const char* MainSideWallUpperId = "barrier-wall-main-side-upper";
    static constexpr const char* SidePassageWallLowerId = "barrier-wall-side-passage-lower";
    static constexpr const char* SidePassageWallUpperId = "barrier-wall-side-passage-upper";
};

FacilityLayout2D demoFacility();

}  // namespace safecrowd::domain::DemoLayouts

