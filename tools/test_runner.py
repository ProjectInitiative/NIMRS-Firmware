#!/usr/bin/env python3
import os
import sys
import glob
import re
import subprocess


def main():
    # Setup environment
    cxx = os.environ.get("CXX", "g++")
    # Default flags from Makefile: -std=c++17 -Itests/mocks -Itests/mocks/freertos -Isrc -Wall
    cxxflags = os.environ.get(
        "CXXFLAGS", "-std=c++17 -Itests/mocks -Itests/mocks/freertos -Isrc -Wall"
    )

    # Locate tests
    test_files = glob.glob("tests/test_*.cpp")
    if not test_files:
        print("No tests found in tests/")
        sys.exit(0)

    print(f"Found {len(test_files)} tests.")

    failed_tests = []

    for test_file in test_files:
        test_name = os.path.basename(test_file).replace(".cpp", "")
        print(f"\n[{test_name}] Processing...")

        # Determine dependencies
        sources = []
        has_metadata = False

        with open(test_file, "r") as f:
            content = f.read()
            # Check for metadata
            match = re.search(r"//\s*TEST_SOURCES:\s*(.*)", content)
            if match:
                has_metadata = True
                sources_str = match.group(1).strip()
                if sources_str:
                    sources = sources_str.split()
                print(f"  Metadata found: {sources}")
            else:
                # Heuristic fallback
                print(f"  No metadata found, using heuristics...")
                # 1. Check for src/NAME.cpp (where NAME is test_NAME without test_ prefix)
                name_part = test_name.replace("test_", "")
                candidate = f"src/{name_part}.cpp"
                if os.path.exists(candidate):
                    sources.append(candidate)

                # 2. Parse includes
                includes = re.findall(r'#include\s*[<"](.*?)[">]', content)
                for inc in includes:
                    # Check if included header corresponds to a source file in src/
                    # e.g. "ConnectivityManager.h" -> "src/ConnectivityManager.cpp"
                    # Handle path prefix if present
                    basename = os.path.basename(inc)
                    name_no_ext = os.path.splitext(basename)[0]
                    src_candidate = f"src/{name_no_ext}.cpp"
                    if os.path.exists(src_candidate) and src_candidate not in sources:
                        sources.append(src_candidate)

                # 3. Always include mocks.cpp by default for fallback?
                if "tests/mocks/mocks.cpp" not in sources:
                    sources.append("tests/mocks/mocks.cpp")

                print(f"  Heuristics resolved: {sources}")

        # Compile
        output_bin = f"tests/{test_name}"
        # Split flags safely? Assumes space separation
        cmd = [cxx] + cxxflags.split() + ["-o", output_bin, test_file] + sources

        print(f"  Compiling: {' '.join(cmd)}")
        result = subprocess.run(cmd, capture_output=True, text=True)

        if result.returncode != 0:
            print(f"  COMPILATION FAILED:")
            print(result.stderr)
            failed_tests.append(test_name)
            continue

        # Run
        print(f"  Running...")
        # Use absolute path or relative?
        run_cmd = [os.path.abspath(output_bin)]
        run_result = subprocess.run(run_cmd, capture_output=True, text=True)

        print(run_result.stdout)
        if run_result.stderr:
            print(run_result.stderr)

        if run_result.returncode != 0:
            print(f"  TEST FAILED (exit code {run_result.returncode})")
            failed_tests.append(test_name)
        else:
            print(f"  PASSED")

    print("\n" + "=" * 40)
    if failed_tests:
        print(f"FAILURES: {len(failed_tests)} tests failed.")
        for t in failed_tests:
            print(f" - {t}")
        sys.exit(1)
    else:
        print("All tests passed successfully.")
        sys.exit(0)


if __name__ == "__main__":
    main()
