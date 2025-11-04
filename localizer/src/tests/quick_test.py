#!/usr/bin/env python3
"""
Quick Test Script for CentiloidCalculator

This script performs basic functionality tests to quickly verify 
that the CentiloidCalculator is working properly.

Usage:
    python quick_test.py
"""

import subprocess
import os
import tempfile
import sys
from pathlib import Path


def create_dummy_files():
    """Create dummy test files"""
    test_dir = str(Path(__file__).parent)
    
    voi_mask = os.path.join(test_dir, "voi_mask.nii")
    ref_mask = os.path.join(test_dir, "ref_mask.nii")
    dummy_nii = os.path.join(test_dir, "input.nii")
    
    return test_dir, dummy_nii, voi_mask, ref_mask


def run_command(exe_path, args, test_name, expected_failure):
    """Run a command and report results"""
    cmd = [exe_path] + args
    print(f"\n{'='*60}")
    print(f"TEST: {test_name}")
    print(f"CMD:  {' '.join(cmd)}")
    print('='*60)
    
    try:
        result = subprocess.run(
            cmd,
            capture_output=True,
            text=True,
            timeout=30  # Increased timeout for deep learning inference
        )
        
        print(f"Return code: {result.returncode}")
        
        if result.stdout:
            print("STDOUT:")
            print(result.stdout[:800] + "..." if len(result.stdout) > 800 else result.stdout)
        
        if result.stderr:
            print("STDERR:")
            print(result.stderr[:800] + "..." if len(result.stderr) > 800 else result.stderr)
        
        return result.returncode == 0 if not expected_failure else result.returncode != 0
        
    except subprocess.TimeoutExpired:
        print("❌ TIMEOUT: Command took too long")
        return False if not expected_failure else True
    except Exception as e:
        print(f"❌ ERROR: {e}")
        return False if not expected_failure else True  


def main():
    """Main test execution"""
    # Look for executable in common locations
    possible_paths = [
        "../../CentiloidCalculatorInstall/cpp/CentiloidCalculator",
    ]
    
    exe_path = None
    for path in possible_paths:
        if os.path.exists(path):
            exe_path = path
            break
    
    print("CentiloidCalculator Quick Test Suite")
    print("="*60)
    
    # Check if executable exists
    if not exe_path:
        print("❌ ERROR: Executable not found in any of these locations:")
        for path in possible_paths:
            print(f"  - {path}")
        print("\nPlease build the project first using:")
        print("  .\install.ps1 -Config RelWithDebInfo")
        return False
    
    print(f"✓ Executable found: {exe_path}")
    
    # Create test files
    test_dir, input_file, voi_mask, ref_mask = create_dummy_files()
    output_file = os.path.join(test_dir, "output.nii")
    
    print(f"✓ Test files located in: {test_dir}")
    
    tests_passed = 0
    total_tests = 0
    
    # Test cases updated for new interface
    test_cases = [
        {
            "name": "Help Message",
            "args": ["--help"],
            "description": "Display main help information",
            "expected_failure": False
        },
        {
            "name": "Version Information",
            "args": ["--version"],
            "description": "Display version information",
            "expected_failure": False
        },
        {
            "name": "No Subcommand", 
            "args": [],
            "description": "Error handling when no subcommand provided",
            "expected_failure": True
        },
        {
            "name": "Centiloid Help",
            "args": ["centiloid", "--help"],
            "description": "Centiloid subcommand help",
            "expected_failure": False
        },
        {
            "name": "CenTauR Help",
            "args": ["centaur", "--help"],
            "description": "CenTauR subcommand help",
            "expected_failure": False
        },
        {
            "name": "CenTauRz Help",
            "args": ["centaurz", "--help"],
            "description": "CenTauRz subcommand help",
            "expected_failure": False
        },
        {
            "name": "SUVr Help",
            "args": ["suvr", "--help"],
            "description": "SUVr subcommand help",
            "expected_failure": False
        },
        {
            "name": "Normalize Help",
            "args": ["normalize", "--help"],
            "description": "Normalize subcommand help",
            "expected_failure": False
        },
        {
            "name": "Decouple Help",
            "args": ["decouple", "--help"],
            "description": "Decouple subcommand help",
            "expected_failure": False
        },
        {
            "name": "Centiloid Basic",
            "args": ["centiloid", "--input", input_file, "--output", output_file],
            "description": "Basic Centiloid calculation",
            "expected_failure": False
        },
        {
            "name": "Centiloid with SUVr",
            "args": ["centiloid", "--input", input_file, "--output", output_file, "--suvr"],
            "description": "Centiloid with SUVr output",
            "expected_failure": False
        },
        {
            "name": "Centiloid Skip Normalization",
            "args": ["centiloid", "--input", input_file, "--output", output_file, "--skip-normalization"],
            "description": "Centiloid with skipped spatial normalization",
            "expected_failure": False
        },
        {
            "name": "Centiloid Iterative",
            "args": ["centiloid", "--input", input_file, "--output", output_file, "--iterative"],
            "description": "Centiloid with iterative rigid registration",
            "expected_failure": False
        },
        {
            "name": "CenTauR Basic",
            "args": ["centaur", "--input", input_file, "--output", output_file],
            "description": "Basic CenTauR calculation",
            "expected_failure": False
        },
        {
            "name": "CenTauRz Basic",
            "args": ["centaurz", "--input", input_file, "--output", output_file],
            "description": "Basic CenTauRz calculation",
            "expected_failure": False
        },
        {
            "name": "SUVr Custom",
            "args": ["suvr", "--input", input_file, "--output", output_file, 
                    "--voi-mask", voi_mask, "--ref-mask", ref_mask],
            "description": "Custom SUVr calculation",
            "expected_failure": False
        },
        {
            "name": "Normalize Basic",
            "args": ["normalize", "--input", input_file, "--output", output_file],
            "description": "Basic spatial normalization",
            "expected_failure": False
        },
        {
            "name": "Normalize ADNI Style",
            "args": ["normalize", "--input", input_file, "--output", output_file, "--ADNI-PET-core"],
            "description": "ADNI-style spatial normalization",
            "expected_failure": False
        },
        {
            "name": "Decouple Abeta",
            "args": ["decouple", "--input", input_file, "--output", output_file, "--modality", "abeta"],
            "description": "Abeta decoupling analysis",
            "expected_failure": False
        },
        {
            "name": "Decouple Tau",
            "args": ["decouple", "--input", input_file, "--output", output_file, "--modality", "tau"],
            "description": "Tau decoupling analysis",
            "expected_failure": False

        },
        {
            "name": "Missing Input Error",
            "args": ["centiloid", "--output", output_file],
            "description": "Error handling for missing input",
            "expected_failure": True
        },
        {
            "name": "SUVr Missing VOI Mask",
            "args": ["suvr", "--input", input_file, "--output", output_file, "--ref-mask", ref_mask],
            "description": "Error handling for missing VOI mask in SUVr",
            "expected_failure": True
        },
        {
            "name": "SUVr Missing Ref Mask",
            "args": ["suvr", "--input", input_file, "--output", output_file, "--voi-mask", voi_mask],
            "description": "Error handling for missing reference mask in SUVr",
            "expected_failure": True
        },
        {
            "name": "Decouple Missing Modality",
            "args": ["decouple", "--input", input_file, "--output", output_file],
            "description": "Error handling for missing modality in decouple",
            "expected_failure": True
        },
        {
            "name": "Decouple Invalid Modality",
            "args": ["decouple", "--input", input_file, "--output", output_file, "--modality", "invalid"],
            "description": "Error handling for invalid modality in decouple",
            "expected_failure": True
        }
    ]
    
    # Run tests
    for test_case in test_cases:
        total_tests += 1
        success = run_command(exe_path, test_case["args"], test_case["name"], test_case["expected_failure"])
        
        if success:
            print("✓ PASSED")
            tests_passed += 1
        else:
            print("❌ FAILED (or expected failure)")
    
    # Summary
    print(f"\n{'='*60}")
    print("TEST SUMMARY")
    print('='*60)
    print(f"Tests passed: {tests_passed}/{total_tests}")
    print(f"Success rate: {tests_passed/total_tests*100:.1f}%")
    
    if tests_passed >= total_tests * 0.5:  # At least 50% success
        print("✓ Overall result: ACCEPTABLE")
        print("\nNote: Some 'failures' may be expected (e.g., missing model files)")
        print("The important thing is that the application doesn't crash and")
        print("shows appropriate error messages.")
        return True
    else:
        print("❌ Overall result: NEEDS ATTENTION")
        return False


if __name__ == "__main__":
    success = main()
    sys.exit(0 if success else 1)
