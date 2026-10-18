#pragma once
#include <QPointF>
#include <QRectF>
#include <QSizeF>

class QQuickWindow;
namespace ux {
// Read-only AppKit geometry. No native/hardware event injection, no screen capture.
struct NativeRetinaGeometry {
    int windowNumber = 0;
    unsigned displayID = 0;
    bool viewFlipped = false;
    double backingScale = 0;
    QSizeF viewLogicalSize;
    QRectF viewBackingBounds;
};
struct NativeRetinaPoint {
    QPointF backingFromTopLeft;
    QPointF roundTripLogical;
};
NativeRetinaGeometry nativeRetinaGeometry(QQuickWindow &window);
NativeRetinaPoint nativeRetinaPoint(QQuickWindow &window, const QPointF &logical);
} // namespace ux
