import pytest

# Global list to store results from test_acc_centiloid_centaurz.py
# Using a list of dictionaries to store the results
ACC_TEST_RESULTS = []

def pytest_terminal_summary(terminalreporter, exitstatus, config):
    """
    Custom hook to print the accuracy test summary table at the end of the test session.
    This avoids using -s and polluting the output with other stdout.
    """
    if not ACC_TEST_RESULTS:
        return

    # Print the table to the terminal
    terminalreporter.section("Accuracy Test Summary")
    
    header = f"{'Image':<40} {'Metric':<12} {'Tracer':<8} {'GT':<8} {'Pred':<8} {'Diff':<8}"
    separator = "-" * 95
    
    terminalreporter.write_line(f"\n{header}")
    terminalreporter.write_line(separator)
    
    for res in ACC_TEST_RESULTS:
        line = f"{res['image']:<40} {res['metric']:<12} {res['tracer']:<8} {res['gt']:<8} {res['pred']:<8} {res['diff']:<8}"
        terminalreporter.write_line(line)
    
    terminalreporter.write_line(separator + "\n")

