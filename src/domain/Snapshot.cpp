#include "domain/snapshot.h"
#include "domain/AgentComponents.h"
#include "domain/Metrics.h"
#include "engine/ComponentRegistry.h"

namespace safecrowd::domain {

    // 인자 타입을 safecrowd::engine::ComponentRegistry로 명확히 지정 (E0276 해결)
    SimulationSnapshot buildSnapshot(const safecrowd::engine::ComponentRegistry& registry, uint64_t frame, float time) {
        SimulationSnapshot snapshot;
        snapshot.frameIndex = frame;
        snapshot.simulationTime = time;

        // 우리 엔진의 저장소(Storage)에서 데이터를 로드
        auto& posStorage = registry.storageFor<Position>();
        auto& compStorage = registry.storageFor<CompressionData>();

        snapshot.agentCount = static_cast<uint32_t>(posStorage.size());
        snapshot.agents.reserve(snapshot.agentCount);

        // Position을 가진 모든 엔티티를 순회하며 스냅샷 생성
        for (const auto& entity : posStorage.getEntities()) {
            // 해당 엔티티에 압박 지표 데이터도 있는지 확인
            if (!compStorage.contains(entity)) continue;

            const auto& pos = posStorage.get(entity);
            const auto& metrics = compStorage.get(entity);

            // AgentSnapshot 구조체에 맞춰 데이터 삽입
            snapshot.agents.push_back({
                static_cast<uint32_t>(entity.index), // id
                pos.value,                            // position (Point2D)
                metrics                               // metrics (CompressionData)
                });
        }

        return snapshot;
    }

} // namespace safecrowd::domain
