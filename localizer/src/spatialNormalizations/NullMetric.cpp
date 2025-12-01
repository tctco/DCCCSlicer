#include "NullMetric.h"
#include "../core/interfaces/IMetricModuleRegistry.h"

namespace Pipeline::SpatialNormalization {

namespace {

class NullMetric final : public IMetricLogic {
public:
    std::string getName() const override {
        return kNullMetricName;
    }

    std::vector<MetricResult> calculate(const MetricComputationRequest&) override {
        return {};
    }
};

} // namespace

void registerNullMetric(ServiceContainer& container) {
    auto registry = container.resolve<IMetricModuleRegistry>();
    if (!registry->hasModule(kNullMetricName)) {
        registry->registerModule(std::make_shared<NullMetric>());
    }
}

} // namespace Pipeline::SpatialNormalization



