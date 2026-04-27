#pragma once

#include "domain/FacilityLayout2D.h"

namespace safecrowd::domain::DemoLayouts {

struct Sprint1FacilityIds {
    static constexpr const char* LayoutId = "demo-fixture-01";
    static constexpr const char* MainRoomZoneId = "zone-room-1";
    static constexpr const char* SideRoomZoneId = "zone-room-2";
    static constexpr const char* ExitCorridorZoneId = "zone-corridor-1";
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
    static constexpr const char* CorridorNorthWallId = "barrier-wall-corridor-north";
    static constexpr const char* CorridorSouthWallId = "barrier-wall-corridor-south";
    static constexpr const char* NorthWallId = MainRoomNorthWallId;
    static constexpr const char* SouthWallId = MainRoomSouthWallId;
    static constexpr const char* CorridorExitWallUpperId = "barrier-wall-corridor-exit-upper";
    static constexpr const char* CorridorExitWallLowerId = "barrier-wall-corridor-exit-lower";
    static constexpr const char* MainSideWallLowerId = "barrier-wall-main-side-lower";
    static constexpr const char* MainSideWallUpperId = "barrier-wall-main-side-upper";
    static constexpr const char* SideCorridorWallLowerId = "barrier-wall-side-corridor-lower";
    static constexpr const char* SideCorridorWallUpperId = "barrier-wall-side-corridor-upper";
};

FacilityLayout2D demoFacility();

}  // namespace safecrowd::domain::DemoLayouts

