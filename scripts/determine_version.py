import os
import re
import subprocess
import sys

def run_command(command):
    try:
        result = subprocess.check_output(command, shell=True, stderr=subprocess.STDOUT)
        return result.decode('utf-8').strip()
    except subprocess.CalledProcessError:
        return ""

def get_current_branch():
    # In GitHub Actions, GITHUB_REF might be refs/heads/branch or refs/pull/x/merge
    # But usually we checkout logic handles it.
    # We will trust arguments or git.
    return run_command("git rev-parse --abbrev-ref HEAD")

def get_latest_tag(prefix):
    # Get tags sorted by creation date
    tags = run_command(f"git tag --list \"{prefix}*\" --sort=-creatordate")
    if not tags:
        return None
    return tags.split('\n')[0]

def parse_semver(tag):
    # v1.0.1 -> (1, 0, 1)
    # dev1.0.1 -> (1, 0, 1)
    match = re.search(r'(\d+)\.(\d+)\.(\d+)', tag)
    if match:
        return tuple(map(int, match.groups()))
    return (0, 0, 0)

def determine_bump(log):
    bump = "patch" # Default to patch if unknown? or none. User said "Others -> Patch or no bump"
    # To correspond exactly to config:
    # feat -> Minor
    # fix, perf -> Patch
    # BREAKING_CHANGE -> Major
    if "BREAKING CHANGE" in log:
        return "major"
    
    lines = log.split('\n')
    has_feat = False
    has_fix = False
    
    for line in lines:
        line = line.strip()
        if line.startswith("feat"):
            has_feat = True
        if line.startswith("fix") or line.startswith("perf"):
            has_fix = True
            
    if has_feat:
        return "minor"
    if has_fix:
        return "patch"
    
    # Check for others
    # docs, style, refactor, test, chore, revert
    for line in lines:
         if re.match(r'^(docs|style|refactor|test|chore|revert|build|ci):', line):
             return "patch" # Per user request "Others -> Patch"
             
    return "patch" # Default fallback for any changes

def main():
    if len(sys.argv) < 2:
        print("Usage: determine_version.py <branch_type> [head_ref]")
        print("branch_type: main or dev")
        sys.exit(1)

    branch_type = sys.argv[1] # 'main' or 'dev'
    prefix = "v" if branch_type == "main" else "dev"

    print(f"Determining version for {branch_type} with prefix {prefix}...", file=sys.stderr)

    latest_tag = get_latest_tag(prefix)
    
    if not latest_tag:
        # Initial version
        new_version = f"{prefix}0.0.1"
        print(new_version)
        return

    # Get commits since latest tag
    log = run_command(f"git log {latest_tag}..HEAD --pretty=format:\"%B\"")
    
    if not log:
        print(f"No changes since {latest_tag}", file=sys.stderr)
        # Output existing tag if no changes? Or nothing?
        # Usually for CI we might want to skip.
        # But if we must output a version:
        print(latest_tag) 
        return

    major, minor, patch = parse_semver(latest_tag)
    bump = determine_bump(log)
    
    new_major, new_minor, new_patch = major, minor, patch
    
    if bump == "major":
        new_major += 1
        new_minor = 0
        new_patch = 0
    elif bump == "minor":
        new_minor += 1
        new_patch = 0
    else:
        new_patch += 1
        
    new_version = f"{prefix}{new_major}.{new_minor}.{new_patch}"
    print(new_version)

if __name__ == "__main__":
    main()
