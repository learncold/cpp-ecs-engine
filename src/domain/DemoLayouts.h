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

struct TwoFloorEvacuationIds {
    static constexpr const char* LayoutId = "demo-two-floor-evacuation";
    static constexpr const char* LowerFloorId = "L1";
    static constexpr const char* UpperFloorId = "L2";
    static constexpr const char* UpperWestTrainingZoneId = "two-floor-upper-west-training";
    static constexpr const char* UpperBriefingZoneId = "two-floor-upper-briefing";
    static constexpr const char* UpperEastTrainingZoneId = "two-floor-upper-east-training";
    static constexpr const char* UpperCorridorZoneId = "two-floor-upper-corridor";
    static constexpr const char* UpperWestStairZoneId = "two-floor-upper-west-stair";
    static constexpr const char* UpperEastStairZoneId = "two-floor-upper-east-stair";
    static constexpr const char* LowerWestStairZoneId = "two-floor-lower-west-stair";
    static constexpr const char* LowerEastStairZoneId = "two-floor-lower-east-stair";
    static constexpr const char* LowerWestVestibuleZoneId = "two-floor-lower-west-vestibule";
    static constexpr const char* LowerLobbyZoneId = "two-floor-lower-lobby";
    static constexpr const char* LowerEastVestibuleZoneId = "two-floor-lower-east-vestibule";
    static constexpr const char* WestExitZoneId = "two-floor-west-exit";
    static constexpr const char* EastExitZoneId = "two-floor-east-exit";
    static constexpr const char* UpperWestTrainingToCorridorConnectionId = "two-floor-upper-west-training-corridor";
    static constexpr const char* UpperBriefingToCorridorConnectionId = "two-floor-upper-briefing-corridor";
    static constexpr const char* UpperEastTrainingToCorridorConnectionId = "two-floor-upper-east-training-corridor";
    static constexpr const char* UpperCorridorWestStairConnectionId = "two-floor-upper-corridor-west-stair";
    static constexpr const char* UpperCorridorEastStairConnectionId = "two-floor-upper-corridor-east-stair";
    static constexpr const char* WestStairVerticalConnectionId = "two-floor-west-stair-vertical";
    static constexpr const char* EastStairVerticalConnectionId = "two-floor-east-stair-vertical";
    static constexpr const char* LowerWestStairVestibuleConnectionId = "two-floor-lower-west-stair-vestibule";
    static constexpr const char* LowerEastStairVestibuleConnectionId = "two-floor-lower-east-stair-vestibule";
    static constexpr const char* LowerWestVestibuleLobbyConnectionId = "two-floor-lower-west-vestibule-lobby";
    static constexpr const char* LowerLobbyEastVestibuleConnectionId = "two-floor-lower-lobby-east-vestibule";
    static constexpr const char* LowerWestExitConnectionId = "two-floor-lower-west-exit";
    static constexpr const char* LowerEastExitConnectionId = "two-floor-lower-east-exit";
};

FacilityLayout2D demoFacility();
FacilityLayout2D twoFloorEvacuationFacility();

}  // namespace safecrowd::domain::DemoLayouts
