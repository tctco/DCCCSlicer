"""
Metric calculation module for PET semi-quantitative analysis

Contains functionality for Centiloid, CenTauR, and CenTauRz calculations
using the new subcommand-based CentiloidCalculator executable
"""

import os
import subprocess
from pathlib import Path

import qt
import slicer


class MetricCalculatorLogic:
    """Logic class for PET metric calculations"""
    
    # Metric to subcommand mapping
    METRIC_SUBCOMMANDS = {
        "Centiloid": "centiloid",
        "CenTauR": "centaur", 
        "CenTauRz": "centaurz",
        "Fill States": "fillstates",
    }
    
    # Config file mapping
    CONFIG_FILES = {
        "SPM style": None,  # Use default config.toml
        "Fast and Accurate": "config.fast_and_acc.toml"
    }
    
    def __init__(self, plugin_path):
        """Initialize metric calculator
        
        Args:
            plugin_path: Plugin directory path
        """
        self.plugin_path = Path(plugin_path)
        self.executable_path = self.plugin_path / "cpp" / "CentiloidCalculator"
        self.last_volume_name = None
        self._current_process = None
        self._process_stdout = []
        self._process_stderr = []
        self._async_callback = None
        self._async_context = None
    
    def calculate_metric(
        self,
        input_node,
        metric_type,
        algorithm_style="SPM style",
        manual_fov=False,
        iterative=False,
        skip_normalization=False,
        tracer=None,
        **kwargs,
    ):
        """Calculate specified metric for input volume
        
        Args:
            input_node: Input volume node
            metric_type: Type of metric (Centiloid/CenTauR/CenTauRz)
            algorithm_style: Algorithm style for config selection
            manual_fov: Enable manual FOV mode
            iterative: Enable iterative processing
            skip_normalization: Skip spatial normalization step
            **kwargs: Additional parameters
            
        Returns:
            tuple: (success, result_text, error_message)
        """
        if not input_node:
            return False, "", "Invalid input node"
            
        if metric_type not in self.METRIC_SUBCOMMANDS:
            return False, "", f"Unsupported metric type: {metric_type}"
            
        try:
            # Save input node and record its name
            self.last_volume_name = input_node.GetName()
            tmp_path = str(self.plugin_path / "tmp.nii")
            slicer.util.saveNode(input_node, tmp_path)
            
            # Get output path
            output_path = str(self.plugin_path / "Normalized.nii")
            
            # Build command
            cmd = self._build_command(
                metric_type,
                tmp_path,
                output_path,
                algorithm_style,
                manual_fov,
                iterative,
                skip_normalization,
                tracer=tracer,
            )
            
            # Execute calculation
            success, stdout, stderr = self._execute_command(cmd)
            
            if not success:
                error_msg = f"Failed to calculate {metric_type}"
                if stderr:
                    error_msg += f"\nError: {stderr}"
                if stdout:
                    error_msg += f"\nOutput: {stdout}"
                return False, "", error_msg
                
            polished_output = self._polish_output(stdout)
            return True, polished_output, ""
            
        except Exception as e:
            return False, "", f"Error during {metric_type} calculation: {str(e)}"

    def calculate_metric_async(
        self,
        input_node,
        metric_type,
        algorithm_style="SPM style",
        manual_fov=False,
        iterative=False,
        skip_normalization=False,
        tracer=None,
        callback=None,
        **kwargs,
    ):
        """Asynchronously calculate specified metric for input volume."""

        if not input_node:
            return self._finalize_async_callback(False, "", "Invalid input node", callback)

        if metric_type not in self.METRIC_SUBCOMMANDS:
            return self._finalize_async_callback(
                False, "", f"Unsupported metric type: {metric_type}", callback
            )

        if self._current_process is not None:
            error_message = (
                "Another calculation is already running. Please wait for it to finish."
            )
            if callback:
                callback(False, "", error_message)
            return False

        try:
            self.last_volume_name = input_node.GetName()
            tmp_path = str(self.plugin_path / "tmp.nii")
            slicer.util.saveNode(input_node, tmp_path)

            output_path = str(self.plugin_path / "Normalized.nii")
            cmd = self._build_command(
                metric_type,
                tmp_path,
                output_path,
                algorithm_style,
                manual_fov,
                iterative,
                skip_normalization,
                tracer=tracer,
            )

            context = {
                "failure_message": f"Failed to calculate {metric_type}",
                "operation": "metric"
            }
            return self._start_async_process(cmd, callback, context)
        except Exception as e:
            return self._finalize_async_callback(
                False, "", f"Error during {metric_type} calculation: {str(e)}", callback
            )
    
    def _build_command(
        self,
        metric_type,
        input_path,
        output_path,
        algorithm_style,
        manual_fov,
        iterative,
        skip_normalization,
        tracer=None,
    ):
        """Build command arguments for metric calculation
        
        Args:
            metric_type: Type of metric
            input_path: Path to input file
            output_path: Path to output file
            algorithm_style: Algorithm style for config selection
            manual_fov: Enable manual FOV mode
            iterative: Enable iterative processing
            skip_normalization: Skip spatial normalization step
            
        Returns:
            list: Command arguments
        """
        subcommand = self.METRIC_SUBCOMMANDS[metric_type]
        
        cmd = [
            str(self.executable_path),
            subcommand,
            "--input",
            input_path,
            "--output",
            output_path,
            "--suvr",
        ]
    
        
        # Add config file if specified
        config_file = self.CONFIG_FILES.get(algorithm_style)
        if config_file:
            cmd.extend(["--config", config_file])
        
        # Add optional flags
        if manual_fov:
            cmd.append("--manual-fov")
        if iterative:
            cmd.append("--iterative")
        if skip_normalization:
            cmd.append("--skip-normalization")

        # Add tracer option for Fill States metric
        if subcommand == "fillstates":
            tracer_value = (tracer or "").strip().lower()
            if not tracer_value:
                raise ValueError(
                    "Tracer must be specified for Fill States metric (fbp, fdg, ftp)."
                )
            if '--suvr' in cmd:
                cmd.remove('--suvr')
            cmd.extend(["--tracer", tracer_value])

        return cmd

    def _start_async_process(self, cmd, callback, context):
        """Start external command asynchronously."""

        process = qt.QProcess()
        process.setProgram(cmd[0])
        process.setArguments(cmd[1:])
        process.setWorkingDirectory(str(self.plugin_path))
        process.setProcessChannelMode(qt.QProcess.SeparateChannels)

        self._current_process = process
        self._async_callback = callback
        self._async_context = context or {}
        self._process_stdout = []
        self._process_stderr = []

        process.readyReadStandardOutput.connect(self._handle_async_stdout)
        process.readyReadStandardError.connect(self._handle_async_stderr)
        process.finished.connect(self._on_async_finished)
        process.errorOccurred.connect(self._on_async_error)
        process.start()

        return True

    def _handle_async_stdout(self):
        if not self._current_process:
            return
        data = self._current_process.readAllStandardOutput()
        if data:
            try:
                text = data.data().decode("utf-8", errors="ignore")
            except AttributeError:
                # Fallback for PyQt implementations where QByteArray lacks data()
                text = bytearray(data).decode("utf-8", errors="ignore")
            self._process_stdout.append(text)

    def _handle_async_stderr(self):
        if not self._current_process:
            return
        data = self._current_process.readAllStandardError()
        if data:
            try:
                text = data.data().decode("utf-8", errors="ignore")
            except AttributeError:
                text = bytearray(data).decode("utf-8", errors="ignore")
            self._process_stderr.append(text)

    def _on_async_finished(self, exit_code, exit_status=qt.QProcess.NormalExit):
        if self._current_process is None:
            return

        stdout = "".join(self._process_stdout)
        stderr = "".join(self._process_stderr)
        context = self._async_context or {}
        failure_message = context.get("failure_message", "Process failed")

        if exit_status == qt.QProcess.NormalExit and exit_code == 0:
            result_text = self._polish_output(stdout)
            self._finalize_async_callback(True, result_text, "")
        else:
            error_msg = failure_message
            if stderr.strip():
                error_msg += f"\nError: {stderr.strip()}"
            if stdout.strip():
                error_msg += f"\nOutput: {stdout.strip()}"
            self._finalize_async_callback(False, "", error_msg)

    def _on_async_error(self, process_error):
        if self._current_process is None:
            return

        error_map = {
            qt.QProcess.FailedToStart: "Failed to start external process. Check executable permissions.",
            qt.QProcess.Crashed: "The external process crashed during execution.",
            qt.QProcess.Timedout: "The external process timed out.",
            qt.QProcess.WriteError: "Write error occurred while communicating with the process.",
            qt.QProcess.ReadError: "Read error occurred while communicating with the process."
        }

        context = self._async_context or {}
        failure_message = context.get("failure_message", "Process failed")
        specific_message = error_map.get(process_error, "An unknown process error occurred.")
        stderr = "".join(self._process_stderr).strip()
        if stderr:
            specific_message += f"\nError: {stderr}"

        self._finalize_async_callback(False, "", f"{failure_message}\n{specific_message}")

    def _finalize_async_callback(self, success, result_text, error_message, callback_override=None):
        """Finalize asynchronous execution, clean up process, and invoke callback."""

        callback = callback_override or self._async_callback
        process = self._current_process
        if process:
            try:
                process.readyReadStandardOutput.disconnect(self._handle_async_stdout)
            except TypeError:
                pass
            try:
                process.readyReadStandardError.disconnect(self._handle_async_stderr)
            except TypeError:
                pass
            try:
                process.finished.disconnect(self._on_async_finished)
            except TypeError:
                pass
            try:
                process.errorOccurred.disconnect(self._on_async_error)
            except TypeError:
                pass
            process.deleteLater()

        self._current_process = None
        self._async_callback = None
        self._async_context = None
        self._process_stdout = []
        self._process_stderr = []

        if callback:
            callback(success, result_text, error_message)

        return success
    
    def _execute_command(self, cmd):
        """Execute the metric calculation command
        
        Args:
            cmd: Command arguments list
            
        Returns:
            tuple: (success, stdout, stderr)
        """
        try:
            print(f"Running command: {' '.join(cmd)}")
            
            result = subprocess.run(cmd, capture_output=True, text=True, cwd=self.plugin_path)
            success = result.returncode == 0
            
            return success, result.stdout, result.stderr
            
        except Exception as e:
            return False, "", str(e)
    
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
            # Skip other common log patterns if needed
            if line.strip().startswith("Starting"):
                continue
            if line.strip().startswith("VOI Mask"):
                continue
            if line.strip().startswith("Reference Mask"):
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
    
    def get_additional_info(self, node):
        """Get additional DICOM information for a node
        
        Args:
            node: Volume node
            
        Returns:
            str: Formatted additional information
        """
        node_name = node.GetName()
        print(f"Node Name: {node_name}")

        additional_information = node_name
        inst_uids = node.GetAttribute("DICOM.instanceUIDs")
        
        if inst_uids:
            inst_uids = inst_uids.split()
            try:
                patient_name = slicer.dicomDatabase.instanceValue(inst_uids[0], "0010,0010")
                study_description = slicer.dicomDatabase.instanceValue(inst_uids[0], "0008,1030")
                study_date = slicer.dicomDatabase.instanceValue(inst_uids[0], "0008,0020")
                additional_information += f"\nPatient Name: {patient_name}\nStudy Date: {study_date}\n\nStudy Description: {study_description}"
            except Exception as e:
                print(f"Failed to get DICOM info: {e}")
                
        return additional_information
    
    def load_normalized_volume(self):
        """Load the normalized output volume
        
        Returns:
            Volume node or None
        """
        nii_file_path = str(self.plugin_path / "Normalized.nii")
        
        # Use custom name if available
        properties = {}
        if self.last_volume_name:
            properties["name"] = f"{self.last_volume_name}_normalized"
            
        return self._load_volume(nii_file_path, properties)
    
    def _load_volume(self, path, properties=None):
        """Load volume file with optional properties
        
        Args:
            path: File path
            properties: Optional properties dict
            
        Returns:
            Volume node or None
        """
        if not os.path.exists(path):
            slicer.util.errorDisplay(
                f"File not found: {path}. It seems that you haven't done any calculation yet."
            )
            return None
            
        if properties is None:
            properties = {}
            
        volume_node = slicer.util.loadVolume(path, properties)
        if not volume_node:
            slicer.util.errorDisplay(
                f"Failed to load volume from {path}. This may be a bug :("
            )
            return None
            
        return volume_node
    
    def calculate_suvr(self, input_node, roi_node, ref_node, algorithm_style="SPM style",
                      manual_fov=False, iterative=False, skip_normalization=False, **kwargs):
        """Calculate SUVr using specified ROI and reference masks
        
        Args:
            input_node: Input PET volume node
            roi_node: ROI mask volume node
            ref_node: Reference mask volume node
            algorithm_style: Algorithm style for config selection
            manual_fov: Enable manual FOV mode
            iterative: Enable iterative processing
            skip_normalization: Skip spatial normalization step
            **kwargs: Additional parameters
            
        Returns:
            tuple: (success, result_text, error_message)
        """
        if not input_node:
            return False, "", "Invalid input node"
        if not roi_node:
            return False, "", "Invalid ROI mask node"
        if not ref_node:
            return False, "", "Invalid reference mask node"
            
        try:
            # Save input node and record its name
            self.last_volume_name = input_node.GetName()
            input_path = str(self.plugin_path / "tmp.nii")
            roi_path = str(self.plugin_path / "roi.nii")
            ref_path = str(self.plugin_path / "ref.nii")
            output_path = str(self.plugin_path / "Normalized.nii")
            
            # Save all nodes as temporary files
            slicer.util.saveNode(input_node, input_path)
            slicer.util.saveNode(roi_node, roi_path)
            slicer.util.saveNode(ref_node, ref_path)
            
            # Build SUVr calculation command by reusing build logic
            cmd = self._build_suvr_command(
                input_path, output_path, roi_path, ref_path, algorithm_style,
                manual_fov, iterative, skip_normalization
            )
            
            # Execute calculation
            success, stdout, stderr = self._execute_command(cmd)
            
            if not success:
                error_msg = "Failed to calculate SUVr"
                if stderr:
                    error_msg += f"\nError: {stderr}"
                if stdout:
                    error_msg += f"\nOutput: {stdout}"
                return False, "", error_msg
                
            polished_output = self._polish_output(stdout)
            return True, polished_output, ""
            
        except Exception as e:
            return False, "", f"Error during SUVr calculation: {str(e)}"

    def calculate_suvr_async(self, input_node, roi_node, ref_node,
                              algorithm_style="SPM style", manual_fov=False,
                              iterative=False, skip_normalization=False, callback=None,
                              **kwargs):
        """Asynchronously calculate SUVr using specified ROI and reference masks."""

        if not input_node:
            return self._finalize_async_callback(False, "", "Invalid input node", callback)
        if not roi_node:
            return self._finalize_async_callback(False, "", "Invalid ROI mask node", callback)
        if not ref_node:
            return self._finalize_async_callback(False, "", "Invalid reference mask node", callback)
        if self._current_process is not None:
            error_message = (
                "Another calculation is already running. Please wait for it to finish."
            )
            if callback:
                callback(False, "", error_message)
            return False

        try:
            self.last_volume_name = input_node.GetName()
            input_path = str(self.plugin_path / "tmp.nii")
            roi_path = str(self.plugin_path / "roi.nii")
            ref_path = str(self.plugin_path / "ref.nii")
            output_path = str(self.plugin_path / "Normalized.nii")

            slicer.util.saveNode(input_node, input_path)
            slicer.util.saveNode(roi_node, roi_path)
            slicer.util.saveNode(ref_node, ref_path)

            cmd = self._build_suvr_command(
                input_path, output_path, roi_path, ref_path, algorithm_style,
                manual_fov, iterative, skip_normalization
            )

            context = {
                "failure_message": "Failed to calculate SUVr",
                "operation": "suvr"
            }
            return self._start_async_process(cmd, callback, context)
        except Exception as e:
            return self._finalize_async_callback(
                False, "", f"Error during SUVr calculation: {str(e)}", callback
            )
    
    def _build_suvr_command(self, input_path, output_path, roi_path, ref_path, algorithm_style,
                           manual_fov, iterative, skip_normalization):
        """Build command arguments for SUVr calculation
        
        Args:
            input_path: Path to input PET file
            output_path: Path to output file
            roi_path: Path to ROI mask file
            ref_path: Path to reference mask file
            algorithm_style: Algorithm style for config selection
            manual_fov: Enable manual FOV mode
            iterative: Enable iterative processing
            skip_normalization: Skip spatial normalization step
            
        Returns:
            list: Command arguments
        """
        # Build base command for SUVr
        cmd = [
            str(self.executable_path),
            "suvr",
            "--input", input_path,
            "--output", output_path,
            "--voi-mask", roi_path,
            "--ref-mask", ref_path
        ]
        
        # Add config file if specified (reuse from _build_command logic)
        config_file = self.CONFIG_FILES.get(algorithm_style)
        if config_file:
            cmd.extend(["--config", config_file])
        
        # Add optional flags (reuse from _build_command logic)
        if manual_fov:
            cmd.append("--manual-fov")
        if iterative:
            cmd.append("--iterative")
        if skip_normalization:
            cmd.append("--skip-normalization")
            
        return cmd

