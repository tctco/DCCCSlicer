import logging
import os
from typing import Annotated, Optional
from pathlib import Path
import subprocess

import vtk

import slicer
from slicer.ScriptedLoadableModule import *
from slicer.util import VTKObservationMixin
from slicer.parameterNodeWrapper import (
    parameterNodeWrapper,
    WithinRange,
)

from slicer import vtkMRMLScalarVolumeNode
import qt


#
# localizer： created by tctco
#


class localizer(ScriptedLoadableModule):
    """Uses ScriptedLoadableModule base class, available at:
    https://github.com/Slicer/Slicer/blob/main/Base/Python/slicer/ScriptedLoadableModule.py
    """

    def __init__(self, parent):
        ScriptedLoadableModule.__init__(self, parent)
        self.parent.title = "DeepCascadeCentiloidCalculator"  # TODO: make this more human readable by adding spaces
        self.parent.categories = [
            "Centiloid"
        ]  # TODO: set categories (folders where the module shows up in the module selector)
        self.parent.dependencies = (
            []
        )  # TODO: add here list of module names that this module requires
        self.parent.contributors = [
            "Cheng Tang (Dept. Nuclear Med, WHUH)"
        ]  # TODO: replace with "Firstname Lastname (Organization)"
        # TODO: update with short description of the module and a link to online module documentation
        self.parent.helpText = "Simple module to calculate Centiloid value. Please note that this module is only for Windows. You may need to recomplie the source code for other platforms."
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
        self.logic = None
        self._parameterNode = None
        self._parameterNodeGuiTag = None

    def setup(self) -> None:
        """
        Called when the user opens the module the first time and the widget is initialized.
        """
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

        # Create logic class. Logic implements all computations that should be possible to run
        # in batch mode, without a graphical user interface.
        self.logic = localizerLogic()

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
        self.ui.calcCentiloidButton.connect("clicked(bool)", self.onCalcCentiloidButton)
        self.ui.showImgButton.connect("clicked(bool)", self.onShowImgButton)
        self.ui.inputSelector.connect(
            "currentNodeChanged(vtkMRMLNode*)", self.onInputVolumeChanged
        )
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
        self.removeSpecificMarkups(["AC", "PC", "Left", "Right"])

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
        self.removeSpecificMarkups(["AC", "PC", "Left", "Right"])
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

    def removeSpecificMarkups(self, names):
        """
        Remove markups nodes with specific names.
        :param names: List of names of markups nodes to be removed.
        """
        # 遍历场景中的所有Markups节点
        noMore = False
        while not noMore:
            noMore = True
            for i in range(
                slicer.mrmlScene.GetNumberOfNodesByClass("vtkMRMLMarkupsFiducialNode")
            ):
                node = slicer.mrmlScene.GetNthNodeByClass(
                    i, "vtkMRMLMarkupsFiducialNode"
                )
                if node is not None and node.GetName() in names:
                    # 删除找到的节点
                    slicer.mrmlScene.RemoveNode(node)
                    # 由于节点被删除，索引将改变
                    noMore = False

    def _findMarkupNodeByName(self, nodeName):
        """
        查找并返回给定名称的Markups节点。
        如果找不到，则返回None。
        """
        for i in range(
            slicer.mrmlScene.GetNumberOfNodesByClass("vtkMRMLMarkupsFiducialNode")
        ):
            node = slicer.mrmlScene.GetNthNodeByClass(i, "vtkMRMLMarkupsFiducialNode")
            if node.GetName() == nodeName:
                return node
        print(f"Markup node '{nodeName}' not found.")
        return None

    def _getPointRAS(self, node):
        """
        获取给定标记点的RAS坐标。
        如果找不到或没有控制点，则返回None。
        """
        if node is None:
            print("Node is None")
            return None
        if node.GetNumberOfControlPoints() > 0:
            pointRAS = [0.0, 0.0, 0.0]
            node.GetNthControlPointPosition(0, pointRAS)
            return pointRAS
        else:
            print(f"Node '{node.GetName()}' exists but has no control points.")
            return None

    def onClearButton(self) -> None:
        """
        处理Clear按钮点击事件。
        """
        self.removeSpecificMarkups(["AC", "PC", "Left", "Right"])

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

    def onCalcCentiloidButton(self) -> None:
        volumes = slicer.util.getNodesByClass("vtkMRMLScalarVolumeNode")
        if not volumes:
            return  # 如果场景中没有Volume，直接返回

        currentNode = self.ui.inputSelector.currentNode()
        if not currentNode:
            return
        dim = currentNode.GetImageData().GetDimensions()
        print(f"input dim: {dim}")
        if len(dim) != 3:
            slicer.util.errorDisplay(
                f"A 3D input is required, but the given node is {dim}."
            )
        # plugin path
        pluginPath = Path(os.path.dirname(__file__))

        # pop up a dialog to show it is calculating
        msg_box = qt.QMessageBox()
        msg_box.setIcon(qt.QMessageBox.Information)
        msg_box.setMinimumWidth(300)
        msg_box.setText("Calculating, please wait...")
        msg_box.setWindowTitle("Processing")
        msg_box.setStandardButtons(qt.QMessageBox.NoButton)  # 不显示任何按钮
        msg_box.show()

        # save currentNode as tmp.nii
        slicer.util.saveNode(currentNode, str(pluginPath / "tmp.nii"))
        executablePath = pluginPath / "cpp" / "CentiloidCalculator.exe"
        cmd = [
            str(executablePath),
            str(pluginPath / "tmp.nii"),
            str(pluginPath / "Normalized.nii"),
        ]
        print(cmd)
        result = subprocess.run(cmd, capture_output=True)

        # close the dialog
        msg_box.close()
        if result.returncode != 0:
            slicer.util.errorDisplay(
                f"Failed to calculate Centiloid\n{result.stdout.decode()}"
            )
            return
        slicer.util.infoDisplay(
            f"Centiloid calculation finished:\n{result.stdout.decode()}"
        )

    def onShowImgButton(self) -> None:
        # 指定本地 NIfTI 文件路径
        pluginPath = Path(os.path.dirname(__file__))
        nii_file_path = str(pluginPath / "Normalized.nii")
        if not os.path.exists(nii_file_path):
            slicer.util.errorDisplay(
                f"IT seems that you haven't calculated Centiloid yet."
            )
            return

        # 读取 NIfTI 文件并加载为 Slicer 的 Volume Node
        volume_node = slicer.util.loadVolume(nii_file_path)

        if not volume_node:
            slicer.util.errorDisplay(
                f"Failed to load volume from {nii_file_path}. This may be a bug :("
            )
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
        markupFiducialNode = slicer.mrmlScene.AddNewNodeByClass(
            "vtkMRMLMarkupsFiducialNode"
        )
        markupFiducialNode.SetName(name)
        slicer.modules.markups.logic().SetActiveListID(markupFiducialNode)

        # 自动进入放置模式
        interactionNode = slicer.mrmlScene.GetNodeByID(
            "vtkMRMLInteractionNodeSingleton"
        )
        interactionNode.SetCurrentInteractionMode(interactionNode.Place)

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
                self.logic.transformACPC(
                    acCoord,
                    pcCoord,
                    self._parameterNode.inputVolume,
                    [acNode, pcNode, leftNode, rightNode],
                )
            elif acCoord is not None and pcCoord is None:
                self.logic.translateAC(acCoord, self._parameterNode.inputVolume, acNode)

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
                self.logic.transformLR(
                    leftCoord,
                    rightCoord,
                    self._parameterNode.inputVolume,
                    [leftNode, rightNode, acNode, pcNode],
                )


#
# localizerLogic
#
import numpy as np


def create_translation_matrix(translation):
    """创建平移矩阵"""
    T = np.eye(4)
    T[:3, 3] = translation
    return T


def create_rotation_matrix(axis, angle):
    """罗德里格斯旋转公式创建旋转矩阵"""
    axis = axis / np.linalg.norm(axis)
    K = np.array(
        [[0, -axis[2], axis[1]], [axis[2], 0, -axis[0]], [-axis[1], axis[0], 0]]
    )
    R = np.eye(3) + np.sin(angle) * K + (1 - np.cos(angle)) * np.dot(K, K)
    rotation_matrix = np.eye(4)
    rotation_matrix[:3, :3] = R
    return rotation_matrix


def create_affine_matrix(ac, bc):
    import numpy as np

    # 平移向量，使ac移动到原点
    translation = -ac
    T = create_translation_matrix(translation)

    # 计算方向向量并规范化
    direction = bc - ac
    direction_normalized = direction / np.linalg.norm(direction)

    # 计算需要旋转的轴和角度
    y_axis = np.array([0, -1, 0])
    axis = np.cross(direction_normalized, y_axis)
    if np.linalg.norm(axis) != 0:  # 需要旋转
        axis_normalized = axis / np.linalg.norm(axis)
        angle = np.arccos(np.dot(direction_normalized, y_axis))
    else:
        axis_normalized = np.array([0, 0, 1])  # 任意轴，因为不需要旋转
        angle = 0

    R = create_rotation_matrix(axis_normalized, angle)

    # 先平移后旋转
    affine_matrix = np.dot(R, T)
    return affine_matrix


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
        self.transformTable = {}  # volumeNodeID: transformNode

    def getParameterNode(self):
        return localizerParameterNode(super().getParameterNode())

    def translateAC(self, acCoord, targetNode, markupNode):
        if targetNode is None:
            logging.error("process: Invalid input node")
            return
        if self.transformTable.get(targetNode.GetID()):
            transformNode = self.transformTable[targetNode.GetID()]
            existingMatrix = vtk.vtkMatrix4x4()
        else:
            transformNode = slicer.vtkMRMLLinearTransformNode()
            slicer.mrmlScene.AddNode(transformNode)
            self.transformTable[targetNode.GetID()] = transformNode
            if targetNode.GetName():
                transformNodeName = targetNode.GetName() + "_Transform"
                transformNode.SetName(transformNodeName)
            existingMatrix = vtk.vtkMatrix4x4()
        affineMatrix = create_translation_matrix(-np.array(acCoord))
        vtkNewMatrix = slicer.util.vtkMatrixFromArray(affineMatrix)
        compositeMatrix = vtk.vtkMatrix4x4()
        vtk.vtkMatrix4x4.Multiply4x4(vtkNewMatrix, existingMatrix, compositeMatrix)
        transformNode.SetMatrixTransformToParent(compositeMatrix)
        targetNode.SetAndObserveTransformNodeID(transformNode.GetID())
        slicer.vtkSlicerTransformLogic().hardenTransform(targetNode)
        if markupNode is not None:
            markupNode.SetAndObserveTransformNodeID(transformNode.GetID())
            slicer.vtkSlicerTransformLogic().hardenTransform(markupNode)

    def transformACPC(
        self, acCoord: list, pcCoord: list, targetNode, markupNodes: list
    ) -> None:
        if targetNode is None:
            logging.error("process: Invalid input node")
            return
        print(f"AC: {acCoord}, PC: {pcCoord}")
        if self.transformTable.get(targetNode.GetID()):
            transformNode = self.transformTable[targetNode.GetID()]
            existingMatrix = vtk.vtkMatrix4x4()
        else:
            transformNode = slicer.vtkMRMLLinearTransformNode()
            slicer.mrmlScene.AddNode(transformNode)
            self.transformTable[targetNode.GetID()] = transformNode
            if targetNode.GetName():
                transformNodeName = targetNode.GetName() + "_Transform"
                transformNode.SetName(transformNodeName)
            existingMatrix = vtk.vtkMatrix4x4()  # 默认为单位矩阵

        affineMatrix = create_affine_matrix(np.array(acCoord), np.array(pcCoord))
        # 将NumPy矩阵转换为VTK矩阵
        vtkNewMatrix = slicer.util.vtkMatrixFromArray(affineMatrix)
        compositeMatrix = vtk.vtkMatrix4x4()
        vtk.vtkMatrix4x4.Multiply4x4(vtkNewMatrix, existingMatrix, compositeMatrix)
        transformNode.SetMatrixTransformToParent(compositeMatrix)

        targetNode.SetAndObserveTransformNodeID(transformNode.GetID())
        slicer.vtkSlicerTransformLogic().hardenTransform(targetNode)
        for node in markupNodes:
            if node is None:
                continue
            node.SetAndObserveTransformNodeID(transformNode.GetID())
            slicer.vtkSlicerTransformLogic().hardenTransform(node)

    def transformLR(self, leftCoord, rightCoord, targetNode, markupNodes):
        if self.transformTable.get(targetNode.GetID()):
            transformNode = self.transformTable[targetNode.GetID()]
            existingMatrix = vtk.vtkMatrix4x4()
        else:
            transformNode = slicer.vtkMRMLLinearTransformNode()
            slicer.mrmlScene.AddNode(transformNode)
            self.transformTable[targetNode.GetID()] = transformNode
            if targetNode.GetName():
                transformNodeName = targetNode.GetName() + "_Transform"
                transformNode.SetName(transformNodeName)
            existingMatrix = vtk.vtkMatrix4x4()  # 默认为单位矩阵

        direction = np.array(rightCoord) - np.array(leftCoord)
        normalised_direction = direction / np.linalg.norm(direction)
        x_axis = np.array([-1, 0, 0])
        axis = np.cross(normalised_direction, x_axis)
        if np.linalg.norm(axis) != 0:  # 需要旋转
            axis_normalized = axis / np.linalg.norm(axis)
            angle = np.arccos(np.dot(normalised_direction, x_axis))
        else:
            axis_normalized = np.array([0, 0, 1])  # 任意轴，因为不需要旋转
            angle = 0
        rotationMatrix = create_rotation_matrix(axis_normalized, angle)
        vtkNewMatrix = slicer.util.vtkMatrixFromArray(rotationMatrix)
        compositeMatrix = vtk.vtkMatrix4x4()
        vtk.vtkMatrix4x4.Multiply4x4(vtkNewMatrix, existingMatrix, compositeMatrix)
        transformNode.SetMatrixTransformToParent(compositeMatrix)

        targetNode.SetAndObserveTransformNodeID(transformNode.GetID())
        slicer.vtkSlicerTransformLogic().hardenTransform(targetNode)
        for node in markupNodes:
            if node is None:
                continue
            node.SetAndObserveTransformNodeID(transformNode.GetID())
            slicer.vtkSlicerTransformLogic().hardenTransform(node)


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

    def test_localizer1(self):
        pass
