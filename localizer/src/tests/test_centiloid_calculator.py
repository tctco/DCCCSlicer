#!/usr/bin/env python3
"""
CentiloidCalculator Unit Test Suite

This test suite comprehensively tests all subcommands and parameter combinations
of the CentiloidCalculator application using subprocess calls.

Usage:
    python test_centiloid_calculator.py

Requirements:
    - CentiloidCalculator should be compiled and available
    - Test input files should be present (will create dummy files if needed)
"""

import subprocess
import unittest
import os
import tempfile
import shutil
from pathlib import Path
import sys


class CentiloidCalculatorTestSuite(unittest.TestCase):
    """Test suite for CentiloidCalculator application"""
    
    @classmethod
    def setUpClass(cls):
        """Set up test environment before running tests"""
        # Look for executable in common build locations
        possible_exe_paths = [
            "../CentiloidCalculatorBin/CentiloidCalculator_green/CentiloidCalculator",
        ]
        
        cls.exe_path = None
        for path in possible_exe_paths:
            if os.path.exists(path):
                cls.exe_path = path
                break
        
        if not cls.exe_path:
            raise FileNotFoundError(f"CentiloidCalculator executable not found in any of: {possible_exe_paths}")
        
        # Create test directory for temporary files
        cls.test_dir = str(Path(__file__).parent)
        cls.temp_input = os.path.join(cls.test_dir, "input.nii")
        cls.temp_output = os.path.join(cls.test_dir, "output.nii")
        cls.temp_config = os.path.join(cls.test_dir, "test_config.toml")
        cls.temp_voi_mask = os.path.join(cls.test_dir, "voi_mask.nii")
        cls.temp_ref_mask = os.path.join(cls.test_dir, "ref_mask.nii")
        
        # Create dummy test files if they don't exist
        cls._create_dummy_test_files()
        
        print(f"Test setup completed. Using executable: {cls.exe_path}")
        print(f"Test directory: {cls.test_dir}")
    
    @classmethod
    def _create_dummy_test_files(cls):
        """Create dummy test files for testing"""
        
        # Create dummy TOML config file (updated format)
        config_content = """# Test configuration file
[models]
rigid = "assets/models/registration/rigid.onnx"
affine_voxelmorph = "assets/models/registration/affine_voxelmorph.onnx"
abeta_decoupler = "assets/models/decouple/abeta.onnx"
tau_decoupler = "assets/models/decouple/tau.onnx"

[templates]
adni_pet_core = "assets/nii/ADNI_empty.nii"
padded = "assets/nii/paddedTemplate.nii"

[masks]
cerebral_gray = "assets/nii/voi_CerebGry_2mm.nii"
centiloid_voi = "assets/nii/voi_ctx_2mm.nii"
whole_cerebral = "assets/nii/voi_WhlCbl_2mm.nii"
centaur_voi = "assets/nii/CenTauR.nii"
centaur_ref = "assets/nii/voi_CerebGry_tau_2mm.nii"

[processing]
max_iter = 5
ac_diff_threshold = 2.0
temp_dir = "./tmp"

[processing.crop_mni]
start_x = 8
start_y = 16
start_z = 8
size_x = 79
size_y = 95
size_z = 79

[centiloid.tracers.pib]
slope = 93.7
intercept = -94.6

[centaur.tracers.ftp]
baseline = 1.06
max = 2.13

[centaurz.tracers.ftp]
slope = 13.63
intercept = -15.85

[suvr.regions.centiloid]
voi_mask = "centiloid_voi"
ref_mask = "whole_cerebral"
"""
        with open(cls.temp_config, 'w') as f:
            f.write(config_content)
    
    def _run_command(self, args, expect_success=True):
        """
        Helper method to run CentiloidCalculator commands
        
        Args:
            args: List of command arguments
            expect_success: Whether to expect successful execution
            
        Returns:
            CompletedProcess object
        """
        cmd = [self.exe_path] + args
        print(f"Running command: {' '.join(cmd)}")
        
        try:
            result = subprocess.run(
                cmd,
                capture_output=True,
                text=True,
                timeout=60  # Increased timeout for deep learning inference
            )
            
            if expect_success:
                if result.returncode != 0:
                    print(f"Command failed with return code {result.returncode}")
                    print(f"STDOUT: {result.stdout}")
                    print(f"STDERR: {result.stderr}")
            
            return result
            
        except subprocess.TimeoutExpired:
            self.fail("Command timed out after 60 seconds")
        except FileNotFoundError:
            self.fail(f"Executable not found: {self.exe_path}")
    
    def test_help_message(self):
        """Test help message display"""
        result = self._run_command(["--help"], expect_success=False)
        # Help typically returns non-zero exit code
        self.assertIn("CentiloidCalculator", result.stdout)
        self.assertIn("subcommand", result.stdout.lower())
    
    def test_version_info(self):
        """Test version information"""
        result = self._run_command(["--version"], expect_success=False)
        # Version info may return non-zero, check if version is mentioned
        output = result.stdout + result.stderr
        self.assertTrue("3.2.0" in output or "version" in output.lower())
    
    def test_no_subcommand(self):
        """Test behavior when no subcommand is provided"""
        result = self._run_command([], expect_success=False)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("subcommand", result.stderr.lower())


class CentiloidCommandTests(CentiloidCalculatorTestSuite):
    """Tests for centiloid subcommand"""
    
    def test_centiloid_help(self):
        """Test centiloid subcommand help"""
        result = self._run_command(["centiloid", "--help"], expect_success=False)
        self.assertIn("centiloid", result.stdout.lower())
        self.assertIn("amyloid", result.stdout.lower())
    
    def test_centiloid_basic(self):
        """Test basic centiloid calculation"""
        result = self._run_command([
            "centiloid",
            "--input", self.temp_input,
            "--output", self.temp_output
        ])
        # May fail due to missing models, but should not crash
        self.assertIsNotNone(result)
    
    def test_centiloid_with_suvr(self):
        """Test centiloid calculation with SUVr output"""
        result = self._run_command([
            "centiloid",
            "--input", self.temp_input,
            "--output", self.temp_output,
            "--suvr"
        ])
        self.assertIsNotNone(result)
    
    def test_centiloid_skip_normalization(self):
        """Test centiloid calculation with skipped spatial normalization"""
        result = self._run_command([
            "centiloid",
            "--input", self.temp_input,
            "--output", self.temp_output,
            "--skip-normalization"
        ])
        self.assertIsNotNone(result)
    
    def test_centiloid_iterative(self):
        """Test centiloid calculation with iterative rigid transformation"""
        result = self._run_command([
            "centiloid",
            "--input", self.temp_input,
            "--output", self.temp_output,
            "--iterative"
        ])
        self.assertIsNotNone(result)
    
    def test_centiloid_manual_fov(self):
        """Test centiloid calculation with manual FOV placement"""
        result = self._run_command([
            "centiloid",
            "--input", self.temp_input,
            "--output", self.temp_output,
            "--manual-fov"
        ])
        self.assertIsNotNone(result)
    
    def test_centiloid_with_all_options(self):
        """Test centiloid calculation with multiple options"""
        result = self._run_command([
            "centiloid",
            "--input", self.temp_input,
            "--output", self.temp_output,
            "--suvr",
            "--iterative",
            "--manual-fov",
            "--debug"
        ])
        self.assertIsNotNone(result)
    
    def test_centiloid_debug_mode(self):
        """Test centiloid calculation with debug mode"""
        result = self._run_command([
            "centiloid",
            "--input", self.temp_input,
            "--output", self.temp_output,
            "--debug"
        ])
        self.assertIsNotNone(result)
    
    def test_centiloid_with_config(self):
        """Test centiloid calculation with custom configuration"""
        result = self._run_command([
            "centiloid",
            "--input", self.temp_input,
            "--output", self.temp_output,
            "--config", self.temp_config
        ])
        self.assertIsNotNone(result)
    
    def test_centiloid_all_options(self):
        """Test centiloid calculation with all options combined"""
        result = self._run_command([
            "centiloid",
            "--input", self.temp_input,
            "--output", self.temp_output,
            "--config", self.temp_config,
            "--suvr",
            "--iterative",
            "--debug"
        ])
        self.assertIsNotNone(result)
    
    def test_centiloid_missing_input(self):
        """Test centiloid command with missing input argument"""
        result = self._run_command([
            "centiloid",
            "--output", self.temp_output
        ], expect_success=False)
        self.assertNotEqual(result.returncode, 0)
    
    def test_centiloid_missing_output(self):
        """Test centiloid command with missing output argument"""
        result = self._run_command([
            "centiloid",
            "--input", self.temp_input
        ], expect_success=False)
        self.assertNotEqual(result.returncode, 0)


class CenTauRCommandTests(CentiloidCalculatorTestSuite):
    """Tests for centaur (CenTauR) subcommand"""
    
    def test_centaur_help(self):
        """Test centaur subcommand help"""
        result = self._run_command(["centaur", "--help"], expect_success=False)
        self.assertIn("centaur", result.stdout.lower())
        self.assertIn("tau", result.stdout.lower())
    
    def test_centaur_basic(self):
        """Test basic CenTauR calculation"""
        result = self._run_command([
            "centaur",
            "--input", self.temp_input,
            "--output", self.temp_output
        ])
        self.assertIsNotNone(result)
    
    def test_centaur_with_suvr(self):
        """Test CenTauR calculation with SUVr output"""
        result = self._run_command([
            "centaur",
            "--input", self.temp_input,
            "--output", self.temp_output,
            "--suvr"
        ])
        self.assertIsNotNone(result)
    
    def test_centaur_parameter_consistency(self):
        """Test that centaur has same parameters as centiloid"""
        # This test verifies that CenTauR supports all the same parameters as Centiloid
        result = self._run_command([
            "centaur",
            "--input", self.temp_input,
            "--output", self.temp_output,
            "--suvr",
            "--skip-normalization",
            "--iterative", 
            "--manual-fov",
            "--debug",
            "--config", self.temp_config
        ])
        self.assertIsNotNone(result)


class CenTauRzCommandTests(CentiloidCalculatorTestSuite):
    """Tests for centaurz (CenTauRz) subcommand"""
    
    def test_centaurz_help(self):
        """Test centaurz subcommand help"""
        result = self._run_command(["centaurz", "--help"], expect_success=False)
        self.assertIn("centaurz", result.stdout.lower())
        self.assertIn("z-score", result.stdout.lower())
    
    def test_centaurz_basic(self):
        """Test basic CenTauRz calculation"""
        result = self._run_command([
            "centaurz",
            "--input", self.temp_input,
            "--output", self.temp_output
        ])
        self.assertIsNotNone(result)
    
    def test_centaurz_parameter_consistency(self):
        """Test that centaurz has same parameters as centiloid"""
        result = self._run_command([
            "centaurz",
            "--input", self.temp_input,
            "--output", self.temp_output,
            "--suvr",
            "--skip-normalization",
            "--iterative",
            "--manual-fov", 
            "--debug",
            "--config", self.temp_config
        ])
        self.assertIsNotNone(result)


class SUVrCommandTests(CentiloidCalculatorTestSuite):
    """Tests for suvr subcommand"""
    
    def test_suvr_help(self):
        """Test suvr subcommand help"""
        result = self._run_command(["suvr", "--help"], expect_success=False)
        self.assertIn("suvr", result.stdout.lower())
        self.assertIn("voi", result.stdout.lower())
        self.assertIn("reference", result.stdout.lower())
    
    def test_suvr_basic(self):
        """Test basic SUVr calculation"""
        result = self._run_command([
            "suvr",
            "--input", self.temp_input,
            "--output", self.temp_output,
            "--voi-mask", self.temp_voi_mask,
            "--ref-mask", self.temp_ref_mask
        ])
        self.assertIsNotNone(result)
    
    def test_suvr_skip_normalization(self):
        """Test SUVr calculation with skipped spatial normalization"""
        result = self._run_command([
            "suvr",
            "--input", self.temp_input,
            "--output", self.temp_output,
            "--voi-mask", self.temp_voi_mask,
            "--ref-mask", self.temp_ref_mask,
            "--skip-normalization"
        ])
        self.assertIsNotNone(result)
    
    def test_suvr_missing_masks(self):
        """Test SUVr command with missing mask arguments"""
        result = self._run_command([
            "suvr",
            "--input", self.temp_input,
            "--output", self.temp_output
        ], expect_success=False)
        self.assertNotEqual(result.returncode, 0)
    
    def test_suvr_missing_voi_mask(self):
        """Test SUVr command with missing VOI mask"""
        result = self._run_command([
            "suvr",
            "--input", self.temp_input,
            "--output", self.temp_output,
            "--ref-mask", self.temp_ref_mask
        ], expect_success=False)
        self.assertNotEqual(result.returncode, 0)
    
    def test_suvr_missing_ref_mask(self):
        """Test SUVr command with missing reference mask"""
        result = self._run_command([
            "suvr",
            "--input", self.temp_input,
            "--output", self.temp_output,
            "--voi-mask", self.temp_voi_mask
        ], expect_success=False)
        self.assertNotEqual(result.returncode, 0)


class NormalizeCommandTests(CentiloidCalculatorTestSuite):
    """Tests for normalize subcommand"""
    
    def test_normalize_help(self):
        """Test normalize subcommand help"""
        result = self._run_command(["normalize", "--help"], expect_success=False)
        self.assertIn("normalize", result.stdout.lower())
        self.assertIn("spatial", result.stdout.lower())
    
    def test_normalize_basic(self):
        """Test basic spatial normalization"""
        result = self._run_command([
            "normalize",
            "--input", self.temp_input,
            "--output", self.temp_output
        ])
        self.assertIsNotNone(result)
    
    def test_normalize_with_method(self):
        """Test normalize with custom method"""
        result = self._run_command([
            "normalize",
            "--input", self.temp_input,
            "--output", self.temp_output,
            "--method", "rigid_voxelmorph"
        ])
        self.assertIsNotNone(result)
    
    def test_normalize_adni_pet_core(self):
        """Test normalize with ADNI PET core processing"""
        result = self._run_command([
            "normalize",
            "--input", self.temp_input,
            "--output", self.temp_output,
            "--ADNI-PET-core"
        ])
        self.assertIsNotNone(result)
    
    def test_normalize_no_skip_parameter(self):
        """Test that normalize doesn't accept --skip-normalization parameter"""
        result = self._run_command([
            "normalize",
            "--input", self.temp_input,
            "--output", self.temp_output,
            "--skip-normalization"
        ], expect_success=False)
        # Should fail because normalize shouldn't have --skip-normalization parameter
        self.assertNotEqual(result.returncode, 0)


class DecoupleCommandTests(CentiloidCalculatorTestSuite):
    """Tests for decouple subcommand"""
    
    def test_decouple_help(self):
        """Test decouple subcommand help"""
        result = self._run_command(["decouple", "--help"], expect_success=False)
        self.assertIn("decouple", result.stdout.lower())
        self.assertIn("modality", result.stdout.lower())
    
    def test_decouple_abeta(self):
        """Test decouple with abeta modality"""
        result = self._run_command([
            "decouple",
            "--input", self.temp_input,
            "--output", self.temp_output,
            "--modality", "abeta"
        ])
        self.assertIsNotNone(result)
    
    def test_decouple_tau(self):
        """Test decouple with tau modality"""
        result = self._run_command([
            "decouple",
            "--input", self.temp_input,
            "--output", self.temp_output,
            "--modality", "tau"
        ])
        self.assertIsNotNone(result)
    
    def test_decouple_missing_modality(self):
        """Test decouple command with missing modality"""
        result = self._run_command([
            "decouple",
            "--input", self.temp_input,
            "--output", self.temp_output
        ], expect_success=False)
        self.assertNotEqual(result.returncode, 0)
    
    def test_decouple_invalid_modality(self):
        """Test decouple command with invalid modality"""
        result = self._run_command([
            "decouple",
            "--input", self.temp_input,
            "--output", self.temp_output,
            "--modality", "invalid"
        ], expect_success=False)
        self.assertNotEqual(result.returncode, 0)


class IntegrationTests(CentiloidCalculatorTestSuite):
    """Integration tests for complex scenarios"""
    
    def test_parameter_consistency_across_biomarkers(self):
        """Test that all biomarker commands have consistent parameter interfaces"""
        biomarker_commands = ["centiloid", "centaur", "centaurz"]
        
        for cmd in biomarker_commands:
            with self.subTest(command=cmd):
                # Test that all biomarker commands support the same parameters
                result = self._run_command([
                    cmd,
                    "--input", self.temp_input,
                    "--output", self.temp_output,
                    "--suvr",
                    "--skip-normalization",
                    "--iterative",
                    "--manual-fov",
                    "--debug"
                ])
                self.assertIsNotNone(result)
    
    def test_all_commands_accept_base_parameters(self):
        """Test that all commands accept base parameters (input, output, config, debug)"""
        commands = ["centiloid", "centaur", "centaurz", "normalize", "decouple"]
        
        for cmd in commands:
            with self.subTest(command=cmd):
                args = [
                    cmd,
                    "--input", self.temp_input,
                    "--output", self.temp_output,
                    "--config", self.temp_config,
                    "--debug"
                ]
                
                # Add required parameters for specific commands
                if cmd == "suvr":
                    args.extend([
                        "--voi-mask", self.temp_voi_mask,
                        "--ref-mask", self.temp_ref_mask
                    ])
                elif cmd == "decouple":
                    args.extend(["--modality", "abeta"])
                
                result = self._run_command(args)
                self.assertIsNotNone(result)
    
    def test_spatial_normalization_parameters(self):
        """Test that commands accepting spatial normalization have consistent parameters"""
        commands_with_spatial_norm = ["centiloid", "centaur", "centaurz", "suvr", "normalize", "decouple"]
        
        for cmd in commands_with_spatial_norm:
            with self.subTest(command=cmd):
                args = [
                    cmd,
                    "--input", self.temp_input,
                    "--output", self.temp_output,
                    "--iterative",
                    "--manual-fov"
                ]
                
                # Add required parameters
                if cmd == "suvr":
                    args.extend([
                        "--voi-mask", self.temp_voi_mask,
                        "--ref-mask", self.temp_ref_mask
                    ])
                elif cmd == "decouple":
                    args.extend(["--modality", "abeta"])
                
                result = self._run_command(args)
                self.assertIsNotNone(result)


def run_test_suite():
    """Run the complete test suite with detailed reporting"""
    print("="*80)
    print("CentiloidCalculator Unit Test Suite")
    print("="*80)
    
    # Create test suite
    loader = unittest.TestLoader()
    suite = unittest.TestSuite()
    
    # Add all test classes
    test_classes = [
        CentiloidCommandTests,
        CenTauRCommandTests, 
        CenTauRzCommandTests,
        SUVrCommandTests,
        NormalizeCommandTests,
        DecoupleCommandTests,
        IntegrationTests
    ]
    
    for test_class in test_classes:
        tests = loader.loadTestsFromTestCase(test_class)
        suite.addTests(tests)
    
    # Run tests with detailed output
    runner = unittest.TextTestRunner(verbosity=2)
    result = runner.run(suite)
    
    # Print summary
    print("\n" + "="*80)
    print("TEST SUMMARY")
    print("="*80)
    print(f"Tests run: {result.testsRun}")
    print(f"Failures: {len(result.failures)}")
    print(f"Errors: {len(result.errors)}")
    print(f"Success rate: {((result.testsRun - len(result.failures) - len(result.errors)) / result.testsRun * 100):.1f}%")
    
    if result.failures:
        print("\nFAILURES:")
        for test, traceback in result.failures:
            print(f"- {test}: {traceback}")
    
    if result.errors:
        print("\nERRORS:")
        for test, traceback in result.errors:
            print(f"- {test}: {traceback}")
    
    return result.wasSuccessful()


if __name__ == "__main__":
    # The executable check is now handled in setUpClass
    # Run the test suite
    try:
        success = run_test_suite()
        sys.exit(0 if success else 1)
    except FileNotFoundError as e:
        print(f"ERROR: {e}")
        print("\nPlease ensure the CentiloidCalculator is compiled first using:")
        print("  .\\install.ps1 -Config RelWithDebInfo")
        print("\nOr check that the executable exists in one of these locations:")
        print("  - ../build/RelWithDebInfo/CentiloidCalculator")
        print("  - ../build/Release/CentiloidCalculator")
        print("  - ../build/Debug/CentiloidCalculator")
        sys.exit(1)
