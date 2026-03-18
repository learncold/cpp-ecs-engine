#include "domain/SafeCrowdDomain.h"

namespace safecrowd
{
Overview SafeCrowdDomain::buildOverview() const
{
    return {
        "SafeCrowd",
        std::string(engine_.name()) + ": " + std::string(engine_.summary()),
        "Domain layer owns scenario rules, simulation behavior, and risk evaluation."
    };
}
} // namespace safecrowd
