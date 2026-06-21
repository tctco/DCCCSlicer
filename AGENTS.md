每次递增版本号时，更新以下文件（写路径全名，避免改错同名文件）：
- `localizer/src/CMakeLists.txt`
  - `project(DCCCcore VERSION X.Y.Z)` 只写纯数字版本。
  - 如果是预发布版本，再同步更新 `DCCCCORE_VERSION_SUFFIX`（例如 `-alpha`）。
- `localizer/src/conanfile.py`
  - `version = "X.Y.Z"` 或 `version = "X.Y.Z-alpha"`，需要与对外发布版本保持一致。
- `localizer/src/core/config/Version.h`
  - 当前仓库会提交这个生成后的头文件，里面的 `SOFTWARE_VERSION` 也要同步更新，避免 `--version` 或批处理日志显示旧版本。

补充说明：
- `localizer/src/core/config/Version.h.in` 是模板文件，日常递增版本号时不需要手改具体版本号；它会从 `localizer/src/CMakeLists.txt` 里的完整软件版本变量生成。
- 仓库根目录的 `CMakeLists.txt` 当前没有单独的项目版本号，不属于版本递增时必须同步的文件。
- 发布时还要注意 Git tag。`.github/workflows/cross-compile.yml` 会优先用 tag 名 `vX.Y.Z` 生成发布包版本；如果源码版本已经更新但 tag 没跟上，发布产物版本仍可能不一致。
