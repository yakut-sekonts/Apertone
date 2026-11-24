import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
ColumnLayout {
    id:root
    required property string source
    required property string title
    required property real zoom
    required property real centerX
    required property real centerY
    property bool pixelScale:false
    property bool onCanvas: false
    property bool syncing:false
    readonly property real viewWidth: canvas.width
    readonly property real viewHeight: canvas.height
    readonly property rect visibleSourceRect: Qt.rect(
        Math.max(0, (canvas.contentX - photo.x) / Math.max(1, photo.width)),
        Math.max(0, (canvas.contentY - photo.y) / Math.max(1, photo.height)),
        Math.min(1, canvas.width / Math.max(1, photo.width)),
        Math.min(1, canvas.height / Math.max(1, photo.height)))
    signal panned(real x,real y)
    function position() {
        if(!canvas)return
        syncing=true
        canvas.contentX=Math.max(0,Math.min(canvas.contentWidth-canvas.width,centerX*canvas.contentWidth-canvas.width/2))
        canvas.contentY=Math.max(0,Math.min(canvas.contentHeight-canvas.height,centerY*canvas.contentHeight-canvas.height/2))
        syncing=false
    }
    function publish() {
        if(!syncing && canvas.dragging)
            panned(canvas.contentWidth>canvas.width?(canvas.contentX+canvas.width/2)/canvas.contentWidth:centerX,
                   canvas.contentHeight>canvas.height?(canvas.contentY+canvas.height/2)/canvas.contentHeight:centerY)
    }
    onCenterXChanged:position()
    onCenterYChanged:position()
    Label {visible:root.title.length>0;text:root.title;color:root.onCanvas ? Theme.canvasText : Theme.text;textFormat:Text.PlainText;elide:Text.ElideMiddle;Layout.fillWidth:true}
    Flickable {
        id:canvas
        Layout.fillWidth:true;Layout.fillHeight:true;clip:true
        boundsBehavior:Flickable.StopAtBounds
        property real imageScale:root.pixelScale?root.zoom:Math.min(width/Math.max(1,photo.implicitWidth),height/Math.max(1,photo.implicitHeight))*root.zoom
        contentWidth:Math.max(width,photo.width);contentHeight:Math.max(height,photo.height)
        onContentWidthChanged:root.position()
        onContentHeightChanged:root.position()
        onContentXChanged:root.publish()
        onContentYChanged:root.publish()
        Image {
            id:photo
            objectName: root.objectName + ".image"
            source:root.source;cache:false;asynchronous:true
            width:implicitWidth*canvas.imageScale;height:implicitHeight*canvas.imageScale
            x:Math.max(0,(canvas.width-width)/2);y:Math.max(0,(canvas.height-height)/2)
            smooth: !root.pixelScale || Math.abs(root.zoom * Screen.devicePixelRatio - 1) > 0.001
        }
    }
}
