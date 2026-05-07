#include "domain/DemoFixtureService.h"

#include <utility>

#include "domain/DemoLayouts.h"

namespace safecrowd::domain {
namespace {

ScenarioDraft makeSprint1BlockedDoorAlternative(const ScenarioDraft& baseline) {
    auto alternative = duplicateScenarioDraft(
        baseline,
        "scenario-2",
        "Doorway blocked alternative");
    alternative.control.connectionBlocks.push_back({
        .id = "block-1",
        .connectionId = DemoLayouts::Sprint1FacilityIds::DoorwayConnectionId,
        .intervals = {{0.0, 60.0}},
    });
    alternative.variationDiffKeys = computeScenarioDiffKeys(baseline, alternative);
    return alternative;
}

DensityCellMetric densityCell(
    double centerX,
    double centerY,
    double minX,
    double minY,
    double maxX,
    double maxY,
    std::size_t agentCount,
    double densityPeoplePerSquareMeter) {
    return {
        .center = {centerX, centerY},
        .cellMin = {minX, minY},
        .cellMax = {maxX, maxY},
        .floorId = DemoLayouts::Sprint1FacilityIds::FloorId,
        .agentCount = agentCount,
        .densityPeoplePerSquareMeter = densityPeoplePerSquareMeter,
    };
}

EvacuationProgressSample progressSample(
    double timeSeconds,
    std::size_t evacuatedCount,
    double evacuatedRatio) {
    return {
        .timeSeconds = timeSeconds,
        .evacuatedCount = evacuatedCount,
        .totalCount = 100,
        .evacuatedRatio = evacuatedRatio,
    };
}

std::vector<SimulationFrame> makeBlockedDoorReplayFrames() {
    const auto startFrame = SimulationFrame{
        .elapsedSeconds = 0,
        .complete = false,
        .totalAgentCount = 100,
        .evacuatedAgentCount = 0,
        .agents = {
            {.id = 0, .position = {1.15, 1.15}, .velocity = {0, 0}, .radius = 0.25, .floorId = "L1", .stalled = true},
            {.id = 1, .position = {1.45, 1.15}, .velocity = {0, 0}, .radius = 0.25, .floorId = "L1", .stalled = true},
            {.id = 2, .position = {1.75, 1.15}, .velocity = {0, 0}, .radius = 0.25, .floorId = "L1", .stalled = true},
            {.id = 3, .position = {2.05, 1.15}, .velocity = {0, 0}, .radius = 0.25, .floorId = "L1", .stalled = true},
            {.id = 4, .position = {2.35, 1.15}, .velocity = {0, 0}, .radius = 0.25, .floorId = "L1", .stalled = true},
            {.id = 5, .position = {2.65, 1.15}, .velocity = {0, 0}, .radius = 0.25, .floorId = "L1", .stalled = true},
            {.id = 6, .position = {2.95, 1.15}, .velocity = {0, 0}, .radius = 0.25, .floorId = "L1", .stalled = true},
            {.id = 7, .position = {3.25, 1.15}, .velocity = {0, 0}, .radius = 0.25, .floorId = "L1", .stalled = true},
            {.id = 8, .position = {3.55, 1.15}, .velocity = {0, 0}, .radius = 0.25, .floorId = "L1", .stalled = true},
            {.id = 9, .position = {3.85, 1.15}, .velocity = {0, 0}, .radius = 0.25, .floorId = "L1", .stalled = true},
            {.id = 10, .position = {1.15, 1.45}, .velocity = {0, 0}, .radius = 0.25, .floorId = "L1", .stalled = true},
            {.id = 11, .position = {1.45, 1.45}, .velocity = {0, 0}, .radius = 0.25, .floorId = "L1", .stalled = true},
            {.id = 12, .position = {1.75, 1.45}, .velocity = {0, 0}, .radius = 0.25, .floorId = "L1", .stalled = true},
            {.id = 13, .position = {2.05, 1.45}, .velocity = {0, 0}, .radius = 0.25, .floorId = "L1", .stalled = true},
            {.id = 14, .position = {2.35, 1.45}, .velocity = {0, 0}, .radius = 0.25, .floorId = "L1", .stalled = true},
            {.id = 15, .position = {2.65, 1.45}, .velocity = {0, 0}, .radius = 0.25, .floorId = "L1", .stalled = true},
            {.id = 16, .position = {2.95, 1.45}, .velocity = {0, 0}, .radius = 0.25, .floorId = "L1", .stalled = true},
            {.id = 17, .position = {3.25, 1.45}, .velocity = {0, 0}, .radius = 0.25, .floorId = "L1", .stalled = true},
            {.id = 18, .position = {3.55, 1.45}, .velocity = {0, 0}, .radius = 0.25, .floorId = "L1", .stalled = true},
            {.id = 19, .position = {3.85, 1.45}, .velocity = {0, 0}, .radius = 0.25, .floorId = "L1", .stalled = true},
            {.id = 20, .position = {1.15, 1.75}, .velocity = {0, 0}, .radius = 0.25, .floorId = "L1", .stalled = true},
            {.id = 21, .position = {1.45, 1.75}, .velocity = {0, 0}, .radius = 0.25, .floorId = "L1", .stalled = true},
            {.id = 22, .position = {1.75, 1.75}, .velocity = {0, 0}, .radius = 0.25, .floorId = "L1", .stalled = true},
            {.id = 23, .position = {2.05, 1.75}, .velocity = {0, 0}, .radius = 0.25, .floorId = "L1", .stalled = true},
            {.id = 24, .position = {2.35, 1.75}, .velocity = {0, 0}, .radius = 0.25, .floorId = "L1", .stalled = true},
            {.id = 25, .position = {2.65, 1.75}, .velocity = {0, 0}, .radius = 0.25, .floorId = "L1", .stalled = true},
            {.id = 26, .position = {2.95, 1.75}, .velocity = {0, 0}, .radius = 0.25, .floorId = "L1", .stalled = true},
            {.id = 27, .position = {3.25, 1.75}, .velocity = {0, 0}, .radius = 0.25, .floorId = "L1", .stalled = true},
            {.id = 28, .position = {3.55, 1.75}, .velocity = {0, 0}, .radius = 0.25, .floorId = "L1", .stalled = true},
            {.id = 29, .position = {3.85, 1.75}, .velocity = {0, 0}, .radius = 0.25, .floorId = "L1", .stalled = true},
            {.id = 30, .position = {1.15, 2.05}, .velocity = {0, 0}, .radius = 0.25, .floorId = "L1", .stalled = true},
            {.id = 31, .position = {1.45, 2.05}, .velocity = {0, 0}, .radius = 0.25, .floorId = "L1", .stalled = true},
            {.id = 32, .position = {1.75, 2.05}, .velocity = {0, 0}, .radius = 0.25, .floorId = "L1", .stalled = true},
            {.id = 33, .position = {2.05, 2.05}, .velocity = {0, 0}, .radius = 0.25, .floorId = "L1", .stalled = true},
            {.id = 34, .position = {2.35, 2.05}, .velocity = {0, 0}, .radius = 0.25, .floorId = "L1", .stalled = true},
            {.id = 35, .position = {2.65, 2.05}, .velocity = {0, 0}, .radius = 0.25, .floorId = "L1", .stalled = true},
            {.id = 36, .position = {2.95, 2.05}, .velocity = {0, 0}, .radius = 0.25, .floorId = "L1", .stalled = true},
            {.id = 37, .position = {3.25, 2.05}, .velocity = {0, 0}, .radius = 0.25, .floorId = "L1", .stalled = true},
            {.id = 38, .position = {3.55, 2.05}, .velocity = {0, 0}, .radius = 0.25, .floorId = "L1", .stalled = true},
            {.id = 39, .position = {3.85, 2.05}, .velocity = {0, 0}, .radius = 0.25, .floorId = "L1", .stalled = true},
            {.id = 40, .position = {1.15, 2.35}, .velocity = {0, 0}, .radius = 0.25, .floorId = "L1", .stalled = true},
            {.id = 41, .position = {1.45, 2.35}, .velocity = {0, 0}, .radius = 0.25, .floorId = "L1", .stalled = true},
            {.id = 42, .position = {1.75, 2.35}, .velocity = {0, 0}, .radius = 0.25, .floorId = "L1", .stalled = true},
            {.id = 43, .position = {2.05, 2.35}, .velocity = {0, 0}, .radius = 0.25, .floorId = "L1", .stalled = true},
            {.id = 44, .position = {2.35, 2.35}, .velocity = {0, 0}, .radius = 0.25, .floorId = "L1", .stalled = true},
            {.id = 45, .position = {2.65, 2.35}, .velocity = {0, 0}, .radius = 0.25, .floorId = "L1", .stalled = true},
            {.id = 46, .position = {2.95, 2.35}, .velocity = {0, 0}, .radius = 0.25, .floorId = "L1", .stalled = true},
            {.id = 47, .position = {3.25, 2.35}, .velocity = {0, 0}, .radius = 0.25, .floorId = "L1", .stalled = true},
            {.id = 48, .position = {3.55, 2.35}, .velocity = {0, 0}, .radius = 0.25, .floorId = "L1", .stalled = true},
            {.id = 49, .position = {3.85, 2.35}, .velocity = {0, 0}, .radius = 0.25, .floorId = "L1", .stalled = true},
            {.id = 50, .position = {1.15, 2.65}, .velocity = {0, 0}, .radius = 0.25, .floorId = "L1", .stalled = true},
            {.id = 51, .position = {1.45, 2.65}, .velocity = {0, 0}, .radius = 0.25, .floorId = "L1", .stalled = true},
            {.id = 52, .position = {1.75, 2.65}, .velocity = {0, 0}, .radius = 0.25, .floorId = "L1", .stalled = true},
            {.id = 53, .position = {2.05, 2.65}, .velocity = {0, 0}, .radius = 0.25, .floorId = "L1", .stalled = true},
            {.id = 54, .position = {2.35, 2.65}, .velocity = {0, 0}, .radius = 0.25, .floorId = "L1", .stalled = true},
            {.id = 55, .position = {2.65, 2.65}, .velocity = {0, 0}, .radius = 0.25, .floorId = "L1", .stalled = true},
            {.id = 56, .position = {2.95, 2.65}, .velocity = {0, 0}, .radius = 0.25, .floorId = "L1", .stalled = true},
            {.id = 57, .position = {3.25, 2.65}, .velocity = {0, 0}, .radius = 0.25, .floorId = "L1", .stalled = true},
            {.id = 58, .position = {3.55, 2.65}, .velocity = {0, 0}, .radius = 0.25, .floorId = "L1", .stalled = true},
            {.id = 59, .position = {3.85, 2.65}, .velocity = {0, 0}, .radius = 0.25, .floorId = "L1", .stalled = true},
            {.id = 60, .position = {1.15, 2.95}, .velocity = {0, 0}, .radius = 0.25, .floorId = "L1", .stalled = true},
            {.id = 61, .position = {1.45, 2.95}, .velocity = {0, 0}, .radius = 0.25, .floorId = "L1", .stalled = true},
            {.id = 62, .position = {1.75, 2.95}, .velocity = {0, 0}, .radius = 0.25, .floorId = "L1", .stalled = true},
            {.id = 63, .position = {2.05, 2.95}, .velocity = {0, 0}, .radius = 0.25, .floorId = "L1", .stalled = true},
            {.id = 64, .position = {2.35, 2.95}, .velocity = {0, 0}, .radius = 0.25, .floorId = "L1", .stalled = true},
            {.id = 65, .position = {2.65, 2.95}, .velocity = {0, 0}, .radius = 0.25, .floorId = "L1", .stalled = true},
            {.id = 66, .position = {2.95, 2.95}, .velocity = {0, 0}, .radius = 0.25, .floorId = "L1", .stalled = true},
            {.id = 67, .position = {3.25, 2.95}, .velocity = {0, 0}, .radius = 0.25, .floorId = "L1", .stalled = true},
            {.id = 68, .position = {3.55, 2.95}, .velocity = {0, 0}, .radius = 0.25, .floorId = "L1", .stalled = true},
            {.id = 69, .position = {3.85, 2.95}, .velocity = {0, 0}, .radius = 0.25, .floorId = "L1", .stalled = true},
            {.id = 70, .position = {1.15, 3.25}, .velocity = {0, 0}, .radius = 0.25, .floorId = "L1", .stalled = true},
            {.id = 71, .position = {1.45, 3.25}, .velocity = {0, 0}, .radius = 0.25, .floorId = "L1", .stalled = true},
            {.id = 72, .position = {1.75, 3.25}, .velocity = {0, 0}, .radius = 0.25, .floorId = "L1", .stalled = true},
            {.id = 73, .position = {2.05, 3.25}, .velocity = {0, 0}, .radius = 0.25, .floorId = "L1", .stalled = true},
            {.id = 74, .position = {2.35, 3.25}, .velocity = {0, 0}, .radius = 0.25, .floorId = "L1", .stalled = true},
            {.id = 75, .position = {2.65, 3.25}, .velocity = {0, 0}, .radius = 0.25, .floorId = "L1", .stalled = true},
            {.id = 76, .position = {2.95, 3.25}, .velocity = {0, 0}, .radius = 0.25, .floorId = "L1", .stalled = true},
            {.id = 77, .position = {3.25, 3.25}, .velocity = {0, 0}, .radius = 0.25, .floorId = "L1", .stalled = true},
            {.id = 78, .position = {3.55, 3.25}, .velocity = {0, 0}, .radius = 0.25, .floorId = "L1", .stalled = true},
            {.id = 79, .position = {3.85, 3.25}, .velocity = {0, 0}, .radius = 0.25, .floorId = "L1", .stalled = true},
            {.id = 80, .position = {1.15, 3.55}, .velocity = {0, 0}, .radius = 0.25, .floorId = "L1", .stalled = true},
            {.id = 81, .position = {1.45, 3.55}, .velocity = {0, 0}, .radius = 0.25, .floorId = "L1", .stalled = true},
            {.id = 82, .position = {1.75, 3.55}, .velocity = {0, 0}, .radius = 0.25, .floorId = "L1", .stalled = true},
            {.id = 83, .position = {2.05, 3.55}, .velocity = {0, 0}, .radius = 0.25, .floorId = "L1", .stalled = true},
            {.id = 84, .position = {2.35, 3.55}, .velocity = {0, 0}, .radius = 0.25, .floorId = "L1", .stalled = true},
            {.id = 85, .position = {2.65, 3.55}, .velocity = {0, 0}, .radius = 0.25, .floorId = "L1", .stalled = true},
            {.id = 86, .position = {2.95, 3.55}, .velocity = {0, 0}, .radius = 0.25, .floorId = "L1", .stalled = true},
            {.id = 87, .position = {3.25, 3.55}, .velocity = {0, 0}, .radius = 0.25, .floorId = "L1", .stalled = true},
            {.id = 88, .position = {3.55, 3.55}, .velocity = {0, 0}, .radius = 0.25, .floorId = "L1", .stalled = true},
            {.id = 89, .position = {3.85, 3.55}, .velocity = {0, 0}, .radius = 0.25, .floorId = "L1", .stalled = true},
            {.id = 90, .position = {1.15, 3.85}, .velocity = {0, 0}, .radius = 0.25, .floorId = "L1", .stalled = true},
            {.id = 91, .position = {1.45, 3.85}, .velocity = {0, 0}, .radius = 0.25, .floorId = "L1", .stalled = true},
            {.id = 92, .position = {1.75, 3.85}, .velocity = {0, 0}, .radius = 0.25, .floorId = "L1", .stalled = true},
            {.id = 93, .position = {2.05, 3.85}, .velocity = {0, 0}, .radius = 0.25, .floorId = "L1", .stalled = true},
            {.id = 94, .position = {2.35, 3.85}, .velocity = {0, 0}, .radius = 0.25, .floorId = "L1", .stalled = true},
            {.id = 95, .position = {2.65, 3.85}, .velocity = {0, 0}, .radius = 0.25, .floorId = "L1", .stalled = true},
            {.id = 96, .position = {2.95, 3.85}, .velocity = {0, 0}, .radius = 0.25, .floorId = "L1", .stalled = true},
            {.id = 97, .position = {3.25, 3.85}, .velocity = {0, 0}, .radius = 0.25, .floorId = "L1", .stalled = true},
            {.id = 98, .position = {3.55, 3.85}, .velocity = {0, 0}, .radius = 0.25, .floorId = "L1", .stalled = true},
            {.id = 99, .position = {3.85, 3.85}, .velocity = {0, 0}, .radius = 0.25, .floorId = "L1", .stalled = true},
        },
    };

    const auto bottleneckFrame = SimulationFrame{
        .elapsedSeconds = 19,
        .complete = false,
        .totalAgentCount = 100,
        .evacuatedAgentCount = 0,
        .agents = {
            {.id = 0, .position = {11.35025886, 3.742546785}, .velocity = {1.08183834, 0.3945518787}, .radius = 0.25, .floorId = "L1", .stalled = false},
            {.id = 1, .position = {11.28479507, 2.744369197}, .velocity = {0.3510316241, 1.10917109}, .radius = 0.25, .floorId = "L1", .stalled = true},
            {.id = 2, .position = {11.31638053, 3.236381718}, .velocity = {0.7645669912, 0.8730304571}, .radius = 0.25, .floorId = "L1", .stalled = false},
            {.id = 3, .position = {12.70267813, 3.003714869}, .velocity = {0, 0}, .radius = 0.25, .floorId = "L1", .stalled = true},
            {.id = 4, .position = {11.74833994, 2.9758753}, .velocity = {-0.5910812625, 1.096562723}, .radius = 0.25, .floorId = "L1", .stalled = false},
            {.id = 5, .position = {12.32011306, 4.13119078}, .velocity = {0, 0}, .radius = 0.25, .floorId = "L1", .stalled = true},
            {.id = 6, .position = {11.70964609, 2.4825143}, .velocity = {-0.6799568041, 1.107997572}, .radius = 0.25, .floorId = "L1", .stalled = false},
            {.id = 7, .position = {11.74730305, 3.469825717}, .velocity = {-0.5945781552, 0.9308547873}, .radius = 0.25, .floorId = "L1", .stalled = false},
            {.id = 8, .position = {13.3698194, 3.105415349}, .velocity = {0, 0}, .radius = 0.25, .floorId = "L1", .stalled = true},
            {.id = 9, .position = {13.13999272, 2.661442427}, .velocity = {0, 0}, .radius = 0.25, .floorId = "L1", .stalled = true},
            {.id = 10, .position = {12.75525791, 4.372735587}, .velocity = {0, 0}, .radius = 0.25, .floorId = "L1", .stalled = true},
            {.id = 11, .position = {11.85731084, 3.952009843}, .velocity = {0, 0}, .radius = 0.25, .floorId = "L1", .stalled = true},
            {.id = 12, .position = {13.6354278, 4.840043773}, .velocity = {0, 0}, .radius = 0.25, .floorId = "L1", .stalled = true},
            {.id = 13, .position = {14.09872171, 5.025129178}, .velocity = {0, 0}, .radius = 0.25, .floorId = "L1", .stalled = true},
            {.id = 14, .position = {13.91006162, 3.934423993}, .velocity = {0, 0}, .radius = 0.25, .floorId = "L1", .stalled = true},
            {.id = 15, .position = {13.82268831, 3.349405021}, .velocity = {0, 0}, .radius = 0.25, .floorId = "L1", .stalled = true},
            {.id = 16, .position = {13.02069254, 3.46293626}, .velocity = {0, 0}, .radius = 0.25, .floorId = "L1", .stalled = true},
            {.id = 17, .position = {12.25000064, 2.768187387}, .velocity = {0, 0}, .radius = 0.25, .floorId = "L1", .stalled = true},
            {.id = 18, .position = {12.66861481, 2.494774594}, .velocity = {0, 0}, .radius = 0.25, .floorId = "L1", .stalled = true},
            {.id = 19, .position = {13.79944338, 2.85004843}, .velocity = {0, 0}, .radius = 0.25, .floorId = "L1", .stalled = true},
            {.id = 20, .position = {12.58440602, 3.707010355}, .velocity = {0, 0}, .radius = 0.25, .floorId = "L1", .stalled = true},
            {.id = 21, .position = {13.48557103, 4.197889529}, .velocity = {0, 0}, .radius = 0.25, .floorId = "L1", .stalled = true},
            {.id = 22, .position = {12.29792314, 3.297266036}, .velocity = {0, 0}, .radius = 0.25, .floorId = "L1", .stalled = true},
            {.id = 23, .position = {13.92619022, 4.434067152}, .velocity = {0, 0}, .radius = 0.25, .floorId = "L1", .stalled = true},
            {.id = 24, .position = {14.4250927, 4.465089184}, .velocity = {0, 0}, .radius = 0.25, .floorId = "L1", .stalled = true},
            {.id = 25, .position = {14.59738178, 4.994171861}, .velocity = {0, 0}, .radius = 0.25, .floorId = "L1", .stalled = true},
            {.id = 26, .position = {14.26485485, 3.582824027}, .velocity = {0, 0}, .radius = 0.25, .floorId = "L1", .stalled = true},
            {.id = 27, .position = {14.86033607, 5.419321678}, .velocity = {0, 0}, .radius = 0.25, .floorId = "L1", .stalled = true},
            {.id = 28, .position = {14.76024068, 3.515434658}, .velocity = {0, 0}, .radius = 0.25, .floorId = "L1", .stalled = true},
            {.id = 29, .position = {13.5697702, 2.405927838}, .velocity = {0, 0}, .radius = 0.25, .floorId = "L1", .stalled = true},
            {.id = 30, .position = {12.35835059, 5.149634103}, .velocity = {0, 0}, .radius = 0.25, .floorId = "L1", .stalled = true},
            {.id = 31, .position = {13.19600087, 4.604775185}, .velocity = {0, 0}, .radius = 0.25, .floorId = "L1", .stalled = true},
            {.id = 32, .position = {13.46528114, 3.698607551}, .velocity = {0, 0}, .radius = 0.25, .floorId = "L1", .stalled = true},
            {.id = 33, .position = {13.25036514, 6.115835059}, .velocity = {0, 0}, .radius = 0.25, .floorId = "L1", .stalled = true},
            {.id = 34, .position = {13.65982601, 5.342965258}, .velocity = {0, 0}, .radius = 0.25, .floorId = "L1", .stalled = true},
            {.id = 35, .position = {14.11900326, 6.073559735}, .velocity = {0, 0}, .radius = 0.25, .floorId = "L1", .stalled = true},
            {.id = 36, .position = {14.58808032, 3.992405187}, .velocity = {0, 0}, .radius = 0.25, .floorId = "L1", .stalled = true},
            {.id = 37, .position = {14.90789408, 4.594834202}, .velocity = {0, 0}, .radius = 0.25, .floorId = "L1", .stalled = true},
            {.id = 38, .position = {14.24594883, 3.08326905}, .velocity = {0, 0}, .radius = 0.25, .floorId = "L1", .stalled = true},
            {.id = 39, .position = {12.30921869, 2.147169497}, .velocity = {0, 0}, .radius = 0.25, .floorId = "L1", .stalled = true},
            {.id = 40, .position = {10.86608414, 3.020205148}, .velocity = {0.8610335095, 0.7963412118}, .radius = 0.25, .floorId = "L1", .stalled = true},
            {.id = 41, .position = {13.04072756, 3.962526431}, .velocity = {0, 0}, .radius = 0.25, .floorId = "L1", .stalled = true},
            {.id = 42, .position = {13.2195701, 5.115078348}, .velocity = {0, 0}, .radius = 0.25, .floorId = "L1", .stalled = true},
            {.id = 43, .position = {14.12419776, 5.524338453}, .velocity = {0, 0}, .radius = 0.25, .floorId = "L1", .stalled = true},
            {.id = 44, .position = {13.23425809, 5.616772624}, .velocity = {0, 0}, .radius = 0.25, .floorId = "L1", .stalled = true},
            {.id = 45, .position = {13.69402498, 6.343663431}, .velocity = {0, 0}, .radius = 0.25, .floorId = "L1", .stalled = true},
            {.id = 46, .position = {14.13783098, 6.573158975}, .velocity = {0, 0}, .radius = 0.25, .floorId = "L1", .stalled = true},
            {.id = 47, .position = {15.0204828, 5.93535408}, .velocity = {0, 0}, .radius = 0.25, .floorId = "L1", .stalled = true},
            {.id = 48, .position = {14.5619451, 6.304681455}, .velocity = {0, 0}, .radius = 0.25, .floorId = "L1", .stalled = true},
            {.id = 49, .position = {15.07095147, 4.122199488}, .velocity = {0, 0}, .radius = 0.25, .floorId = "L1", .stalled = true},
            {.id = 50, .position = {12.33961424, 4.651000004}, .velocity = {0, 0}, .radius = 0.25, .floorId = "L1", .stalled = true},
            {.id = 51, .position = {11.91291322, 4.940102568}, .velocity = {0, 0}, .radius = 0.25, .floorId = "L1", .stalled = true},
            {.id = 52, .position = {12.7967149, 5.38202994}, .velocity = {0, 0}, .radius = 0.25, .floorId = "L1", .stalled = true},
            {.id = 53, .position = {12.38482751, 6.147091327}, .velocity = {0, 0}, .radius = 0.25, .floorId = "L1", .stalled = true},
            {.id = 54, .position = {12.8121429, 5.883777486}, .velocity = {0, 0}, .radius = 0.25, .floorId = "L1", .stalled = true},
            {.id = 55, .position = {13.26035381, 6.616876642}, .velocity = {0, 0}, .radius = 0.25, .floorId = "L1", .stalled = true},
            {.id = 56, .position = {12.79761294, 6.888527743}, .velocity = {0, 0}, .radius = 0.25, .floorId = "L1", .stalled = true},
            {.id = 57, .position = {12.25365054, 7.128091308}, .velocity = {0, 0}, .radius = 0.25, .floorId = "L1", .stalled = true},
            {.id = 58, .position = {15.16552781, 5.023319369}, .velocity = {0, 0}, .radius = 0.25, .floorId = "L1", .stalled = true},
            {.id = 59, .position = {14.22267007, 2.583840183}, .velocity = {0, 0}, .radius = 0.25, .floorId = "L1", .stalled = true},
            {.id = 60, .position = {10.93112752, 4.00757823}, .velocity = {0, 0}, .radius = 0.25, .floorId = "L1", .stalled = true},
            {.id = 61, .position = {12.37555486, 5.64920483}, .velocity = {0, 0}, .radius = 0.25, .floorId = "L1", .stalled = true},
            {.id = 62, .position = {12.78084688, 4.88234464}, .velocity = {0, 0}, .radius = 0.25, .floorId = "L1", .stalled = true},
            {.id = 63, .position = {11.77130244, 6.397929273}, .velocity = {0, 0}, .radius = 0.25, .floorId = "L1", .stalled = true},
            {.id = 64, .position = {10.13024802, 5.791811896}, .velocity = {0.1484035248, -1.15973597}, .radius = 0.25, .floorId = "L1", .stalled = true},
            {.id = 65, .position = {13.6787546, 5.842955184}, .velocity = {0, 0}, .radius = 0.25, .floorId = "L1", .stalled = true},
            {.id = 66, .position = {11.03495927, 7.145360725}, .velocity = {0, 0}, .radius = 0.25, .floorId = "L1", .stalled = true},
            {.id = 67, .position = {14.53891082, 5.802018097}, .velocity = {0, 0}, .radius = 0.25, .floorId = "L1", .stalled = true},
            {.id = 68, .position = {13.42434664, 7.271339795}, .velocity = {0, 0}, .radius = 0.25, .floorId = "L1", .stalled = true},
            {.id = 69, .position = {14.14182072, 7.084791273}, .velocity = {0, 0}, .radius = 0.25, .floorId = "L1", .stalled = true},
            {.id = 70, .position = {10.91051601, 3.513460205}, .velocity = {1.124047401, 0.310703058}, .radius = 0.25, .floorId = "L1", .stalled = true},
            {.id = 71, .position = {11.92785083, 5.435329431}, .velocity = {0, 0}, .radius = 0.25, .floorId = "L1", .stalled = true},
            {.id = 72, .position = {11.4930401, 5.719715762}, .velocity = {0, 0}, .radius = 0.25, .floorId = "L1", .stalled = true},
            {.id = 73, .position = {11.01454638, 6.648353034}, .velocity = {0, 0}, .radius = 0.25, .floorId = "L1", .stalled = true},
            {.id = 74, .position = {11.48676603, 6.805625206}, .velocity = {0, 0}, .radius = 0.25, .floorId = "L1", .stalled = true},
            {.id = 75, .position = {9.927711329, 6.409162707}, .velocity = {0, 0}, .radius = 0.25, .floorId = "L1", .stalled = true},
            {.id = 76, .position = {13.69986742, 6.85200329}, .velocity = {0, 0}, .radius = 0.25, .floorId = "L1", .stalled = true},
            {.id = 77, .position = {12.93546497, 7.376178004}, .velocity = {0, 0}, .radius = 0.25, .floorId = "L1", .stalled = true},
            {.id = 78, .position = {11.75, 7.728055782}, .velocity = {0, 0}, .radius = 0.25, .floorId = "L1", .stalled = true},
            {.id = 79, .position = {12.47688647, 7.575435749}, .velocity = {0, 0}, .radius = 0.25, .floorId = "L1", .stalled = true},
            {.id = 80, .position = {11.46645278, 4.723975199}, .velocity = {0, 0}, .radius = 0.25, .floorId = "L1", .stalled = true},
            {.id = 81, .position = {11.88874407, 4.446505794}, .velocity = {0, 0}, .radius = 0.25, .floorId = "L1", .stalled = true},
            {.id = 82, .position = {11.31963553, 6.18783107}, .velocity = {0, 0}, .radius = 0.25, .floorId = "L1", .stalled = true},
            {.id = 83, .position = {11.94114305, 5.932918801}, .velocity = {0, 0}, .radius = 0.25, .floorId = "L1", .stalled = true},
            {.id = 84, .position = {12.25000051, 6.628401856}, .velocity = {0, 0}, .radius = 0.25, .floorId = "L1", .stalled = true},
            {.id = 85, .position = {12.81852156, 6.388455924}, .velocity = {0, 0}, .radius = 0.25, .floorId = "L1", .stalled = true},
            {.id = 86, .position = {10.61552348, 5.24354826}, .velocity = {1.158385979, 0.01410759789}, .radius = 0.25, .floorId = "L1", .stalled = true},
            {.id = 87, .position = {11.02238418, 4.492240538}, .velocity = {0, 0}, .radius = 0.25, .floorId = "L1", .stalled = true},
            {.id = 88, .position = {11.74999583, 7.228512799}, .velocity = {0, 0}, .radius = 0.25, .floorId = "L1", .stalled = true},
            {.id = 89, .position = {10.54113621, 6.805012917}, .velocity = {0, 0}, .radius = 0.25, .floorId = "L1", .stalled = true},
            {.id = 90, .position = {11.47984129, 5.223123942}, .velocity = {0, 0}, .radius = 0.25, .floorId = "L1", .stalled = true},
            {.id = 91, .position = {11.05433596, 5.485242593}, .velocity = {0, 0}, .radius = 0.25, .floorId = "L1", .stalled = true},
            {.id = 92, .position = {10.82152163, 6.194014563}, .velocity = {0, 0}, .radius = 0.25, .floorId = "L1", .stalled = true},
            {.id = 93, .position = {10.62409657, 5.73778121}, .velocity = {0, 0}, .radius = 0.25, .floorId = "L1", .stalled = true},
            {.id = 94, .position = {11.44258455, 4.226667269}, .velocity = {0, 0}, .radius = 0.25, .floorId = "L1", .stalled = true},
            {.id = 95, .position = {10.48463777, 3.769534892}, .velocity = {1.172437168, 0.01874217448}, .radius = 0.25, .floorId = "L1", .stalled = true},
            {.id = 96, .position = {10.50327056, 4.264977411}, .velocity = {1.167533715, 0.09956946714}, .radius = 0.25, .floorId = "L1", .stalled = true},
            {.id = 97, .position = {10.59687548, 4.753686672}, .velocity = {1.158385979, -0.009532943609}, .radius = 0.25, .floorId = "L1", .stalled = true},
            {.id = 98, .position = {11.03983458, 4.987081213}, .velocity = {0, 0}, .radius = 0.25, .floorId = "L1", .stalled = true},
            {.id = 99, .position = {9.718396415, 5.511010769}, .velocity = {0, 0}, .radius = 0.25, .floorId = "L1", .stalled = true},
        },
    };

    const auto doorReleaseFrame = SimulationFrame{
        .elapsedSeconds = 60,
        .complete = false,
        .totalAgentCount = 100,
        .evacuatedAgentCount = 0,
        .agents = {
            {.id = 0, .position = {11.89209176, 5.000828227}, .velocity = {0, 0}, .radius = 0.25, .floorId = "L1", .stalled = true},
            {.id = 1, .position = {11.54916482, 4.636958587}, .velocity = {0, 0}, .radius = 0.25, .floorId = "L1", .stalled = true},
            {.id = 2, .position = {12.08760482, 4.45330384}, .velocity = {0, 0}, .radius = 0.25, .floorId = "L1", .stalled = true},
            {.id = 3, .position = {12.69789833, 2.803293639}, .velocity = {0, 0}, .radius = 0.25, .floorId = "L1", .stalled = true},
            {.id = 4, .position = {12.32937293, 4.005284187}, .velocity = {0, 0}, .radius = 0.25, .floorId = "L1", .stalled = true},
            {.id = 5, .position = {12.78849755, 3.807209093}, .velocity = {0, 0}, .radius = 0.25, .floorId = "L1", .stalled = true},
            {.id = 6, .position = {11.889097, 3.768309281}, .velocity = {0, 0}, .radius = 0.25, .floorId = "L1", .stalled = true},
            {.id = 7, .position = {12.38969943, 3.505613244}, .velocity = {0, 0}, .radius = 0.25, .floorId = "L1", .stalled = true},
            {.id = 8, .position = {13.52227492, 2.863085614}, .velocity = {0, 0}, .radius = 0.25, .floorId = "L1", .stalled = true},
            {.id = 9, .position = {13.13044579, 2.552489534}, .velocity = {0, 0}, .radius = 0.25, .floorId = "L1", .stalled = true},
            {.id = 10, .position = {13.21961401, 4.321202239}, .velocity = {0, 0}, .radius = 0.25, .floorId = "L1", .stalled = true},
            {.id = 11, .position = {12.71962518, 4.317859257}, .velocity = {0, 0}, .radius = 0.25, .floorId = "L1", .stalled = true},
            {.id = 12, .position = {14.00745397, 4.937072344}, .velocity = {0, 0}, .radius = 0.25, .floorId = "L1", .stalled = true},
            {.id = 13, .position = {14.42192746, 5.216735929}, .velocity = {0, 0}, .radius = 0.25, .floorId = "L1", .stalled = true},
            {.id = 14, .position = {14.14518681, 3.941044193}, .velocity = {0, 0}, .radius = 0.25, .floorId = "L1", .stalled = true},
            {.id = 15, .position = {13.93928471, 3.171779103}, .velocity = {0, 0}, .radius = 0.25, .floorId = "L1", .stalled = true},
            {.id = 16, .position = {13.33979056, 3.328595502}, .velocity = {0, 0}, .radius = 0.25, .floorId = "L1", .stalled = true},
            {.id = 17, .position = {12.25, 2.525525684}, .velocity = {0, 0}, .radius = 0.25, .floorId = "L1", .stalled = true},
            {.id = 18, .position = {12.69746817, 2.302428772}, .velocity = {0, 0}, .radius = 0.25, .floorId = "L1", .stalled = true},
            {.id = 19, .position = {13.98509223, 2.673876378}, .velocity = {0, 0}, .radius = 0.25, .floorId = "L1", .stalled = true},
            {.id = 20, .position = {12.84195794, 3.282090911}, .velocity = {0, 0}, .radius = 0.25, .floorId = "L1", .stalled = true},
            {.id = 21, .position = {13.68329495, 4.132501509}, .velocity = {0, 0}, .radius = 0.25, .floorId = "L1", .stalled = true},
            {.id = 22, .position = {12.25, 3.025525682}, .velocity = {0, 0}, .radius = 0.25, .floorId = "L1", .stalled = true},
            {.id = 23, .position = {14.07801369, 4.439993864}, .velocity = {0, 0}, .radius = 0.25, .floorId = "L1", .stalled = true},
            {.id = 24, .position = {14.57568599, 4.488183878}, .velocity = {0, 0}, .radius = 0.25, .floorId = "L1", .stalled = true},
            {.id = 25, .position = {14.83966669, 4.941974339}, .velocity = {0, 0}, .radius = 0.25, .floorId = "L1", .stalled = true},
            {.id = 26, .position = {14.34121501, 3.48044674}, .velocity = {0, 0}, .radius = 0.25, .floorId = "L1", .stalled = true},
            {.id = 27, .position = {14.9923345, 5.418096739}, .velocity = {0, 0}, .radius = 0.25, .floorId = "L1", .stalled = true},
            {.id = 28, .position = {14.84033526, 3.510094328}, .velocity = {0, 0}, .radius = 0.25, .floorId = "L1", .stalled = true},
            {.id = 29, .position = {13.5929206, 2.362438949}, .velocity = {0, 0}, .radius = 0.25, .floorId = "L1", .stalled = true},
            {.id = 30, .position = {12.70052269, 5.266294297}, .velocity = {0, 0}, .radius = 0.25, .floorId = "L1", .stalled = true},
            {.id = 31, .position = {13.61460958, 4.627761358}, .velocity = {0, 0}, .radius = 0.25, .floorId = "L1", .stalled = true},
            {.id = 32, .position = {13.75009778, 3.634605531}, .velocity = {0, 0}, .radius = 0.25, .floorId = "L1", .stalled = true},
            {.id = 33, .position = {13.48476917, 6.303349452}, .velocity = {0, 0}, .radius = 0.25, .floorId = "L1", .stalled = true},
            {.id = 34, .position = {14.11926403, 5.61472443}, .velocity = {0, 0}, .radius = 0.25, .floorId = "L1", .stalled = true},
            {.id = 35, .position = {14.42857892, 6.235558498}, .velocity = {0, 0}, .radius = 0.25, .floorId = "L1", .stalled = true},
            {.id = 36, .position = {14.64251444, 3.992670048}, .velocity = {0, 0}, .radius = 0.25, .floorId = "L1", .stalled = true},
            {.id = 37, .position = {15.07607769, 4.501395354}, .velocity = {0, 0}, .radius = 0.25, .floorId = "L1", .stalled = true},
            {.id = 38, .position = {14.4025433, 2.983652859}, .velocity = {0, 0}, .radius = 0.25, .floorId = "L1", .stalled = true},
            {.id = 39, .position = {12.25676549, 2.025571223}, .velocity = {0, 0}, .radius = 0.25, .floorId = "L1", .stalled = true},
            {.id = 40, .position = {11.51219173, 4.138327473}, .velocity = {0, 0}, .radius = 0.25, .floorId = "L1", .stalled = true},
            {.id = 41, .position = {13.28814728, 3.825921319}, .velocity = {0, 0}, .radius = 0.25, .floorId = "L1", .stalled = true},
            {.id = 42, .position = {13.70479054, 5.335060845}, .velocity = {0, 0}, .radius = 0.25, .floorId = "L1", .stalled = true},
            {.id = 43, .position = {14.60481567, 5.734054259}, .velocity = {0, 0}, .radius = 0.25, .floorId = "L1", .stalled = true},
            {.id = 44, .position = {13.53707366, 5.806092739}, .velocity = {0, 0}, .radius = 0.25, .floorId = "L1", .stalled = true},
            {.id = 45, .position = {13.89930838, 6.583019945}, .velocity = {0, 0}, .radius = 0.25, .floorId = "L1", .stalled = true},
            {.id = 46, .position = {14.36814095, 6.756789966}, .velocity = {0, 0}, .radius = 0.25, .floorId = "L1", .stalled = true},
            {.id = 47, .position = {15.43414607, 5.957657148}, .velocity = {0, 0}, .radius = 0.25, .floorId = "L1", .stalled = true},
            {.id = 48, .position = {14.83550235, 6.57910113}, .velocity = {0, 0}, .radius = 0.25, .floorId = "L1", .stalled = true},
            {.id = 49, .position = {15.14234233, 4.005805811}, .velocity = {0, 0}, .radius = 0.25, .floorId = "L1", .stalled = true},
            {.id = 50, .position = {12.86823957, 4.795262399}, .velocity = {0, 0}, .radius = 0.25, .floorId = "L1", .stalled = true},
            {.id = 51, .position = {12.21436106, 5.383114109}, .velocity = {0, 0}, .radius = 0.25, .floorId = "L1", .stalled = true},
            {.id = 52, .position = {13.14422927, 5.496781754}, .velocity = {0, 0}, .radius = 0.25, .floorId = "L1", .stalled = true},
            {.id = 53, .position = {12.64012374, 6.256932936}, .velocity = {0, 0}, .radius = 0.25, .floorId = "L1", .stalled = true},
            {.id = 54, .position = {13.06304945, 5.990223654}, .velocity = {0, 0}, .radius = 0.25, .floorId = "L1", .stalled = true},
            {.id = 55, .position = {13.46232222, 6.826007794}, .velocity = {0, 0}, .radius = 0.25, .floorId = "L1", .stalled = true},
            {.id = 56, .position = {12.80365983, 6.982729615}, .velocity = {0, 0}, .radius = 0.25, .floorId = "L1", .stalled = true},
            {.id = 57, .position = {12.25, 7.226840603}, .velocity = {0, 0}, .radius = 0.25, .floorId = "L1", .stalled = true},
            {.id = 58, .position = {15.33044544, 5.037557938}, .velocity = {0, 0}, .radius = 0.25, .floorId = "L1", .stalled = true},
            {.id = 59, .position = {14.43519977, 2.456153601}, .velocity = {0, 0}, .radius = 0.25, .floorId = "L1", .stalled = true},
            {.id = 60, .position = {10.55346098, 4.632304572}, .velocity = {0, 0}, .radius = 0.25, .floorId = "L1", .stalled = true},
            {.id = 61, .position = {12.53572395, 5.767953711}, .velocity = {0, 0}, .radius = 0.25, .floorId = "L1", .stalled = true},
            {.id = 62, .position = {13.31194615, 5.02574986}, .velocity = {0, 0}, .radius = 0.25, .floorId = "L1", .stalled = true},
            {.id = 63, .position = {11.75, 6.837272604}, .velocity = {0, 0}, .radius = 0.25, .floorId = "L1", .stalled = true},
            {.id = 64, .position = {11.40336701, 3.649707547}, .velocity = {0, 0}, .radius = 0.25, .floorId = "L1", .stalled = true},
            {.id = 65, .position = {13.95154715, 6.085756324}, .velocity = {0, 0}, .radius = 0.25, .floorId = "L1", .stalled = true},
            {.id = 66, .position = {10.92884749, 7.388489945}, .velocity = {0, 0}, .radius = 0.25, .floorId = "L1", .stalled = true},
            {.id = 67, .position = {14.95282954, 6.0930617}, .velocity = {0, 0}, .radius = 0.25, .floorId = "L1", .stalled = true},
            {.id = 68, .position = {13.44130857, 7.325566024}, .velocity = {0, 0}, .radius = 0.25, .floorId = "L1", .stalled = true},
            {.id = 69, .position = {14.37156636, 7.256778231}, .velocity = {0, 0}, .radius = 0.25, .floorId = "L1", .stalled = true},
            {.id = 70, .position = {11.05152903, 4.588393027}, .velocity = {0, 0}, .radius = 0.25, .floorId = "L1", .stalled = true},
            {.id = 71, .position = {11.97536689, 5.822297202}, .velocity = {0, 0}, .radius = 0.25, .floorId = "L1", .stalled = true},
            {.id = 72, .position = {11.64448611, 6.348532572}, .velocity = {0, 0}, .radius = 0.25, .floorId = "L1", .stalled = true},
            {.id = 73, .position = {10.82227434, 6.898254887}, .velocity = {0, 0}, .radius = 0.25, .floorId = "L1", .stalled = true},
            {.id = 74, .position = {11.29821413, 7.051491014}, .velocity = {0, 0}, .radius = 0.25, .floorId = "L1", .stalled = true},
            {.id = 75, .position = {9.914026916, 6.423925323}, .velocity = {0, 0}, .radius = 0.25, .floorId = "L1", .stalled = true},
            {.id = 76, .position = {13.90273379, 7.083008209}, .velocity = {0, 0}, .radius = 0.25, .floorId = "L1", .stalled = true},
            {.id = 77, .position = {12.95914921, 7.457938045}, .velocity = {0, 0}, .radius = 0.25, .floorId = "L1", .stalled = true},
            {.id = 78, .position = {11.75, 7.8372726}, .velocity = {0, 0}, .radius = 0.25, .floorId = "L1", .stalled = true},
            {.id = 79, .position = {12.50138011, 7.659053557}, .velocity = {0, 0}, .radius = 0.25, .floorId = "L1", .stalled = true},
            {.id = 80, .position = {11.57332216, 5.386037187}, .velocity = {0, 0}, .radius = 0.25, .floorId = "L1", .stalled = true},
            {.id = 81, .position = {12.37296937, 4.863873046}, .velocity = {0, 0}, .radius = 0.25, .floorId = "L1", .stalled = true},
            {.id = 82, .position = {11.19237357, 6.56206066}, .velocity = {0, 0}, .radius = 0.25, .floorId = "L1", .stalled = true},
            {.id = 83, .position = {12.14149166, 6.293892962}, .velocity = {0, 0}, .radius = 0.25, .floorId = "L1", .stalled = true},
            {.id = 84, .position = {12.36623184, 6.740538066}, .velocity = {0, 0}, .radius = 0.25, .floorId = "L1", .stalled = true},
            {.id = 85, .position = {13.04781481, 6.546394486}, .velocity = {0, 0}, .radius = 0.25, .floorId = "L1", .stalled = true},
            {.id = 86, .position = {10.60376519, 5.700764739}, .velocity = {0, 0}, .radius = 0.25, .floorId = "L1", .stalled = true},
            {.id = 87, .position = {10.68748419, 5.114007569}, .velocity = {0, 0}, .radius = 0.25, .floorId = "L1", .stalled = true},
            {.id = 88, .position = {11.75, 7.337272602}, .velocity = {0, 0}, .radius = 0.25, .floorId = "L1", .stalled = true},
            {.id = 89, .position = {10.32762282, 6.825317395}, .velocity = {0, 0}, .radius = 0.25, .floorId = "L1", .stalled = true},
            {.id = 90, .position = {11.47836135, 5.876936812}, .velocity = {0, 0}, .radius = 0.25, .floorId = "L1", .stalled = true},
            {.id = 91, .position = {11.02596759, 6.089868444}, .velocity = {0, 0}, .radius = 0.25, .floorId = "L1", .stalled = true},
            {.id = 92, .position = {10.61993747, 6.381655256}, .velocity = {0, 0}, .radius = 0.25, .floorId = "L1", .stalled = true},
            {.id = 93, .position = {10.24585124, 6.049903106}, .velocity = {0, 0}, .radius = 0.25, .floorId = "L1", .stalled = true},
            {.id = 94, .position = {11.18557539, 5.070359442}, .velocity = {0, 0}, .radius = 0.25, .floorId = "L1", .stalled = true},
            {.id = 95, .position = {10.91690813, 4.106856722}, .velocity = {0, 0}, .radius = 0.25, .floorId = "L1", .stalled = true},
            {.id = 96, .position = {10.06290986, 4.535559563}, .velocity = {0, 0}, .radius = 0.25, .floorId = "L1", .stalled = true},
            {.id = 97, .position = {10.18991402, 5.064774238}, .velocity = {0, 0}, .radius = 0.25, .floorId = "L1", .stalled = true},
            {.id = 98, .position = {11.08566233, 5.567441288}, .velocity = {0, 0}, .radius = 0.25, .floorId = "L1", .stalled = true},
            {.id = 99, .position = {9.661911257, 5.4861031}, .velocity = {0, 0}, .radius = 0.25, .floorId = "L1", .stalled = true},
        },
    };

    const auto firstEvacueesFrame = SimulationFrame{
        .elapsedSeconds = 67,
        .complete = false,
        .totalAgentCount = 100,
        .evacuatedAgentCount = 2,
        .agents = {
            {.id = 0, .position = {18.80121342, 6.045188352}, .velocity = {1.160369857, 0.0235530389}, .radius = 0.25, .floorId = "L1", .stalled = false},
            {.id = 1, .position = {17.95975142, 5.318615395}, .velocity = {1.134008267, 0.1319480066}, .radius = 0.25, .floorId = "L1", .stalled = false},
            {.id = 2, .position = {18.40492183, 5.660844875}, .velocity = {1.143559402, 0.05563003006}, .radius = 0.25, .floorId = "L1", .stalled = false},
            {.id = 3, .position = {18.91417407, 4.581410821}, .velocity = {0.8363588039, 0.7001167803}, .radius = 0.25, .floorId = "L1", .stalled = false},
            {.id = 4, .position = {18.88732734, 5.560626}, .velocity = {1.158202201, -0.1495465668}, .radius = 0.25, .floorId = "L1", .stalled = false},
            {.id = 5, .position = {19.38924069, 5.419722015}, .velocity = {1.123943478, -0.03707930358}, .radius = 0.25, .floorId = "L1", .stalled = false},
            {.id = 6, .position = {18.41197315, 5.180774193}, .velocity = {1.106774197, 0.2406820048}, .radius = 0.25, .floorId = "L1", .stalled = false},
            {.id = 7, .position = {18.86635757, 5.070901515}, .velocity = {1.057593338, 0.3777404259}, .radius = 0.25, .floorId = "L1", .stalled = false},
            {.id = 8, .position = {20.61738464, 5.140990504}, .velocity = {1.184516195, -0.1420166509}, .radius = 0.25, .floorId = "L1", .stalled = false},
            {.id = 9, .position = {19.06730962, 4.076982618}, .velocity = {0.5670014887, 1.002595318}, .radius = 0.25, .floorId = "L1", .stalled = true},
            {.id = 10, .position = {21.36167692, 5.799653872}, .velocity = {1.171960761, -0.0002972573557}, .radius = 0.25, .floorId = "L1", .stalled = false},
            {.id = 11, .position = {19.76345377, 5.757446635}, .velocity = {1.183157482, 0.02555714912}, .radius = 0.25, .floorId = "L1", .stalled = false},
            {.id = 12, .position = {22.5214374, 5.944308998}, .velocity = {1.168284184, 0.00150955843}, .radius = 0.25, .floorId = "L1", .stalled = false},
            {.id = 13, .position = {23.24910374, 5.874903035}, .velocity = {1.169948414, 0.06260756215}, .radius = 0.25, .floorId = "L1", .stalled = false},
            {.id = 14, .position = {22.00951306, 5.038263395}, .velocity = {1.196295811, -0.008221538138}, .radius = 0.25, .floorId = "L1", .stalled = false},
            {.id = 15, .position = {21.34077294, 5.048018079}, .velocity = {1.190775831, -0.07849648892}, .radius = 0.25, .floorId = "L1", .stalled = false},
            {.id = 16, .position = {19.84282424, 5.244905884}, .velocity = {0.5600458823, 1.120563553}, .radius = 0.25, .floorId = "L1", .stalled = false},
            {.id = 17, .position = {18.59968862, 4.201192818}, .velocity = {0.8416662717, 0.7822433408}, .radius = 0.25, .floorId = "L1", .stalled = false},
            {.id = 18, .position = {18.72024088, 3.722959155}, .velocity = {0.6145550865, 0.9870853769}, .radius = 0.25, .floorId = "L1", .stalled = true},
            {.id = 19, .position = {19.47179848, 3.809835674}, .velocity = {0.03138863936, 1.144416685}, .radius = 0.25, .floorId = "L1", .stalled = true},
            {.id = 20, .position = {19.31519937, 4.956769346}, .velocity = {0.9022147042, 0.6682570783}, .radius = 0.25, .floorId = "L1", .stalled = false},
            {.id = 21, .position = {21.67945886, 5.414173123}, .velocity = {1.172463811, 0.005721815236}, .radius = 0.25, .floorId = "L1", .stalled = false},
            {.id = 22, .position = {18.48026669, 4.682062659}, .velocity = {0.910830669, 0.6173281532}, .radius = 0.25, .floorId = "L1", .stalled = false},
            {.id = 23, .position = {22.43297079, 5.302650538}, .velocity = {1.169770674, 0.08942008316}, .radius = 0.25, .floorId = "L1", .stalled = false},
            {.id = 24, .position = {22.85396124, 5.571503589}, .velocity = {1.171871251, 0.05187441565}, .radius = 0.25, .floorId = "L1", .stalled = false},
            {.id = 25, .position = {23.70758375, 5.685411449}, .velocity = {1.169948414, -0.1015547214}, .radius = 0.25, .floorId = "L1", .stalled = false},
            {.id = 26, .position = {19.74824632, 4.203148041}, .velocity = {-0.7488475427, 1.06265104}, .radius = 0.25, .floorId = "L1", .stalled = false},
            {.id = 27, .position = {23.69764124, 6.184008712}, .velocity = {1.173679296, 0.02601596082}, .radius = 0.25, .floorId = "L1", .stalled = false},
            {.id = 28, .position = {19.73986094, 4.758568467}, .velocity = {-0.5700662465, 0.9517315693}, .radius = 0.25, .floorId = "L1", .stalled = false},
            {.id = 29, .position = {19.13825599, 3.451514314}, .velocity = {0.2547661146, 1.129524563}, .radius = 0.25, .floorId = "L1", .stalled = true},
            {.id = 30, .position = {20.22819643, 5.990870664}, .velocity = {1.156741954, 0.09689351992}, .radius = 0.25, .floorId = "L1", .stalled = false},
            {.id = 31, .position = {22.10295961, 5.678323361}, .velocity = {1.168284184, -0.004427074422}, .radius = 0.25, .floorId = "L1", .stalled = false},
            {.id = 32, .position = {21.0247456, 5.431749156}, .velocity = {1.168958102, -0.0005678075839}, .radius = 0.25, .floorId = "L1", .stalled = false},
            {.id = 33, .position = {21.85186035, 6.685047328}, .velocity = {1.172581467, 0.01764457033}, .radius = 0.25, .floorId = "L1", .stalled = false},
            {.id = 34, .position = {22.91675138, 6.24528473}, .velocity = {1.16976406, 0.00159614706}, .radius = 0.25, .floorId = "L1", .stalled = false},
            {.id = 35, .position = {22.87177748, 7.052949917}, .velocity = {1.083450037, -0.4482325581}, .radius = 0.25, .floorId = "L1", .stalled = false},
            {.id = 36, .position = {22.85790065, 5.032723526}, .velocity = {1.204099008, 0.1110479857}, .radius = 0.25, .floorId = "L1", .stalled = false},
            {.id = 37, .position = {23.90194921, 5.229976793}, .velocity = {0.3450531976, 1.253370722}, .radius = 0.25, .floorId = "L1", .stalled = false},
            {.id = 38, .position = {19.35979469, 4.475478379}, .velocity = {0.4267904112, 1.034032104}, .radius = 0.25, .floorId = "L1", .stalled = false},
            {.id = 39, .position = {18.23908627, 3.85194517}, .velocity = {0.9339900567, 0.7114136397}, .radius = 0.25, .floorId = "L1", .stalled = false},
            {.id = 40, .position = {18.02529714, 4.827680262}, .velocity = {1.082478679, 0.3580440814}, .radius = 0.25, .floorId = "L1", .stalled = false},
            {.id = 41, .position = {20.27016777, 5.496413916}, .velocity = {1.166734083, -0.03334414109}, .radius = 0.25, .floorId = "L1", .stalled = false},
            {.id = 42, .position = {22.20519888, 6.331200316}, .velocity = {1.17200816, 0.001999511478}, .radius = 0.25, .floorId = "L1", .stalled = false},
            {.id = 43, .position = {23.33062004, 6.521956762}, .velocity = {1.171142923, 0.002809860533}, .radius = 0.25, .floorId = "L1", .stalled = false},
            {.id = 44, .position = {21.42485581, 6.430124846}, .velocity = {1.170135182, -0.02379635754}, .radius = 0.25, .floorId = "L1", .stalled = false},
            {.id = 45, .position = {22.25063152, 6.985803247}, .velocity = {1.196346079, -0.03393152793}, .radius = 0.25, .floorId = "L1", .stalled = false},
            {.id = 46, .position = {19.7499363, 7.22297302}, .velocity = {-0.595250156, -0.982602377}, .radius = 0.25, .floorId = "L1", .stalled = false},
            {.id = 48, .position = {23.37054833, 7.019825654}, .velocity = {1.144422625, -0.4019551018}, .radius = 0.25, .floorId = "L1", .stalled = false},
            {.id = 49, .position = {23.27809767, 5.306847456}, .velocity = {1.163840731, 0.1605080532}, .radius = 0.25, .floorId = "L1", .stalled = false},
            {.id = 50, .position = {20.6748587, 5.786221859}, .velocity = {1.156741954, -0.01238234403}, .radius = 0.25, .floorId = "L1", .stalled = false},
            {.id = 51, .position = {19.38310556, 6.328941041}, .velocity = {1.129513581, -0.2201012517}, .radius = 0.25, .floorId = "L1", .stalled = false},
            {.id = 52, .position = {21.01061302, 6.155132689}, .velocity = {1.170216672, -0.04600913753}, .radius = 0.25, .floorId = "L1", .stalled = false},
            {.id = 53, .position = {19.11889372, 6.739465075}, .velocity = {1.110405157, -0.3184863259}, .radius = 0.25, .floorId = "L1", .stalled = false},
            {.id = 54, .position = {20.55784552, 6.366204354}, .velocity = {1.173537677, 0.001385975774}, .radius = 0.25, .floorId = "L1", .stalled = false},
            {.id = 55, .position = {21.36616621, 6.947828153}, .velocity = {1.197529186, 0.08073639295}, .radius = 0.25, .floorId = "L1", .stalled = false},
            {.id = 56, .position = {19.62039406, 6.763296546}, .velocity = {1.121265037, -0.2923002174}, .radius = 0.25, .floorId = "L1", .stalled = false},
            {.id = 57, .position = {18.51218167, 7.496476799}, .velocity = {1.063650394, -0.5039730334}, .radius = 0.25, .floorId = "L1", .stalled = false},
            {.id = 59, .position = {19.74980312, 3.404233132}, .velocity = {-0.6297905766, 1.137261494}, .radius = 0.25, .floorId = "L1", .stalled = false},
            {.id = 60, .position = {17.20579372, 4.671770176}, .velocity = {1.143416033, 0.2047415077}, .radius = 0.25, .floorId = "L1", .stalled = false},
            {.id = 61, .position = {19.88007165, 6.337598997}, .velocity = {1.157199515, 0.01521808538}, .radius = 0.25, .floorId = "L1", .stalled = false},
            {.id = 62, .position = {21.78443329, 6.063848563}, .velocity = {1.171960761, 0.02269351975}, .radius = 0.25, .floorId = "L1", .stalled = false},
            {.id = 63, .position = {17.98054586, 7.547302439}, .velocity = {1.105524981, -0.3805692843}, .radius = 0.25, .floorId = "L1", .stalled = false},
            {.id = 64, .position = {18.12902477, 4.33538695}, .velocity = {1.038101552, 0.5235975903}, .radius = 0.25, .floorId = "L1", .stalled = false},
            {.id = 65, .position = {22.60359613, 6.632385876}, .velocity = {1.170048926, -0.04881660935}, .radius = 0.25, .floorId = "L1", .stalled = false},
            {.id = 66, .position = {16.16813922, 5.751692919}, .velocity = {1.167509689, -0.1230266727}, .radius = 0.25, .floorId = "L1", .stalled = false},
            {.id = 67, .position = {23.80207162, 6.688005715}, .velocity = {0.7642752272, -0.8708796336}, .radius = 0.25, .floorId = "L1", .stalled = false},
            {.id = 68, .position = {19.25902735, 8.088676951}, .velocity = {0.1637425838, -1.159463737}, .radius = 0.25, .floorId = "L1", .stalled = true},
            {.id = 69, .position = {19.74878622, 8.010526762}, .velocity = {-0.8399108279, -0.9922447668}, .radius = 0.25, .floorId = "L1", .stalled = false},
            {.id = 70, .position = {17.57739555, 5.006652771}, .velocity = {1.136801164, 0.1660934067}, .radius = 0.25, .floorId = "L1", .stalled = false},
            {.id = 71, .position = {18.67987597, 6.52315505}, .velocity = {1.153506181, -0.1948306565}, .radius = 0.25, .floorId = "L1", .stalled = false},
            {.id = 72, .position = {18.21283164, 7.113140363}, .velocity = {1.097053401, -0.3972462541}, .radius = 0.25, .floorId = "L1", .stalled = false},
            {.id = 73, .position = {17.00859329, 6.246620353}, .velocity = {1.167481723, 0.05802127647}, .radius = 0.25, .floorId = "L1", .stalled = false},
            {.id = 74, .position = {17.49404294, 7.293756883}, .velocity = {1.144450494, -0.2406300896}, .radius = 0.25, .floorId = "L1", .stalled = false},
            {.id = 75, .position = {16.63335172, 5.92105115}, .velocity = {1.165192249, 0.126715306}, .radius = 0.25, .floorId = "L1", .stalled = false},
            {.id = 76, .position = {19.46108876, 7.618866253}, .velocity = {0.1362923203, -1.144007715}, .radius = 0.25, .floorId = "L1", .stalled = false},
            {.id = 77, .position = {20.20284757, 6.718218203}, .velocity = {1.295889579, -0.1032960548}, .radius = 0.25, .floorId = "L1", .stalled = false},
            {.id = 78, .position = {16.19726063, 6.336211088}, .velocity = {1.187102397, -0.1048334443}, .radius = 0.25, .floorId = "L1", .stalled = false},
            {.id = 79, .position = {18.96527602, 7.686331964}, .velocity = {0.71992051, -0.9211257948}, .radius = 0.25, .floorId = "L1", .stalled = true},
            {.id = 80, .position = {18.31179567, 6.142143004}, .velocity = {1.160968906, 0.0706364151}, .radius = 0.25, .floorId = "L1", .stalled = false},
            {.id = 81, .position = {19.2602138, 5.875075818}, .velocity = {1.129513581, 0.1057671484}, .radius = 0.25, .floorId = "L1", .stalled = false},
            {.id = 82, .position = {17.40341297, 6.545255821}, .velocity = {1.166467348, 0.03009124323}, .radius = 0.25, .floorId = "L1", .stalled = false},
            {.id = 83, .position = {18.70271079, 7.021441149}, .velocity = {1.140880629, -0.2742494047}, .radius = 0.25, .floorId = "L1", .stalled = false},
            {.id = 84, .position = {19.15784788, 7.226535028}, .velocity = {0.9224046743, -0.6944230108}, .radius = 0.25, .floorId = "L1", .stalled = false},
            {.id = 85, .position = {20.93150113, 6.698338185}, .velocity = {1.176347453, 0.02530897749}, .radius = 0.25, .floorId = "L1", .stalled = false},
            {.id = 86, .position = {17.43635027, 5.992219237}, .velocity = {1.169119198, 0.04358933034}, .radius = 0.25, .floorId = "L1", .stalled = false},
            {.id = 87, .position = {17.51983529, 5.499829548}, .velocity = {1.146373733, -0.008628153644}, .radius = 0.25, .floorId = "L1", .stalled = false},
            {.id = 88, .position = {16.62674713, 6.611529961}, .velocity = {1.174803178, 0.03446589858}, .radius = 0.25, .floorId = "L1", .stalled = false},
            {.id = 89, .position = {17.04341152, 6.887870291}, .velocity = {1.187309448, 0.1170077958}, .radius = 0.25, .floorId = "L1", .stalled = false},
            {.id = 90, .position = {18.19792799, 6.624758089}, .velocity = {1.1637927, 0.09974030128}, .radius = 0.25, .floorId = "L1", .stalled = false},
            {.id = 91, .position = {17.77028038, 6.881830419}, .velocity = {1.155275914, -0.1629576168}, .radius = 0.25, .floorId = "L1", .stalled = false},
            {.id = 92, .position = {17.05082566, 5.660600655}, .velocity = {1.165192249, 0.09256313138}, .radius = 0.25, .floorId = "L1", .stalled = false},
            {.id = 93, .position = {16.64703925, 5.338646163}, .velocity = {1.166112121, 0.1498833388}, .radius = 0.25, .floorId = "L1", .stalled = false},
            {.id = 94, .position = {17.93194956, 5.807278634}, .velocity = {1.158266846, 0.06455896513}, .radius = 0.25, .floorId = "L1", .stalled = false},
            {.id = 95, .position = {17.65725951, 4.482303956}, .velocity = {1.151583985, 0.2373121025}, .radius = 0.25, .floorId = "L1", .stalled = false},
            {.id = 96, .position = {16.9854221, 4.226330162}, .velocity = {1.131864742, 0.3160374513}, .radius = 0.25, .floorId = "L1", .stalled = false},
            {.id = 97, .position = {17.11233298, 5.163370413}, .velocity = {1.151105687, 0.1541944273}, .radius = 0.25, .floorId = "L1", .stalled = false},
            {.id = 98, .position = {17.83016048, 6.293767471}, .velocity = {1.166467348, 0.1010310375}, .radius = 0.25, .floorId = "L1", .stalled = false},
            {.id = 99, .position = {16.7403524, 4.827900961}, .velocity = {1.154133436, 0.1424934094}, .radius = 0.25, .floorId = "L1", .stalled = false},
        },
    };

    const auto t50Frame = SimulationFrame{
        .elapsedSeconds = 72,
        .complete = false,
        .totalAgentCount = 100,
        .evacuatedAgentCount = 51,
        .agents = {
            {.id = 1, .position = {22.63795116, 5.774396603}, .velocity = {1.164143757, 0.07324429703}, .radius = 0.25, .floorId = "L1", .stalled = false},
            {.id = 3, .position = {23.07103088, 4.879153253}, .velocity = {1.07822679, 0.477474339}, .radius = 0.25, .floorId = "L1", .stalled = false},
            {.id = 6, .position = {23.17054128, 5.756451625}, .velocity = {1.150738778, -0.2011098982}, .radius = 0.25, .floorId = "L1", .stalled = false},
            {.id = 9, .position = {21.95044822, 5.088821677}, .velocity = {1.191484915, -0.01036824171}, .radius = 0.25, .floorId = "L1", .stalled = false},
            {.id = 17, .position = {21.040459, 5.294592001}, .velocity = {1.17297514, -0.04773855283}, .radius = 0.25, .floorId = "L1", .stalled = false},
            {.id = 18, .position = {19.87031288, 5.28137841}, .velocity = {0.4766097836, 1.209480463}, .radius = 0.25, .floorId = "L1", .stalled = false},
            {.id = 19, .position = {20.55553269, 5.183226614}, .velocity = {1.185210585, -0.1638318345}, .radius = 0.25, .floorId = "L1", .stalled = false},
            {.id = 22, .position = {22.89007851, 5.344424844}, .velocity = {1.179796106, -0.06793335713}, .radius = 0.25, .floorId = "L1", .stalled = false},
            {.id = 26, .position = {22.57253514, 4.904447632}, .velocity = {1.120683008, 0.3446885332}, .radius = 0.25, .floorId = "L1", .stalled = false},
            {.id = 28, .position = {24.0000969, 5.292878915}, .velocity = {0.8095884521, 1.017136379}, .radius = 0.25, .floorId = "L1", .stalled = false},
            {.id = 29, .position = {19.2929861, 4.554472457}, .velocity = {0.4952746142, 1.060809275}, .radius = 0.25, .floorId = "L1", .stalled = false},
            {.id = 38, .position = {23.56985264, 4.90877886}, .velocity = {0.8797870194, 0.7995232428}, .radius = 0.25, .floorId = "L1", .stalled = false},
            {.id = 39, .position = {19.2292362, 5.048713041}, .velocity = {1.00897732, 0.5886299104}, .radius = 0.25, .floorId = "L1", .stalled = false},
            {.id = 40, .position = {22.37070426, 5.359683729}, .velocity = {1.160325546, 0.1192513638}, .radius = 0.25, .floorId = "L1", .stalled = false},
            {.id = 46, .position = {23.74759369, 7.178063405}, .velocity = {-0.5878158329, -0.9847574889}, .radius = 0.25, .floorId = "L1", .stalled = false},
            {.id = 57, .position = {20.97921533, 6.793055568}, .velocity = {1.199822172, 0.1156890178}, .radius = 0.25, .floorId = "L1", .stalled = false},
            {.id = 59, .position = {19.72497808, 4.806172341}, .velocity = {-0.5392577956, 1.014204439}, .radius = 0.25, .floorId = "L1", .stalled = false},
            {.id = 60, .position = {19.42521043, 5.505720068}, .velocity = {1.166561949, 0.01369926541}, .radius = 0.25, .floorId = "L1", .stalled = false},
            {.id = 63, .position = {19.51100413, 6.684498909}, .velocity = {1.131421496, -0.3172108527}, .radius = 0.25, .floorId = "L1", .stalled = false},
            {.id = 64, .position = {21.52144214, 5.416908929}, .velocity = {1.168339156, -0.06118340676}, .radius = 0.25, .floorId = "L1", .stalled = false},
            {.id = 66, .position = {20.30598574, 6.09625182}, .velocity = {1.193613966, -0.0206903575}, .radius = 0.25, .floorId = "L1", .stalled = false},
            {.id = 68, .position = {19.58542116, 7.178554116}, .velocity = {-0.04577569649, -0.9434645694}, .radius = 0.25, .floorId = "L1", .stalled = false},
            {.id = 69, .position = {20.06388777, 6.751965389}, .velocity = {1.209070496, -0.4776488382}, .radius = 0.25, .floorId = "L1", .stalled = false},
            {.id = 70, .position = {21.96158738, 5.64282606}, .velocity = {1.16502111, 0.02943007196}, .radius = 0.25, .floorId = "L1", .stalled = false},
            {.id = 72, .position = {21.36764047, 6.454014818}, .velocity = {1.180942161, -0.02261707635}, .radius = 0.25, .floorId = "L1", .stalled = false},
            {.id = 73, .position = {21.66137164, 6.043112718}, .velocity = {1.164409521, 0.012825373}, .radius = 0.25, .floorId = "L1", .stalled = false},
            {.id = 74, .position = {20.52612274, 6.562095662}, .velocity = {1.172989636, -0.006937173717}, .radius = 0.25, .floorId = "L1", .stalled = false},
            {.id = 75, .position = {19.83741685, 6.308734646}, .velocity = {1.164904257, 0.07422508487}, .radius = 0.25, .floorId = "L1", .stalled = false},
            {.id = 76, .position = {22.65332409, 7.079393676}, .velocity = {1.178822905, -0.1673072994}, .radius = 0.25, .floorId = "L1", .stalled = false},
            {.id = 78, .position = {19.02341389, 6.575979141}, .velocity = {1.169408042, -0.08931327993}, .radius = 0.25, .floorId = "L1", .stalled = false},
            {.id = 79, .position = {21.676796, 6.891810108}, .velocity = {1.176577662, -0.06436434927}, .radius = 0.25, .floorId = "L1", .stalled = false},
            {.id = 80, .position = {23.7040306, 5.889843718}, .velocity = {1.183339958, 0.02500515253}, .radius = 0.25, .floorId = "L1", .stalled = false},
            {.id = 82, .position = {22.48102664, 6.483215368}, .velocity = {1.152759854, -0.03756488905}, .radius = 0.25, .floorId = "L1", .stalled = false},
            {.id = 83, .position = {22.92819702, 6.668322881}, .velocity = {1.14941853, -0.0893121594}, .radius = 0.25, .floorId = "L1", .stalled = false},
            {.id = 84, .position = {23.82267166, 6.609052069}, .velocity = {1.164663812, -0.1643490791}, .radius = 0.25, .floorId = "L1", .stalled = false},
            {.id = 86, .position = {22.9203815, 6.17938688}, .velocity = {1.156621857, -0.06824032961}, .radius = 0.25, .floorId = "L1", .stalled = false},
            {.id = 87, .position = {22.22678414, 6.05786062}, .velocity = {1.16502111, 0.004118347295}, .radius = 0.25, .floorId = "L1", .stalled = false},
            {.id = 88, .position = {20.88701776, 6.216523461}, .velocity = {1.172668094, 0.03142027317}, .radius = 0.25, .floorId = "L1", .stalled = false},
            {.id = 89, .position = {21.93201985, 6.461831813}, .velocity = {1.170828678, 0.03205078178}, .radius = 0.25, .floorId = "L1", .stalled = false},
            {.id = 90, .position = {23.33312127, 6.947058848}, .velocity = {0.9758908025, -0.5800454252}, .radius = 0.25, .floorId = "L1", .stalled = false},
            {.id = 91, .position = {22.19106695, 6.88913831}, .velocity = {1.190726791, 0.008203280923}, .radius = 0.25, .floorId = "L1", .stalled = false},
            {.id = 92, .position = {21.22192315, 5.815663761}, .velocity = {1.164409521, 0.07717913339}, .radius = 0.25, .floorId = "L1", .stalled = false},
            {.id = 93, .position = {19.34916535, 6.166612557}, .velocity = {1.178945458, -0.1552024573}, .radius = 0.25, .floorId = "L1", .stalled = false},
            {.id = 94, .position = {23.51342045, 5.404581277}, .velocity = {1.156923386, 0.08671032944}, .radius = 0.25, .floorId = "L1", .stalled = false},
            {.id = 95, .position = {20.73274048, 5.741939963}, .velocity = {1.164875362, -0.1076185873}, .radius = 0.25, .floorId = "L1", .stalled = false},
            {.id = 96, .position = {20.26146764, 5.586560738}, .velocity = {1.167186325, -0.1003137055}, .radius = 0.25, .floorId = "L1", .stalled = false},
            {.id = 97, .position = {19.81640459, 5.811343309}, .velocity = {1.173081999, -0.0374945735}, .radius = 0.25, .floorId = "L1", .stalled = false},
            {.id = 98, .position = {23.42115639, 6.311083711}, .velocity = {1.174482023, 0.08730477874}, .radius = 0.25, .floorId = "L1", .stalled = false},
            {.id = 99, .position = {19.00409965, 5.773907062}, .velocity = {1.166161994, -0.1216109379}, .radius = 0.25, .floorId = "L1", .stalled = false},
        },
    };

    const auto t90Frame = SimulationFrame{
        .elapsedSeconds = 75.93333333,
        .complete = false,
        .totalAgentCount = 100,
        .evacuatedAgentCount = 90,
        .agents = {
            {.id = 29, .position = {22.60507372, 5.264398896}, .velocity = {1.179995422, 0.01914347738}, .radius = 0.25, .floorId = "L1", .stalled = false},
            {.id = 39, .position = {23.06439174, 5.461745569}, .velocity = {1.174370966, 0.1165447805}, .radius = 0.25, .floorId = "L1", .stalled = false},
            {.id = 46, .position = {23.58093292, 6.965946243}, .velocity = {-0.02750324124, -0.8360083413}, .radius = 0.25, .floorId = "L1", .stalled = false},
            {.id = 59, .position = {23.48329019, 5.188793571}, .velocity = {1.144293425, 0.3160615671}, .radius = 0.25, .floorId = "L1", .stalled = false},
            {.id = 60, .position = {23.63190602, 5.666121439}, .velocity = {1.174119278, 0.0001272192605}, .radius = 0.25, .floorId = "L1", .stalled = false},
            {.id = 63, .position = {23.88974318, 6.572708362}, .velocity = {1.174295987, 0.1142215396}, .radius = 0.25, .floorId = "L1", .stalled = false},
            {.id = 68, .position = {23.06289372, 6.739982908}, .velocity = {1.176794456, -0.08964208989}, .radius = 0.25, .floorId = "L1", .stalled = false},
            {.id = 78, .position = {23.42277031, 6.392909256}, .velocity = {1.174418616, -0.1147143882}, .radius = 0.25, .floorId = "L1", .stalled = false},
            {.id = 93, .position = {23.92182317, 6.073458043}, .velocity = {1.1741896, 0.0001511215651}, .radius = 0.25, .floorId = "L1", .stalled = false},
            {.id = 99, .position = {23.21304319, 5.939062554}, .velocity = {1.174361056, 0.1167046035}, .radius = 0.25, .floorId = "L1", .stalled = false},
        },
    };

    const auto t95Frame = SimulationFrame{
        .elapsedSeconds = 76.43333333,
        .complete = false,
        .totalAgentCount = 100,
        .evacuatedAgentCount = 95,
        .agents = {
            {.id = 29, .position = {23.10865868, 5.251076571}, .velocity = {1.174933324, 0.1110953606}, .radius = 0.25, .floorId = "L1", .stalled = false},
            {.id = 39, .position = {23.53676721, 5.509323236}, .velocity = {1.174396681, 0.1166206631}, .radius = 0.25, .floorId = "L1", .stalled = false},
            {.id = 46, .position = {23.90580771, 6.698389384}, .velocity = {0.5771364544, -1.164866254}, .radius = 0.25, .floorId = "L1", .stalled = false},
            {.id = 68, .position = {23.42458827, 6.562647265}, .velocity = {1.174369284, -0.1176589448}, .radius = 0.25, .floorId = "L1", .stalled = false},
            {.id = 99, .position = {23.75330453, 5.95999758}, .velocity = {1.174458192, 0.1165744918}, .radius = 0.25, .floorId = "L1", .stalled = false},
        },
    };

    const auto finalFrame = SimulationFrame{
        .elapsedSeconds = 77.23333333,
        .complete = true,
        .totalAgentCount = 100,
        .evacuatedAgentCount = 100,
        .agents = {
        },
    };

    return {
        startFrame,
        bottleneckFrame,
        doorReleaseFrame,
        firstEvacueesFrame,
        t50Frame,
        t90Frame,
        t95Frame,
        finalFrame,
    };
}

ScenarioRiskSnapshot makeBlockedDoorRiskSnapshot(const SimulationFrame& hotspotDetectionFrame, const SimulationFrame& bottleneckDetectionFrame) {
    ScenarioRiskSnapshot risk;
    risk.completionRisk = ScenarioRiskLevel::High;
    risk.stalledAgentCount = 100;
    risk.hotspots = {
        {
            .center = {2.35, 2.35},
            .cellMin = {1.5, 1.5},
            .cellMax = {3.0, 3.0},
            .floorId = DemoLayouts::Sprint1FacilityIds::FloorId,
            .agentCount = 25,
            .detectedAtSeconds = 0.0,
            .detectionFrame = hotspotDetectionFrame,
        },
        {
            .center = {2.35, 3.55},
            .cellMin = {1.5, 3.0},
            .cellMax = {3.0, 4.5},
            .floorId = DemoLayouts::Sprint1FacilityIds::FloorId,
            .agentCount = 15,
            .detectedAtSeconds = 0.0,
            .detectionFrame = hotspotDetectionFrame,
        },
        {
            .center = {3.55, 2.35},
            .cellMin = {3.0, 1.5},
            .cellMax = {4.5, 3.0},
            .floorId = DemoLayouts::Sprint1FacilityIds::FloorId,
            .agentCount = 15,
            .detectedAtSeconds = 0.0,
            .detectionFrame = hotspotDetectionFrame,
        },
        {
            .center = {2.35, 1.3},
            .cellMin = {1.5, 0.0},
            .cellMax = {3.0, 1.5},
            .floorId = DemoLayouts::Sprint1FacilityIds::FloorId,
            .agentCount = 10,
            .detectedAtSeconds = 0.0,
            .detectionFrame = hotspotDetectionFrame,
        },
        {
            .center = {1.3, 2.35},
            .cellMin = {0.0, 1.5},
            .cellMax = {1.5, 3.0},
            .floorId = DemoLayouts::Sprint1FacilityIds::FloorId,
            .agentCount = 10,
            .detectedAtSeconds = 0.0,
            .detectionFrame = hotspotDetectionFrame,
        },
    };
    risk.bottlenecks = {{
        .connectionId = DemoLayouts::Sprint1FacilityIds::OpeningConnectionId,
        .label = "Main Demo Room -> Side Demo Room",
        .floorId = DemoLayouts::Sprint1FacilityIds::FloorId,
        .passage = {{12.0, 3.5}, {12.0, 6.5}},
        .nearbyAgentCount = 55,
        .stalledAgentCount = 50,
        .averageSpeed = 0.173202,
        .detectedAtSeconds = 18.8667,
        .detectionFrame = bottleneckDetectionFrame,
    }};
    return risk;
}

ScenarioResultArtifacts makeBlockedDoorResultArtifacts(std::vector<SimulationFrame> replayFrames) {
    const auto finalFrame = replayFrames.back();
    const auto t90Frame = replayFrames.at(5);
    const auto t95Frame = replayFrames.at(6);
    ScenarioResultArtifacts artifacts;
    artifacts.evacuationProgress = {
        progressSample(0.0, 0, 0.0),
        progressSample(30.0, 0, 0.0),
        progressSample(60.0, 0, 0.0),
        progressSample(66.9333, 2, 0.02),
        progressSample(67.8, 10, 0.10),
        progressSample(69.9667, 31, 0.31),
        progressSample(71.9333, 50, 0.50),
        progressSample(72.9667, 61, 0.61),
        progressSample(73.7667, 70, 0.70),
        progressSample(74.8333, 80, 0.80),
        progressSample(75.9667, 90, 0.90),
        progressSample(76.4667, 95, 0.95),
        progressSample(76.9667, 99, 0.99),
        progressSample(77.2333, 100, 1.0),
    };
    artifacts.replayFrames = std::move(replayFrames);
    artifacts.timingSummary.t50Seconds = 71.9;
    artifacts.timingSummary.t90Seconds = 75.93333333;
    artifacts.timingSummary.t95Seconds = 76.43333333;
    artifacts.timingSummary.finalEvacuationTimeSeconds = 77.2;
    artifacts.timingSummary.t90Frame = t90Frame;
    artifacts.timingSummary.t95Frame = t95Frame;
    artifacts.timingSummary.targetTimeSeconds = 600.0;
    artifacts.timingSummary.marginSeconds = 522.8;

    auto peakCells = std::vector<DensityCellMetric>{
        densityCell(2.35, 2.35, 1.5, 1.5, 3.0, 3.0, 25, 11.1111),
        densityCell(3.55, 2.35, 3.0, 1.5, 4.5, 3.0, 15, 6.66667),
        densityCell(2.35, 3.55, 1.5, 3.0, 3.0, 4.5, 15, 6.66667),
        densityCell(9.84153, 3.70549, 9.0, 3.0, 10.5, 4.5, 13, 5.77778),
        densityCell(9.75901, 2.14418, 9.0, 1.5, 10.5, 3.0, 13, 5.77778),
    };

    artifacts.densitySummary.cellSizeMeters = 1.5;
    artifacts.densitySummary.highDensityThresholdPeoplePerSquareMeter = 4.0;
    artifacts.densitySummary.peakDensityPeoplePerSquareMeter = 11.1111;
    artifacts.densitySummary.peakAgentCount = 25;
    artifacts.densitySummary.peakAtSeconds = 0.0;
    artifacts.densitySummary.peakCell = peakCells.front();
    artifacts.densitySummary.highDensityDurationSeconds = 71.9667;
    artifacts.densitySummary.peakField = {
        .timeSeconds = finalFrame.elapsedSeconds,
        .cellSizeMeters = 1.5,
        .cells = peakCells,
    };
    artifacts.densitySummary.peakCells = std::move(peakCells);

    artifacts.exitUsage.push_back({
        .exitZoneId = DemoLayouts::Sprint1FacilityIds::ExitZoneId,
        .exitLabel = "Main Exit",
        .floorId = DemoLayouts::Sprint1FacilityIds::FloorId,
        .evacuatedCount = 100,
        .usageRatio = 1.0,
        .lastExitTimeSeconds = 77.2,
    });
    artifacts.zoneCompletion.push_back({
        .zoneId = DemoLayouts::Sprint1FacilityIds::MainRoomZoneId,
        .zoneLabel = "Main Demo Room",
        .floorId = DemoLayouts::Sprint1FacilityIds::FloorId,
        .initialCount = 100,
        .evacuatedCount = 100,
        .lastCompletionTimeSeconds = 77.2,
    });
    artifacts.placementCompletion.push_back({
        .placementId = "placement-1",
        .zoneId = DemoLayouts::Sprint1FacilityIds::MainRoomZoneId,
        .floorId = DemoLayouts::Sprint1FacilityIds::FloorId,
        .initialCount = 100,
        .evacuatedCount = 100,
        .lastCompletionTimeSeconds = 77.2,
    });
    return artifacts;
}

}  // namespace

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

    fixture.baselineScenario.scenarioId = "scenario-1";
    fixture.baselineScenario.name = "Sprint 1 baseline";
    fixture.baselineScenario.role = ScenarioRole::Baseline;
    fixture.baselineScenario.population = fixture.population;
    fixture.baselineScenario.execution.timeLimitSeconds = 600.0;
    fixture.baselineScenario.execution.sampleIntervalSeconds = 1.0;
    fixture.baselineScenario.execution.repeatCount = 1;
    fixture.baselineScenario.execution.baseSeed = 1;
    fixture.baselineScenario.sourceTemplateId = "after-sprint-1-baseline";

    return fixture;
}

DemoScenarioResultFixture DemoFixtureService::createSprint1BlockedDoorResultFixture() const {
    const auto baselineFixture = createSprint1DemoFixture();

    DemoScenarioResultFixture fixture;
    fixture.layout = baselineFixture.layout;
    fixture.population = baselineFixture.population;
    fixture.baselineScenario = baselineFixture.baselineScenario;
    fixture.alternativeScenario = makeSprint1BlockedDoorAlternative(fixture.baselineScenario);
    auto replayFrames = makeBlockedDoorReplayFrames();
    fixture.frame = replayFrames.back();
    fixture.risk = makeBlockedDoorRiskSnapshot(replayFrames.front(), replayFrames.at(1));
    fixture.artifacts = makeBlockedDoorResultArtifacts(std::move(replayFrames));

    return fixture;
}

}  // namespace safecrowd::domain
