# WebWallpaper

A **KDE Plasma 6 wallpaper plugin** that renders live **HTML/WebGL wallpapers** using Qt WebEngine (Chromium), plus a **theme store app** to browse, download, and install themes.

## Components

### 1. `webwallpaper` — Plasma wallpaper plugin

Creates a `WallpaperItem` that hosts a full `WebEngineView`. Any HTML5/WebGL page can be a live desktop wallpaper. It automatically **pauses** (freezes a screenshot, stops GPU rendering) when any application goes fullscreen, and **resumes** when they exit.

- 🖼️ **Live web wallpapers** — any HTML5/WebGL page
- 🌐 **WebGL + hardware acceleration** enabled
- 🎬 **Smart pause/resume** — freezes rendering (including WebGL draw calls) when a fullscreen window is detected, shows a frozen frame, restores on exit
- 📦 **Theme system** — `.theme` INI descriptor with name, description, author, version, thumbnail, and entry point
- 📁 **Two theme locations**:
  - System-wide: `/usr/share/webwallpaper/themes`
  - User-local: `~/.local/share/webwallpaper/themes`
- 🖥️ **Multi-monitor** support (each screen gets its own wallpaper)

### 2. `webwallpaper-store` — Theme store app

A Qt Widgets GUI to browse the theme catalog, preview thumbnails, and install themes with one click. Installed themes appear in the KDE wallpaper picker.

- 🏪 **Theme store** — browse, download, and install themes from a Firebase/Firestore backend

## Requirements

- **KDE Plasma 6** (6.0+)
- Qt 6 (Widgets, Network; and QtWebEngine, QML/Quick for the plugin)
- CMake 3.16+
- KDE Frameworks (KF6WindowSystem, KWayland)

## Repository layout

```
CMakeLists.txt              # builds the webwallpaper-store app
plugin/
  CMakeLists.txt            # builds the WebWallpaper QML module + wallpaper package
  src/                      # ThemeModel + WindowMonitor (fullscreen detection)
  package/                  # Plasma wallpaper package (main.qml, config.qml)
store_app/                  # Qt Widgets store application
packaging/deb/              # .deb build script
resources/                  # app icon, demo theme, Firebase config template
```

## Building from source

This project uses CMake with presets (`CMakePresets.json`):

- **`dev`** (Debug) — for development
- **`release`** (Release) — optimized build

Build the store app:

```
cmake --preset dev
cmake --build --preset dev
```

The binaries land in `build/dev/` (or `build/release/` for the release preset).
See "Installing" below for the plugin build and install flow.

### Recommended extensions

Install these from the VSCode marketplace:

- **CMake Tools** (`ms-vscode.cmake-tools`) — configure/build from the status bar
- **C/C++** (`ms-vscode.cpptools`) — IntelliSense + debugger
- Optional: **clangd** (`llvm-vs-code-extensions.vscode-clangd`) for better completion

### Using it

1. Make sure `cmake` is on your `PATH` (e.g. `sudo pacman -S cmake`).
2. Open the project folder in VSCode.
3. The CMake Tools extension will prompt to configure — pick the `dev` preset.
4. Build the store app with **Ctrl+Shift+B** (or the CMake Tools status-bar button).
5. Press **F5** to run the store app under the debugger.

> Note: configure the **CMake Tools** extension's kit to use your `C++` toolchain if prompted.

## Installing the wallpaper plugin

The plugin is a separate CMake project under `plugin/`. `install-wallpaper.sh` builds and installs it to user-local paths (no root needed):

```bash
./install-wallpaper.sh              # user-local install
./install-wallpaper.sh --system     # system-wide (requires root)
```

Then restart plasmashell:

```bash
systemctl --user restart plasma-plasmashell.service
```

Finally: **Right-click desktop → Configure Desktop and Wallpaper → Wallpaper type: WebWallpaper**, and pick a theme.

## Packages (.deb)

`./packaging/deb/build-deb.sh` builds two Debian packages for Ubuntu 24.04+ / Debian 12+:

- `webwallpaper-plugin_<ver>_amd64.deb` — KDE Plasma 6 wallpaper plugin (QML module + wallpaper package + demo themes)
- `webwallpaper-store_<ver>_amd64.deb` — theme store app (binary + icon + desktop entry)

Output goes to `packaging/deb/dist/`. Build dependencies:

```bash
sudo apt install cmake g++ extra-cmake-modules \
    qt6-base-dev qt6-declarative-dev qt6-webengine-dev \
    libkf6windowsystem-dev libkf6waylandclient-dev \
    libplasma-dev fakeroot
```

## Theme format

A theme is a directory containing a `.theme` file plus the web assets (HTML, JS, images, etc.). The `.theme` file uses the INI format:

```ini
[Theme]
name=Starry Night
description=Animated starfield with nebula colors
author=WebWallpaper
version=1.0
thumbnail=thumbnail.png
entry=index.html
```

| Key | Required | Description |
|-----|----------|-------------|
| `name` | Yes | Display name (used as the theme ID / folder matcher) |
| `description` | No | Short description |
| `author` | No | Author name |
| `version` | No | Theme version |
| `thumbnail` | No | Relative path to preview image (PNG/JPG) |
| `entry` | Yes | Relative path to HTML entry point |

### Creating a theme

```bash
mkdir -p ~/.local/share/webwallpaper/themes/my-theme
cd ~/.local/share/webwallpaper/themes/my-theme

# index.html
cat <<'EOF' > index.html
<!DOCTYPE html>
<html><head><style>
html,body{margin:0;height:100%;overflow:hidden;background:transparent}
</style></head><body>
<canvas id="c"></canvas>
<script>
// your WebGL code here
</script>
</body></html>
EOF

# my.theme
cat <<'EOF' > my.theme
[Theme]
name=my-theme
description=My custom theme
author=Me
version=1.0
entry=index.html
EOF
```

Installed themes are picked up from `~/.local/share/webwallpaper/themes/{themeId}/` (or system-wide from `/usr/share/webwallpaper/themes/`).

## Firebase store

The store reads the theme catalog from a **Firestore** collection named `themes` in a single shared Firebase project (the developer's). Users don't bring their own project — they all point at the shared catalog, so everyone sees the same published themes.

The Firebase **API key is a secret** and is therefore supplied per-user via a local `.env` file. It is never embedded in the application binary.

### Data schema (per document)

```json
{
  "themes": {
    "starry-night": {
      "name": "Starry Night",
      "description": "Animated starfield",
      "author": "John",
      "version": "1.0",
      "thumbnailUrl": "https://storage.../thumb.png",
      "downloadUrl": "https://storage.../starry-night.zip"
    }
  }
}
```

### Configure the app (per-user)

The store reads its Firebase config from `resources/config/config.env`, which is
compiled into the binary at build time. A placeholder template ships as
`resources/config/config.env.example` (committed); copy it to
`resources/config/config.env` and fill in your values **before building**:

```bash
# resources/config/config.env
FIREBASE_API_KEY=YOUR_ASSIGNED_API_KEY
FIREBASE_PROJECT_ID=YOUR_SHARED_PROJECT_ID
# Optional — defaults to "(default)". Set if you use a named Firestore database
# (Firestore now supports multiple databases per project).
FIREBASE_DATABASE_ID=(default)
```

The real `config.env` is gitignored so your key never gets committed. Because
`projectId` always points to the shared project, all users see the same theme
catalog even though each holds a private API key.

As an optional override, the app also looks for an external
`~/.config/webwallpaper/.env` at runtime, which takes precedence over the
embedded value.

## Notes

- The wallpaper is transparent, so theme HTML should use `background: transparent` on `html, body` for the desktop to show through (or set a solid background to fully cover).
- Fullscreen pause/resume is handled on both Wayland and X11 via `WindowMonitor` (`KWayland`/`KF6WindowSystem`) with signal-driven detection and a periodic poll fallback.

## License

GPL-2.0-or-later. See [LICENSE](LICENSE).

