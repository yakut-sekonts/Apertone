import QtQuick
Item {
    id: overlay
    objectName: "localPhotoOverlay"
    visible: editor.localMode && editor.hasPhoto && !editor.original
    clip: true
    property var handles: editor.localHandles
    Canvas {
        id: canvas
        anchors.fill: parent
        onWidthChanged: requestPaint()
        onHeightChanged: requestPaint()
        onPaint: {
            const ctx = getContext("2d")
            ctx.clearRect(0,0,width,height)
            if (overlay.handles.length !== 3) return
            ctx.strokeStyle = "white"; ctx.lineWidth = 1.5
            ctx.beginPath()
            ctx.moveTo(overlay.handles[0].x*width, overlay.handles[0].y*height)
            ctx.lineTo(overlay.handles[1].x*width, overlay.handles[1].y*height)
            ctx.moveTo(overlay.handles[0].x*width, overlay.handles[0].y*height)
            ctx.lineTo(overlay.handles[2].x*width, overlay.handles[2].y*height)
            ctx.stroke()
            for(const handle of overlay.handles) {
                ctx.fillStyle = "#dfae77"; ctx.beginPath()
                ctx.arc(handle.x*width,handle.y*height,6,0,2*Math.PI); ctx.fill(); ctx.stroke()
            }
        }
    }
    Connections { target: editor; function onStateChanged() { canvas.requestPaint() } }
    MouseArea {
        objectName: "localMaskGestureArea"
        anchors.fill: parent
        enabled: !editor.busy && !editor.editingLocked
        cursorShape: Qt.CrossCursor
        property bool adjusting: false
        onPressed: function(mouse) {
            if(width<=0||height<=0) return
            let selected=-1, distance=14*14
            for(const handle of overlay.handles) {
                const dx=handle.x*width-mouse.x,dy=handle.y*height-mouse.y,d=dx*dx+dy*dy
                if(d<distance) { selected=handle.handle;distance=d }
            }
            adjusting=editor.beginLocalGesture(selected,mouse.x/width,mouse.y/height)
            if(!adjusting) mouse.accepted=false
        }
        onPositionChanged: function(mouse) { if(pressed&&adjusting)editor.adjustLocalGesture(Math.max(0,Math.min(1,mouse.x/width)),Math.max(0,Math.min(1,mouse.y/height))) }
        onReleased: { if(adjusting)editor.endEdit();adjusting=false }
        onCanceled: { if(adjusting)editor.endEdit();adjusting=false }
    }
}
