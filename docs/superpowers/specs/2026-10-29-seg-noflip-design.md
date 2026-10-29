# Bound no-flip segmentation model

Approved in conversation on 2026-10-29. Replace the segmentation model with
`dccc_seg_noflip_fp16_20261029.tar.gz` from the host desktop. Its network uses
FP16 weight storage, FP32 arithmetic and external I/O, 96 classes, empty
mirroring axes, and exactly one unflipped prediction. No performance/accuracy
claim beyond the supplied validation is implied by storage precision.

## Approaches and decision

Only changing the default would still let callers misuse this no-flip-trained
model. The user explicitly rejected that approach. Remove the option entirely:
native Options/CLI, Slicer controls and adapters, dcccpy, and shipped reference
scripts must not accept TTA/flip selectors. Old Python keyword calls raise
TypeError and old CLI switches are unrecognized. Delete mirror inference code.
Reject bundles with nonempty mirroring axes or enabled default TTA. Retain
`tta: false` in receipts as an audit fact, not a configurable setting.

Keep all original-space restoration, class vocabularies, pons limitations,
non-overwrite guards and CPU deployment. Preserve the verified Gaussian and
FP16 accumulator protocol; FP16 weights are not FP16 external tensors.

## Delivery

Work on synchronized dev as before; all local compilation/tests/package checks
and uploads run in Docker. Publish core/Slicer `4.6.1-alpha`, dcccpy `0.3.0a5`,
four runtime packages `0.2.0a4`. Native/Slicer archives include the model; PyPI
wheels do not. Publish standalone model ZIP and checksum manifest on Release.
Update versioned downloads and model SHA-256, never overwrite the old release
or its cache. Trigger the five-platform workflow from dev and inspect slow
cloud builds no more frequently than every half hour unless asked.
