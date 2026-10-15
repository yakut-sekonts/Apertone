import QtQuick
Item {
    id: overlay
    objectName: "localPhotoOverlay"
    visible: editor.localMode && editor.hasPhoto && !editor.original
    clip: true
    property var handles: editor.localHandles
    property var brushOutline: []
    property real hoverX: -1
    property real hoverY: -1
    function refreshBrush() {brushOutline=hoverX>=0?editor.localBrushOutline(hoverX,hoverY):[];canvas.requestPaint()}
    Canvas {
        id: canvas
        anchors.fill: parent
        onWidthChanged: requestPaint()
        onHeightChanged: requestPaint()
        onPaint: {
            const ctx = getContext("2d")
            ctx.clearRect(0,0,width,height)
            if (overlay.handles.length === 3) {
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
            if(overlay.brushOutline.length>0){
                ctx.strokeStyle=editor.localBrushErase?"#00cbea":"white";ctx.lineWidth=1.5
                ctx.beginPath()
                for(let i=0;i<overlay.brushOutline.length;++i){const p=overlay.brushOutline[i];if(i===0)ctx.moveTo(p.x*width,p.y*height);else ctx.lineTo(p.x*width,p.y*height)}
                ctx.closePath();ctx.stroke()
            }
        }
    }
    Connections { target: editor; function onStateChanged() { if(gesture.adjusting&&!editor.localGestureActive)gesture.adjusting=false;overlay.refreshBrush() } }
    MouseArea {
        id: gesture
        objectName: "localMaskGestureArea"
        anchors.fill: parent
        enabled: !editor.busy && !editor.editingLocked
        cursorShape: Qt.CrossCursor
        hoverEnabled: true
        onExited: {overlay.hoverX=-1;overlay.refreshBrush()}
        property bool adjusting: false
        onPressed: function(mouse) {
            if(width<=0||height<=0) return
            if(editor.localState.shape==="color"||editor.localState.shape==="luminance"){
                editor.pickLocalRange(mouse.x/width,mouse.y/height);return
            }
            let selected=-1, distance=14*14
            for(const handle of overlay.handles) {
                const dx=handle.x*width-mouse.x,dy=handle.y*height-mouse.y,d=dx*dx+dy*dy
                if(d<distance) { selected=handle.handle;distance=d }
            }
            adjusting=editor.beginLocalGesture(selected,mouse.x/width,mouse.y/height)
            if(!adjusting) mouse.accepted=false
        }
        onPositionChanged: function(mouse) {
            if(editor.localState.shape==="brush"&&(mouse.x<0||mouse.y<0||mouse.x>width||mouse.y>height)){
                if(adjusting)editor.endEdit()
                adjusting=false;overlay.hoverX=-1;overlay.refreshBrush();return
            }
            overlay.hoverX=Math.max(0,Math.min(1,mouse.x/width));overlay.hoverY=Math.max(0,Math.min(1,mouse.y/height));overlay.refreshBrush()
            if(pressed&&adjusting)editor.adjustLocalGesture(overlay.hoverX,overlay.hoverY)
        }
        onReleased: function(mouse) {
            if(adjusting){editor.adjustLocalGesture(Math.max(0,Math.min(1,mouse.x/width)),Math.max(0,Math.min(1,mouse.y/height)));editor.endEdit()}
            adjusting=false
        }
        onCanceled: { if(adjusting)editor.endEdit();adjusting=false }
    }
}
