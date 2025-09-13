"""
Atlas管理模块

包含脑Atlas ROI/Ref点管理和脑区计算功能
支持动态选择不同的Atlas文件（AAL、BN_Atlas_246_2mm等）
"""

import os
from pathlib import Path
import numpy as np
import vtk
import slicer

from lib.ui_components import MarkupManager


class AtlasManager:
    """Atlas管理器类，用于处理脑Atlas相关的功能"""

    # Available atlas configurations
    AVAILABLE_ATLASES = {
        "AAL.seg.nrrd": "AAL.seg.nrrd",
        "BN_Atlas_246_2mm.seg.nrrd": "BN_Atlas_246_2mm.seg.nrrd"
    }

    def __init__(self, plugin_path):
        """初始化Atlas管理器

        Args:
            plugin_path: 插件路径
        """
        self.plugin_path = Path(plugin_path)
        self.templates_path = self.plugin_path / "templates"
        self.current_atlas_name = None
        self.atlas_path = None
        self.atlas_node = None
        self.atlas_node_arr = None
        self.atlas_labels = {}

    def set_atlas(self, atlas_name):
        """设置当前要使用的Atlas
        
        Args:
            atlas_name: Atlas名称，必须在AVAILABLE_ATLASES中
            
        Returns:
            bool: 设置是否成功
        """
        if atlas_name not in self.AVAILABLE_ATLASES:
            slicer.util.errorDisplay(f"Unknown atlas: {atlas_name}")
            return False
            
        self.current_atlas_name = atlas_name
        atlas_filename = self.AVAILABLE_ATLASES[atlas_name]
        self.atlas_path = self.templates_path / atlas_filename
        
        # 如果切换Atlas，需要清理之前的数据
        if self.atlas_node:
            self.clear_current_atlas()
            
        return True
    
    def clear_current_atlas(self):
        """清理当前Atlas数据"""
        self.atlas_node = None
        self.atlas_node_arr = None
        self.atlas_labels = {}
    
    def get_available_atlases(self):
        """获取可用的Atlas列表
        
        Returns:
            list: 可用Atlas名称列表
        """
        return list(self.AVAILABLE_ATLASES.keys())
    
    def get_current_atlas_name(self):
        """获取当前Atlas名称
        
        Returns:
            str: 当前Atlas名称，如果未设置则返回None
        """
        return self.current_atlas_name

    def load_atlas(self, atlas_name=None):
        """加载Atlas文件
        
        Args:
            atlas_name: 可选的Atlas名称，如果提供则先设置Atlas
            
        Returns:
            Atlas节点或None
        """
        # 如果提供了atlas_name，先设置Atlas
        if atlas_name:
            if not self.set_atlas(atlas_name):
                return None
        
        # 检查是否已经设置了Atlas
        if not self.current_atlas_name or not self.atlas_path:
            slicer.util.errorDisplay("No atlas selected. Please select an atlas first.")
            return None
            
        if not self.atlas_path.exists():
            slicer.util.errorDisplay(f"Atlas file not found: {self.atlas_path}")
            return None

        # 检查是否已经加载了当前Atlas
        existing_atlas = slicer.util.getFirstNodeByName(self.current_atlas_name)
        if existing_atlas:
            self.atlas_node = existing_atlas
            # 重新加载体积数据用于计算
            self._load_atlas_volume_data()
            return self.atlas_node

        # 加载分割节点用于显示
        self.atlas_node = slicer.util.loadSegmentation(str(self.atlas_path))
        
        # 加载体积节点用于数据处理
        self._load_atlas_volume_data()
        
        if self.atlas_node:
            self.atlas_node.SetName(self.current_atlas_name)
            # 设置显示属性
            self._setup_atlas_display()
            print(f"Successfully loaded atlas: {self.current_atlas_name}")
            return self.atlas_node
        else:
            slicer.util.errorDisplay(f"Failed to load atlas file: {self.atlas_path}")
            return None
    
    def _load_atlas_volume_data(self):
        """加载Atlas体积数据用于计算"""
        temp_node = slicer.util.loadVolume(
            str(self.atlas_path), properties={"show": False}
        )
        self.ras_to_ijk = vtk.vtkMatrix4x4()
        temp_node.GetRASToIJKMatrix(self.ras_to_ijk)
        self.atlas_node_arr = slicer.util.arrayFromVolume(temp_node)
        slicer.mrmlScene.RemoveNode(temp_node)

    def _setup_atlas_display(self):
        """设置Atlas显示属性"""
        if not self.atlas_node:
            return

        display_node = self.atlas_node.GetDisplayNode()
        if display_node:
            # 设置为半透明显示
            display_node.SetOpacity(0.3)
            display_node.SetVisibility(True)

    def get_roi_points(self):
        """获取所有ROI点"""
        return self._get_points_by_prefix("ROI_")

    def get_ref_points(self):
        """获取所有Ref点"""
        return self._get_points_by_prefix("Ref_")

    def _get_points_by_prefix(self, prefix):
        """根据前缀获取标记点"""
        points = []
        num_nodes = slicer.mrmlScene.GetNumberOfNodesByClass(
            "vtkMRMLMarkupsFiducialNode"
        )

        for i in range(num_nodes):
            node = slicer.mrmlScene.GetNthNodeByClass(i, "vtkMRMLMarkupsFiducialNode")
            if node and node.GetName().startswith(prefix):
                for j in range(node.GetNumberOfControlPoints()):
                    points.append(
                        {
                            "name": f"{node.GetName()}_{j}",
                            "coordinates": MarkupManager.get_point_ras(node),
                            "node": node,
                        }
                    )

        return points

    def add_roi_point(self):
        """添加ROI点"""
        roi_points = self.get_roi_points()
        point_index = len(roi_points) + 1
        point_name = f"ROI_{point_index:02d}"

        self._add_fiducial_point(point_name)

    def add_ref_point(self):
        """添加Ref点"""
        ref_points = self.get_ref_points()
        point_index = len(ref_points) + 1
        point_name = f"Ref_{point_index:02d}"

        self._add_fiducial_point(point_name)

    def _add_fiducial_point(self, name):
        """添加标记点"""
        markup_node = slicer.mrmlScene.AddNewNodeByClass("vtkMRMLMarkupsFiducialNode")
        markup_node.SetName(name)
        slicer.modules.markups.logic().SetActiveListID(markup_node)

        # 设置显示属性
        display_node = markup_node.GetDisplayNode()
        if display_node:
            if name.startswith("ROI_"):
                display_node.SetColor(1.0, 0.0, 0.0)  # 红色
            elif name.startswith("Ref_"):
                display_node.SetColor(0.0, 0.0, 1.0)  # 蓝色

        # 自动进入放置模式
        interaction_node = slicer.mrmlScene.GetNodeByID(
            "vtkMRMLInteractionNodeSingleton"
        )
        interaction_node.SetCurrentInteractionMode(interaction_node.Place)

    def calculate_brain_regions(self):
        """计算ROI和Ref点命中的脑区"""
        if not self.atlas_node or not self.current_atlas_name:
            slicer.util.errorDisplay("Atlas not loaded. Please load atlas first.")
            return None, None

        roi_points = self.get_roi_points()
        ref_points = self.get_ref_points()

        if not roi_points and not ref_points:
            slicer.util.warningDisplay("No ROI or Ref points found.")
            return [], []

        roi_regions = self._get_regions_for_points(roi_points, "ROI")
        ref_regions = self._get_regions_for_points(ref_points, "Ref")

        return roi_regions, ref_regions

    def _get_regions_for_points(self, points, point_type):
        """为给定的点获取对应的脑区"""
        if not points:
            return []

        regions = []

        for point in points:
            # 将RAS坐标转换为体素坐标
            ras_point = point["coordinates"]
            voxel_coords = self._ras_to_voxel(ras_point)
            point["voxel_coords"] = voxel_coords

            if voxel_coords is not None:
                # 获取该体素的标签值
                label_value = self._get_label_at_voxel(voxel_coords)

                if label_value > 0:  # 非背景区域
                    region_info = {
                        "point_name": point["name"],
                        "coordinates": ras_point,
                        "voxel_coords": voxel_coords,
                        "label_value": int(label_value),
                    }
                    regions.append(region_info)

        return regions

    def _ras_to_voxel(self, ras_coords):
        """将RAS坐标转换为体素坐标"""
        # 获取Atlas的图像数据
        image_data = self.atlas_node_arr
        # 转换坐标
        ras_point = [ras_coords[0], ras_coords[1], ras_coords[2], 1.0]
        ijk_point = [0.0, 0.0, 0.0, 0.0]
        self.ras_to_ijk.MultiplyPoint(ras_point, ijk_point)

        # 转换为整数体素坐标
        voxel_coords = [
            int(round(ijk_point[0])),
            int(round(ijk_point[1])),
            int(round(ijk_point[2])),
        ]

        # 检查坐标是否在图像范围内
        dims = image_data.shape
        if (
            0 <= voxel_coords[0] < dims[0]
            and 0 <= voxel_coords[1] < dims[1]
            and 0 <= voxel_coords[2] < dims[2]
        ):
            return voxel_coords
        else:
            return None

    def _get_label_at_voxel(self, voxel_coords):
        """获取指定体素位置的标签值"""
        image_data = self.atlas_node_arr

        # 获取体素值
        label_value = image_data[
            voxel_coords[2], voxel_coords[1], voxel_coords[0]
        ]

        return label_value

    def clear_roi_ref_points(self):
        """清除所有ROI和Ref点"""
        from .ui_components import MarkupManager

        # 获取所有ROI和Ref节点名称
        roi_ref_names = []
        num_nodes = slicer.mrmlScene.GetNumberOfNodesByClass(
            "vtkMRMLMarkupsFiducialNode"
        )

        for i in range(num_nodes):
            node = slicer.mrmlScene.GetNthNodeByClass(i, "vtkMRMLMarkupsFiducialNode")
            if node and (
                node.GetName().startswith("ROI_") or node.GetName().startswith("Ref_")
            ):
                roi_ref_names.append(node.GetName())

        if roi_ref_names:
            MarkupManager.remove_specific_markups(roi_ref_names)

    def generate_combined_regions(self):
        """生成ROI和Ref的联合脑区Volume并载入"""
        roi_regions, ref_regions = self.calculate_brain_regions()

        if not roi_regions and not ref_regions:
            slicer.util.warningDisplay("No valid regions found.")
            return

        # 生成联合脑区Volume
        roi_volume, ref_volume = self._create_combined_volumes(roi_regions, ref_regions)

        return roi_regions, ref_regions, roi_volume, ref_volume

    def _create_combined_volumes(self, roi_regions, ref_regions):
        """创建ROI和Ref的联合脑区Volume"""
        roi_volume = None
        ref_volume = None
        
        if roi_regions:
            roi_labels = set(region["label_value"] for region in roi_regions)
            roi_volume_name = f"ROI_Combined_{self.current_atlas_name}" if self.current_atlas_name else "ROI_Combined"
            roi_volume = self._create_volume_from_labels(roi_labels, roi_volume_name)
            
        if ref_regions:
            ref_labels = set(region["label_value"] for region in ref_regions)
            ref_volume_name = f"Ref_Combined_{self.current_atlas_name}" if self.current_atlas_name else "Ref_Combined"
            ref_volume = self._create_volume_from_labels(ref_labels, ref_volume_name)
            
        return roi_volume, ref_volume
    
    def _create_volume_from_labels(self, labels, volume_name):
        """根据标签创建新的Volume"""
        if not labels:
            return None
            
        # 创建新的图像数据
        combined_array = np.zeros_like(self.atlas_node_arr)
        
        # 将指定标签的区域标记为1
        for label in labels:
            combined_array[self.atlas_node_arr == label] = 1
        
        # 创建MRML体积节点
        volume_node = slicer.mrmlScene.AddNewNodeByClass("vtkMRMLScalarVolumeNode")
        volume_node.SetName(volume_name)
        
        # 使用slicer.util.updateVolumeFromArray写入体素数据
        slicer.util.updateVolumeFromArray(volume_node, combined_array)
        
        # 获取Atlas的几何信息并应用
        atlas_volume_node = self._get_atlas_volume_node()
        if atlas_volume_node:
            volume_node.CopyOrientation(atlas_volume_node)
            volume_node.SetSpacing(atlas_volume_node.GetSpacing())
            volume_node.SetOrigin(atlas_volume_node.GetOrigin())
            
            # 复制变换矩阵
            ijk_to_ras = vtk.vtkMatrix4x4()
            atlas_volume_node.GetIJKToRASMatrix(ijk_to_ras)
            volume_node.SetIJKToRASMatrix(ijk_to_ras)
            
            # 清理临时节点
            slicer.mrmlScene.RemoveNode(atlas_volume_node)
        
        # 设置显示属性
        display_node = volume_node.GetDisplayNode()
        if display_node is None:
            volume_node.CreateDefaultDisplayNodes()
            display_node = volume_node.GetDisplayNode()
            
        if display_node:
            if "ROI" in volume_name:
                display_node.SetAndObserveColorNodeID("vtkMRMLColorTableNodeRed")
            else:
                display_node.SetAndObserveColorNodeID("vtkMRMLColorTableNodeBlue")
            display_node.SetOpacity(0.5)
            display_node.SetVisibility(True)
        
        return volume_node
    
    def _get_atlas_volume_node(self):
        """获取Atlas的Volume节点用于几何信息"""
        # 创建临时Volume节点来获取几何信息
        temp_volume_node = slicer.util.loadVolume(
            str(self.atlas_path), properties={"show": False}
        )
        return temp_volume_node
