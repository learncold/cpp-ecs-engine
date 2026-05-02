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

struct TwoFloorFacilityIds {
    static constexpr const char* LayoutId = "demo-fixture-02";
    static constexpr const char* Floor1Id = "L1";
    static constexpr const char* Floor2Id = "L2";

    static constexpr const char* HallZoneL1Id = "zone-l1-hall";
    static constexpr const char* HallZoneL2Id = "zone-l2-hall";
    static constexpr const char* CornerRoomL1Id = "zone-l1-corner-room";
    static constexpr const char* CornerRoomL2Id = "zone-l2-corner-room";
    static constexpr const char* CornerRoomRightL1Id = "zone-l1-corner-room-right";
    static constexpr const char* CornerRoomRightL2Id = "zone-l2-corner-room-right";

    static constexpr const char* TopRoomL1Prefix = "zone-l1-top-room-";
    static constexpr const char* BottomRoomL1Prefix = "zone-l1-bottom-room-";
    static constexpr const char* TopRoomL2Prefix = "zone-l2-top-room-";
    static constexpr const char* BottomRoomL2Prefix = "zone-l2-bottom-room-";

    static constexpr const char* LeftStairZoneL1Id = "zone-l1-stairs-left";
    static constexpr const char* RightStairZoneL1Id = "zone-l1-stairs-right";
    static constexpr const char* LeftStairZoneL2Id = "zone-l2-stairs-left";
    static constexpr const char* RightStairZoneL2Id = "zone-l2-stairs-right";
    static constexpr const char* ExitZoneL1Id = "zone-l1-exit";

    static constexpr const char* CornerDoorL1Id = "conn-l1-corner-door";
    static constexpr const char* CornerDoorL2Id = "conn-l2-corner-door";
    static constexpr const char* CornerRightDoorL1Id = "conn-l1-corner-right-door";
    static constexpr const char* CornerRightDoorL2Id = "conn-l2-corner-right-door";

    static constexpr const char* TopDoorL1Prefix = "conn-l1-top-door-";
    static constexpr const char* BottomDoorL1Prefix = "conn-l1-bottom-door-";
    static constexpr const char* TopDoorL2Prefix = "conn-l2-top-door-";
    static constexpr const char* BottomDoorL2Prefix = "conn-l2-bottom-door-";

    static constexpr const char* LeftStairDoorL1Id = "conn-l1-stairs-left-door";
    static constexpr const char* RightStairDoorL1Id = "conn-l1-stairs-right-door";
    static constexpr const char* LeftStairDoorL2Id = "conn-l2-stairs-left-door";
    static constexpr const char* RightStairDoorL2Id = "conn-l2-stairs-right-door";
    static constexpr const char* ExitDoorL1Id = "conn-l1-exit";
    static constexpr const char* LeftStairLinkId = "conn-stairs-left-l1-l2";
    static constexpr const char* RightStairLinkId = "conn-stairs-right-l1-l2";

    static constexpr const char* OuterWallL1Id = "barrier-l1-outline";
    static constexpr const char* OuterWallL2Id = "barrier-l2-outline";
};

FacilityLayout2D demoTwoFloorFacility();

}  // namespace safecrowd::domain::DemoLayouts

