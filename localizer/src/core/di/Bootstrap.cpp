#include "Bootstrap.h"
#include "../services/SpatialNormalizationService.h"
#include "../services/MetricService.h"
#include "../services/MetricModuleRegistry.h"
#include "../services/FileService.h"
#include "../../metrics/ModuleCatalog.h"
#include "../config/ConfigLoader.h"

namespace Pipeline {

std::shared_ptr<ServiceContainer> buildDefaultContainer(const BootstrapOptions& options) {
    ConfigurationLoadOptions loadOptions;
    loadOptions.configPath = options.configPath;
    loadOptions.enableDebugOutput = options.enableConfigDebug;
    loadOptions.logTag = options.logTag;

    auto config = loadConfigurationWithLogging(loadOptions);

    auto container = std::make_shared<ServiceContainer>();
    container->registerSingleton<IConfiguration>([config](auto&) { return config; });
    container->registerSingleton<ISpatialNormalizationService>(
        [](auto& c) {
            auto cfg = c.template resolve<IConfiguration>();
            return std::make_shared<SpatialNormalizationService>(cfg);
        });
    container->registerSingleton<IMetricModuleRegistry>(
        [](auto&) {
            return std::make_shared<MetricModuleRegistry>();
        });
    container->registerSingleton<IMetricService>(
        [](auto& c) {
            auto registry = c.template resolve<IMetricModuleRegistry>();
            return std::make_shared<MetricService>(registry);
        });
    container->registerSingleton<IFileService>(
        [](auto&) {
            return std::make_shared<FileService>();
        });
    container->registerSingleton<PipelineApplication>(
        [](auto& c) {
            auto spatial = c.template resolve<ISpatialNormalizationService>();
            auto metric = c.template resolve<IMetricService>();
            auto fileService = c.template resolve<IFileService>();
            return std::make_shared<PipelineApplication>(spatial, metric, fileService);
        });

    Metrics::registerAllMetricModules(*container);

    return container;
}

std::shared_ptr<PipelineApplication> resolveApplication(ServiceContainer& container) {
    return container.resolve<PipelineApplication>();
}

} // namespace Pipeline

