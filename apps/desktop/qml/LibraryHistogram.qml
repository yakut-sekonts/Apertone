import QtQuick
import QtQuick.Controls

Canvas {
    id: graph
    objectName: "library.histogram"
    property url source
    implicitWidth: 160
    implicitHeight: 92
    Accessible.role: Accessible.Graphic
    Accessible.name: qsTranslate("Apertone", "Histogram")
    Accessible.description: qsTranslate("Apertone", "RGB histogram sampled from the selected photo preview.")
    ToolTip.visible: hover.hovered
    ToolTip.delay: 700
    ToolTip.text: Accessible.description
    HoverHandler { id: hover }

    readonly property color backgroundColor: Theme.canvas
    readonly property color gridColor: Theme.divider
    property var samples: null
    property var captured: null  // Owns the itemgrabber URL until sampling is complete.
    readonly property url sampleSource: captured ? captured.url : ""
    property int revision: 0

    function capturePreview() {
        if (preview.status !== Image.Ready || !available || !visible) return
        const requestedRevision = revision
        preview.grabToImage(function(result) {
            if (requestedRevision === graph.revision && result) {
                graph.captured = result
                graph.loadImage(result.url)
                graph.requestPaint()
            }
        }, Qt.size(Math.max(1, Math.round(96 / Screen.devicePixelRatio)),
                   Math.max(1, Math.round(64 / Screen.devicePixelRatio))))
    }

    function invalidateSamples() {
        ++revision
        if (sampleSource.toString().length) unloadImage(sampleSource)
        captured = null
        samples = null
        requestPaint()
        Qt.callLater(capturePreview)
    }

    // Sharing Qt Quick's Image cache with the grid and Navigator keeps one
    // thumbnail request per generation, including while its render is pending.
    Image {
        id: preview
        // Render behind the opaque graph so its ready pixels can be sampled.
        z: -1
        source: graph.visible ? graph.source : ""
        sourceSize: Theme.libraryThumbnailSize
        fillMode: Image.PreserveAspectFit
        asynchronous: true
        onStatusChanged: graph.invalidateSamples()
    }

    clip: true
    onSourceChanged: invalidateSamples()
    onVisibleChanged: invalidateSamples()
    onAvailableChanged: invalidateSamples()
    onImageLoaded: requestPaint()
    onBackgroundColorChanged: requestPaint()
    onGridColorChanged: requestPaint()
    onWidthChanged: requestPaint()
    onHeightChanged: requestPaint()

    function sampleImage(ctx) {
        const pixels = ctx.createImageData(sampleSource.toString()).data
        const channels = [new Uint32Array(64), new Uint32Array(64), new Uint32Array(64)]
        let peak = 0
        for (let i = 0; i < pixels.length; i += 4) {
            const alpha = pixels[i + 3]
            if (!alpha) continue
            for (let c = 0; c < 3; ++c) {
                const bin = pixels[i + c] >> 2
                channels[c][bin] += alpha
                peak = Math.max(peak, channels[c][bin])
            }
        }
        return {channels: channels, peak: peak}
    }
    onPaint: {
        if (width < 1 || height < 1) return
        const ctx = getContext("2d")
        if (!samples && captured && isImageLoaded(sampleSource))
            samples = sampleImage(ctx)
        ctx.fillStyle = backgroundColor
        ctx.fillRect(0, 0, width, height)
        ctx.strokeStyle = gridColor
        ctx.lineWidth = 1
        for (let i = 1; i < 4; ++i) {
            const x = Math.round(width * i / 4) + 0.5
            ctx.beginPath(); ctx.moveTo(x, 0); ctx.lineTo(x, height); ctx.stroke()
        }
        if (!samples || !samples.peak) return
        const colors = ["#ff7c78", "#83d18b", "#78aaff"]
        const scale = (height - 2) / samples.peak
        for (let c = 0; c < 3; ++c) {
            const channel = samples.channels[c]
            ctx.beginPath(); ctx.moveTo(0, height)
            for (let i = 0; i < channel.length; ++i)
                ctx.lineTo(i * (width - 1) / (channel.length - 1), height - 1 - channel[i] * scale)
            ctx.lineTo(width, height); ctx.closePath()
            ctx.fillStyle = colors[c]; ctx.globalAlpha = 0.35; ctx.fill()
            ctx.strokeStyle = colors[c]; ctx.globalAlpha = 0.85; ctx.stroke()
        }
        ctx.globalAlpha = 1
    }
}
