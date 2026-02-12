"""
Extra script: Automatically upload LittleFS filesystem image before
firmware upload, but only when web assets have actually changed.

Tracks content via a SHA-256 hash stored in .littlefs_hash.
"""

import hashlib
import subprocess
import sys
from pathlib import Path

Import("env")

DATA_DIR = Path("data")
HASH_FILE = Path(".littlefs_hash")


def compute_data_hash():
    """Compute a SHA-256 hash over every file in data/."""
    h = hashlib.sha256()
    if not DATA_DIR.exists():
        return ""
    for f in sorted(DATA_DIR.rglob("*")):
        if f.is_file():
            # Include relative path so renames are detected
            h.update(str(f.relative_to(DATA_DIR)).encode())
            h.update(f.read_bytes())
    return h.hexdigest()


def upload_fs_if_changed(source, target, env):
    if not DATA_DIR.exists() or not any(DATA_DIR.rglob("*")):
        print("LittleFS: No data/ directory, skipping filesystem upload")
        return

    current_hash = compute_data_hash()

    previous_hash = ""
    if HASH_FILE.exists():
        previous_hash = HASH_FILE.read_text().strip()

    if current_hash == previous_hash:
        print("LittleFS: Web assets unchanged — skipping filesystem upload")
        return

    print("LittleFS: Web assets changed — uploading filesystem image...")
    result = subprocess.run(
        [
            sys.executable,
            "-m",
            "platformio",
            "run",
            "--target",
            "uploadfs",
            "--environment",
            env.subst("$PIOENV"),
        ],
        check=False,
    )

    if result.returncode == 0:
        HASH_FILE.write_text(current_hash)
        print("LittleFS: Filesystem uploaded successfully")
    else:
        print("LittleFS: Filesystem upload FAILED!", file=sys.stderr)


# Hook into the firmware upload action
env.AddPreAction("upload", upload_fs_if_changed)
