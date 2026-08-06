#!/usr/bin/env python3
"""
Rebuild the third-party notices in the top-level LICENSE from the exact
upstream license text of every generated asset (Lucide icons, JetBrains Mono
font). Never hand-edit past the project's own BSD header — run this instead
so the text always matches what's actually fetched (see
firmware/tools/gen_icons.py, firmware/tools/gen_fonts.py). Sections are
separated by a bare "---" line; everything before the first one is kept
as-is and treated as the project's own license header.

Usage:
    python3 scripts/update_licenses.py
"""

import sys
import urllib.request
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(REPO_ROOT / "firmware" / "tools"))
from gen_fonts import JETBRAINS_MONO_VERSION  # noqa: E402

LUCIDE_LICENSE_URL = "https://raw.githubusercontent.com/lucide-icons/lucide/main/LICENSE"
JETBRAINS_OFL_URL = (
    f"https://raw.githubusercontent.com/JetBrains/JetBrainsMono/v{JETBRAINS_MONO_VERSION}/OFL.txt"
)

LICENSE_PATH = REPO_ROOT / "LICENSE"


def fetch(url: str) -> str:
    print(f"==> Fetching {url}", file=sys.stderr)
    with urllib.request.urlopen(url) as resp:
        return resp.read().decode("utf-8").rstrip("\n")


def main() -> None:
    lucide_license = fetch(LUCIDE_LICENSE_URL)
    jetbrains_ofl = fetch(JETBRAINS_OFL_URL)

    header = LICENSE_PATH.read_text().split("\n---\n", 1)[0].rstrip("\n")

    sections = [
        header,
        "The icon assets under firmware/src/ui/icons/ are generated from the Lucide\n"
        "icon set (https://github.com/lucide-icons/lucide), fetched from\n"
        "https://unpkg.com/lucide-static (see firmware/tools/gen_icons.py), and are\n"
        "licensed under the following (verbatim upstream LICENSE):\n\n"
        + lucide_license,
        "The bitmap fonts under firmware/src/ui/fonts/ are generated from JetBrains\n"
        f"Mono v{JETBRAINS_MONO_VERSION} (https://github.com/JetBrains/JetBrainsMono),\n"
        "fetched at build time (see firmware/tools/gen_fonts.py), and are licensed\n"
        "under the following (verbatim upstream OFL.txt):\n\n"
        + jetbrains_ofl,
    ]

    LICENSE_PATH.write_text("\n\n---\n\n".join(sections) + "\n")
    print(f"==> Wrote {LICENSE_PATH}", file=sys.stderr)


if __name__ == "__main__":
    main()
