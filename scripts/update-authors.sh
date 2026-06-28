#!/usr/bin/env bash

# Exit immediately if a command exits with a non-zero status
set -e

# Run python script to parse git authors and update source code files
python3 - << 'EOF'
import re
import subprocess
import os

cpp_file = 'obs_scene_tree_view/obs_scene_tree_view.cpp'

if not os.path.exists(cpp_file):
    print(f"Error: {cpp_file} not found. Run this script from the project root directory.")
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
        # Clean up name if it contains email or leading/trailing spaces
        name = name.strip()
        if name not in latest or ts > latest[name]:
            latest[name] = ts
    except ValueError:
        continue

# Sort authors by latest commit timestamp in descending order
sorted_authors = sorted(latest.keys(), key=lambda x: latest[x], reverse=True)
authors_str = ", ".join(sorted_authors) + ", & contributors"

# Update C++ file content
with open(cpp_file, 'r', encoding='utf-8') as f:
    content = f.read()

# Replace OBS_MODULE_AUTHOR(...)
new_content, count_macro = re.subn(
    r'OBS_MODULE_AUTHOR\("[^"]*"\);',
    f'OBS_MODULE_AUTHOR("{authors_str}");',
    content
)

# Replace <p><b>Authors:</b> ...</p>
new_content, count_text = re.subn(
    r'<p><b>Authors:</b> [^<]*</p>',
    f'<p><b>Authors:</b> {authors_str}</p>',
    new_content
)

if count_macro == 0 and count_text == 0:
    print("Warning: No author patterns found to replace. Check if they were already updated.")
else:
    with open(cpp_file, 'w', encoding='utf-8') as f:
        f.write(new_content)
    print(f"Updated {cpp_file} successfully.")
    print(f"New Authors List: {authors_str}")

EOF
