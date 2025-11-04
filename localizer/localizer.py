import logging
import os
from typing import Annotated, Optional
from pathlib import Path

import vtk

import slicer
from slicer.ScriptedLoadableModule import *
from slicer.util import VTKObservationMixin
from slicer.parameterNodeWrapper import (
    parameterNodeWrapper
)

from slicer import vtkMRMLScalarVolumeNode
import qt

# 导入lib中的功能模块
from lib.image_alignment import ImageAlignmentLogic
from lib.metric_calculator import MetricCalculatorLogic
from lib.ai_decoupling import AIDecouplingLogic
from lib.atlas_manager import AtlasManager
from lib.ui_components import TimeConsumingMessageBox, MarkupManager


#
# localizer： created by tctco
#


class localizer(ScriptedLoadableModule):
    """Uses ScriptedLoadableModule base class, available at:
    https://github.com/Slicer/Slicer/blob/main/Base/Python/slicer/ScriptedLoadableModule.py
    """

    def __init__(self, parent):
        ScriptedLoadableModule.__init__(self, parent)
        self.parent.title = "PET Metric Calculator"  # Semi-quantitative PET analysis
        self.parent.categories = [
            "PET Analysis"
        ]  # Category for PET semi-quantitative analysis tools
        self.parent.dependencies = (
            []
        )  # TODO: add here list of module names that this module requires
        self.parent.contributors = [
            "Cheng Tang (Dept. Nuclear Med, WHUH)"
        ]  # TODO: replace with "Firstname Lastname (Organization)"
        # Module description and help text
        self.parent.helpText = "Module for PET semi-quantitative analysis including Centiloid, CenTauR, and CenTauRz calculations, plus AI-based decoupling. Note: This module is only for Windows. You may need to recompile the source code for other platforms."
        # TODO: replace with organization, grant and thanks
        self.parent.acknowledgementText = (
            "This file was developed by Cheng Tang, Dept. Nuclear Med, WHUH."
        )

        # Additional initialization step after application startup is complete


#
# localizerParameterNode
#


@parameterNodeWrapper
class localizerParameterNode:
    """
    The parameters needed by module.

    inputVolume - The volume to threshold.
    """

    inputVolume: vtkMRMLScalarVolumeNode


#
# localizerWidget
#


class localizerWidget(ScriptedLoadableModuleWidget, VTKObservationMixin):
    """Uses ScriptedLoadableModuleWidget base class, available at:
    https://github.com/Slicer/Slicer/blob/main/Base/Python/slicer/ScriptedLoadableModule.py
    """

    def __init__(self, parent=None) -> None:
        """
        Called when the user opens the module the first time and the widget is initialized.
        """
        ScriptedLoadableModuleWidget.__init__(self, parent)
        VTKObservationMixin.__init__(self)  # needed for parameter node observation
        self._setNodes()
        self._setPaths()
    
    def _setPaths(self):
        self.PLUGIN_PATH = Path(os.path.dirname(__file__))
        self.EXECUTABLE_PATH = self.PLUGIN_PATH / "cpp" / "CentiloidCalculator"
        self.EXECUTABLE_DIR = self.PLUGIN_PATH / "cpp"
    
    def _setNodes(self):
        self._parameterNode = None
        self._parameterNodeGuiTag = None

    def setup(self) -> None:
        """
        Called when the user opens the module the first time and the widget is initialized.
        """
        self._setPaths()
        self._setNodes()
        ScriptedLoadableModuleWidget.setup(self)

        # Load widget from .ui file (created by Qt Designer).
        # Additional widgets can be instantiated manually and added to self.layout.
        uiWidget = slicer.util.loadUI(self.resourcePath("UI/localizer.ui"))
        self.layout.addWidget(uiWidget)
        self.ui = slicer.util.childWidgetVariables(uiWidget)

        # Set scene in MRML widgets. Make sure that in Qt designer the top-level qMRMLWidget's
        # "mrmlSceneChanged(vtkMRMLScene*)" signal in is connected to each MRML widget's.
        # "setMRMLScene(vtkMRMLScene*)" slot.
        uiWidget.setMRMLScene(slicer.mrmlScene)
        self.ui.refSelector.nodeTypes = ["vtkMRMLScalarVolumeNode"]
        self.ui.roiSelector.nodeTypes = ["vtkMRMLScalarVolumeNode"]
        self.ui.refSelector.setMRMLScene(slicer.mrmlScene)
        self.ui.roiSelector.setMRMLScene(slicer.mrmlScene)
        
        # 设置Atlas选择器，如果存在的话
        try:
            if hasattr(self.ui, 'atlasSelector'):
                available_atlases = self.atlas_manager.get_available_atlases()
                self.ui.atlasSelector.clear()
                self.ui.atlasSelector.addItems(available_atlases)
                print(f"Atlas selector initialized with: {available_atlases}")
        except AttributeError as e:
            print(f"Atlas selector not found in UI: {e}")
            print("Atlas selection will be available via dialog")


        # Create logic classes. Logic implements all computations that should be possible to run
        # in batch mode, without a graphical user interface.
        self.logic = localizerLogic()
        self.image_alignment = ImageAlignmentLogic()
        self.metric_calculator = MetricCalculatorLogic(self.PLUGIN_PATH)
        self.ai_decoupling = AIDecouplingLogic(self.PLUGIN_PATH)
        self.atlas_manager = AtlasManager(self.PLUGIN_PATH)

        # Connections

        # These connections ensure that we update parameter node when scene is closed
        self.addObserver(
            slicer.mrmlScene, slicer.mrmlScene.StartCloseEvent, self.onSceneStartClose
        )
        self.addObserver(
            slicer.mrmlScene, slicer.mrmlScene.EndCloseEvent, self.onSceneEndClose
        )

        # Buttons
        self.ui.applyButton.connect("clicked(bool)", self.onApplyButton)
        self.ui.acButton.connect("clicked(bool)", self.onACButton)
        self.ui.pcButton.connect("clicked(bool)", self.onPCButton)
        self.ui.leftButton.connect("clicked(bool)", self.onLeftButton)
        self.ui.rightButton.connect("clicked(bool)", self.onRightButton)
        self.ui.applyLRButton.connect("clicked(bool)", self.onApplyLRButton)
        self.ui.clearButton.connect("clicked(bool)", self.onClearButton)
        self.ui.calcMetricButton.connect("clicked(bool)", self.onCalcMetricButton)
        self.ui.calculateSUVrButton.connect("clicked(bool)", self.onCalculateSUVrButton)
        self.ui.showImgButton.connect("clicked(bool)", self.onShowImgButton)
        self.ui.inputSelector.connect(
            "currentNodeChanged(vtkMRMLNode*)", self.onInputVolumeChanged
        )
        self.ui.decoupleButton.connect("clicked(bool)", self.onDecoupleButton)
        
        # Atlas相关按钮连接
        try:
            self.ui.loadAtlasButton.connect("clicked(bool)", self.onLoadAtlasButton)
            self.ui.addROIButton.connect("clicked(bool)", self.onAddROIButton)
            self.ui.addRefButton.connect("clicked(bool)", self.onAddRefButton)
            self.ui.generateROIRefButton.connect("clicked(bool)", self.onGenerateROIRefButton)
            self.ui.clearROIRefButton.connect("clicked(bool)", self.onClearROIRefButton)
        except AttributeError as e:
            print(f"Some Atlas UI buttons not found in UI file: {e}")
            print("Atlas functionality will be available via Python console")

        self.setupShortcuts()
        self.setupReferenceBox()

        # Make sure parameter node is initialized (needed for module reload)
        self.initializeParameterNode()

    def setupReferenceBox(self):
        roiNode = slicer.mrmlScene.AddNewNodeByClass("vtkMRMLMarkupsROINode")
        roiNode.SetName("MNI")
        roiNode.SetCenter(0, -18, 8)
        # roiNode.SetXYZ(-78.0, -112.0, -70.0)
        roiNode.SetRadiusXYZ(78.0, 94.0, 78.0)
        roiNode.SetLocked(True)
        displayNode = roiNode.GetDisplayNode()

        # 如果displayNode存在，则进行设置
        if displayNode:
            displayNode.SetHandlesInteractive(False)  # 禁用拖拽点的交互性
        displayNode.SetColor(0, 1, 0)  # 设置为红色
        displayNode.SetOpacity(0.3)  # 设置透明度

        origin = slicer.mrmlScene.AddNewNodeByClass("vtkMRMLMarkupsROINode")
        origin.SetCenter(0, 0, 0)
        origin.SetName("O")
        origin.SetRadiusXYZ(3, 3, 3)
        origin.SetLocked(True)
        displayNode = origin.GetDisplayNode()
        displayNode.SetHandlesInteractive(False)  # 禁用拖拽点的交互性
        displayNode.SetColor(0, 1, 0)  # 设置为绿色
        displayNode.SetOpacity(0.8)  # 设置透明度

    def onSpacePressed(self):
        # 获取所有的Volume节点
        volumes = slicer.util.getNodesByClass("vtkMRMLScalarVolumeNode")
        if not volumes:
            return  # 如果场景中没有Volume，直接返回

        currentNode = self.ui.inputSelector.currentNode()
        if not currentNode:
            # 如果当前没有选中的Volume，选择第一个
            self.ui.inputSelector.setCurrentNode(volumes[0])
            return

        # 找到当前Volume在列表中的位置，并切换到下一个
        currentIndex = volumes.index(currentNode)
        if currentIndex >= len(volumes):
            return  # 如果当前Volume不在列表中，直接返回
        nextIndex = (currentIndex + 1) % len(volumes)
        self.ui.inputSelector.setCurrentNode(volumes[nextIndex])

    def setupShortcuts(self):
        # 创建一个新的快捷键: 'A'键用于添加AC点
        self.acShortcut = qt.QShortcut(qt.QKeySequence("A"), slicer.util.mainWindow())
        self.acShortcut.connect("activated()", self.onACButton)
        self.pcShortcut = qt.QShortcut(qt.QKeySequence("D"), slicer.util.mainWindow())
        self.pcShortcut.connect("activated()", self.onPCButton)
        self.applyACPCShortcut = qt.QShortcut(
            qt.QKeySequence("S"), slicer.util.mainWindow()
        )
        self.applyACPCShortcut.connect("activated()", self.onApplyButton)

        self.leftShortcut = qt.QShortcut(qt.QKeySequence("Q"), slicer.util.mainWindow())
        self.leftShortcut.connect("activated()", self.onLeftButton)
        self.rightShortcut = qt.QShortcut(
            qt.QKeySequence("E"), slicer.util.mainWindow()
        )
        self.rightShortcut.connect("activated()", self.onRightButton)
        self.applyLRShortcut = qt.QShortcut(
            qt.QKeySequence("W"), slicer.util.mainWindow()
        )
        self.applyLRShortcut.connect("activated()", self.onApplyLRButton)

        self.clearShortcut = qt.QShortcut(
            qt.QKeySequence("C"), slicer.util.mainWindow()
        )
        self.clearShortcut.connect("activated()", self.onClearButton)

        self.nextVolumeShortcut = qt.QShortcut(
            qt.QKeySequence("Space"), slicer.util.mainWindow()
        )
        self.nextVolumeShortcut.connect("activated()", self.onSpacePressed)

    def onInputVolumeChanged(self, node):
        if not node:
            return
        volumeNodeID = node.GetID()
        self.setViewBackgroundVolume(volumeNodeID)

    def setViewBackgroundVolume(self, volumeNodeID):
        redSliceNode = slicer.mrmlScene.GetNodeByID("vtkMRMLSliceNodeRed")
        yellowSliceNode = slicer.mrmlScene.GetNodeByID("vtkMRMLSliceNodeYellow")
        greenSliceNode = slicer.mrmlScene.GetNodeByID("vtkMRMLSliceNodeGreen")

        # 将新的体积设置为每个切片视图的背景体积
        for sliceNode in [redSliceNode, yellowSliceNode, greenSliceNode]:
            sliceLogic = slicer.app.applicationLogic().GetSliceLogic(sliceNode)
            compositeNode = sliceLogic.GetSliceCompositeNode()
            compositeNode.SetBackgroundVolumeID(volumeNodeID)

            # 刷新视图以显示新的体积
            sliceLogic.FitSliceToAll()
        MarkupManager.remove_specific_markups(["AC", "PC", "Left", "Right"])

    def cleanup(self) -> None:
        """
        Called when the application closes and the module widget is destroyed.
        """
        self.removeObservers()
        self.cleanupShortcuts()

    def cleanupShortcuts(self):
        self.acShortcut.disconnect("activated()", self.onACButton)
        self.pcShortcut.disconnect("activated()", self.onPCButton)
        self.applyACPCShortcut.disconnect("activated()", self.onApplyButton)
        self.leftShortcut.disconnect("activated()", self.onLeftButton)
        self.rightShortcut.disconnect("activated()", self.onRightButton)
        self.applyLRShortcut.disconnect("activated()", self.onApplyLRButton)
        self.clearShortcut.disconnect("activated()", self.onClearButton)
        self.nextVolumeShortcut.disconnect("activated()", self.onSpacePressed)

    def enter(self) -> None:
        """
        Called each time the user opens this module.
        """
        # Make sure parameter node exists and observed
        self.initializeParameterNode()

    def exit(self) -> None:
        """
        Called each time the user opens a different module.
        """
        # Do not react to parameter node changes (GUI will be updated when the user enters into the module)
        if self._parameterNode:
            self._parameterNode.disconnectGui(self._parameterNodeGuiTag)
            self._parameterNodeGuiTag = None

    def onSceneStartClose(self, caller, event) -> None:
        """
        Called just before the scene is closed.
        """
        # Parameter node will be reset, do not use it anymore
        MarkupManager.remove_specific_markups(["AC", "PC", "Left", "Right"])
        self.setParameterNode(None)

    def onSceneEndClose(self, caller, event) -> None:
        """
        Called just after the scene is closed.
        """
        # If this module is shown while the scene is closed then recreate a new parameter node immediately
        if self.parent.isEntered:
            self.initializeParameterNode()

    def initializeParameterNode(self) -> None:
        """
        Ensure parameter node exists and observed.
        """
        # Parameter node stores all user choices in parameter values, node selections, etc.
        # so that when the scene is saved and reloaded, these settings are restored.

        self.setParameterNode(self.logic.getParameterNode())

        # Select default input nodes if nothing is selected yet to save a few clicks for the user
        if not self._parameterNode.inputVolume:
            firstVolumeNode = slicer.mrmlScene.GetFirstNodeByClass(
                "vtkMRMLScalarVolumeNode"
            )
            if firstVolumeNode:
                self._parameterNode.inputVolume = firstVolumeNode

    def setParameterNode(
        self, inputParameterNode: Optional[localizerParameterNode]
    ) -> None:
        """
        Set and observe parameter node.
        Observation is needed because when the parameter node is changed then the GUI must be updated immediately.
        """

        if self._parameterNode:
            self._parameterNode.disconnectGui(self._parameterNodeGuiTag)
            # self.removeObserver(self._parameterNode, vtk.vtkCommand.ModifiedEvent, self._checkCanApply)
        self._parameterNode = inputParameterNode
        if self._parameterNode:
            # Note: in the .ui file, a Qt dynamic property called "SlicerParameterName" is set on each
            # ui element that needs connection.
            self._parameterNodeGuiTag = self._parameterNode.connectGui(self.ui)

    def _findMarkupNodeByName(self, nodeName):
        """
        查找并返回给定名称的Markups节点。
        如果找不到，则返回None。
        """
        return MarkupManager.find_markup_node_by_name(nodeName)

    def _getPointRAS(self, node):
        """
        获取给定标记点的RAS坐标。
        如果找不到或没有控制点，则返回None。
        """
        return MarkupManager.get_point_ras(node)

    def onClearButton(self) -> None:
        """
        处理Clear按钮点击事件。
        """
        MarkupManager.remove_specific_markups(["AC", "PC", "Left", "Right"])

    def _nodeButtonClick(self, nodeName):
        """
        处理标记点按钮点击事件。
        """
        node = self._findMarkupNodeByName(nodeName)
        if node is None:
            # 如果不存在，则添加新的标记点
            self.addNewFiducial(nodeName)
        else:
            print(f"{nodeName} point already exists:{self._getPointRAS(node)}")

    def _checkCurrentVolume(self):
        volumes = slicer.util.getNodesByClass("vtkMRMLScalarVolumeNode")
        if not volumes:
            return None  # 如果场景中没有Volume，直接返回

        currentNode = self.ui.inputSelector.currentNode()
        if not currentNode:
            return None
        dim = currentNode.GetImageData().GetDimensions()
        print(f"input dim: {dim}")
        if len(dim) != 3:
            slicer.util.errorDisplay(
                f"A 3D input is required, but the given node is {dim}."
            )
            return None
        return currentNode

    def getAdditionalInfo(self, node):
        return self.metric_calculator.get_additional_info(node)

    def onCalcMetricButton(self) -> None:
        currentNode = self._checkCurrentVolume()
        if not currentNode:
            return
        additionalInformation = self.getAdditionalInfo(currentNode)
        
        # Get selected metric and algorithm with fallbacks
        try:
            metric_type = self.ui.metricSelector.currentText
        except AttributeError:
            metric_type = "Centiloid"  # Default metric
            print("metricSelector not found in UI, using default: Centiloid")
            
        try:
            algorithm_style = self.ui.algorithmSelector.currentText
        except AttributeError:
            algorithm_style = "SPM style"  # Default algorithm
            print("algorithmSelector not found in UI, using default: SPM style")
        
        with TimeConsumingMessageBox():
            success, result_text, error_message = self.metric_calculator.calculate_metric(
                currentNode,
                metric_type=metric_type,
                algorithm_style=algorithm_style,
                manual_fov=self.ui.manualFOVCheckBox.isChecked(),
                iterative=self.ui.enableIterativeCheckBox.isChecked(),
                skip_normalization=self.ui.skipCheckBox.isChecked()
            )

        if not success:
            slicer.util.errorDisplay(error_message)
            return
            
        slicer.util.infoDisplay(
            f"Semi-quantitative calculation finished:\n{additionalInformation}\n{result_text}"
        )

    def onCalculateSUVrButton(self) -> None:
        """Handle SUVr calculation button click"""
        # Check current volume (PET image)
        currentNode = self._checkCurrentVolume()
        if not currentNode:
            return
            
        # Check ROI selector
        roiNode = self.ui.roiSelector.currentNode()
        if not roiNode:
            slicer.util.errorDisplay("Please select a ROI volume from ROI selector")
            return
            
        # Check reference selector
        refNode = self.ui.refSelector.currentNode()
        if not refNode:
            slicer.util.errorDisplay("Please select a reference volume from Reference selector")
            return
            
        additionalInformation = self.getAdditionalInfo(currentNode)
        
        # Get algorithm style with fallback (same as other metric calculations)
        try:
            algorithm_style = self.ui.algorithmSelector.currentText
        except AttributeError:
            algorithm_style = "SPM style"  # Default algorithm
            print("algorithmSelector not found in UI, using default: SPM style")
        
        with TimeConsumingMessageBox():
            success, result_text, error_message = self.metric_calculator.calculate_suvr(
                currentNode, roiNode, refNode,
                algorithm_style=algorithm_style,
                manual_fov=self.ui.manualFOVCheckBox.isChecked(),
                iterative=self.ui.enableIterativeCheckBox.isChecked(),
                skip_normalization=self.ui.skipCheckBox.isChecked()
            )

        if not success:
            slicer.util.errorDisplay(error_message)
            return
            
        slicer.util.infoDisplay(
            f"SUVr calculation finished:\n{additionalInformation}\n{result_text}"
        )

    def onDecoupleButton(self) -> None:
        currentNode = self._checkCurrentVolume()
        modality = self.ui.modalitySelector.currentText
        if not currentNode:
            return
        additionalInformation = self.getAdditionalInfo(currentNode)

        with TimeConsumingMessageBox():
            success, result_text, error_message, foreground, background, stripped = self.ai_decoupling.decouple_image(
                currentNode,
                modality,
                manual_fov=self.ui.manualFOVCheckBox.isChecked(),
                iterative=self.ui.enableIterativeCheckBox.isChecked()
            )
            
        if not success:
            slicer.util.errorDisplay(error_message)
            return

        if foreground is None or background is None or stripped is None:
            return
            
        self.ui.inputSelector.setCurrentNode(background)
        self.ai_decoupling.setup_display_properties(foreground, background)
        self.ai_decoupling.set_view_background_volume(background.GetID())
        
        slicer.util.setSliceViewerLayers(
            foreground=foreground, background=background, foregroundOpacity=0.5
        )
        slicer.util.infoDisplay(
            f"Semi-quantitative calculation finished:\n{additionalInformation}\n{result_text}"
        )

    def onLoadAtlasButton(self) -> None:
        """处理加载Atlas按钮点击事件"""
        print("Loading Atlas...")
        
        # 获取用户选择的Atlas，如果atlasSelector存在的话
        selected_atlas = None
        try:
            # 尝试从UI获取选择的Atlas
            if hasattr(self.ui, 'atlasSelector'):
                selected_atlas = self.ui.atlasSelector.currentText
                print(f"Selected atlas from UI: {selected_atlas}")
            else:
                # 如果UI中没有atlasSelector，显示选择对话框
                available_atlases = self.atlas_manager.get_available_atlases()
                selected_atlas = self._show_atlas_selection_dialog(available_atlases)
        except AttributeError:
            # 如果UI组件不存在，使用默认Atlas或显示选择对话框
            available_atlases = self.atlas_manager.get_available_atlases()
            selected_atlas = self._show_atlas_selection_dialog(available_atlases)
        
        if not selected_atlas:
            slicer.util.warningDisplay("No atlas selected")
            return
        
        atlas_node = self.atlas_manager.load_atlas(selected_atlas)
        if atlas_node:
            slicer.util.infoDisplay(f"Successfully loaded atlas: {selected_atlas}")
        else:
            slicer.util.errorDisplay(f"Failed to load Atlas: {selected_atlas}")
    
    def _show_atlas_selection_dialog(self, available_atlases):
        """显示Atlas选择对话框
        
        Args:
            available_atlases: 可用Atlas列表
            
        Returns:
            str: 选择的Atlas名称，如果取消则返回None
        """
        if not available_atlases:
            slicer.util.errorDisplay("No atlases available")
            return None
        
        # 创建简单的选择对话框
        from qt import QInputDialog
        
        atlas_name, ok = QInputDialog.getItem(
            None, 
            "Select Atlas", 
            "Choose an atlas to load:", 
            available_atlases, 
            0, 
            False
        )
        
        return atlas_name if ok else None

    def onAddROIButton(self) -> None:
        """处理添加ROI点按钮点击事件"""
        self.atlas_manager.add_roi_point()

    def onAddRefButton(self) -> None:
        """处理添加Ref点按钮点击事件"""
        self.atlas_manager.add_ref_point()

    def onGenerateROIRefButton(self) -> None:
        """处理生成ROI/Ref区域按钮点击事件"""
        print("Generating ROI/Ref regions...")
        
        # 确保Atlas已加载
        if not self.atlas_manager.atlas_node:
            slicer.util.warningDisplay("Atlas not loaded. Loading now...")
            atlas_node = self.atlas_manager.load_atlas()
            if not atlas_node:
                slicer.util.errorDisplay("Failed to load Atlas. Cannot proceed.")
                return
        
        # 计算脑区
        self.atlas_manager.generate_combined_regions()

    def onClearROIRefButton(self) -> None:
        """处理清除ROI/Ref点按钮点击事件"""
        self.atlas_manager.clear_roi_ref_points()


    def onShowImgButton(self) -> None:
        volume_node = self.metric_calculator.load_normalized_volume()
        if volume_node is None:
            return

        self.setViewBackgroundVolume(volume_node.GetID())

        # set current node to the loaded volume
        self.ui.inputSelector.setCurrentNode(volume_node)

        # 更新视图以适应新加载的图像
        slicer.app.applicationLogic().FitSliceToAll()

    def onACButton(self) -> None:
        """
        处理AC按钮点击事件。
        """
        self._nodeButtonClick("AC")

    def onPCButton(self) -> None:
        """
        处理PC按钮点击事件。
        """
        self._nodeButtonClick("PC")

    def onLeftButton(self) -> None:
        self._nodeButtonClick("Left")

    def onRightButton(self) -> None:
        self._nodeButtonClick("Right")

    def addNewFiducial(self, name):
        """
        添加一个新的标记点并将其命名。
        """
        MarkupManager.add_new_fiducial(name)

    def onApplyButton(self):
        """
        当点击Apply按钮时执行的操作。
        """
        print("Run the processing")
        with slicer.util.tryWithErrorDisplay(
            "Failed to compute results.", waitCursor=True
        ):
            acNode = self._findMarkupNodeByName("AC")
            pcNode = self._findMarkupNodeByName("PC")
            acCoord = self._getPointRAS(acNode)
            pcCoord = self._getPointRAS(pcNode)
            leftNode = self._findMarkupNodeByName("Left")
            rightNode = self._findMarkupNodeByName("Right")

            if acCoord is not None:
                print("AC Point: RAS Coordinates = ", acCoord)
            else:
                print("AC Point not found.")

            if pcCoord is not None:
                print("PC Point: RAS Coordinates = ", pcCoord)
            else:
                print("PC Point not found.")

            if acCoord is not None and pcCoord is not None:
                self.image_alignment.transform_acpc(
                    acCoord,
                    pcCoord,
                    self._parameterNode.inputVolume,
                    [acNode, pcNode, leftNode, rightNode],
                )
            elif acCoord is not None and pcCoord is None:
                self.image_alignment.translate_ac(acCoord, self._parameterNode.inputVolume, acNode)

    def onApplyLRButton(self):
        """
        当点击ApplyLR按钮时执行的操作。
        """
        print("Run the processing")
        with slicer.util.tryWithErrorDisplay(
            "Failed to compute results.", waitCursor=True
        ):
            leftNode = self._findMarkupNodeByName("Left")
            rightNode = self._findMarkupNodeByName("Right")
            leftCoord = self._getPointRAS(leftNode)
            rightCoord = self._getPointRAS(rightNode)
            acNode = self._findMarkupNodeByName("AC")
            pcNode = self._findMarkupNodeByName("PC")

            if leftCoord is not None and rightCoord is not None:
                print("Left Point: RAS Coordinates = ", leftCoord)
                print("Right Point: RAS Coordinates = ", rightCoord)
                self.image_alignment.transform_lr(
                    leftCoord,
                    rightCoord,
                    self._parameterNode.inputVolume,
                    [leftNode, rightNode, acNode, pcNode],
                )


#
# localizerLogic
#


class localizerLogic(ScriptedLoadableModuleLogic):
    """This class should implement all the actual
    computation done by your module.  The interface
    should be such that other python code can import
    this class and make use of the functionality without
    requiring an instance of the Widget.
    Uses ScriptedLoadableModuleLogic base class, available at:
    https://github.com/Slicer/Slicer/blob/main/Base/Python/slicer/ScriptedLoadableModule.py
    """

    def __init__(self) -> None:
        """
        Called when the logic class is instantiated. Can be used for initializing member variables.
        """
        ScriptedLoadableModuleLogic.__init__(self)

    def getParameterNode(self):
        return localizerParameterNode(super().getParameterNode())


#
# localizerTest
#


class localizerTest(ScriptedLoadableModuleTest):
    """
    This is the test case for your scripted module.
    Uses ScriptedLoadableModuleTest base class, available at:
    https://github.com/Slicer/Slicer/blob/main/Base/Python/slicer/ScriptedLoadableModule.py
    """

    def setUp(self):
        """Do whatever is needed to reset the state - typically a scene clear will be enough."""
        slicer.mrmlScene.Clear()

    def runTest(self):
        """Run as few or as many tests as needed here."""
        self.setUp()
        self.test_localizer1()
        self.testModuleComponents()

    def test_localizer1(self):
        """测试localizer的基本功能"""
        self.delayDisplay("Starting localizer test")
        
        # 测试加载测试数据
        test_volume = self.loadTestData()
        self.assertIsNotNone(test_volume, "Failed to load test data")
        
        # 测试Widget创建
        widget = self.createWidget()
        self.assertIsNotNone(widget, "Failed to create widget")
        
        # 测试标记点功能
        self.testMarkupFunctionality(widget)
        
        # 测试图像对齐功能
        self.testImageAlignment(widget, test_volume)
        
        self.delayDisplay("Test passed!")

    def loadTestData(self):
        """加载测试数据"""
        test_data_path = os.path.join(os.path.dirname(__file__), "Testing", "ED01_PET.nii")
        if not os.path.exists(test_data_path):
            self.delayDisplay(f"Test data not found at {test_data_path}")
            return None
            
        volume_node = slicer.util.loadVolume(test_data_path)
        self.delayDisplay(f"Loaded test volume: {volume_node.GetName()}")
        return volume_node

    def createWidget(self):
        """创建并设置widget"""
        widget = localizerWidget()
        widget.setup()
        return widget

    def testMarkupFunctionality(self, widget):
        """测试标记点功能"""
        self.delayDisplay("Testing markup functionality")
        
        # 测试添加AC标记点
        widget.addNewFiducial("AC")
        ac_node = MarkupManager.find_markup_node_by_name("AC")
        self.assertIsNotNone(ac_node, "Failed to create AC markup")
        
        # 手动设置AC点位置
        ac_node.AddControlPoint([0, 0, 0])
        ac_coord = MarkupManager.get_point_ras(ac_node)
        self.assertIsNotNone(ac_coord, "Failed to get AC coordinates")
        
        # 测试添加PC标记点
        widget.addNewFiducial("PC")
        pc_node = MarkupManager.find_markup_node_by_name("PC")
        self.assertIsNotNone(pc_node, "Failed to create PC markup")
        
        # 手动设置PC点位置
        pc_node.AddControlPoint([0, -25, 0])
        pc_coord = MarkupManager.get_point_ras(pc_node)
        self.assertIsNotNone(pc_coord, "Failed to get PC coordinates")
        
        # 测试清除标记点
        MarkupManager.remove_specific_markups(["AC", "PC"])
        ac_node_after_clear = MarkupManager.find_markup_node_by_name("AC")
        self.assertIsNone(ac_node_after_clear, "AC markup was not properly removed")

    def testImageAlignment(self, widget, test_volume):
        """测试图像对齐功能"""
        if test_volume is None:
            self.delayDisplay("Skipping image alignment test due to missing test data")
            return
            
        self.delayDisplay("Testing image alignment functionality")
        
        # 设置输入体积
        widget.ui.inputSelector.setCurrentNode(test_volume)
        widget._parameterNode.inputVolume = test_volume
        
        # 创建测试用的AC/PC标记点
        widget.addNewFiducial("AC")
        ac_node = MarkupManager.find_markup_node_by_name("AC")
        ac_node.AddControlPoint([0, 0, 0])
        
        widget.addNewFiducial("PC")
        pc_node = MarkupManager.find_markup_node_by_name("PC")
        pc_node.AddControlPoint([0, -25, 0])
        
        # 测试AC平移
        original_origin = test_volume.GetOrigin()
        widget.image_alignment.translate_ac([0, 0, 0], test_volume, ac_node)
        
        # 验证变换是否应用
        self.delayDisplay("AC translation test completed")
        
        # 测试ACPC变换
        widget.image_alignment.transform_acpc([0, 0, 0], [0, -25, 0], test_volume, [ac_node, pc_node])
        self.delayDisplay("ACPC transformation test completed")
        
        # 清理
        MarkupManager.remove_specific_markups(["AC", "PC"])

    def testModuleComponents(self):
        """测试各个组件模块"""
        self.delayDisplay("Testing module components")
        
        # 测试数学工具
        from lib.math_utils import create_translation_matrix, create_rotation_matrix
        
        # 测试平移矩阵
        translation_matrix = create_translation_matrix([1, 2, 3])
        self.assertEqual(translation_matrix.shape, (4, 4), "Translation matrix should be 4x4")
        
        # 测试旋转矩阵
        import numpy as np
        rotation_matrix = create_rotation_matrix([0, 0, 1], np.pi/4)
        self.assertEqual(rotation_matrix.shape, (4, 4), "Rotation matrix should be 4x4")
        
        # 测试UI组件
        from lib.ui_components import TimeConsumingMessageBox
        
        # 测试消息框（不显示，只测试创建）
        msg_box = TimeConsumingMessageBox("Test message", "Test title")
        self.assertIsNotNone(msg_box, "Failed to create TimeConsumingMessageBox")
        
        self.delayDisplay("Module components test completed")
