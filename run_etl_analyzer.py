#!/usr/bin/env python3
import argparse
from concurrent.futures import ThreadPoolExecutor, as_completed
import json
import subprocess
import sys
import os
import shlex
from pathlib import Path

def get_git_diff_files(base_branch):
    """Retrieve a list of modified C++ files compared to the base branch."""
    try:
        # Use merge-base to find the fork point, then diff against it
        cmd = ["git", "diff", "--name-only", f"{base_branch}...HEAD"]
        output = subprocess.check_output(cmd, text=True)
        files = output.splitlines()
        # Filter for C++ files
        return {os.path.abspath(f) for f in files if f.endswith(('.cpp', '.cxx', '.cc', '.c'))}
    except subprocess.CalledProcessError as e:
        print(f"Error running git diff: {e}", file=sys.stderr)
        sys.exit(1)

def filter_compiler_args(args):
    """Strip out flags that interfere with static analysis."""
    filtered = []
    skip_next = False
    
    # Flags we want to explicitly drop
    drop_prefixes = ('-g', '-O', '-W', '-fno-exceptions', '-fno-rtti', '-MD', '-MF', '-MT')
    
    for arg in args[1:]: # Skip the compiler executable (e.g., /usr/bin/c++)
        if skip_next:
            skip_next = False
            continue
            
        # Strip output flags
        if arg == '-o' or arg == '-c':
            skip_next = (arg == '-o') # Skip the output filename
            continue
            
        if any(arg.startswith(prefix) for prefix in drop_prefixes):
            continue
            
        filtered.append(arg)
        
    return filtered

def run_analysis_for_entry(entry, plugin_path, extra_sysroot):
    """Run static analysis for a single compilation database entry."""
    file_path = os.path.abspath(entry['file'])

    # Parse command arguments
    if 'arguments' in entry:
        raw_args = entry['arguments']
    else:
        raw_args = shlex.split(entry['command'])

    clean_args = filter_compiler_args(raw_args)

    # Append extra includes
    for sys_inc in extra_sysroot:
        clean_args.extend(['-isystem', sys_inc])

    cmd = [
        "clang++", "--analyze", "--analyzer-no-default-checks",
        "-Xclang", "-load", "-Xclang", plugin_path,
        "-Xclang", "-analyzer-checker=custom.EtlAccessChecker"
    ] + clean_args + [file_path]

    try:
        result = subprocess.run(cmd, cwd=entry['directory'], capture_output=True, text=True)
        stderr = result.stderr or ""
        has_issue = result.returncode != 0 or ("warning:" in stderr.lower())
        return file_path, stderr, has_issue, None
    except Exception as e:
        return file_path, "", True, str(e)

def main():
    parser = argparse.ArgumentParser(description="Run the ETL Static Analyzer on a project.")
    parser.add_argument("--compile-commands", default="build/compile_commands.json", help="Path to compile_commands.json")
    parser.add_argument("--plugin", required=True, help="Path to libEtlChecker.so")
    
    # Target selection group
    group = parser.add_mutually_exclusive_group(required=True)
    group.add_argument("--all", action="store_true", help="Analyze all files in the compilation database")
    group.add_argument("--diff", metavar="BRANCH", help="Analyze only files changed relative to BRANCH (e.g., master)")
    group.add_argument("--dir", metavar="DIR", help="Analyze only files within a specific subdirectory")

    parser.add_argument("--extra-sysroot", action="append", default=[], help="Additional system include paths (can be used multiple times)")
    parser.add_argument("-j", "--jobs", type=int, default=max(1, (os.cpu_count() or 1)), help="Number of files to analyze in parallel")
    args = parser.parse_args()

    if args.jobs < 1:
        print("Error: --jobs must be >= 1.", file=sys.stderr)
        sys.exit(1)

    # 1. Load Compilation Database
    if not os.path.exists(args.compile_commands):
        print(f"Error: {args.compile_commands} not found.", file=sys.stderr)
        sys.exit(1)

    with open(args.compile_commands, 'r') as f:
        compdb = json.load(f)

    # 2. Determine target files
    target_files = set()
    if args.diff:
        target_files = get_git_diff_files(args.diff)
    elif args.dir:
        target_dir = os.path.abspath(args.dir)
    
    # 3. Process Files
    failures = 0
    analyzed_count = 0

    selected_entries = []
    for entry in compdb:
        file_path = os.path.abspath(entry['file'])

        # Filter based on mode
        if args.diff and file_path not in target_files:
            continue
        if args.dir and not file_path.startswith(target_dir):
            continue

        selected_entries.append(entry)

    total_files = len(selected_entries)
    if total_files == 0:
        print("No matching files to analyze.")
        print("\nAnalysis complete. Analyzed 0 files.")
        print("No issues found! 🎉")
        return

    worker_count = min(args.jobs, total_files)
    plugin_path = os.path.abspath(args.plugin)
    print(f"Analyzing {total_files} files with {worker_count} parallel worker(s)...")

    with ThreadPoolExecutor(max_workers=worker_count) as executor:
        futures = [
            executor.submit(run_analysis_for_entry, entry, plugin_path, args.extra_sysroot)
            for entry in selected_entries
        ]

        for future in as_completed(futures):
            file_path, stderr, has_issue, error_message = future.result()
            analyzed_count += 1
            print(f"[{analyzed_count}/{total_files}] {os.path.basename(file_path)}")

            # Print any warnings or errors emitted by the analyzer
            if stderr.strip():
                print(stderr)

            if error_message:
                print(f"Failed to execute analyzer on {file_path}: {error_message}", file=sys.stderr)

            if has_issue:
                failures += 1

    print(f"\nAnalysis complete. Analyzed {analyzed_count} files.")
    if failures > 0:
        print(f"Found issues in {failures} files.", file=sys.stderr)
        sys.exit(1)
    else:
        print("No issues found! 🎉")

if __name__ == "__main__":
    main()