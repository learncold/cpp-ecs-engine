#include "engine/EntityRegistry.h"

#include <sstream>
#include <stdexcept>
#include <utility>

namespace safecrowd::engine {
namespace {

std::string describeEntity(Entity entity) {
    std::ostringstream stream;
    stream << entity;
    return stream.str();
}

}  // namespace

EntityRegistry::EntityRegistry(std::size_t maxEntityCount)
    : entries_(maxEntityCount) {
    freeIndices_.resize(maxEntityCount);

    for (std::size_t index = 0; index < maxEntityCount; ++index) {
        freeIndices_[index] = static_cast<EntityIndex>(index);
    }
}

Entity EntityRegistry::allocate() {
    if (freeIndices_.empty()) {
        throw std::overflow_error("EntityRegistry capacity exhausted.");
    }

    const EntityIndex index = freeIndices_.front();
    freeIndices_.pop_front();

    Entry& entry = entries_[index];
    entry.alive = true;
    entry.signature.reset();

    return Entity{index, entry.generation};
}

void EntityRegistry::release(Entity entity) {
    Entry& entry = entryFor(entity);
    entry.alive = false;
    entry.signature.reset();
    ++entry.generation;
    freeIndices_.push_back(entity.index);
}

bool EntityRegistry::isAlive(Entity entity) const noexcept {
    if (!entity.isValid()) {
        return false;
    }

    const auto index = static_cast<std::size_t>(entity.index);
    if (index >= entries_.size()) {
        return false;
    }

    const Entry& entry = entries_[index];
    return entry.alive && entry.generation == entity.generation;
}

void EntityRegistry::setSignature(Entity entity, Signature signature) {
    Entry& entry = entryFor(entity);
    entry.signature = signature;
}

Signature EntityRegistry::signatureOf(Entity entity) const {
    return entryFor(entity).signature;
}

const EntityRegistry::Entry& EntityRegistry::entryFor(Entity entity) const {
    if (!entity.isValid()) {
        throw std::invalid_argument("Invalid entity handle.");
    }

    const auto index = static_cast<std::size_t>(entity.index);
    if (index >= entries_.size()) {
        throw std::out_of_range("Entity index is out of range.");
    }

    const Entry& entry = entries_[index];
    if (!entry.alive || entry.generation != entity.generation) {
        throw std::invalid_argument("Stale or dead entity handle: " + describeEntity(entity));
    }

    return entry;
}

EntityRegistry::Entry& EntityRegistry::entryFor(Entity entity) {
    return const_cast<Entry&>(std::as_const(*this).entryFor(entity));
}

}  // namespace safecrowd::engine
