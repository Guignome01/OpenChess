import shutil
from pathlib import Path
import subprocess
Import("env")

SRC = Path("src/web")
DST = Path("src/web/build")

DST.mkdir(exist_ok=True)

def run(cmd):
    subprocess.check_call(cmd, shell=True)

try:
    for f in SRC.iterdir():
        if not f.is_file():
            continue

        out = DST / f.name

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
            run(f'terser "{f}" -c -m -o "{out}"')

        else:
            shutil.copy(f, out)

    print("Web assets minified")
except Exception as e:
    print("Warning: Minifiers not found. Skipping minification.")
    print("Using existing pre-generated files from repository.")
    print("To enable minification, install: npm install -g html-minifier-terser clean-css-cli terser")
