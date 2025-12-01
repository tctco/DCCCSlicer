# Refactor Prototype

## Overview
The `localizer/src/refactor` tree experiments with rebuilding the PET localization pipeline around explicit service boundaries and dependency injection. The goal is to decouple spatial normalization, file IO, and metric computation so that new metrics or execution surfaces can be added without threading dependencies through the legacy monolith. The current prototype exposes five CLI subcommands (`refactor-suvr`, `refactor-centiloid`, `refactor-centaurz`, `refactor-fillstates`, and `refactor-adad`) that exercise the new service graph while still reusing the existing normalizers, calculators, and configuration objects.

## Directory layout
- `application/` – thin `PipelineApplication` façade that orchestrates services based on a `ProcessingRequest`.
- `common/` – request/response contracts (`ProcessingContracts.h`) shared by services and CLIs.
- `di/` – lightweight `ServiceContainer` plus `Bootstrap` helpers that wire the graph together.
- `config/` – configuration contracts and the TOML-backed `Configuration`/`Version` pair reused by both legacy and refactor paths.
- `interfaces/` – abstraction layer for CLI modules (`IMetricCLI`), metric logic (`IMetricLogic`), the runtime registry (`IMetricModuleRegistry`), and shared configuration/normalizer contracts.
- `metrics/` – CLI/logic pairs for each metric family and a `ModuleCatalog` that enumerates available modules. Currently includes `suvr`, `centiloid`, `centaurz`, `fillstates`, and `adad`.
- `providers/` – adapters that bridge into legacy factories (e.g., `LegacyNormalizerProvider` wraps `SpatialNormalizerFactory` to supply a rigid VoxelMorph normalizer).
- `services/` – concrete runtime services such as spatial normalization, metric execution, file persistence, and the metric registry façade that exposes them through clean interfaces.

## Processing flow
1. **CLI parsing (`IMetricCLI`)** – Each metric-specific CLI (e.g., `SUVrCLI`) now owns its argument definitions, registers any metric modules it needs, and translates CLI arguments into metric-scoped option structs before invoking the pipeline.
2. **Container bootstrap (`buildDefaultContainer`)** – The CLI provides the requested config path + debug flags to `buildDefaultContainer`. Bootstrap invokes `core/config/ConfigLoader`, logs the load results once, and receives a `ServiceContainer` populated with singletons: configuration, `LegacyNormalizerProvider`, `SpatialNormalizationService`, `MetricModuleRegistry`, `MetricService`, `FileService`, and `PipelineApplication`.
3. **Metric registration (`registerAllMetricModules`)** – `Bootstrap` invokes `Metrics::registerAllMetricModules` immediately after wiring services, so every known `IMetricLogic` (`SUVrLogic`, `CentiloidLogic`, …) is registered with `IMetricModuleRegistry` once per container.
4. **Application execution (`PipelineApplication::run`)** – The CLI constructs a `ProcessingRequest`, then calls into `PipelineApplication`, which orchestrates normalization, optional persistence, and metric evaluation using the injected services. Responses contain the normalization outputs plus any metric results, which the CLI formats for stdout.

## Key services and providers
- `ServiceContainer` – Minimal DI container that supports singleton registration/resolution by type. It is purposely small to keep the prototype dependency free.
- `SpatialNormalizationService` – Wraps the existing `RigidVoxelMorphNormalizer` behavior. It interprets `SpatialNormalizationRequest` flags (skip, manual FOV, iterative rigid) and delegates to the legacy normalizer created by `LegacyNormalizerProvider`.
- `MetricService` – Resolves the requested metric by name via `IMetricModuleRegistry`, ensures an image is available, and forwards the `MetricComputationRequest` to the metric logic.
- `MetricModuleRegistry` – Keeps a normalized map of metric names to `IMetricLogic` instances, enforcing uniqueness and providing lookup/list capabilities.
- `FileService` – Simple adapter around `refactorCommon::nifti::saveImage` so persistence can be mocked or replaced later.
- `LegacyNormalizerProvider` – Hides the details of calling `SpatialNormalizerFactory` and ensures a typed `RigidVoxelMorphNormalizer` is returned. This allows new implementations to swap in without touching the services layer.

## Metric modules and CLI integration
- Every metric family supplies both a CLI (`IMetricCLI`) and logic (`IMetricLogic`).
  - `metrics/suvr` implements `refactor-suvr`. Its logic (`SUVrLogic`) validates mask parameters and uses `SUVrCalculator` to produce `MetricResult` entries.
  - `metrics/centiloid` implements `refactor-centiloid`. It reuses the SUVR-derived CLI arguments, creates a `CentiloidCalculator`, and prints tracer values (optionally exposing the intermediate SUVr).
  - `metrics/centaurz` implements `refactor-centaurz`. It follows the same CLI contract as the other SUVr-derived metrics, wraps `CenTauRzCalculator`, and reports the tracer-specific z-score conversions (with optional SUVr echo).
  - `metrics/fillstates` implements `refactor-fillstates`. It requires an explicit tracer flag, uses `FillStatesCalculator` to produce meta-ROI suprathreshold fractions, and optionally saves the fill-states mask map alongside the normalized output.
  - `metrics/adad` implements `refactor-adad`. It prepares an ADNI-style volume, runs the ONNX decoupler (single model or ensemble), and converts the ADAD score into tracer-specific values using the configuration-provided slopes/intercepts.
- `metrics/ModuleCatalog` returns the list of available CLI modules, which can be plugged into the top-level command dispatcher. This keeps CLI discovery data-driven once more modules land.
- All refactor CLIs currently opt out of batch processing; they emit clear stderr messaging if `--batch` is supplied so downstream tooling understands the limitation.

## Adding a new metric
1. **Implement the logic** – Create `metrics/<name>/<Name>Logic.{h,cpp}` that inherits `IMetricLogic`, validates configuration/options, and returns one or more `MetricResult` entries.
2. **Expose a CLI** – Add `<Name>CLI.{h,cpp}` that implements `IMetricCLI`, defines the arguments it needs, and translates parsed parameters into a `ProcessingRequest` + metric-specific options.
3. **Register the module centrally** – Add your `registerMetric(ServiceContainer&)` call to `Metrics::registerAllMetricModules`, which runs during container bootstrap so every CLI gets the logic automatically.
4. **Add to the catalog** – Update `metrics/ModuleCatalog.cpp` to push your CLI into the returned vector so the command dispatcher can discover it.
5. **(Optional) Extend services** – If your metric needs new runtime dependencies (e.g., another calculator or data source), register them inside `buildDefaultContainer` before `PipelineApplication` is resolved.

## Sample commands
```powershell
# SUVR prototype – masks currently mandatory
localizer.exe refactor-suvr --input PET.nii --output PET_norm.nii `
    --config config.toml --voi-mask path/to/voi.nii --ref-mask path/to/ref.nii `
    --iterative --debug

# Centiloid prototype – reuses SUVr-derived arguments
localizer.exe refactor-centiloid --input PET.nii --output PET_centiloid.nii `
    --config config.toml --suvr

localizer.exe refactor-centaurz --input PET.nii --output PET_centaurz.nii `
    --config config.toml --suvr

localizer.exe refactor-centiloid --input PET_dir --output centiloid_outputs `
    --config config.toml --batch --skip-normalization

localizer.exe refactor-fillstates --input PET.nii --output PET_fillstates.nii `
    --config config.toml --tracer fbp

# ADAD prototype – selects the decoupling modality
localizer.exe refactor-adad --input PET.nii --output PET_adad.nii `
    --config config.toml --modality abeta
```

Both commands still rely on the legacy configuration loader (`Configuration::findConfigFile`) and save the normalized NIfTI image before printing metrics to stdout.

## Current limitations / next steps
- Batch mode is currently only implemented for `refactor-centiloid` (directory-to-directory execution). Other refactor CLIs still emit clear stderr messaging if `--batch` is supplied so downstream tooling understands the limitation.
- Only the rigid VoxelMorph normalizer is available; additional normalizers must be exposed through new providers.
- Error handling is intentionally minimal; production-ready tooling should surface structured status data instead of console strings.
- The service graph is built once per CLI invocation; persisting the container (or supporting scoped lifetimes) would be required for interactive or server scenarios.

Despite those gaps, the refactor prototype demonstrates how dependency injection and modular metric registries can simplify future growth while continuing to leverage the proven legacy components.

