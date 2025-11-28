# DCCCSlicer

<p align="center">
<img src="./DCCCSlicer.png" style="width:20%" alt="logo">
</p>
3DSlicer plugin for Deep Cascaded Cerebral Calculator (formerly known as Deep Cascaded Centiloid Calculator).

An open-source, super-simple, ultra-fast, fully-automated, fairly-accurate and PET-only solution to conduct spatial normalization and semi-quantification for almost any brain PET modalities.

Abeta, tau, FDG, DAT, MET, synaptic density... you name it!

> DCCCSlicer is currently only available on Windows. To use it on other platforms, you may need to recompile it from the [source](https://github.com/tctco/Beyond-Centiloid-code).

Use DCCC to calculate Centiloid and CenTauRz.

https://github.com/user-attachments/assets/7ba5346a-3214-4a92-80f3-370f4c46f29c

**Shiny new features** DCCCSlicer can interpret input Abeta and tau PET images now. It not only help determine if a patient may have AD, but also identifies the regions of pathological deposition (you may need to adjust the window/level to get a better illustration).

https://github.com/user-attachments/assets/680490d2-ebec-4846-871c-98fdc383b513

## Quality control

It’s always a good idea to manually verify that DCCC has performed spatial normalization correctly. After processing, click the `Show Normalization` button to inspect the normalization quality. Poorly normalized images will result in inaccurate semi-quantitative metrics - see this [issue](https://github.com/tctco/DCCCSlicer/issues/1) for details. This is especially important for PET scans containing substantial non-brain anatomy (e.g., neck, shoulders). If automatic spatial normalization proves suboptimal, **roll back to the original image and try one of these** rescue strategies:

- Enable the `Iterative Rigid` option in the plugin interface
- Or enable the `Manual FOV` option and perform a manual rigid registration (field-of-view placement). DCCC will crop this image, skip rigid registration, and go directly to Affine + Elastic normalization.

<p align="center"><img src="https://github.com/user-attachments/assets/ba4457a3-4d71-4cb6-af17-c2f0a7df4463" width="800"/></p>

> You can quickly align brain images with rigid transformation to the MNI space by localizing AC and PC. This step may be required for spatial normalization with [SPM](https://github.com/spm/spm12)/[rPOP](https://github.com/LeoIacca/rPOP/tree/master).

<https://github.com/tctco/ACPCLocalizer/assets/45505657/c34980e6-c214-4200-9146-aac951fb7f4f>

## Metrics

Relative SUV (ratio) error and time consumption on the Centiloid/CenTauRz projects.

$$
\Delta \mathrm{SUV} \ \\% = \frac{|\mathrm{SUV_{GT} - \mathrm{SUV_{Method}}|}}{\mathrm{SUV_{GT}}}\times 100\\%
$$

| **Methods**         | **PiB (%)**          | **AV45 (%)**         | **FBB (%)**          | **FMM (%)**          | **NAV4694 (%)**      | **FTP (%)**          | **Time (s)**         |
| ------------------- | -------------------- | -------------------- | -------------------- | -------------------- | -------------------- | -------------------- | -------------------- |
| SPM12               | 1.33±1.07            | 1.66±1.32            | 1.40±0.85            | 3.00±2.84            | 1.90±2.77            | 1.07±1.27            | 198.96±59.37         |
| SPM PET             | 7.84±5.31            | 15.75±36.98          | 14.18±11.13          | 10.70±8.41           | 16.43±9.60           | 12.40±11.39          | 6.43±1.80            |
| SPM PET (Template)  | 3.97±2.85            | 6.44±3.83            | 6.16±3.27            | 8.93±3.38            | 5.43±3.27            | 3.65±2.78            | 4.41±0.96            |
| ANTs PET (Template)<sup>1</sup> | 21.27±19.06          | 15.04±7.89           | 19.05±9.68           | 21.89±7.24           | 21.59±8.21           | 6.68±4.58            | 10.07±1.72           |
| rPOP<sup>2</sup>               | 3.13±2.37            | 5.04±8.44            | 3.51±2.74            | 4.76±4.09            | 5.72±11.29           | 5.92±5.38            | 5.32±1.05            |
| SNBPI               | **2.29±1.91**        | **2.24±1.97**        | 2.85±2.33            | 3.83±2.98            | 3.15±2.66            | **1.41±1.13**        | 160.98±46.92         |
| Ours (Pytorch)      | <ins>2.35±1.66</ins> | <ins>2.75±1.68</ins> | <ins>2.75±2.32</ins> | <ins>3.79±3.84</ins> | <ins>2.77±2.16</ins> | 1.50±1.64            | **1.22±0.64**        |
| Ours (Iterative)    | 2.39±1.74            | 2.77±1.73            | 2.76±2.30            | 3.81±3.84            | **2.76±2.15**        | 1.50±1.72            | <ins>1.72±1.16</ins> |
| Ours (ONNX)         | 2.42±1.76            | 2.78±1.71            | **2.72±2.29**        | **3.78±3.84**        | 2.78±2.17            | <ins>1.49±1.67</ins> | 16.60±1.41           |

> The smallest error and shortest computation time are marked with **bold**, while the second smallest error and shortest computation time are <ins>underlined</ins>. SPM12 is employed to reproduce the original literature results using the standard pipeline and does not participate in result ranking; it provides a baseline for reproducibility. SPM PET refers to the PET-only spatial normalization algorithm provided by SPM5, utilizing the 15O-H2O template. SPM PET (Template) refers to the same algorithm, but the templates used are the average of each tracer in the Centiloid/CenTauR dataset after normalization. Check [SNBPI](https://github.com/ZhangTianhao1993/Spatial-Normalization-of-Brain-PET-Images) and [rPOP](https://github.com/LeoIacca/rPOP/) for their wonderful work!
> 
> <sup>1</sup>: ANTsPy exhibited suboptimal performance on Aβ PET images, though results were more acceptable on FTP scans, possibly due to lower image quality in parts of the GAINN Centiloid Project dataset. We consulted the ANTsPy developers (issue [#832](https://github.com/ANTsX/ANTsPy/issues/832#issuecomment-3036727001)), but the problem remains unresolved. Readers should note that these atypical results may not reflect ANTsPy’s performance on higher-quality Aβ PET images.
>
> <sup>2</sup>: rPOP failed completely in spatial normalization on 3 PiB images (yielding infinite or undefined SUVr values) with its fully automated pipeline. Only valid SUVr results were included in the relative error statistics reported in the table.

## Supported metrics and algorithms

Please cite these metrics/algorithms if you use them in your research

Metrics:
- `Centiloid`: Klunk WE, Koeppe RA, Price JC, Benzinger TL, Devous Sr. MD, Jagust WJ, et al. The Centiloid Project: Standardizing quantitative amyloid plaque estimation by PET. Alzheimer’s & Dementia. 2015;11(1):1-15.e4.
- `CenTauR`: Leuzy A, Raket LL, Villemagne VL, Klein G, Tonietto M, Olafson E, et al. Harmonizing tau positron emission tomography in Alzheimer’s disease: The CenTauR scale and the joint propagation model. Alzheimer’s & Dementia. 2024;20(9):5833–48.
- `CenTauRz`: Villemagne VL, Leuzy A, Bohorquez SS, Bullich S, Shimada H, Rowe CC, et al. CenTauR: Toward a universal scale and masks for standardizing tau imaging studies. Alzheimer’s & Dementia: Diagnosis, Assessment & Disease Monitoring. 2023;15(3):e12454.
- `Fill States`: Doering E, Hoenig MC, Giehl K, et al. “Fill States”: PET-derived Markers of the Spatial Extent of Alzheimer Disease Pathology. Radiology. 2025;314(3):e241482. doi:10.1148/radiol.241482
- `ADAD`: not published yet

Spatial normalization algorithms:
- `DCCC SPM12 style`: not published yet
- `Fast and Accurate` (the following publications used the same algorithm):
  - Kang SK, Kim D, Shin SA, Kim YK, Choi H, Lee JS. Fast and Accurate Amyloid Brain PET Quantification Without MRI Using Deep Neural Networks. Journal of Nuclear Medicine. 2023 Apr 1;64(4):659–66.
  - Kim D, Kang SK, Shin SA, Choi H, Lee JS. Improving 18F-FDG PET Quantification Through a Spatial Normalization Method. Journal of Nuclear Medicine. 2024 Aug 29; Available from: https://jnm.snmjournals.org/content/early/2024/08/29/jnumed.123.267360
  - Kang SK, Kim D, Shin SA, Kim YK, Choi H, Lee JS. Evaluation of BTXBrain-Tau, an AI-powered automated quantification software for Flortaucipir PET images. Alzheimer’s & Dementia. 2023;19(S16):e074520. 
  - Yoo HB, Kang SK, Shin SA, Kim D, Choi H, Kim YK, et al. Artificial Intelligence–Powered Quantification of Flortaucipir PET for Detecting Tau Pathology. Journal of Nuclear Medicine. 2025 Sep 11; Available from: https://jnm.snmjournals.org/content/early/2025/09/11/jnumed.125.269636

> Please note that Lee’s algorithm is not publicly available. The version we reproduced based on the DCCC framework may differ in network architecture and implementation details, and it has not yet undergone extensive validation. While Lee et al. achieved cross-modality applicability through transfer learning, our DCCC implementation was directly trained on more than 5,000 multimodal PET images to provide inherent multimodal support.

## Command-line interface

For users who prefer running the core calculator from the command line (including batch processing and advanced options), please refer to the standalone [CLI documentation](./localizer/src/README.md).

## TODO

- [x] Add support for skipping spatial normalization to directly calculate Centiloid/CenTauR.
- [ ] Support other brain PET semi-quantitative metrics, such as Z-scores and basal ganglia asymmetry index.
  - [x] Add support for “Fill States”: PET-derived Markers of the Spatial Extent of Alzheimer Disease Pathology
- [ ] Add support for other spatial normalization algorithms.
  - [x] Added support for Fast and Accurate Amyloid Brain PET Quantification Without MRI Using Deep Neural Networks
- [ ] Improve the UI.

> Check our [reproduction report](./docs/Fill-states.md) on fill states!

## Acknowledgements

We sincerely thank the passionate and outstanding users and contributors of DCCC. Many of our contributors come from the medical community and may not be accustomed to using GitHub, so we would like to acknowledge their contributions here. Your valuable feedback has been the greatest driving force behind the continuous improvement of the project.

<table width="100%" center>
  <tbody>
    <tr>
      <th width="33%">Yulai Li, MD, PhD @ Xiangya Hospital, Changsha, CHINA<br/><sup>For providing the SV2A dataset</sup></th>
      <th width="33%">Chenpeng Zhang, MD, PhD @ Renji Hospital, Shanghai, CHINA<br/><sup>For reporting rigid registration failures</sup></th>
      <th width="33%">Daoyan Hu, PhD @ The Second Affiliated Hospital Zhejiang University School of Medicine, Hangzhou, CHINA<br/><sup>For providing testing feedback with SPM</sup></th>
    </tr>
  </tbody>
  <tbody>
    <tr>
      <th width="33%"><img src="https://upload.wikimedia.org/wikipedia/zh/f/fc/%E6%B9%98%E9%9B%85%E5%8C%BB%E9%99%A2%E9%99%A2%E5%BE%BD.jpg" alt="Xiangya Hospital logo" width="160" /></th>
      <th width="33%"><img src="Not authorized to use the hospital logo" alt="Renji Hospital" /></th>
      <th width="33%"><img src="https://p3-sdbk2-media.byteimg.com/tos-cn-i-xv4ileqgde/8e3cdb956c3f4fb5a5e977e9bad931f8~tplv-xv4ileqgde-resize-w:750.image" alt="The Second Affiliated Hospital Zhejiang University School of Medicine" width="220"/></th>
    </tr>
    <tr>
      <th width="33%">Fanglan Li, MD, PhD @ West China Hospital, Chengdu, CHINA<br><sup>For reporting rigid registration failures</sup></th>
      <th width="33%">And more...</th>
      <th width="33%"></th>
    </tr>
    <tr>
      <th width="33%"><img src="https://media.licdn.com/dms/image/v2/C560BAQH784Gt17OeVg/company-logo_200_200/company-logo_200_200/0/1643080250401?e=2147483647&v=beta&t=egCFbtkNMfczQq8w1PE77L8Ha2h-9MgEBw_-V2Zg0rg" width="190" /></th>
      <th width="33%">Feel free to get in touch if you'd like to contribute, report a bug, share a poorly spatially normalized PET image, or suggest support for a new brain PET metric - we'll be sure to credit you!</th>
      <th width="33%"></th>
    </tr>
  </tbody>
</table>

## License

> This project is open-sourced under a CC-BY-NC 4.0 license and therefore not allowed for commercial use. This project is for research only and is prohibited in clinical practice.
>
> **IMPORTANT LICENSE UPDATE**: Since DCCCSlicer V2.0, we no longer allow results generated using DCCCSlicer to be published in closed-access journals. If you use our software in your research, please consider publishing your results in an open-access journal or making them publicly available from the journal press, unless you obtain our commercial use license in advance.

<p align="center"><img src="./demo/dept_logo.png" style="width:15%;" /></p>
