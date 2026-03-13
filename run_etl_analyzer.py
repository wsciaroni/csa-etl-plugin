#!/usr/bin/env python3
import argparse
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
    args = parser.parse_args()

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

    for entry in compdb:
        file_path = os.path.abspath(entry['file'])
        
        # Filter based on mode
        if args.diff and file_path not in target_files:
            continue
        if args.dir and not file_path.startswith(target_dir):
            continue

        # Parse command arguments
        if 'arguments' in entry:
            raw_args = entry['arguments']
        else:
            raw_args = shlex.split(entry['command'])

        clean_args = filter_compiler_args(raw_args)

        # Append extra includes
        for sys_inc in args.extra_sysroot:
            clean_args.extend(['-isystem', sys_inc])

        # Construct the Clang Analyzer command
        # We use the normal driver but pass -Xclang flags to load the plugin
        cmd = [
            "clang++", "--analyze",
            "-Xclang", "-load", "-Xclang", os.path.abspath(args.plugin),
            "-Xclang", "-analyzer-checker=custom.EtlAccessChecker",
            # Optional: Disable all default checkers to speed up analysis and reduce noise
            # "-Xclang", "-analyzer-disable-all-checks", 
        ] + clean_args + [file_path]

        print(f"Analyzing {os.path.basename(file_path)}...")
        
        try:
            # Run the analyzer. It outputs warnings to stderr.
            result = subprocess.run(cmd, cwd=entry['directory'], capture_output=True, text=True)
            
            # Print any warnings or errors emitted by the analyzer
            if result.stderr.strip():
                print(result.stderr)
            
            # If the analyzer found a bug (or failed to compile), it returns non-zero
            if result.returncode != 0 or "warning:" in result.stderr.lower():
                failures += 1
                
        except Exception as e:
            print(f"Failed to execute analyzer on {file_path}: {e}", file=sys.stderr)
            failures += 1
            
        analyzed_count += 1

    print(f"\nAnalysis complete. Analyzed {analyzed_count} files.")
    if failures > 0:
        print(f"Found issues in {failures} files.", file=sys.stderr)
        sys.exit(1)
    else:
        print("No issues found! 🎉")

if __name__ == "__main__":
    main()