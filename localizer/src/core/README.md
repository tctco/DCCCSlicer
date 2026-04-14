# Core Architecture

## Overview
`core/` is now the base capability layer for the PET tooling. It does not own metric orchestration. Metrics live under `metrics/`, and each metric decides its own workflow through a `CLI + Service + Calculator` structure.

The current boundary is:
- `core` provides shared runtime capabilities
- `metrics` owns business workflows
- `spatialNormalizations` exposes standalone normalization CLIs

## What belongs in `core`
- `config/` – TOML-backed configuration loading and version metadata
- `common/` – shared image/path/filesystem/logging utilities and normalization contracts
- `di/` – the lightweight `ServiceContainer` and `buildCoreContainer(...)`
- `interfaces/` – only base capability interfaces such as `IConfiguration` and `ISpatialNormalizer`
- `services/` – reusable services such as `SpatialNormalizationService` and `FileService`
- `normalizers/` and `preprocessing/` – reusable image processing implementations

## What does not belong in `core`
`core` no longer contains:
- global metric pipeline orchestration
- metric registry / metric dispatch
- metric logic abstractions such as `IMetricLogic`
- metric CLI abstractions
- metric-specific request/response contracts

Those concepts now live in `metrics/` or `metrics/shared/`.

## Processing model
1. A metric CLI parses arguments and builds metric-specific options.
2. The CLI creates a base container with `buildCoreContainer(...)`.
3. The metric service resolves shared dependencies from the container.
4. The metric service decides its own workflow:
   - whether to normalize
   - whether to save intermediate output
   - how to run single-file or batch execution
   - how to call its calculator
5. The calculator performs the metric-specific computation.

## Shared services
- `ServiceContainer` – minimal singleton DI container
- `SpatialNormalizationService` – shared normalization entrypoint driven by `SpatialNormalizationRequest`
- `FileService` – shared persistence adapter for normalized images

## Notes for new work
- If a concern is reusable across many metrics but still metric-facing, prefer `metrics/shared/` over `core/`.
- If a concern is domain-agnostic and useful outside metrics, it belongs in `core/`.
- New metrics should be added as self-contained modules and registered through `metrics/ModuleCatalog`.
