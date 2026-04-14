# Developer Guide

This guide describes the current `localizer/src` architecture after the metric refactor. The main rule is simple:

- `core/` provides shared capabilities
- `metrics/` owns metric workflows
- `spatialNormalizations/` exposes normalization-only CLIs

There is no longer a global metric pipeline in `core`.

## Architecture

### `core/`

`core/` is the foundation layer. It should not know how a specific metric works.

Key areas:

- `core/config/`
  - TOML loading
  - executable-relative path resolution
  - default configuration values
- `core/common/`
  - NIfTI IO
  - image operations
  - filesystem/path helpers
  - logging helpers
  - normalization contracts
- `core/services/`
  - `SpatialNormalizationService`
  - `FileService`
- `core/di/`
  - `ServiceContainer`
  - `buildCoreContainer(...)`
- `core/normalizers/`
  - rigid + deformable normalization implementation details

`core/` should not contain:

- metric registries for calculation dispatch
- metric logic abstractions
- metric-specific workflows
- metric CLI abstractions
- a global `PipelineApplication`

### `metrics/`

Each metric is a self-contained module under `localizer/src/metrics/<name>/`.

Current examples:

- `suvr/`
- `centiloid/`
- `centaur/`
- `centaurz/`
- `fillstates/`
- `abetaload/`
- `abetaindex/`
- `adad/`

Each metric should own its own execution path:

- parse arguments in its CLI
- build a core container
- resolve shared services
- decide whether to normalize
- call its calculator
- format outputs

### `metrics/shared/`

This folder is for metric-facing shared utilities that do not belong in `core/`.

Current shared pieces:

- `IMetricCLI.h`
- `MetricRegistry.*`
- `MetricTypes.h`
- `MetricRunResult.h`
- `SingleRunner.h`
- `BatchRunner.h`
- `BatchLogging.*`
- `DebugPathHelpers.h`

Use `metrics/shared/` for cross-metric helpers. Do not push metric workflow abstractions back into `core/`.

### `spatialNormalizations/`

These commands expose normalization without metric calculation:

- `normalize`
- `adni-pet-core`

They directly use `ISpatialNormalizationService` and `IFileService`. They do not go through a metric pipeline.

## Runtime Flow

The executable entrypoint is [main.cpp](../localizer/src/main.cpp).

At startup:

1. `main.cpp` builds metric CLIs from `Pipeline::Metrics::buildCLIModules()`
2. `main.cpp` builds normalization CLIs from `Pipeline::SpatialNormalization::buildCLIModules()`
3. the selected subcommand executes its own module

For a metric command, the flow is:

1. CLI parses arguments into a metric-specific options struct
2. CLI calls `buildCoreContainer(...)`
3. metric service resolves:
   - `IConfiguration`
   - `ISpatialNormalizationService`
   - `IFileService`
4. metric service runs single-file or batch workflow
5. calculator performs the actual metric computation

In other words:

- `main.cpp` dispatches commands
- `core` provides capabilities
- `metric service` owns workflow
- `metric calculator` owns computation

## Current Module Shape

The preferred metric shape is:

```text
metrics/<name>/
  <Name>CLI.h
  <Name>CLI.cpp
  <Name>Service.h
  <Name>Service.cpp
  <Name>Calculator.h
  <Name>Calculator.cpp
  <Name>Module.h
  <Name>Module.cpp
```

Responsibilities:

- `CLI`
  - declares subcommand name
  - defines CLI arguments
  - builds options
  - creates the core container
  - calls the service
- `Service`
  - validates metric-specific runtime requirements
  - builds normalization requests
  - chooses single vs batch execution
  - saves normalized output
  - prepares calculator inputs from config/assets
- `Calculator`
  - pure metric computation
  - should not parse CLI flags
  - should not bootstrap services
- `Module`
  - registers the CLI with `MetricRegistry`

If a metric is very small, `Service` and `Calculator` may look close in size. That is acceptable. The important split is:

- workflow in `Service`
- numeric/image computation in `Calculator`

## Registry and Discovery

Metric discovery is handled in:

- [metrics/shared/MetricRegistry.h](../localizer/src/metrics/shared/MetricRegistry.h)
- [metrics/ModuleCatalog.cpp](../localizer/src/metrics/ModuleCatalog.cpp)

This registry is for CLI/module discovery, not for a core-side metric dispatch pipeline.

To expose a new metric:

1. implement `createCLI()` in the metric folder
2. implement `registerModule(MetricRegistry&)`
3. add the metric module to `metrics/ModuleCatalog.cpp`

## Shared Execution Helpers

Most metrics should reuse the shared runners:

- `SingleRunner.h`
- `BatchRunner.h`

These helpers standardize:

- logging
- batch iteration
- debug path derivation
- `results.csv` / `batch_info.txt`

Use them when the metric fits the common pattern:

- normalize
- save normalized output
- calculate metric
- report results

If a metric has a special flow, it can still own that flow directly inside its service.

## Configuration Rules

Default configuration lives in:

- [localizer/src/assets/configs/config.toml](../localizer/src/assets/configs/config.toml)
- [localizer/src/assets/configs/config.fast_and_acc.toml](../localizer/src/assets/configs/config.fast_and_acc.toml)

Important rules:

- add metric-specific templates, masks, and coefficients to the config that actually supports that metric
- do not silently enable a metric under an incompatible normalization strategy
- if a metric depends on a specific normalization regime, validate that explicitly in the service

Example:

- `abetaindex` is intentionally enabled only in the standard `config.toml`
- `config.fast_and_acc.toml` does not declare it
- `AbetaIndexService` validates that the metric is enabled and that its templates exist

That pattern is preferred over silently falling back to defaults.

## Adding a New Metric

### 1. Create the metric folder

Add:

- `<Name>CLI.*`
- `<Name>Service.*`
- `<Name>Calculator.*`
- `<Name>Module.*`

Start from one of these references:

- `centiloid/` for a typical derived metric
- `suvr/` for a simple VOI/reference metric
- `fillstates/` for a metric with extra output files
- `abetaindex/` for a config-gated template metric
- `adad/` for a model-driven metric

### 2. Add the CLI

The CLI should:

- inherit `Pipeline::Metrics::IMetricCLI`
- define the subcommand name and description
- configure arguments
- map parser values into a metric-specific options struct
- call `buildCoreContainer(...)`
- call `createService(*container)`

Look at:

- [CentiloidCLI.cpp](../localizer/src/metrics/centiloid/CentiloidCLI.cpp)
- [AbetaIndexCLI.cpp](../localizer/src/metrics/abetaindex/AbetaIndexCLI.cpp)

### 3. Add the service

The service should:

- accept shared services through the constructor
- validate configuration and runtime preconditions
- build `SpatialNormalizationRequest`
- save the normalized NIfTI via `IFileService`
- build calculator input
- call `runSingle(...)` or `runBatch(...)`

Prefer reusing:

- `Pipeline::Metrics::Shared::runSingle(...)`
- `Pipeline::Metrics::Shared::runBatch(...)`

### 4. Add the calculator

The calculator should:

- accept an explicit `Input` struct
- return `Pipeline::Metrics::MetricResult`
- throw descriptive `std::runtime_error` / `std::invalid_argument` when inputs are invalid

Keep it focused on the actual metric math or image operation.

### 5. Register the module

Implement:

```cpp
void registerModule(MetricRegistry& registry) {
    registry.registerModule({"my_metric", true, &createCLI});
}
```

Then add it to [metrics/ModuleCatalog.cpp](../localizer/src/metrics/ModuleCatalog.cpp).

### 6. Add config and assets

Typical things you may need:

- mask paths
- template paths
- tracer parameters
- model paths
- feature flags such as `enabled = true`

If a metric should only work with one config profile, encode that intentionally and validate it in the service.

### 7. Add tests

At minimum:

- add `--help` and basic invocation coverage to [test_quick_cli.py](../localizer/src/tests/test_quick_cli.py)
- add a metric-specific CLI test under `localizer/src/tests/`

Examples:

- [test_abetaload_cli.py](../localizer/src/tests/test_abetaload_cli.py)
- [test_abetaindex_cli.py](../localizer/src/tests/test_abetaindex_cli.py)
- [test_fillstates_cli.py](../localizer/src/tests/test_fillstates_cli.py)
- [test_adad_cli.py](../localizer/src/tests/test_adad_cli.py)

If the metric has known reference values, add a dedicated accuracy test.

### 8. Update documentation

Update:

- [localizer/src/README.md](../localizer/src/README.md) for CLI behavior
- root [README.md](../README.md) if the metric is user-facing at the product level
- metric reproduction docs under `docs/` if applicable

## Design Rules

When extending the project, prefer these rules:

- keep `core` ignorant of metric business logic
- keep metric workflow inside the metric service
- keep computation inside the calculator
- put cross-metric workflow helpers in `metrics/shared/`, not `core`
- avoid adding vague abstractions like `Logic`, `Handler`, or a global metric pipeline unless there is a strong, proven need
- validate configuration compatibility explicitly

## Notes on Existing Metrics

- `centiloid`, `centaur`, `centaurz`
  - normalization + metric conversion workflows
- `suvr`
  - custom VOI/reference masks supplied through CLI
- `fillstates`
  - writes an extra mask output
- `abetaload`
  - template decomposition metric
- `abetaindex`
  - AV45-only template metric with config gating
- `adad`
  - decoupler-based metric, single-file only

## What Not to Reintroduce

Do not add back:

- `IMetricLogic`
- `IMetricService` as a core dispatch abstraction
- `IMetricModuleRegistry` in `core`
- `PipelineApplication`
- `NullMetric`
- a core-owned global metric orchestration layer

Those abstractions were removed on purpose because they blurred the boundary between shared infrastructure and metric business logic.

## Practical References

Useful files when working on new modules:

- [localizer/src/core/README.md](../localizer/src/core/README.md)
- [localizer/src/core/di/Bootstrap.cpp](../localizer/src/core/di/Bootstrap.cpp)
- [localizer/src/core/common/NormalizationContracts.h](../localizer/src/core/common/NormalizationContracts.h)
- [localizer/src/metrics/shared/SingleRunner.h](../localizer/src/metrics/shared/SingleRunner.h)
- [localizer/src/metrics/shared/BatchRunner.h](../localizer/src/metrics/shared/BatchRunner.h)
- [localizer/src/metrics/shared/BatchLogging.cpp](../localizer/src/metrics/shared/BatchLogging.cpp)
- [localizer/src/metrics/ModuleCatalog.cpp](../localizer/src/metrics/ModuleCatalog.cpp)

If a new feature does not clearly fit the current boundaries, fix the boundary first in your head before writing code. Most architecture drift in this codebase comes from letting `core` absorb metric-specific workflow.
