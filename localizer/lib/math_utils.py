"""
数学工具函数模块

包含图像变换所需的矩阵计算函数
"""

import numpy as np


def create_translation_matrix(translation):
    """创建平移矩阵
    
    Args:
        translation: 平移向量 [x, y, z]
        
    Returns:
        4x4 平移矩阵
    """
    T = np.eye(4)
    T[:3, 3] = translation
    return T


def create_rotation_matrix(axis, angle):
    """使用罗德里格斯旋转公式创建旋转矩阵
    
    Args:
        axis: 旋转轴向量
        angle: 旋转角度（弧度）
        
    Returns:
        4x4 旋转矩阵
    """
    axis = axis / np.linalg.norm(axis)
    K = np.array(
        [[0, -axis[2], axis[1]], [axis[2], 0, -axis[0]], [-axis[1], axis[0], 0]]
    )
    R = np.eye(3) + np.sin(angle) * K + (1 - np.cos(angle)) * np.dot(K, K)
    rotation_matrix = np.eye(4)
    rotation_matrix[:3, :3] = R
    return rotation_matrix


def create_affine_matrix(ac, bc):
    """创建仿射变换矩阵，将AC-PC线对齐到标准方向
    
    Args:
        ac: AC点坐标
        bc: PC点坐标
        
    Returns:
        4x4 仿射变换矩阵
    """
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
