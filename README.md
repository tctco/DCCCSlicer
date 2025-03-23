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

PS: You can also quickly align brain images with rigid transformation to the MNI space by localizing AC and PC. This step may be required for spatial normalization with [SPM](https://github.com/spm/spm12)/[rPOP](https://github.com/LeoIacca/rPOP/tree/master).

<https://github.com/tctco/ACPCLocalizer/assets/45505657/c34980e6-c214-4200-9146-aac951fb7f4f>

## Metrics

Relative SUV (ratio) error and time consumption on the Centiloid/CenTauRz projects.

$$
\Delta \mathrm{SUV} \ \\% = \frac{|\mathrm{SUV_{GT} - \mathrm{SUV_{Method}}|}}{\mathrm{SUV_{GT}}}\times 100\\%
$$


| **Methods**        | **PiB (%)**   | **AV45 (%)**  | **FBB (%)**   | **FMM (%)**   | **NAV4694 (%)** | **FTP (%)**   | **Time (s)**  |
| ------------------ | ------------- | ------------- | ------------- | ------------- | --------------- | ------------- | ------------- |
| SPM12              | 1.33±1.07     | 1.66±1.32     | 1.40±0.85     | 3.00±2.84     | 1.90±2.77       | 1.07±1.27     | 198.96±59.37  |
| SPM PET            | 8.14±11.71    | 15.75±36.98   | 14.18±11.13   | 8.85±5.89     | 16.43±9.60      | 12.40±11.39   | 5.77±1.68     |
| SPM PET (Template) | 3.97±2.85     | 6.44±3.83     | 6.16±3.27     | 8.93±3.38     | 5.43±3.27       | 3.65±2.78     | 4.47±0.88     |
| SNBPI              | **1.68±1.49** | **2.24±1.97** | 2.85±2.33     | **2.74±2.27** | 3.15±2.66       | <ins>1.41±1.13</ins>     | 130.33±41.27  |
| Ours (Pytorch)     | 2.40±1.55     | <ins>2.67±2.22</ins>     | **2.56±2.23** | 3.68±3.40     | **2.59±2.13**   | 2.07±4.31     | **1.07±0.63** |
| Ours (Iterative)   | 2.45±1.69     | 3.61±3.92     | 2.68±2.22     | 3.68±3.40     | <ins>2.62±2.09</ins>       | **1.40±1.35** | <ins>1.53±1.01</ins>     |
| Ours (ONNX)        | <ins>2.29±1.50</ins>     | 3.11±2.15     | <ins>2.64±2.06</ins>     | <ins>3.51±3.31</ins>     | 2.63±2.15       | 1.44±1.26     | 15.85±2.30    |

> The smallest error and shortest computation time are marked with **bold**, while the second smallest error and shortest computation time are <ins>underlined</ins>. SPM12 is employed to reproduce the original literature results using the standard pipeline and does not participate in result ranking; it provides a baseline for reproducibility. SPM PET refers to the PET-only spatial normalization algorithm provided by SPM5, utilizing the 15O-H2O template. SPM PET (Template) refers to the same algorithm, but the templates used are the average of each tracer in the Centiloid/CenTauR dataset after normalization. Check [SNBPI](https://github.com/ZhangTianhao1993/Spatial-Normalization-of-Brain-PET-Images) for their wonderful work!

## Acknowledgements

We thank Prof. Yulai Li from Xiangya Hospital for kindly providing the SV2A dataset.

## License

> This project is open-sourced under a CC-BY-NC 4.0 license and therefore not allowed for commercial use. This project is for research only and is prohibited in clinical practice.
>
> **IMPORTANT LICENSE UPDATE**: Since DCCCSlicer V2.0, we no longer allow results generated using DCCCSlicer to be published in closed-access journals. If you use our software in your research, please consider publishing your results in an open-access journal or making them publicly available from the journal press, unless you obtain our commercial use license in advance.

<p align="center"><img src="./demo/dept_logo.png" style="width:15%;" /></p>
