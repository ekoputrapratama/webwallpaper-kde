import QtQuick 2.15
import QtWebEngine
import org.kde.plasma.plasmoid
import WebWallpaper

WallpaperItem {
    id: root
    anchors.fill: parent

    property string themePath: root.configuration.themePath || ""
    // Fall back to the bundled demo theme when nothing is configured yet, so a
    // fresh install renders a wallpaper immediately instead of a placeholder.
    readonly property string activeTheme: {
        if (root.themePath.length > 0)
            return root.themePath
        return themeModel.defaultThemePath
    }
    property bool isPaused: false
    property url pausedScreenshot
    property var pausedGrabResult: null

    function pauseScript() {
        return "(function(){" +
            "if(window.__ww_paused)return;" +
            "window.__ww_paused=true;" +
            "window.__ww_raf=window.requestAnimationFrame;" +
            "window.__ww_caf=window.cancelAnimationFrame;" +
            "window.__ww_frames=[];" +
            "window.requestAnimationFrame=function(cb){" +
            "  var id=window.__ww_raf.call(window,cb);" +
            "  window.__ww_frames.push(id);" +
            "  return id;" +
            "};" +
            "for(var i=0;i<10000;i++){window.cancelAnimationFrame.call(window,i);}" +
            "window.__ww_setInterval=window.setInterval;" +
            "window.__ww_setTimeout=window.setTimeout;" +
            "window.setInterval=function(){" +
            "  var id=window.__ww_setInterval.apply(window,arguments);" +
            "  window.__ww_frames.push(id);" +
            "  return id;" +
            "};" +
            "window.setTimeout=function(){" +
            "  var id=window.__ww_setTimeout.apply(window,arguments);" +
            "  window.__ww_frames.push(id);" +
            "  return id;" +
            "};" +
            "if(!window.__ww_gl_overridden){" +
            "  window.__ww_gl_overridden=true;" +
            "  window.__ww_gl_draws=[];" +
            "  var checkGL=function(name){" +
            "    var P=window[name];" +
            "    if(!P||!P.prototype)return;" +
            "    var methods=['drawArrays','drawElements'," +
            "      'drawArraysInstanced','drawElementsInstanced'," +
            "      'drawRangeElements'];" +
            "    methods.forEach(function(m){" +
            "      if(typeof P.prototype[m]==='function'){" +
            "        window.__ww_gl_draws.push({proto:P,m:m,orig:P.prototype[m]});" +
            "        P.prototype[m]=function(){};" +
            "      }" +
            "    });" +
            "  };" +
            "  checkGL('WebGLRenderingContext');" +
            "  checkGL('WebGL2RenderingContext');" +
            "}" +
            "document.dispatchEvent(new CustomEvent('webwallpaper:pause'));"+
            "if(typeof document.hidden!=='undefined'){" +
            "  try{Object.defineProperty(document,'hidden',{value:true,configurable:true,writable:true});}catch(e){}" +
            "  try{Object.defineProperty(document,'visibilityState',{value:'hidden',configurable:true,writable:true});}catch(e){}" +
            "  document.dispatchEvent(new Event('visibilitychange'));" +
            "}" +
        "})()"
    }

    function resumeScript() {
        return "(function(){" +
            "if(!window.__ww_paused)return;" +
            "window.__ww_paused=false;" +
            "window.requestAnimationFrame=window.__ww_raf;" +
            "window.cancelAnimationFrame=window.__ww_caf;" +
            "window.setInterval=window.__ww_setInterval;" +
            "window.setTimeout=window.__ww_setTimeout;" +
            "if(window.__ww_gl_draws){" +
            "  window.__ww_gl_draws.forEach(function(d){" +
            "    d.proto.prototype[d.m]=d.orig;" +
            "  });" +
            "  window.__ww_gl_draws=[];" +
            "}" +
            "document.dispatchEvent(new CustomEvent('webwallpaper:resume'));"+
            "try{Object.defineProperty(document,'hidden',{value:false,configurable:true,writable:true});}catch(e){}" +
            "try{Object.defineProperty(document,'visibilityState',{value:'visible',configurable:true,writable:true});}catch(e){}" +
            "document.dispatchEvent(new Event('visibilitychange'));" +
        "})()"
    }

    function doPause() {
        if (root.isPaused) return;
        view.grabToImage(function(result) {
            if (!windowMonitor.hasFullscreenWindow) return;
            if (result && result.url) {
                root.pausedGrabResult = result;
                root.pausedScreenshot = result.url;
            }
            root.isPaused = true;
            view.runJavaScript(pauseScript());
            console.log("[webwallpaper] paused (fullscreen window detected)");
        });
    }

    function doResume() {
        if (!root.isPaused) return;
        root.isPaused = false;
        root.pausedScreenshot = "";
        root.pausedGrabResult = null;
        view.runJavaScript(resumeScript());
        console.log("[webwallpaper] resumed (no fullscreen windows)");
    }

    WindowMonitor {
        id: windowMonitor
    }

    Timer {
        id: fullscreenPoll
        interval: 500
        running: root.activeTheme.length > 0
        repeat: true
        property bool lastValue: false
        onTriggered: {
            var current = windowMonitor.hasFullscreenWindow;
            if (current !== lastValue) {
                lastValue = current;
                if (current) {
                    root.doPause();
                } else {
                    root.doResume();
                }
            }
        }
    }

    function entryFile() {
        let dir = root.activeTheme
        if (!dir || dir.length === 0) {
            return ""
        }
        if (dir.startsWith("file://")) {
            dir = dir.slice(7)
        }
        while (dir.length > 1 && dir.endsWith("/")) {
            dir = dir.slice(0, -1)
        }
        return "file://" + dir + "/index.html"
    }

    ThemeModel {
        id: themeModel
    }

    WebEngineProfile {
        id: wpProfile
        offTheRecord: true
    }

    Image {
        id: frozenFrame
        anchors.fill: parent
        source: root.pausedScreenshot
        visible: root.isPaused && root.pausedScreenshot.toString().length > 0
        fillMode: Image.PreserveAspectCrop
        z: 1
    }

    WebEngineView {
        id: view
        anchors.fill: parent
        visible: root.activeTheme.length > 0 && !root.isPaused
        profile: wpProfile
        url: root.activeTheme.length > 0 ? entryFile() : ""
        backgroundColor: "#111111"
        settings.javascriptEnabled: true
        settings.webGLEnabled: true
        settings.localContentCanAccessFileUrls: true
        settings.localContentCanAccessRemoteUrls: true
        settings.accelerated2dCanvasEnabled: true
        onJavaScriptConsoleMessage: function(level, message, lineNumber, sourceID) {
            console.log("[webwallpaper]", sourceID + ":" + lineNumber + ":", message)
        }
        onLoadingChanged: function(loadRequest) {
            if (loadRequest.status === WebEngineView.LoadSucceededStatus) {
                if (root.isPaused) {
                    view.runJavaScript(pauseScript())
                } else {
                    view.runJavaScript("window.__ww_paused=false;")
                }
            }
        }
    }

    Rectangle {
        anchors.fill: parent
        visible: root.activeTheme.length === 0
        color: "#111111"
        Text {
            anchors.fill: parent
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            text: "Select a WebWallpaper theme in wallpaper settings"
            color: "#cccccc"
            font.pixelSize: 14
            wrapMode: Text.Wrap
        }
    }
}
