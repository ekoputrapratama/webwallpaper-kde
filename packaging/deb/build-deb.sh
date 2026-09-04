#!/usr/bin/env bash
#
# Build .deb packages for WebWallpaper (KDE plugin + store app).
#
# Usage:
#   ./packaging/deb/build-deb.sh                # build everything
#   ./packaging/deb/build-deb.sh --skip-build   # repackage from existing build
#
# Output: packaging/deb/dist/webwallpaper-{plugin,store}_1.0_amd64.deb
#
# Build dependencies (Ubuntu 24.04+):
#   sudo apt install cmake g++ extra-cmake-modules \
#       qt6-base-dev qt6-declarative-dev qt6-webengine-dev \
#       libkf6windowsystem-dev libkf6waylandclient-dev \
#       libplasma-dev fakeroot
#
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "${SCRIPT_DIR}/../.." && pwd)"
STAGING="${SCRIPT_DIR}/staging"
DIST="${SCRIPT_DIR}/dist"
BUILD_ROOT="${ROOT_DIR}/build/deb"
VERSION=$1
ARCH="amd64"

SKIP_BUILD=false
for arg in "$@"; do
    case "$arg" in
        --skip-build) SKIP_BUILD=true ;;
    esac
done

# ── Detect the Qt6 QML install dir for this system ──────────────────────
detect_qml_dir() {
    local qt6_dir
    qt6_dir=$(cmake --find-package -DNAME=Qt6 -DCOMPILER_ID=GNU -DLANGUAGE=CXX -DMODE=COMPILER 2>/dev/null | head -1 || true)
    # Fallback: query cmake for the Qt6 QML install path
    local qml_dir
    qml_dir=$(cmake -E echo "\${Qt6_DIR}" 2>/dev/null || true)

    # Try the standard Debian/Ubuntu path first
    local arch
    arch=$(dpkg-architecture -qDEB_HOST_MULTIARCH 2>/dev/null || echo "x86_64-linux-gnu")
    local candidate="/usr/lib/${arch}/qt6/qml"
    if [ -d "${candidate}" ]; then
        echo "${candidate}"
        return
    fi

    # Fallback: Arch-style
    if [ -d "/usr/lib/qt6/qml" ]; then
        echo "/usr/lib/qt6/qml"
        return
    fi

    # Last resort
    echo "/usr/lib/${arch}/qt6/qml"
}

# ── Build ────────────────────────────────────────────────────────────────
build_all() {
    echo "==> Building store app..."
    cmake -S "${ROOT_DIR}" -B "${BUILD_ROOT}" \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INSTALL_PREFIX=/usr
    cmake --build "${BUILD_ROOT}" -j"$(nproc)"

    echo "==> Building plugin..."
    cmake -S "${ROOT_DIR}/plugin" -B "${BUILD_ROOT}/plugin" \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INSTALL_PREFIX=/usr \
        -DINSTALL_USER_LOCAL=OFF
    cmake --build "${BUILD_ROOT}/plugin" -j"$(nproc)"
}

# ── Stage files ──────────────────────────────────────────────────────────
stage_plugin() {
    local dest="${STAGING}/webwallpaper-plugin"
    local qml_dir
    qml_dir=$(detect_qml_dir)

    echo "==> Staging plugin -> ${dest}"
    rm -rf "${dest}"
    mkdir -p "${dest}"

    # Install everything via cmake --install with DESTDIR
    DESTDIR="${dest}" cmake --install "${BUILD_ROOT}/plugin" 2>/dev/null || true

    # ── Fix QML module path ──────────────────────────────────────────────
    # The cmake install might use a distro-specific path. Move to the
    # canonical location for this system.
    local expected_qml="${dest}${qml_dir}/WebWallpaper"
    local cmake_qml
    cmake_qml=$(find "${dest}" -path "*/qt6/qml/WebWallpaper/qmldir" -print -quit 2>/dev/null || true)

    if [ -n "${cmake_qml}" ]; then
        local cmake_qml_dir
        cmake_qml_dir=$(dirname "${cmake_qml}")
        if [ "${cmake_qml_dir}" != "${expected_qml}" ]; then
            mkdir -p "$(dirname "${expected_qml}")"
            mv "${cmake_qml_dir}" "${expected_qml}"
        fi
    fi

    # ── Fix wallpaper package path (user-local -> system) ─────────────────
    # If files landed under ~/.local/share, move them to /usr/share
    local user_local_wallpaper="${dest}/home"
    if [ -d "${user_local_wallpaper}" ]; then
        while IFS= read -r d; do
            local name
            name=$(basename "$d")
            mkdir -p "${dest}/usr/share/plasma/wallpapers"
            mv "$d" "${dest}/usr/share/plasma/wallpapers/${name}"
        done < <(find "${user_local_wallpaper}" -path "*/.local/share/plasma/wallpapers/*" -mindepth 1 -maxdepth 1 -type d 2>/dev/null || true)
        while IFS= read -r d; do
            mkdir -p "${dest}/usr/share/webwallpaper"
            mv "$d" "${dest}/usr/share/webwallpaper/themes"
        done < <(find "${user_local_wallpaper}" -path "*/.local/share/webwallpaper/themes" -mindepth 1 -maxdepth 1 -type d 2>/dev/null || true)
        rm -rf "${user_local_wallpaper}"
    fi

    # Verify key files exist
    if [ ! -f "${expected_qml}/qmldir" ]; then
        echo "ERROR: QML module not found at ${expected_qml}/qmldir" >&2
        echo "  Files in staging:" >&2
        find "${dest}" -type f | head -20 >&2
        exit 1
    fi

    echo "  QML module: ${expected_qml}"
    echo "  Wallpaper:  ${dest}/usr/share/plasma/wallpapers/webwallpaper/"
}

stage_store() {
    local dest="${STAGING}/webwallpaper-store"

    echo "==> Staging store app -> ${dest}"
    rm -rf "${dest}"
    mkdir -p "${dest}"

    DESTDIR="${dest}" cmake --install "${BUILD_ROOT}" 2>/dev/null || true

    # Verify binary exists
    if [ ! -f "${dest}/usr/bin/webwallpaper-store" ]; then
        echo "ERROR: Store binary not found at ${dest}/usr/bin/webwallpaper-store" >&2
        find "${dest}" -type f | head -20 >&2
        exit 1
    fi

    echo "  Binary:   ${dest}/usr/bin/webwallpaper-store"
    echo "  Desktop:  ${dest}/usr/share/applications/webwallpaper-store.desktop"
}

# ── Control file generation ──────────────────────────────────────────────
generate_plugin_control() {
    cat <<EOF
Package: webwallpaper-plugin
Version: ${VERSION}
Section: kde
Priority: optional
Architecture: ${ARCH}
Maintainer: WebWallpaper Team <user@example.com>
Depends: qml6-module-qtquick,
         qml6-module-qtwebengine,
         qml6-module-qtwaylandclient,
         libkf6windowsystem1,
         libkf6waylandclient1,
         libplasma6
Description: WebWallpaper - KDE Plasma 6 wallpaper plugin
 Live HTML/WebGL wallpapers rendered with Qt WebEngine.
 Supports animated themes, WebGL, and automatic pause/resume
 when applications enter fullscreen.
 .
 This package contains the KDE Plasma wallpaper plugin
 (QML module + wallpaper package + demo themes).
EOF
}

generate_store_control() {
    cat <<EOF
Package: webwallpaper-store
Version: ${VERSION}
Section: graphics
Priority: optional
Architecture: ${ARCH}
Maintainer: WebWallpaper Team <user@example.com>
Depends: libqt6core6t64 | libqt6core6,
         libqt6gui6t64 | libqt6gui6,
         libqt6widgets6t64 | libqt6widgets6,
         libqt6network6t64 | libqt6network6
Description: WebWallpaper Store - theme browser and installer
 GUI application to browse, preview, and install WebWallpaper
 themes from the online catalog. Download themes with one click
 and they appear in the KDE Plasma wallpaper picker.
 .
 This package contains the store application.
EOF
}

# ── Build .deb packages ──────────────────────────────────────────────────
build_deb() {
    local pkg_name="$1"
    local stage_dir="${STAGING}/${pkg_name}"
    local control_dir="${stage_dir}/DEBIAN"

    mkdir -p "${control_dir}"

    case "${pkg_name}" in
        webwallpaper-plugin) generate_plugin_control > "${control_dir}/control" ;;
        webwallpaper-store)  generate_store_control  > "${control_dir}/control" ;;
    esac

    # Strip binaries in the package
    while IFS= read -r f; do
        strip --strip-unneeded "$f" 2>/dev/null || true
    done < <(find "${stage_dir}/usr/bin" -type f -executable 2>/dev/null || true)

    # Strip shared libraries
    while IFS= read -r f; do
        strip --strip-unneeded "$f" 2>/dev/null || true
    done < <(find "${stage_dir}/usr/lib" -name "*.so*" -type f 2>/dev/null || true)

    # Fix permissions
    find "${stage_dir}" -type d -exec chmod 0755 {} \;
    find "${stage_dir}" -type f -exec chmod 0644 {} \;
    find "${stage_dir}/usr/bin" -type f -exec chmod 0755 {} \; 2>/dev/null || true

    # Calculate installed size (in KB)
    local size
    size=$(du -sk "${stage_dir}" | cut -f1)
    sed -i "s/^Installed-Size:.*/Installed-Size: ${size}/" "${control_dir}/control" 2>/dev/null || \
        echo "Installed-Size: ${size}" >> "${control_dir}/control"

    echo "==> Building ${pkg_name}_${VERSION}_${ARCH}.deb"
    fakeroot dpkg-deb --build "${stage_dir}" "${DIST}/${pkg_name}_${VERSION}_${ARCH}.deb"
}

# ── Main ─────────────────────────────────────────────────────────────────
mkdir -p "${DIST}"
rm -rf "${STAGING}"

if [ "${SKIP_BUILD}" = false ]; then
    # Clean deb build dirs to avoid stale cmake cache (e.g. INSTALL_USER_LOCAL)
    rm -rf "${BUILD_ROOT}"
    build_all
fi

stage_plugin
stage_store

build_deb webwallpaper-plugin
build_deb webwallpaper-store

echo
echo "==> Done. Packages in:"
ls -lh "${DIST}/"*.deb
echo
echo "Install with:"
echo "  sudo dpkg -i ${DIST}/webwallpaper-plugin_${VERSION}_${ARCH}.deb"
echo "  sudo dpkg -i ${DIST}/webwallpaper-store_${VERSION}_${ARCH}.deb"
echo "  sudo apt-get install -f   # fix missing dependencies if needed"
