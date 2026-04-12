#pragma once

namespace safecrowd::domain {
    // 특정 에이전트가 받는 물리적/시간적 압박 상태를 담음
    struct CompressionData {
        // CompressionForce: 주변 객체와의 접촉으로 인해 발생하는 즉각적인 하중
        float force = 0.0f;

        // CompressionExposure: 위험 임계값 이상의 압박이 지속된 누적 시간 (단위: sec)
        float exposure = 0.0f;

        // CompressionCriticalState: 하중과 지속시간을 종합한 고위험 판정 플래그
        bool isCritical = false;
    };

} // namespace safecrowd::domain