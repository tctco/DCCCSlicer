"""
UI组件模块

包含用户界面相关的组件和工具
"""

import qt


class TimeConsumingMessageBox:
    """耗时操作的消息框上下文管理器"""
    
    def __init__(self, message="Calculating, please wait...", title="Processing"):
        """初始化消息框
        
        Args:
            message: 显示的消息
            title: 窗口标题
        """
        self.message = message
        self.title = title
        self.msg_box = None

    def __enter__(self):
        """进入上下文时显示消息框"""
        msg_box = qt.QMessageBox()
        self.msg_box = msg_box
        msg_box.setIcon(qt.QMessageBox.Information)
        msg_box.setMinimumWidth(300)
        msg_box.setText(self.message)
        msg_box.setWindowTitle(self.title)
        msg_box.setStandardButtons(qt.QMessageBox.NoButton)  # 不显示任何按钮
        msg_box.show()
        return self

    def __exit__(self, exc_type, exc_value, traceback):
        """退出上下文时关闭消息框"""
        if self.msg_box:
            self.msg_box.close()


class MarkupManager:
    """标记点管理器"""
    
    @staticmethod
    def remove_specific_markups(names):
        """删除特定名称的标记点
        
        Args:
            names: 要删除的标记点名称列表
        """
        import slicer
        
        no_more = False
        while not no_more:
            no_more = True
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
                    no_more = False
                    break
    
    @staticmethod
    def find_markup_node_by_name(node_name):
        """查找并返回给定名称的标记点节点
        
        Args:
            node_name: 标记点名称
            
        Returns:
            标记点节点或None
        """
        import slicer
        
        for i in range(
            slicer.mrmlScene.GetNumberOfNodesByClass("vtkMRMLMarkupsFiducialNode")
        ):
            node = slicer.mrmlScene.GetNthNodeByClass(i, "vtkMRMLMarkupsFiducialNode")
            if node.GetName() == node_name:
                return node
        print(f"Markup node '{node_name}' not found.")
        return None
    
    @staticmethod
    def get_point_ras(node):
        """获取给定标记点的RAS坐标
        
        Args:
            node: 标记点节点
            
        Returns:
            RAS坐标列表或None
        """
        if node is None:
            print("Node is None")
            return None
            
        if node.GetNumberOfControlPoints() > 0:
            point_ras = [0.0, 0.0, 0.0]
            node.GetNthControlPointPosition(0, point_ras)
            return point_ras
        else:
            print(f"Node '{node.GetName()}' exists but has no control points.")
            return None
    
    @staticmethod
    def add_new_fiducial(name):
        """添加一个新的标记点并将其命名
        
        Args:
            name: 标记点名称
        """
        import slicer
        
        markup_fiducial_node = slicer.mrmlScene.AddNewNodeByClass(
            "vtkMRMLMarkupsFiducialNode"
        )
        markup_fiducial_node.SetName(name)
        slicer.modules.markups.logic().SetActiveListID(markup_fiducial_node)

        # 自动进入放置模式
        interaction_node = slicer.mrmlScene.GetNodeByID(
            "vtkMRMLInteractionNodeSingleton"
        )
        interaction_node.SetCurrentInteractionMode(interaction_node.Place)
