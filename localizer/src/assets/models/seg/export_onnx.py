"""Export Dataset618 fold-0 final network and portable inference controls."""
import argparse
import ast
import csv
import json
import logging
import os
from pathlib import Path
import shutil
import time

import numpy as np
import torch

LOG = logging.getLogger('export')


def read(path):
    return json.loads(Path(path).read_text())


def write(path, value):
    path.write_text(json.dumps(value, indent=2)+'\n')


def export(train_root, output, neuro_csv, threads=4):
    os.environ['nnUNet_compile'] = 'false'
    torch.set_num_threads(threads)
    from nnunetv2.inference.predict_from_raw_data import nnUNetPredictor
    import onnx
    import onnxruntime as ort
    model = train_root/'nnunet_results/Dataset618_DCCC_FineDKT_1p5mm_Fold0/nnUNetTrainer__nnUNetPlans__3d_fullres'
    output.mkdir(parents=True, exist_ok=False, mode=0o700)
    predictor = nnUNetPredictor(device=torch.device('cpu'), perform_everything_on_device=False)
    predictor.initialize_from_trained_model_folder(str(model), use_folds=(0,), checkpoint_name='checkpoint_final.pth')
    network = predictor.network.eval().float()
    network.load_state_dict(predictor.list_of_parameters[0], strict=True)
    ontology = read(train_root/'label_ontology.json')
    if predictor.label_manager.num_segmentation_heads != 96 or ontology['num_output_channels'] != 96:
        raise RuntimeError('Expected final fine96 checkpoint')
    patch = list(predictor.configuration_manager.patch_size)
    LOG.info('Exporting FP32 network, prescribed inference patch %s', patch)
    # Small tracing input keeps export cheap. All spatial dimensions are dynamic;
    # runtime uses the actual prescribed patch, never a resized PET volume.
    rng = np.random.default_rng(618)
    probe = rng.normal(size=(1, 1, 32, 64, 32)).astype(np.float32)
    with torch.inference_mode():
        torch.onnx.export(network, torch.from_numpy(probe), str(output/'dccc_seg.onnx'),
                          input_names=['image'], output_names=['logits'], opset_version=17,
                          dynamo=False, dynamic_axes={'image': {2:'d',3:'h',4:'w'},
                                                     'logits': {2:'d',3:'h',4:'w'}})
    onnx.checker.check_model(str(output/'dccc_seg.onnx'))
    options = ort.SessionOptions(); options.intra_op_num_threads=threads; options.inter_op_num_threads=1
    session = ort.InferenceSession(str(output/'dccc_seg.onnx'), sess_options=options, providers=['CPUExecutionProvider'])
    checks=[]
    for shape in [(32,64,32), (64,32,64)]:
        x=rng.normal(size=(1,1,*shape)).astype(np.float32)
        with torch.inference_mode(): reference=network(torch.from_numpy(x)).numpy()
        actual=session.run(['logits'],{'image':x})[0]
        difference=np.abs(reference-actual)
        check={'shape':list(x.shape),'max_absolute_error':float(difference.max()),
               'mean_absolute_error':float(difference.mean()),
               'argmax_agreement':float(np.mean(reference.argmax(1)==actual.argmax(1))),
               'allclose_rtol_1e-3_atol_1e-4':bool(np.allclose(reference,actual,rtol=1e-3,atol=1e-4))}
        checks.append(check);LOG.info('Numerical check: %s',check)
        if not check['allclose_rtol_1e-3_atol_1e-4']:
            write(output/'validation.json',{'status':'failed','checks':checks})
            raise RuntimeError('ONNX numerical validation failed')
    for source,target in [('plans.json','plans.json'),('dataset.json','dataset.json')]:
        shutil.copyfile(model/source, output/target)
    # Only ontology content is shipped; private teacher paths are unnecessary.
    write(output/'labels.json',{k:ontology[k] for k in ['schema','raw_ids','raw_to_training','training_to_raw','labels','num_output_channels']})
    grid=read(train_root/'grid_control.json')
    write(output/'grid.json',{k:grid[k] for k in ['schema','shape','affine','spacing_mm','PET_sampling']})
    rows=list(csv.DictReader(neuro_csv.open()))
    lookup={};regions=[]
    for row in rows:
        target=int(row['Label']); sources=[int(x) for x in ast.literal_eval(row['FastSurferLabels'] or '[]')]
        # Official CSV equates whole brainstem 16 with brainstem-without-pons.
        # Refuse this anatomical mismatch: neither 45 nor 46 is synthesized.
        supported=[x for x in sources if x in ontology['raw_ids'] and x != 16]
        for source in supported:
            if str(source) in lookup: raise ValueError('Overlapping NeuroSegmenter labels')
            lookup[str(source)]=target
        regions.append({'id':target,'name':row['AnatomicalStruct'],'raw_ids':supported,
                        'official_raw_ids':sources,'available':bool(supported)})
    write(output/'neurosegmenter_mapping.json',{'raw_to_neuro':lookup,'regions':regions,
        'source':'FDG-NeuroSegmenter/src/fdg_neurosegmenter/data/label_correspondence.csv',
        'limitations':['Partial anatomical label compatibility; not NeuroSegmenter model inference.',
                       'Whole brainstem (raw 16) is unmapped (0); labels 45 and 46 are unavailable.',
                       'White matter, ventricles, CSF, and other unmapped structures become 0.',
                       'Pons-based SUVR normalization is unsupported by this output.']})
    checkpoint=model/'fold_0/checkpoint_final.pth'; stat=checkpoint.stat()
    write(output/'manifest.json',{'schema':'dccc_seg_onnx_v1','dataset':model.parent.name,
        'fold':0,'checkpoint':'checkpoint_final.pth','checkpoint_size':stat.st_size,
        'checkpoint_mtime_ns':stat.st_mtime_ns,'trainer_name':predictor.trainer_name,
        'mirroring_axes':list(predictor.allowed_mirroring_axes or []),
        'input':{'name':'image','dtype':'float32','layout':'NCDHW (official nnU-Net preprocessed axes)',
                 'inference_shape':[1,1,*patch]},
        'output':{'name':'logits','dtype':'float32','layout':'NCDHW','channels':96},
        'preprocessing':'two iterative rigid iterations, materialized 1.5mm training grid, official nnU-Net',
        'opset':17,'torch_version':torch.__version__,'onnx_version':onnx.__version__,
        'onnxruntime_version':ort.__version__,'export_unix':time.time()})
    write(output/'validation.json',{'status':'passed_network_probes','checks':checks,
                                   'full_volume_validation':'Not established by network probes; run a representative native pilot.'})
    for name in ['dccc_seg.py','export_onnx.py','test_bundle.py','validate_pilot.py','README.md','QUICKSTART.md']:
        shutil.copyfile(Path(__file__).with_name(name),output/name)
    LOG.info('Exported portable bundle: %s',output)


if __name__ == '__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--train-root',required=True,type=Path)
    parser.add_argument('--output',required=True,type=Path)
    parser.add_argument('--neuro-csv',required=True,type=Path)
    parser.add_argument('--threads',type=int,default=4)
    args=parser.parse_args()
    logging.basicConfig(level=logging.INFO,format='%(asctime)s %(levelname)s %(message)s')
    os.umask(0o077)
    export(args.train_root,args.output,args.neuro_csv,args.threads)
