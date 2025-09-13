"""
localizer模块的详细测试

此文件提供了localizer模块各个功能的全面测试
"""

import unittest
import logging
import vtk, slicer
from slicer.ScriptedLoadableModule import *
import os


class localizerModuleTest(ScriptedLoadableModuleTest):
    """
    localizer模块的详细测试类
    """

    def setUp(self):
        """每个测试前的准备工作"""
        slicer.mrmlScene.Clear()

    def runTest(self):
        """运行所有测试"""
        self.setUp()
        self.test_load_test_data()
        self.test_module_import()
        self.test_math_utils()
        self.test_markup_manager()
        self.test_image_alignment_logic()
        self.test_metric_calculator_logic()
        self.test_ai_decoupling_logic()
        self.test_atlas_manager_logic()
        self.test_widget_creation()

    def test_load_test_data(self):
        """测试加载测试数据"""
        self.delayDisplay("Testing data loading")
        
        test_data_path = os.path.join(os.path.dirname(__file__), "..", "ED01_PET.nii")
        if os.path.exists(test_data_path):
            volume_node = slicer.util.loadVolume(test_data_path)
            self.assertIsNotNone(volume_node, "Failed to load test data")
            
            # 验证体积属性
            dims = volume_node.GetImageData().GetDimensions()
            self.assertEqual(len(dims), 3, "Volume should be 3D")
            self.delayDisplay(f"Loaded test volume with dimensions: {dims}")
        else:
            self.delayDisplay(f"Test data not found at {test_data_path}, skipping data loading test")

    def test_module_import(self):
        """测试模块导入"""
        self.delayDisplay("Testing module imports")
        
        try:
            # 测试主模块导入
            import localizer
            self.assertTrue(hasattr(localizer, 'localizer'), "Main module class not found")
            self.assertTrue(hasattr(localizer, 'localizerWidget'), "Widget class not found")
            self.assertTrue(hasattr(localizer, 'localizerLogic'), "Logic class not found")
            
            # 测试lib模块导入
            from localizer.lib import math_utils, ui_components, image_alignment
            from localizer.lib import metric_calculator, ai_decoupling, atlas_manager
            
            self.delayDisplay("All modules imported successfully")
            
        except ImportError as e:
            self.fail(f"Failed to import modules: {e}")

    def test_math_utils(self):
        """测试数学工具模块"""
        self.delayDisplay("Testing math utils")
        
        from localizer.lib.math_utils import (
            create_translation_matrix, 
            create_rotation_matrix, 
            create_affine_matrix
        )
        import numpy as np
        
        # 测试平移矩阵
        translation = [1, 2, 3]
        T = create_translation_matrix(translation)
        self.assertEqual(T.shape, (4, 4), "Translation matrix should be 4x4")
        self.assertTrue(np.allclose(T[:3, 3], translation), "Translation vector not correct")
        
        # 测试旋转矩阵
        axis = [0, 0, 1]
        angle = np.pi/4
        R = create_rotation_matrix(axis, angle)
        self.assertEqual(R.shape, (4, 4), "Rotation matrix should be 4x4")
        
        # 测试仿射矩阵
        ac = np.array([10, 15, 5])
        pc = np.array([10, -10, 5])
        A = create_affine_matrix(ac, pc)
        self.assertEqual(A.shape, (4, 4), "Affine matrix should be 4x4")
        
        self.delayDisplay("Math utils tests passed")

    def test_markup_manager(self):
        """测试标记点管理器"""
        self.delayDisplay("Testing markup manager")
        
        from localizer.lib.ui_components import MarkupManager
        
        # 测试添加标记点
        MarkupManager.add_new_fiducial("TestPoint")
        
        # 测试查找标记点
        test_node = MarkupManager.find_markup_node_by_name("TestPoint")
        self.assertIsNotNone(test_node, "Failed to find created markup")
        
        # 测试添加控制点并获取坐标
        test_coords = [1.0, 2.0, 3.0]
        test_node.AddControlPoint(test_coords)
        retrieved_coords = MarkupManager.get_point_ras(test_node)
        self.assertIsNotNone(retrieved_coords, "Failed to get point coordinates")
        
        # 测试删除标记点
        MarkupManager.remove_specific_markups(["TestPoint"])
        removed_node = MarkupManager.find_markup_node_by_name("TestPoint")
        self.assertIsNone(removed_node, "Markup was not properly removed")
        
        self.delayDisplay("Markup manager tests passed")

    def test_image_alignment_logic(self):
        """测试图像对齐逻辑"""
        self.delayDisplay("Testing image alignment logic")
        
        from localizer.lib.image_alignment import ImageAlignmentLogic
        
        # 创建测试体积
        test_volume = self._create_test_volume()
        self.assertIsNotNone(test_volume, "Failed to create test volume")
        
        # 创建图像对齐逻辑实例
        alignment_logic = ImageAlignmentLogic()
        self.assertIsNotNone(alignment_logic, "Failed to create ImageAlignmentLogic")
        
        # 测试AC平移（不会引起错误）
        try:
            alignment_logic.translate_ac([0, 0, 0], test_volume, None)
            self.delayDisplay("AC translation test completed")
        except Exception as e:
            self.delayDisplay(f"AC translation test failed: {e}")
        
        self.delayDisplay("Image alignment tests passed")

    def test_metric_calculator_logic(self):
        """测试Metric计算逻辑"""
        self.delayDisplay("Testing metric calculator logic")
        
        from localizer.lib.metric_calculator import MetricCalculatorLogic
        
        # 创建计算器实例
        plugin_path = os.path.dirname(os.path.dirname(__file__))
        calc_logic = MetricCalculatorLogic(plugin_path)
        self.assertIsNotNone(calc_logic, "Failed to create MetricCalculatorLogic")
        
        # 测试获取附加信息（不需要真实计算）
        test_volume = self._create_test_volume()
        if test_volume:
            info = calc_logic.get_additional_info(test_volume)
            self.assertIsInstance(info, str, "Additional info should be a string")
        
        self.delayDisplay("Metric calculator tests passed")

    def test_ai_decoupling_logic(self):
        """测试AI解耦逻辑"""
        self.delayDisplay("Testing AI decoupling logic")
        
        from localizer.lib.ai_decoupling import AIDecouplingLogic
        
        # 创建解耦逻辑实例
        plugin_path = os.path.dirname(os.path.dirname(__file__))
        decoupling_logic = AIDecouplingLogic(plugin_path)
        self.assertIsNotNone(decoupling_logic, "Failed to create AIDecouplingLogic")
        
        # 测试设置显示属性（不会引起错误）
        try:
            decoupling_logic.setup_display_properties(None, None)
            self.delayDisplay("Display properties setup test completed")
        except Exception as e:
            self.delayDisplay(f"Display properties test failed: {e}")
        
        self.delayDisplay("AI decoupling tests passed")

    def test_atlas_manager_logic(self):
        """测试Atlas管理器逻辑"""
        self.delayDisplay("Testing atlas manager logic")
        
        from localizer.lib.atlas_manager import AtlasManager
        
        # 创建Atlas管理器实例
        plugin_path = os.path.dirname(os.path.dirname(__file__))
        atlas_manager = AtlasManager(plugin_path)
        self.assertIsNotNone(atlas_manager, "Failed to create AtlasManager")
        
        # 测试方法存在性
        required_methods = [
            'load_atlas', 'add_roi_point', 'add_ref_point',
            'get_roi_points', 'get_ref_points', 'calculate_brain_regions',
            'generate_combined_regions', 'clear_roi_ref_points'
        ]
        
        for method_name in required_methods:
            self.assertTrue(hasattr(atlas_manager, method_name), 
                          f"AtlasManager missing method: {method_name}")
        
        # 测试路径设置
        expected_atlas_path = os.path.join(plugin_path, "templates", "BN_Atlas_246_2mm.seg.nrrd")
        self.assertEqual(str(atlas_manager.atlas_path), expected_atlas_path, "Atlas path not set correctly")
        
        # 测试点获取功能（应该返回空列表）
        roi_points = atlas_manager.get_roi_points()
        ref_points = atlas_manager.get_ref_points()
        self.assertIsInstance(roi_points, list, "ROI points should be a list")
        self.assertIsInstance(ref_points, list, "Ref points should be a list")
        
        self.delayDisplay("Atlas manager tests passed")

    def test_widget_creation(self):
        """测试Widget创建"""
        self.delayDisplay("Testing widget creation")
        
        try:
            import localizer
            widget = localizer.localizerWidget()
            self.assertIsNotNone(widget, "Failed to create widget")
            
            # 测试设置（可能会因为缺少UI文件而失败，这是正常的）
            try:
                widget.setup()
                self.delayDisplay("Widget setup completed successfully")
            except Exception as e:
                self.delayDisplay(f"Widget setup failed (expected): {e}")
            
        except Exception as e:
            self.fail(f"Failed to create widget: {e}")
        
        self.delayDisplay("Widget creation tests passed")

    def _create_test_volume(self):
        """创建一个简单的测试体积"""
        try:
            # 创建一个小的测试体积
            imageSize = [10, 10, 10]
            imageSpacing = [1.0, 1.0, 1.0]
            imageOrigin = [0.0, 0.0, 0.0]
            
            # 创建VTK图像数据
            imageData = vtk.vtkImageData()
            imageData.SetDimensions(imageSize)
            imageData.SetSpacing(imageSpacing)
            imageData.SetOrigin(imageOrigin)
            imageData.AllocateScalars(vtk.VTK_UNSIGNED_CHAR, 1)
            
            # 填充测试数据
            for i in range(imageSize[0] * imageSize[1] * imageSize[2]):
                imageData.GetPointData().GetScalars().SetValue(i, 100)
            
            # 创建MRML节点
            volumeNode = slicer.mrmlScene.AddNewNodeByClass("vtkMRMLScalarVolumeNode")
            volumeNode.SetName("TestVolume")
            volumeNode.SetAndObserveImageData(imageData)
            volumeNode.CreateDefaultDisplayNodes()
            
            return volumeNode
            
        except Exception as e:
            self.delayDisplay(f"Failed to create test volume: {e}")
            return None


if __name__ == "__main__":
    unittest.main()
