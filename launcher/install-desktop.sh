#!/usr/bin/env bash
# Adds the launcher to the application menu (~/.local/share/applications) for this checkout.
set -euo pipefail
here=$(cd -- "$(dirname -- "$0")" && pwd)
dest=${XDG_DATA_HOME:-$HOME/.local/share}/applications
mkdir -p "$dest"
sed "s|@LAUNCHER@|$here/woc-launcher.sh|" "$here/io.github.wocport.Launcher.desktop.in" \
    > "$dest/io.github.wocport.Launcher.desktop"
echo "Installed $dest/io.github.wocport.Launcher.desktop"
