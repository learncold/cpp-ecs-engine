#include "domain/DemoFixtureService.h"

#include "domain/DemoLayouts.h"

namespace safecrowd::domain {

DemoFixture DemoFixtureService::createSprint1DemoFixture() const {
    DemoFixture fixture;
    fixture.layout = DemoLayouts::demoFacility();

    fixture.population.initialPlacements.push_back({
        .id = "placement-1",
        .zoneId = DemoLayouts::Sprint1FacilityIds::MainRoomZoneId,
        .area = {
            .outline = {
                {1.0, 1.0},
                {4.0, 1.0},
                {4.0, 4.0},
                {1.0, 4.0},
            },
        },
        .targetAgentCount = 100,
    });

    fixture.baselineScenario.scenarioId = "scenario-sprint1-baseline";
    fixture.baselineScenario.name = "Baseline evacuation";
    fixture.baselineScenario.role = ScenarioRole::Baseline;
    fixture.baselineScenario.population = fixture.population;
    fixture.baselineScenario.execution.timeLimitSeconds = 180.0;
    fixture.baselineScenario.execution.sampleIntervalSeconds = 1.0;
    fixture.baselineScenario.execution.repeatCount = 1;
    fixture.baselineScenario.execution.baseSeed = 1;
    fixture.baselineScenario.sourceTemplateId = "sprint1-baseline";
    fixture.baselineScenario.control.events.push_back({
        .id = "event-exit-closure",
        .name = "Exit Closure",
        .triggerSummary = "Operator command",
        .targetSummary = "Primary exit route",
    });
    fixture.baselineScenario.control.events.push_back({
        .id = "event-staged-release",
        .name = "Staged Release",
        .triggerSummary = "After initial evacuation wave",
        .targetSummary = "Main Demo Room occupants",
    });

    return fixture;
}

}  // namespace safecrowd::domain
