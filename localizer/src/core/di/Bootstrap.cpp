#include "Bootstrap.h"
#include "../config/ConfigLoader.h"
#include "../services/FileService.h"
#include "../services/SpatialNormalizationService.h"

namespace Pipeline {

std::shared_ptr<ServiceContainer> buildCoreContainer(const BootstrapOptions& options) {
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
    container->registerSingleton<IFileService>(
        [](auto&) {
            return std::make_shared<FileService>();
        });

    return container;
}

} // namespace Pipeline
