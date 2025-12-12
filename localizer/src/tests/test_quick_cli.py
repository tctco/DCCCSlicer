from pathlib import Path
from typing import Any, Dict, List, cast

import pytest


TEST_CASES: List[Dict[str, Any]] = [
    {"id": "help_message", "args": ["--help"], "expected_failure": False},
    {"id": "version_information", "args": ["--version"], "expected_failure": False},
    {"id": "no_subcommand", "args": [], "expected_failure": True},
    {"id": "refactor_centiloid_help", "args": ["centiloid", "--help"], "expected_failure": False},
    {"id": "refactor_centaur_help", "args": ["centaur", "--help"], "expected_failure": False},
    {"id": "refactor_centaurz_help", "args": ["centaurz", "--help"], "expected_failure": False},
    {"id": "refactor_suvr_help", "args": ["suvr", "--help"], "expected_failure": False},
    {"id": "refactor_fillstates_help", "args": ["fillstates", "--help"], "expected_failure": False},
    {"id": "normalize_help", "args": ["normalize", "--help"], "expected_failure": False},
    {"id": "adni_pet_core_help", "args": ["adni-pet-core", "--help"], "expected_failure": False},
    {
        "id": "refactor_centiloid_basic",
        "args": ["centiloid", "--input", "{input}", "--output", "{output}"],
        "expected_failure": False,
    },
    {
        "id": "refactor_centiloid_with_suvr",
        "args": ["centiloid", "--input", "{input}", "--output", "{output}", "--suvr"],
        "expected_failure": False,
    },
    {
        "id": "refactor_centiloid_skip_normalization",
        "args": ["centiloid", "--input", "{input}", "--output", "{output}", "--skip-normalization"],
        "expected_failure": False,
    },
    {
        "id": "refactor_centiloid_iterative",
        "args": ["centiloid", "--input", "{input}", "--output", "{output}", "--iterative"],
        "expected_failure": False,
    },
    {
        "id": "refactor_centaur_basic",
        "args": ["centaur", "--input", "{input}", "--output", "{output}"],
        "expected_failure": False,
    },
    {
        "id": "refactor_centaurz_basic",
        "args": ["centaurz", "--input", "{input}", "--output", "{output}"],
        "expected_failure": False,
    },
    {
        "id": "refactor_suvr_custom",
        "args": [
            "suvr",
            "--input",
            "{input}",
            "--output",
            "{output}",
            "--voi-mask",
            "{voi_mask}",
            "--ref-mask",
            "{ref_mask}",
        ],
        "expected_failure": False,
    },
    {
        "id": "refactor_fillstates_basic",
        "args": ["fillstates", "--input", "{input}", "--output", "{output}", "--tracer", "fbp"],
        "expected_failure": False,
    },
    {"id": "normalize_basic", "args": ["normalize", "--input", "{input}", "--output", "{output}"], "expected_failure": False},
    {
        "id": "adni_pet_core_basic",
        "args": ["adni-pet-core", "--input", "{input}", "--output", "{output}"],
        "expected_failure": False,
    },
    {"id": "missing_input_error", "args": ["centiloid", "--output", "{output}"], "expected_failure": True},
    {
        "id": "refactor_suvr_missing_voi_mask",
        "args": ["suvr", "--input", "{input}", "--output", "{output}", "--ref-mask", "{ref_mask}"],
        "expected_failure": True,
    },
    {
        "id": "refactor_suvr_missing_ref_mask",
        "args": ["suvr", "--input", "{input}", "--output", "{output}", "--voi-mask", "{voi_mask}"],
        "expected_failure": True,
    },
]

TEST_CASE_IDS: List[str] = [cast(str, case["id"]) for case in TEST_CASES]


class TestQuickCommandSuite:
    """Pytest-based replacement for the old quick_test script."""

    @pytest.mark.parametrize("case", TEST_CASES, ids=TEST_CASE_IDS)
    def test_cli_behaviors(self, case, run_subprocess, tmp_path, test_files):
        args = self._prepare_args(case["args"], tmp_path, test_files)
        result = run_subprocess(args)

        if case["expected_failure"]:
            assert result.returncode != 0, (
                f"Expected failure for {case['id']} but return code was zero.\n"
                f"STDOUT:\n{result.stdout}\nSTDERR:\n{result.stderr}"
            )
        else:
            assert result.returncode == 0, (
                f"Command failed for {case['id']}.\n"
                f"STDOUT:\n{result.stdout}\nSTDERR:\n{result.stderr}"
            )

    @staticmethod
    def _prepare_args(args_template, tmp_path, test_files):
        """Replace placeholders in the argument template with real paths."""
        output_file = tmp_path / "output.nii"
        placeholder_map = {
            "{input}": test_files["input"],
            "{output}": output_file,
            "{voi_mask}": test_files["voi_mask"],
            "{ref_mask}": test_files["ref_mask"],
        }

        resolved_args = []
        for token in args_template:
            resolved_args.append(str(placeholder_map.get(token, token)))
        return resolved_args
