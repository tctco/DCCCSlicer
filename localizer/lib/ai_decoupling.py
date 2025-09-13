"""
AI decoupling functionality module

Contains AI-based pathology-specific signal extraction for brain PET images
"""

import os
import subprocess
from pathlib import Path
import slicer


class AIDecouplingLogic:
    """Logic class for AI-based PET image decoupling"""
    
    # Modality mapping for different pathology types
    MODALITY_MAPPING = {
        "Abeta": "abeta",
        "Tau": "tau",
    }
    
    def __init__(self, plugin_path):
        """Initialize AI decoupling logic
        
        Args:
            plugin_path: Plugin directory path
        """
        self.plugin_path = Path(plugin_path)
        self.executable_path = self.plugin_path / "cpp" / "CentiloidCalculator.exe"
        self.executable_dir = self.plugin_path / "cpp"
        self.last_volume_name = None
    
    def decouple_image(self, input_node, modality, **kwargs):
        """Perform AI-based decoupling of PET image
        
        Args:
            input_node: Input volume node
            modality: Modality type (amyloid/tau or abeta/tau)
            **kwargs: Additional parameters (for future extensibility)
            
        Returns:
            tuple: (success, result_text, error_message, foreground_node, background_node, stripped_node)
        """
        if not input_node:
            return False, "", "Invalid input node", None, None, None
            
        # Normalize modality name
        normalized_modality = self._normalize_modality(modality)
        if not normalized_modality:
            return False, "", f"Unsupported modality: {modality}", None, None, None
            
        try:
            # Save input node and record its name
            self.last_volume_name = input_node.GetName()
            tmp_path = str(self.plugin_path / "tmp.nii")
            slicer.util.saveNode(input_node, tmp_path)
            
            # Build command using new subcommand format
            cmd = [
                str(self.executable_path),
                "decouple",
                "--input", tmp_path,
                "--output", str(self.plugin_path / "Normalized.nii"),
                "--modality", normalized_modality
            ]
                
            print(f"Running decouple command: {' '.join(cmd)}")
            result = subprocess.run(cmd, capture_output=True, text=True, cwd=self.plugin_path)
            
            if result.returncode != 0:
                error_msg = f"Failed to decouple {modality} signal"
                if result.stderr:
                    error_msg += f"\nError: {result.stderr}"
                if result.stdout:
                    error_msg += f"\nOutput: {result.stdout}"
                return False, "", error_msg, None, None, None
            
            # Load result files based on expected output patterns
            foreground, background, stripped = self._load_result_volumes()
            
            if foreground is None and background is None and stripped is None:
                return False, "", "Failed to load any result volumes", None, None, None
                
            polished_output = self._polish_output(result.stdout)
            return True, polished_output, "", foreground, background, stripped
            
        except Exception as e:
            return False, "", f"Error during {modality} decoupling: {str(e)}", None, None, None
    
    def _normalize_modality(self, modality):
        """Normalize modality name to standard format
        
        Args:
            modality: Input modality string
            
        Returns:
            str: Normalized modality name or None if invalid
        """
        if not modality:
            return None
            
        # Convert to lowercase for case-insensitive matching
        modality_lower = modality.lower()
        
        # Direct mapping
        if modality_lower in self.MODALITY_MAPPING:
            return self.MODALITY_MAPPING[modality_lower]
            
        # Try partial matching
        for key, value in self.MODALITY_MAPPING.items():
            if modality_lower in key.lower() or key.lower() in modality_lower:
                return value
                
        return None
    
    def _load_result_volumes(self):
        """Load decoupling result volumes
        
        Returns:
            tuple: (foreground_node, background_node, stripped_node)
        """
        # Expected output file patterns (may vary based on CentiloidCalculator version)
        potential_files = {
            "foreground": "Normalized_AD_prob_map.nii",
            "background": "Normalized.nii",
            "stripped": "Normalized_stripped_image.nii",
        }
        
        foreground_node = None
        background_node = None
        stripped_node = None
        
        # Try to load foreground volume
        path = str(self.plugin_path / potential_files["foreground"])
        if os.path.exists(path):
            properties = {"name": f"{self.last_volume_name}_probability_map"} if self.last_volume_name else {}
            foreground_node = self._load_volume(path, properties)
        
        # Try to load background volume  
        path = str(self.plugin_path / potential_files["background"])
        if os.path.exists(path):
            properties = {"name": f"{self.last_volume_name}_background"} if self.last_volume_name else {}
            background_node = self._load_volume(path, properties)
        
        # Try to load stripped volume
        path = str(self.plugin_path / potential_files["stripped"])
        if os.path.exists(path):
            properties = {"name": f"{self.last_volume_name}_stripped"} if self.last_volume_name else {}
            stripped_node = self._load_volume(path, properties)
        
        return foreground_node, background_node, stripped_node
    
    def setup_display_properties(self, foreground_node, background_node):
        """Setup display properties for decoupling result volumes
        
        Args:
            foreground_node: Foreground probability map node
            background_node: Background volume node
        """
        if foreground_node:
            foreground_display_node = foreground_node.GetDisplayNode()
            if foreground_display_node:
                foreground_display_node.SetAndObserveColorNodeID(
                    "vtkMRMLPETProceduralColorNodePET-DICOM"
                )
                foreground_display_node.SetWindow(1)
                foreground_display_node.SetLevel(0.5)
        
        if background_node:
            background_display_node = background_node.GetDisplayNode()
            if background_display_node:
                background_display_node.SetAndObserveColorNodeID(
                    "vtkMRMLColorTableNodeInvertedGrey"
                )
    
    def set_view_background_volume(self, volume_node_id):
        """Set background volume for all slice views
        
        Args:
            volume_node_id: Volume node ID to set as background
        """
        red_slice_node = slicer.mrmlScene.GetNodeByID("vtkMRMLSliceNodeRed")
        yellow_slice_node = slicer.mrmlScene.GetNodeByID("vtkMRMLSliceNodeYellow")
        green_slice_node = slicer.mrmlScene.GetNodeByID("vtkMRMLSliceNodeGreen")

        # Set the new volume as background for each slice view
        for slice_node in [red_slice_node, yellow_slice_node, green_slice_node]:
            slice_logic = slicer.app.applicationLogic().GetSliceLogic(slice_node)
            composite_node = slice_logic.GetSliceCompositeNode()
            composite_node.SetBackgroundVolumeID(volume_node_id)

            # Refresh view to display the new volume
            slice_logic.FitSliceToAll()
    
    def _polish_output(self, output):
        """Polish output text with tracer name expansions and filter unwanted log lines
        
        Args:
            output: Raw output string
            
        Returns:
            str: Polished output
        """
        # Filter out unwanted log lines
        lines = output.split('\n')
        filtered_lines = []
        
        for line in lines:
            # Skip configuration loading messages
            if line.strip().startswith("Loading configuration from:"):
                continue  
            if line.strip().startswith("Starting"):
                continue
            filtered_lines.append(line)
        
        result = '\n'.join(filtered_lines)
        
        # Apply tracer name replacements
        replacements = {
            "FBP": "FBP / AV45 / Florbetapir",
            "FMM": "FMM / Flutemetamol",
            "FBB": "FBB / Florbetaben", 
            "NAV": "NAV4694",
            "FTP": "FTP / AV1451 / Flortaucipir",
            "PM-PBB3": "PM-PBB3 / APN / Florzolotau"
        }
        
        for old, new in replacements.items():
            result = result.replace(old, new)
            
        return result.strip()
    
    def _load_volume(self, path, properties=None):
        """Load volume file with optional properties
        
        Args:
            path: File path
            properties: Optional properties dict for volume loading
            
        Returns:
            Volume node or None
        """
        if not os.path.exists(path):
            print(f"File not found: {path}")
            return None
            
        if properties is None:
            properties = {}
            
        try:
            volume_node = slicer.util.loadVolume(path, properties)
            if not volume_node:
                print(f"Failed to load volume from {path}")
                return None
                
            return volume_node
            
        except Exception as e:
            print(f"Error loading volume from {path}: {e}")
            return None
