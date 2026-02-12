import shutil
from pathlib import Path
import subprocess
import sys

Import("env")

SRC = Path("src/web")
DST = Path("src/web/build")

DST.mkdir(exist_ok=True)


def command_exists(cmd):
    """Check if a command is available."""
    try:
        result = subprocess.run(
            cmd, shell=True, capture_output=True, check=False, timeout=1
        )
        return result.returncode == 0
    except:
        return False


def run(cmd):
    subprocess.check_call(cmd, shell=True)


# Check which minifiers are available
has_html_minifier = command_exists("html-minifier-terser --version")
has_cleancss = command_exists("cleancss --version")
has_terser = command_exists("terser --version")

if not (has_html_minifier and has_cleancss and has_terser):
    print("Warning: Minifiers not found. Skipping minification.", file=sys.stderr)
    if not has_html_minifier:
        print("  - html-minifier-terser: not installed", file=sys.stderr)
    if not has_cleancss:
        print("  - clean-css-cli: not installed", file=sys.stderr)
    if not has_terser:
        print("  - terser: not installed", file=sys.stderr)
    print(
        "To enable minification, install: npm install -g html-minifier-terser clean-css-cli terser",
        file=sys.stderr,
    )
else:
    # Recursively process all files, minifying where possible
    for f in sorted(SRC.rglob("*")):
        if not f.is_file():
            continue

        # Skip the build directory itself
        try:
            f.relative_to(DST)
            continue
        except ValueError:
            pass

        rel = f.relative_to(SRC)
        out = DST / rel
        out.parent.mkdir(parents=True, exist_ok=True)

        if f.suffix == ".html":
            run(
                f'html-minifier-terser "{f}" '
                "--collapse-whitespace "
                "--remove-comments "
                "--minify-css true "
                "--minify-js true "
                f'-o "{out}"'
            )

        elif f.suffix == ".css":
            run(f'cleancss -O2 "{f}" -o "{out}"')

        elif f.suffix == ".js":
            run(
                f'terser "{f}" --compress --mangle --comments false --ecma 2020 --toplevel -o "{out}"'
            )

        else:
            shutil.copy(f, out)

    print("Web assets minified")
