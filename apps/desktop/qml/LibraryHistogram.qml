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

    readonly property string activeSource: visible ? source.toString() : ""
    readonly property color backgroundColor: Theme.canvas
    readonly property color gridColor: Theme.divider
    property string requestedSource: ""
    property var samples: null

    function loadPreview() {
        if (!available) return
        if (requestedSource !== activeSource) {
            if (requestedSource.length) unloadImage(requestedSource)
            requestedSource = activeSource
            samples = null
            if (requestedSource.length) loadImage(requestedSource, Qt.size(256, 256))
        }
        requestPaint()
    }
    onAvailableChanged: loadPreview()
    onActiveSourceChanged: loadPreview()
    onImageLoaded: requestPaint()
    onBackgroundColorChanged: requestPaint()
    onGridColorChanged: requestPaint()
    onWidthChanged: requestPaint()
    onHeightChanged: requestPaint()

    function sampleImage(ctx) {
        // A bounded, uniformly spaced sample keeps selection and resize interactive.
        const w = Math.min(96, Math.floor(width))
        const h = Math.min(64, Math.floor(height))
        // getImageData reads backing-store pixels; draw all four corners into that same area.
        const density = Screen.devicePixelRatio
        ctx.save()
        ctx.scale(1 / density, 1 / density)
        ctx.clearRect(0, 0, w, h)
        ctx.drawImage(requestedSource, 0, 0, w, h)
        ctx.restore()
        const pixels = ctx.getImageData(0, 0, w, h).data
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
        if (!samples && requestedSource.length && isImageLoaded(requestedSource))
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
