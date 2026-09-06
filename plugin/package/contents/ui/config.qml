import QtQuick
import QtQuick.Controls as QtControls2
import QtQuick.Layouts

import WebWallpaper

import org.kde.kcmutils as KCM
import org.kde.kirigami as Kirigami

ColumnLayout {
    id: root
    spacing: 0

    property var configDialog
    property var wallpaperConfiguration: wallpaper.configuration
    property string cfg_themePath
    property string cfg_themePathDefault

    signal configurationChanged()

    ThemeModel {
        id: themeModel
    }

    Component.onCompleted: {
        console.log("[webwallpaper-config] loaded, themes=" + themeModel.rowCount())
        // First run: nothing configured yet — pre-select the bundled default
        // theme so the wallpaper has something to load out of the box.
        if (root.cfg_themePath.length === 0 && themeModel.rowCount() > 0) {
            var defaultPath = themeModel.defaultThemePath
            if (defaultPath.length > 0) {
                root.cfg_themePath = defaultPath
            }
        }
    }

    onCfg_themePathChanged: {
        selectCurrent()
        configurationChanged()
    }

    function selectCurrent() {
        var count = themeModel.rowCount()
        for (var i = 0; i < count; ++i) {
            if (themeModel.data(themeModel.index(i, 0), ThemeModel.PathRole) === root.cfg_themePath) {
                grid.view.currentIndex = i
                grid.view.positionViewAtIndex(i, GridView.Center)
                return
            }
        }
    }

    Kirigami.Separator {
        Layout.fillWidth: true
    }

    Kirigami.InlineViewHeader {
        Layout.fillWidth: true
        text: qsTr("Themes")
    }

    KCM.GridView {
        id: grid
        Layout.fillWidth: true
        Layout.fillHeight: true
        objectName: "wallpaperView"

        view.model: themeModel
        view.implicitCellWidth: 240
        view.implicitCellHeight: 180

        view.delegate: KCM.GridDelegate {
            text: model.name
            subtitle: model.description
            thumbnailAvailable: true
            thumbnail: Rectangle {
                anchors.fill: parent
                color: "#111111"
                Image {
                    anchors.fill: parent
                    fillMode: Image.PreserveAspectCrop
                    source: model.preview
                    sourceSize: Qt.size(256, 256)
                    asynchronous: true
                }
            }
            onClicked: {
                root.cfg_themePath = model.path
                grid.view.currentIndex = index
                // Apply the selection to the live desktop immediately.
                if (root.configDialog && root.configDialog.wallpaperConfiguration) {
                    root.configDialog.wallpaperConfiguration.themePath = model.path
                    root.configDialog.applyWallpaper()
                }
                configurationChanged()
            }
        }
    }
}
