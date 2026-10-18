#!/usr/bin/env bash
# tools/pkgtool/PkgTool.Core for scripts/pkg_install.py (the launcher's "Install from .pkg"), when
# running from source: LibOrbisPkg 0.2.231's Linux build by Maxton (LGPL-3.0), checked against its
# SHA-256. The AppImage has it already.
set -euo pipefail
cd -- "$(dirname -- "$0")/.."
url=https://github.com/maxton/LibOrbisPkg/releases/download/v0.2/PkgTool.Core-linux-x64-0.2.231.zip
sum=ac897d06a0c90256f4297712cb503eb29173fbc6d7f8a936ff476c4b72e5f36e
zip=$(mktemp --suffix=.zip)
trap 'rm -f "$zip"' EXIT
curl -fsSL -o "$zip" "$url"
echo "$sum  $zip" | sha256sum -c --quiet
mkdir -p tools/pkgtool
unzip -o -q "$zip" PkgTool.Core LICENSE.txt -d tools/pkgtool
chmod +x tools/pkgtool/PkgTool.Core
echo "tools/pkgtool/PkgTool.Core"
