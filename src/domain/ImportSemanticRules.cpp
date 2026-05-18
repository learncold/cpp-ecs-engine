#include "domain/ImportSemanticRules.h"

#include <algorithm>
#include <cctype>
#include <string>

namespace safecrowd::domain {
namespace {

std::string toUpper(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::toupper(ch));
    });
    return value;
}

std::string semanticSignature(
    const SourceTrace& trace,
    const std::map<std::string, std::string>& metadata) {
    std::string signature = trace.layerName + " " + trace.objectName + " " + trace.externalId;
    for (const auto& [key, value] : metadata) {
        signature += " " + key + " " + value;
    }
    return toUpper(signature);
}

bool containsToken(const std::string& signature, const std::string& token) {
    return !token.empty() && signature.find(toUpper(token)) != std::string::npos;
}

}  // namespace

ImportSemanticRuleSet ImportSemanticRuleSet::defaultRules() {
    return {
        .rules = {
            {
                .semantic = ImportElementSemantic::Exit,
                .tokens = {"EXIT", "EGR", "EMERGENCY_EXIT"},
            },
            {
                .semantic = ImportElementSemantic::Opening,
                .tokens = {"OPEN", "DOOR", "PORTAL", "GATE", "ENTRANCE"},
            },
            {
                .semantic = ImportElementSemantic::Wall,
                .tokens = {"WALL", "WALLS", "BARRIER", "PARTITION"},
            },
            {
                .semantic = ImportElementSemantic::Obstacle,
                .tokens = {"OBST", "OBSTACLE", "COLUMN", "FURN", "EQUIP"},
            },
            {
                .semantic = ImportElementSemantic::Walkable,
                .tokens = {"WALK", "FLOOR", "SPACE", "ROOM", "AREA"},
            },
        },
    };
}

ImportElementSemantic ImportSemanticRuleSet::classify(
    const SourceTrace& trace,
    const std::map<std::string, std::string>& metadata) const {
    const auto signature = semanticSignature(trace, metadata);

    for (const auto& rule : rules) {
        for (const auto& token : rule.tokens) {
            if (containsToken(signature, token)) {
                return rule.semantic;
            }
        }
    }

    return ImportElementSemantic::Unknown;
}

double ImportSemanticRuleSet::confidenceFor(
    ImportElementSemantic semantic,
    const SourceTrace& trace,
    const std::map<std::string, std::string>& metadata) const {
    const auto signature = semanticSignature(trace, metadata);

    for (const auto& rule : rules) {
        if (rule.semantic != semantic) {
            continue;
        }
        for (const auto& token : rule.tokens) {
            if (containsToken(signature, token)) {
                return rule.confidence;
            }
        }
    }

    return 0.0;
}

const char* toString(ImportElementSemantic semantic) noexcept {
    switch (semantic) {
    case ImportElementSemantic::Unknown:
        return "Unknown";
    case ImportElementSemantic::Wall:
        return "Wall";
    case ImportElementSemantic::Opening:
        return "Opening";
    case ImportElementSemantic::Exit:
        return "Exit";
    case ImportElementSemantic::Obstacle:
        return "Obstacle";
    case ImportElementSemantic::Walkable:
        return "Walkable";
    }

    return "Unknown";
}

const char* toString(ImportFallbackPolicy policy) noexcept {
    switch (policy) {
    case ImportFallbackPolicy::Disabled:
        return "Disabled";
    case ImportFallbackPolicy::ReviewableGeometry:
        return "ReviewableGeometry";
    }

    return "Unknown";
}

}  // namespace safecrowd::domain
