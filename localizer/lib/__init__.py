"""
localizer插件的辅助模块库

本包包含localizer 3D Slicer插件的各个功能模块：
- math_utils: 数学工具函数
- image_alignment: 图像对齐功能
- centiloid_calculator: Centiloid计算功能  
- ai_decoupling: AI解耦功能
- ui_components: UI组件
"""

from .math_utils import (
    create_translation_matrix,
    create_rotation_matrix,
    create_affine_matrix
)

from .image_alignment import ImageAlignmentLogic
from .metric_calculator import MetricCalculatorLogic    
from .ai_decoupling import AIDecouplingLogic
from .atlas_manager import AtlasManager
from .ui_components import TimeConsumingMessageBox

__all__ = [
    'create_translation_matrix',
    'create_rotation_matrix', 
    'create_affine_matrix',
    'ImageAlignmentLogic',
    'MetricCalculatorLogic',
    'AIDecouplingLogic',
    'AtlasManager',
    'TimeConsumingMessageBox'
]
