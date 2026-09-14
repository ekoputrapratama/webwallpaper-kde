import QtQuick
import QtTest

import WebWallpaper

// Integration check: the WebWallpaper QML module must load inside a QML engine
// and expose a working ThemeModel. Guards against module-load regressions that
// used to blank the wallpaper config grid (missing shared lib / bad rpath).
TestCase {
    id: root
    name: "WebWallpaperQml"

    function test_themeModelPopulatesFromOverrideDir() {
        const component = Qt.createComponent("WebWallpaper", "ThemeModel")
        compare(component.status, Component.Ready, component.errorString())

        const model = component.createObject(root)
        verify(model)

        // WEBWALLPAPER_THEMES_DIR is set by CMake to a scratch dir holding demo.
        verify(model.rowCount() >= 1)

        const name = model.data(model.index(0, 0), ThemeModel.NameRole)
        verify(name.length > 0)

        const path = model.data(model.index(0, 0), ThemeModel.PathRole)
        verify(path.length > 0)

        // Fixture theme is named "demo", so it must be the default as well.
        verify(model.defaultThemePath.length > 0)

        model.destroy()
        component.destroy()
    }
}