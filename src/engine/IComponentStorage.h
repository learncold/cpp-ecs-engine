#pragma once

#include "engine/Entity.h"

namespace safecrowd::engine {

class IComponentStorage {
public:
    virtual ~IComponentStorage() = default;

    virtual void entityDestroyed(Entity entity) = 0;
};

}  // namespace safecrowd::engine
