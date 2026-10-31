---
name: dccc
description: Use when processing brain PET NIfTI images with DCCC/DCCCSlicer or dcccpy, including preprocessing, registration, segmentation, and PET metrics.
---

# DCCC via dcccpy

优先使用 `dcccpy` Python API。安装 `pip install --pre 'dcccpy[runtime]'`；核对
`dcccpy.DCCCCORE_VERSION` 和 `dcccpy.run(['--version']).stdout`，避免误用旧 core。
具体参数查 [API 文档](https://github.com/tctco/DCCCSlicer/blob/dev/python/dcccpy/README.md)
或 `dcccpy.run([subcommand, '--help']).stdout`，不要猜测参数。
PyPI runtime 为精简版，不含 ADAD 模型；需完整 runtime 时用
`executable=dcccpy.download_dccccore()` 显式选用下载的完整 core。

```python
import dcccpy
r = dcccpy.scan_pet_core('fdg.nii.gz', 'fdg_preprocessed.nii.gz', tracer='fdg')
print(r.output)
```

- 预处理用 `adni_pet_core`，显式指定 `tracer='abeta'|'tau'|'fdg'|'dat'`。
  `level=1` 为动态帧配准（需 4D），`2` 为均值，`3` 为标准化输出；可传列表。
  当前源码的 ADNI FDG 已恢复迭代全脑均值；`scan_pet_core(..., tracer="fdg")`
  单独使用脑桥＋小脑蚓联合区最高 50% 强度均值，rigid 行为不变。
  此拆分从 `dcccpy 0.3.0a7` / core `4.6.3-alpha` 起提供；避免使用改变了 ADNI 语义的 `0.3.0a6`/`4.6.2-alpha`。
- 配准用 `rigid` / `normalize`；指标用 `centiloid`、`centaurz`、`suvr` 等，
  读取 `r.metrics`。仅在输入已符合目标模板空间时使用 `skip_normalization=True`。
- 原生空间分割用 `seg`，默认返回 FreeSurfer/DKT 标签。模型可能首次联网下载；
  离线预备 runtime/model 后设 `DCCCPY_AUTO_DOWNLOAD=0`。不传 TTA/flip 参数；
  NeuroSegmenter 模式缺少脑桥标签，不能据此做脑桥 SUVR。
- 输入为 `.nii`/`.nii.gz` 或 NiBabel 图像；明确输出路径以保留结果。
  需载入输出时用 `r.load_output()`（需 NiBabel），保留几何信息。
