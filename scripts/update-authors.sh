#!/usr/bin/env bash

# Exit immediately if a command exits with a non-zero status
set -e

# Run python script to parse git authors and update buildspec.json
python3 - << 'EOF'
import json
import subprocess
import os

buildspec_file = 'buildspec.json'

if not os.path.exists(buildspec_file):
    print(f"Error: {buildspec_file} not found. Run this script from the project root directory.")
    exit(1)

# Get authors from git log ordered by latest commit timestamp
try:
    git_cmd = 'git log --format="%an|%at"'
    output = subprocess.check_output(git_cmd, shell=True, text=True)
except Exception as e:
    print(f"Error executing git: {e}")
    exit(1)

latest = {}
for line in output.strip().split('\n'):
    if not line:
        continue
    try:
        name, ts = line.rsplit('|', 1)
        ts = int(ts)
        name = name.strip()
        if name not in latest or ts > latest[name]:
            latest[name] = ts
    except ValueError:
        continue

# Sort authors by latest commit timestamp in descending order
sorted_authors = sorted(latest.keys(), key=lambda x: latest[x], reverse=True)
authors_str = ", ".join(sorted_authors) + ", & contributors"

# Load, update, and save buildspec.json
with open(buildspec_file, 'r', encoding='utf-8') as f:
    data = json.load(f)

data['contributors'] = authors_str

with open(buildspec_file, 'w', encoding='utf-8') as f:
    json.dump(data, f, indent=2, ensure_ascii=False)
    # Add trailing newline for formatting consistency
    f.write('\n')

print(f"Updated {buildspec_file} successfully.")
print(f"New Contributors List: {authors_str}")

EOF
