"""
图像对齐功能模块

包含手动影像摆正功能，支持AC/PC标记点对齐
"""

import logging
import numpy as np
import vtk
import slicer
from .math_utils import create_translation_matrix, create_affine_matrix, create_rotation_matrix


class ImageAlignmentLogic:
    """图像对齐逻辑类"""
    
    def __init__(self):
        self.transform_table = {}  # volumeNodeID: transformNode
    
    def translate_ac(self, ac_coord, target_node, markup_node):
        """仅根据AC点进行平移对齐
        
        Args:
            ac_coord: AC点坐标
            target_node: 目标体积节点
            markup_node: 标记点节点
        """
        if target_node is None:
            logging.error("translate_ac: Invalid input node")
            return
            
        if self.transform_table.get(target_node.GetID()):
            transform_node = self.transform_table[target_node.GetID()]
            existing_matrix = vtk.vtkMatrix4x4()
        else:
            transform_node = slicer.vtkMRMLLinearTransformNode()
            slicer.mrmlScene.AddNode(transform_node)
            self.transform_table[target_node.GetID()] = transform_node
            if target_node.GetName():
                transform_node_name = target_node.GetName() + "_Transform"
                transform_node.SetName(transform_node_name)
            existing_matrix = vtk.vtkMatrix4x4()
            
        affine_matrix = create_translation_matrix(-np.array(ac_coord))
        vtk_new_matrix = slicer.util.vtkMatrixFromArray(affine_matrix)
        composite_matrix = vtk.vtkMatrix4x4()
        vtk.vtkMatrix4x4.Multiply4x4(vtk_new_matrix, existing_matrix, composite_matrix)
        transform_node.SetMatrixTransformToParent(composite_matrix)
        target_node.SetAndObserveTransformNodeID(transform_node.GetID())
        slicer.vtkSlicerTransformLogic().hardenTransform(target_node)
        
        if markup_node is not None:
            markup_node.SetAndObserveTransformNodeID(transform_node.GetID())
            slicer.vtkSlicerTransformLogic().hardenTransform(markup_node)

    def transform_acpc(self, ac_coord, pc_coord, target_node, markup_nodes):
        """根据AC/PC点进行完整的仿射变换对齐
        
        Args:
            ac_coord: AC点坐标
            pc_coord: PC点坐标
            target_node: 目标体积节点
            markup_nodes: 相关标记点节点列表
        """
        if target_node is None:
            logging.error("transform_acpc: Invalid input node")
            return
            
        print(f"AC: {ac_coord}, PC: {pc_coord}")
        
        if self.transform_table.get(target_node.GetID()):
            transform_node = self.transform_table[target_node.GetID()]
            existing_matrix = vtk.vtkMatrix4x4()
        else:
            transform_node = slicer.vtkMRMLLinearTransformNode()
            slicer.mrmlScene.AddNode(transform_node)
            self.transform_table[target_node.GetID()] = transform_node
            if target_node.GetName():
                transform_node_name = target_node.GetName() + "_Transform"
                transform_node.SetName(transform_node_name)
            existing_matrix = vtk.vtkMatrix4x4()  # 默认为单位矩阵

        affine_matrix = create_affine_matrix(np.array(ac_coord), np.array(pc_coord))
        # 将NumPy矩阵转换为VTK矩阵
        vtk_new_matrix = slicer.util.vtkMatrixFromArray(affine_matrix)
        composite_matrix = vtk.vtkMatrix4x4()
        vtk.vtkMatrix4x4.Multiply4x4(vtk_new_matrix, existing_matrix, composite_matrix)
        transform_node.SetMatrixTransformToParent(composite_matrix)

        target_node.SetAndObserveTransformNodeID(transform_node.GetID())
        slicer.vtkSlicerTransformLogic().hardenTransform(target_node)
        
        for node in markup_nodes:
            if node is None:
                continue
            node.SetAndObserveTransformNodeID(transform_node.GetID())
            slicer.vtkSlicerTransformLogic().hardenTransform(node)

    def transform_lr(self, left_coord, right_coord, target_node, markup_nodes):
        """根据左右标记点进行左右对齐
        
        Args:
            left_coord: 左侧标记点坐标
            right_coord: 右侧标记点坐标
            target_node: 目标体积节点
            markup_nodes: 相关标记点节点列表
        """
        if self.transform_table.get(target_node.GetID()):
            transform_node = self.transform_table[target_node.GetID()]
            existing_matrix = vtk.vtkMatrix4x4()
        else:
            transform_node = slicer.vtkMRMLLinearTransformNode()
            slicer.mrmlScene.AddNode(transform_node)
            self.transform_table[target_node.GetID()] = transform_node
            if target_node.GetName():
                transform_node_name = target_node.GetName() + "_Transform"
                transform_node.SetName(transform_node_name)
            existing_matrix = vtk.vtkMatrix4x4()  # 默认为单位矩阵

        direction = np.array(right_coord) - np.array(left_coord)
        normalised_direction = direction / np.linalg.norm(direction)
        x_axis = np.array([-1, 0, 0])
        axis = np.cross(normalised_direction, x_axis)
        
        if np.linalg.norm(axis) != 0:  # 需要旋转
            axis_normalized = axis / np.linalg.norm(axis)
            angle = np.arccos(np.dot(normalised_direction, x_axis))
        else:
            axis_normalized = np.array([0, 0, 1])  # 任意轴，因为不需要旋转
            angle = 0
            
        rotation_matrix = create_rotation_matrix(axis_normalized, angle)
        vtk_new_matrix = slicer.util.vtkMatrixFromArray(rotation_matrix)
        composite_matrix = vtk.vtkMatrix4x4()
        vtk.vtkMatrix4x4.Multiply4x4(vtk_new_matrix, existing_matrix, composite_matrix)
        transform_node.SetMatrixTransformToParent(composite_matrix)

        target_node.SetAndObserveTransformNodeID(transform_node.GetID())
        slicer.vtkSlicerTransformLogic().hardenTransform(target_node)
        
        for node in markup_nodes:
            if node is None:
                continue
            node.SetAndObserveTransformNodeID(transform_node.GetID())
            slicer.vtkSlicerTransformLogic().hardenTransform(node)
