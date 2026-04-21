#pragma once

#include "domain/FacilityLayout2D.h"

namespace safecrowd::domain::DemoLayouts {

struct Sprint1FacilityIds {
    static constexpr const char* LayoutId = "demo-fixture-01";
    static constexpr const char* MainRoomZoneId = "zone-room-1";
    static constexpr const char* SideRoomZoneId = "zone-room-2";
    static constexpr const char* ExitZoneId = "zone-exit-1";
    static constexpr const char* OpeningConnectionId = "conn-opening-1";
    static constexpr const char* ExitConnectionId = "conn-exit-1";
    static constexpr const char* BarrierId = "barrier-1";
};

FacilityLayout2D demoFacility();

}  // namespace safecrowd::domain::DemoLayouts

