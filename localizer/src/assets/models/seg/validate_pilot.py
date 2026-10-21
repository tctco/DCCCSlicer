"""Run a private real-input CPU parity pilot and generate geometry QC."""
import argparse
import importlib.util
import json
import logging
from pathlib import Path
import os
import time
import numpy as np
import nibabel as nib
import torch


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--bundle',type=Path,required=True)
    p.add_argument('--source',type=Path,required=True,help='Private JSON with native/rigid path records')
    p.add_argument('--train-root',type=Path,required=True)
    p.add_argument('--output',type=Path,required=True)
    p.add_argument('--tta',action='store_true',help='Validate default full mirror TTA instead of fast no-TTA')
    args=p.parse_args(); os.umask(0o077);torch.set_num_threads(4)
    logging.basicConfig(level=logging.INFO,format='%(asctime)s %(levelname)s %(message)s')
    spec=importlib.util.spec_from_file_location('dccc_seg',Path(__file__).with_name('dccc_seg.py'))
    m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
    source=json.loads(args.source.read_text()); root=args.output;root.mkdir(mode=0o700,exist_ok=False)
    output=root/'prediction_native_raw.nii.gz'
    started=time.time()
    result=m.segment(source['native']['path'],output,args.bundle,root/'work',
                     rigid_header=source['rigid']['path'],tta=args.tta)
    from nnunetv2.inference.predict_from_raw_data import nnUNetPredictor
    from nnunetv2.preprocessing.preprocessors.default_preprocessor import DefaultPreprocessor
    from nnunetv2.inference.export_prediction import convert_predicted_logits_to_segmentation_with_correct_shape
    reference=nnUNetPredictor(device=torch.device('cpu'),perform_everything_on_device=False,
                             use_mirroring=args.tta,tile_step_size=.5,use_gaussian=True,verbose=False)
    folder=args.train_root/'nnunet_results/Dataset618_DCCC_FineDKT_1p5mm_Fold0/nnUNetTrainer__nnUNetPlans__3d_fullres'
    reference.initialize_from_trained_model_folder(str(folder),use_folds=(0,),checkpoint_name='checkpoint_final.pth')
    data,_,properties=DefaultPreprocessor(verbose=False).run_case([str(root/'work/pet_task_0000.nii.gz')],None,
                       reference.plans_manager,reference.configuration_manager,reference.dataset_json)
    logits=reference.predict_logits_from_preprocessed_data(torch.from_numpy(data))
    labels=convert_predicted_logits_to_segmentation_with_correct_shape(logits,reference.plans_manager,
           reference.configuration_manager,reference.label_manager,properties,num_threads_torch=4)
    task_path=root/'pytorch_training_ids_task.nii.gz'
    reference.plans_manager.image_reader_writer_class().write_seg(labels,str(task_path),properties)
    native=nib.load(source['native']['path']);rigid=nib.load(source['rigid']['path'])
    restored=m.restore_native(nib.load(task_path),native,rigid.affine)
    raw=m.map_labels(np.asarray(restored.dataobj),m.read(args.bundle/'labels.json'))
    nib.save(nib.Nifti1Image(raw,native.affine),root/'pytorch_native_raw.nii.gz')
    actual=np.asarray(result.dataobj);mask=(raw>0)|(actual>0)
    stats={'status':'complete','case_id':source['case_id'],'tta':args.tta,'provider':'CPUExecutionProvider',
           'native_shape_matches':result.shape==native.shape,'native_affine_matches':bool(np.allclose(result.affine,native.affine)),
           'native_voxel_agreement':float(np.mean(actual==raw)),
           'foreground_union_voxel_agreement':float(np.mean(actual[mask]==raw[mask])),
           'disagreement_voxels':int(np.count_nonzero(actual!=raw)),
           'seconds':time.time()-started,'full_tta_pilot_verified':args.tta}
    (root/'validation.json').write_text(json.dumps(stats,indent=2)+'\n');print(stats,flush=True)
    import matplotlib
    matplotlib.use('Agg')
    import matplotlib.pyplot as plt
    import seaborn as sns
    plt.rcParams.update({'font.family':'Liberation Sans','pdf.fonttype':42,'ps.fonttype':42});sns.set_context('paper')
    task=np.asarray(nib.load(root/'work/pet_task_0000.nii.gz').dataobj)
    native_data=np.asarray(native.dataobj)
    fig,axes=plt.subplots(3,3,figsize=(5.8,5.1))
    for axis in range(3):
        for row,vol in enumerate([data[0],task,native_data]):
            take=vol.shape[axis]//2;image=np.rot90(np.take(vol,take,axis))
            ax=axes[row,axis];ax.imshow(image,cmap='gray',vmax=np.percentile(vol,99))
            if row==2:
                seg=np.rot90(np.take(actual,take,axis));ax.imshow(np.ma.masked_where(seg==0,seg),cmap='gist_ncar',vmin=0,vmax=2035,alpha=.55)
            ax.set_title(['Official normalized input','Materialized task PET','Native restored labels'][row]+f' / {axis}',fontsize=7)
            ax.set_xticks([]);ax.set_yticks([]);ax.grid(False);sns.despine(ax=ax,left=True,bottom=True)
    fig.tight_layout();fig.savefig(root/'pilot_qc.png',dpi=180);fig.savefig(root/'pilot_qc.pdf');plt.close(fig)


if __name__=='__main__':main()
