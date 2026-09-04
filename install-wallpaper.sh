#!/usr/bin/env bash
#
# Install the WebWallpaper Plasma 6 wallpaper plugin.
#
# Usage:
#   ./install-wallpaper.sh            # user-local install (no root needed)
#   ./install-wallpaper.sh --system   # system-wide install (requires root)
#
# The C++ QML module (WebWallpaper.ThemeModel) is installed to
# ~/.local/lib/qt6/qml and ~/.local/lib/qt6/qml is added to
# QML2_IMPORT_PATH for plasmashell via the systemd user environment.
# A --system install places it in the Qt QML dir instead (no env needed).
#
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PLUGIN_DIR="${SCRIPT_DIR}/plugin"
BUILD_DIR="${SCRIPT_DIR}/build/plugin"
USER_QML_DIR="${HOME}/.local/lib/qt6/qml"

case "${1:-}" in
    --system)
        cmake -S "${PLUGIN_DIR}" -B "${BUILD_DIR}" -DCMAKE_BUILD_TYPE=Release
        sudo cmake --install "${BUILD_DIR}"
        ;;
    *)
        cmake -S "${PLUGIN_DIR}" -B "${BUILD_DIR}" -DINSTALL_USER_LOCAL=ON -DCMAKE_BUILD_TYPE=Release
        cmake --build "${BUILD_DIR}"
        cmake --install "${BUILD_DIR}" --component module
        cmake --install "${BUILD_DIR}" --component package

        # Make plasmashell see ~/.local/lib/qt6/qml for the current session.
        systemctl --user set-environment QML2_IMPORT_PATH="${USER_QML_DIR}"
        # Persist for future logins.
        mkdir -p "${HOME}/.config/environment.d"
        cat > "${HOME}/.config/environment.d/webwallpaper.conf" <<EOF
QML2_IMPORT_PATH=\${HOME}/.local/lib/qt6/qml
EOF
        ;;
esac

echo
echo "WebWallpaper wallpaper plugin installed."
echo "Restart plasmashell to pick it up:"
echo "    systemctl --user restart plasma-plasmashell.service"
echo
echo "Then: Right-click desktop -> Configure Desktop and Wallpaper -> Wallpaper type: WebWallpaper"
echo "and pick a theme in the grid (e.g. demo)."