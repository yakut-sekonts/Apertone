import QtQuick
import QtQuick.Controls
import QtTest
import "../../apps/desktop/qml" as Desktop

TestCase {
    id: test
    name: "FingerprintReview"
    when: windowShown
    width: 1000; height: 700
    visible: true

    QtObject {
        id: library
        property bool fingerprintBusy: false
        property bool fingerprintHasNext: true
        property bool fingerprintHasPrevious: false
        property bool working: false
        property string fingerprintError: ""
        property var fingerprintRows: []
        property int opens: 0
        property int closes: 0
        property int nexts: 0
        property int previous: 0
        function openFingerprintReview() {
            opens++
            fingerprintRows = [{checksum: "a".repeat(64), size: 42, assetID: "id-1", path: "/Фото/<plain>.jpg"}]
            fingerprintHasNext = true
            fingerprintHasPrevious = false
        }
        function closeFingerprintReview() { closes++ }
        function nextFingerprintPage() { nexts++; fingerprintHasNext = false; fingerprintHasPrevious = true }
        function previousFingerprintPage() { previous++; fingerprintHasPrevious = false; fingerprintHasNext = true }
    }
    Desktop.FingerprintReviewDialog { id: dialog; libraryModel: library }

    function test_navigation_and_cancel() {
        dialog.open()
        tryCompare(dialog, "visible", true)
        compare(library.opens, 1)
        const next = findChild(dialog, "library.fingerprintNext")
        const prev = findChild(dialog, "library.fingerprintPrevious")
        const refresh = findChild(dialog, "library.fingerprintRefresh")
        const row = findChild(dialog.contentItem, "library.fingerprintRow")
        verify(next !== null && prev !== null && refresh !== null && row !== null)
        verify(row.text.includes("/Фото/<plain>.jpg"))
        verify(next.enabled && !prev.enabled)
        mouseClick(next)
        compare(library.nexts, 1)
        verify(!next.enabled && prev.enabled)
        mouseClick(prev)
        compare(library.previous, 1)
        verify(next.enabled && !prev.enabled)
        mouseClick(refresh)
        compare(library.opens, 2)
        dialog.close()
        tryCompare(dialog, "visible", false)
        compare(library.closes, 1)
    }
}
