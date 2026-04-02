#pragma once

#include <cstddef>

#include "engine/ComponentRegistry.h"
#include "engine/Entity.h"
#include "engine/EntityRegistry.h"

namespace safecrowd::engine {

// EcsCore
//
// ECS 저장 코어. EntityRegistry와 ComponentRegistry를 하나로 묶어
// 외부에서 raw 레지스트리를 직접 다루지 않고도 엔티티/컴포넌트를 조작하게 한다.
//
// 책임:
//   - 엔티티 생성/소멸 (EntityRegistry 위임)
//   - 컴포넌트 추가/제거 및 entity Signature 자동 갱신 (ComponentRegistry 위임)
//   - 엔티티 소멸 시 cleanup flow 실행 (ComponentRegistry::notifyEntityDestroyed)
//
// 이 클래스는 domain 용어를 알지 않는다.
// "군중", "에이전트" 같은 개념은 domain 계층이 컴포넌트 타입으로 표현한다.
class EcsCore {
public:
    explicit EcsCore(std::size_t maxEntityCount = 4096)
        : entityRegistry_(maxEntityCount) {}

    // ----------------------------------------------------------------
    // 엔티티 생명주기
    // ----------------------------------------------------------------

    // 새 엔티티를 할당하고 핸들을 반환한다.
    [[nodiscard]] Entity createEntity() {
        return entityRegistry_.allocate();
    }

    // 엔티티와 그에 속한 모든 컴포넌트를 삭제한다.
    //
    // cleanup flow:
    //   1. ComponentRegistry::notifyEntityDestroyed() → 등록된 모든 storage에
    //      entityDestroyed()를 호출해 컴포넌트 데이터를 제거
    //   2. EntityRegistry::release() → 해당 슬롯을 free-list에 반환하고
    //      generation을 증가시켜 stale handle을 무효화
    void destroyEntity(Entity entity) {
        componentRegistry_.notifyEntityDestroyed(entity);
        entityRegistry_.release(entity);
    }

    // 엔티티가 현재 살아있는지 확인한다.
    [[nodiscard]] bool isAlive(Entity entity) const noexcept {
        return entityRegistry_.isAlive(entity);
    }

    // ----------------------------------------------------------------
    // 컴포넌트 조작
    // ----------------------------------------------------------------

    // 엔티티에 컴포넌트 T를 추가하고 signature를 갱신한다.
    //
    // T가 처음 추가되는 타입이면 ComponentRegistry에 자동 등록된다.
    // 이미 해당 컴포넌트가 있는 경우 PackedComponentStorage::insert에서 예외 발생.
    template <typename T>
    void addComponent(Entity entity, T component) {
        const ComponentType typeId = componentRegistry_.getOrRegister<T>();
        componentRegistry_.storageFor<T>().insert(entity, std::move(component));

        Signature sig = entityRegistry_.signatureOf(entity);
        sig.set(typeId);
        entityRegistry_.setSignature(entity, sig);
    }

    // 엔티티에서 컴포넌트 T를 제거하고 signature를 갱신한다.
    //
    // T가 등록되지 않았거나 해당 entity에 T가 없으면 조용히 무시한다.
    template <typename T>
    void removeComponent(Entity entity) {
        const auto typeId = componentRegistry_.tryTypeOf<T>();
        if (!typeId.has_value()) {
            return;
        }

        auto& storage = componentRegistry_.storageFor<T>();
        if (!storage.contains(entity)) {
            return;
        }

        storage.remove(entity);

        Signature sig = entityRegistry_.signatureOf(entity);
        sig.reset(typeId.value());
        entityRegistry_.setSignature(entity, sig);
    }

    // 엔티티의 컴포넌트 T를 mutable 참조로 반환한다.
    // T가 없으면 PackedComponentStorage::get에서 예외 발생.
    template <typename T>
    [[nodiscard]] T& getComponent(Entity entity) {
        return componentRegistry_.storageFor<T>().get(entity);
    }

    template <typename T>
    [[nodiscard]] const T& getComponent(Entity entity) const {
        return componentRegistry_.storageFor<T>().get(entity);
    }

    // entity가 컴포넌트 T를 보유하고 있는지 확인한다.
    // T가 한 번도 등록된 적 없으면 false를 반환한다.
    template <typename T>
    [[nodiscard]] bool hasComponent(Entity entity) const {
        if (!componentRegistry_.isRegistered<T>()) {
            return false;
        }
        return componentRegistry_.storageFor<T>().contains(entity);
    }

    // ----------------------------------------------------------------
    // 내부 레지스트리 접근자
    // ----------------------------------------------------------------

    [[nodiscard]] EntityRegistry& entityRegistry() noexcept {
        return entityRegistry_;
    }
    [[nodiscard]] const EntityRegistry& entityRegistry() const noexcept {
        return entityRegistry_;
    }
    [[nodiscard]] ComponentRegistry& componentRegistry() noexcept {
        return componentRegistry_;
    }
    [[nodiscard]] const ComponentRegistry& componentRegistry() const noexcept {
        return componentRegistry_;
    }

private:
    EntityRegistry entityRegistry_;
    ComponentRegistry componentRegistry_;
};

}  // namespace safecrowd::engine
