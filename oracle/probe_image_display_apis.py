"""Probe Pillow 11.3.0 show/toqimage/toqpixmap in isolated subprocesses."""

import subprocess
import sys

CODE_TEMPLATE = (
    "from PIL import Image\n"
    "im = Image.new('L', (2, 2), 7)\n"
    "try:\n"
    "    result = im.{name}()\n"
    "    print('OK', type(result).__name__)\n"
    "except Exception as err:\n"
    "    print('ERR', type(err).__name__, '|', str(err))\n"
)


def main():
    for name in ["toqimage", "toqpixmap", "show"]:
        code = CODE_TEMPLATE.format(name=name)
        proc = subprocess.run(
            [sys.executable, "-c", code],
            capture_output=True,
            text=True,
            timeout=60,
        )
        print(name, "exit", proc.returncode)
        print("  stdout:", proc.stdout.strip())
        if proc.stderr.strip():
            print("  stderr:", proc.stderr.strip()[:200])


if __name__ == "__main__":
    main()
