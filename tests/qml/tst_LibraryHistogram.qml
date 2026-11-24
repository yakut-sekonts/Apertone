import QtQuick
import QtTest
import "../../apps/desktop/qml" as Desktop

TestCase {
    name: "LibraryHistogram"
    when: windowShown
    width: 300; height: 160
    visible: true

    Desktop.LibraryHistogram { id: graph; width: 240; height: 90 }

    function swatch(red, green, blue) {
        // An ASCII PPM keeps the test image independent of platform image plugins.
        return "data:image/x-portable-pixmap," + encodeURIComponent(
            "P3\n1 1\n255\n" + red + " " + green + " " + blue + "\n")
    }
    function checkSwatch(red, green, blue) {
        tryVerify(function() { return graph.samples && graph.samples.peak > 0 })
        const expected = [red, green, blue]
        for (let index = 0; index < expected.length; ++index) {
            const channel = graph.samples.channels[index]
            for (let bin = 0; bin < channel.length; ++bin)
                compare(channel[bin], bin === (expected[index] >> 2) ? graph.samples.peak : 0)
        }
    }
    function test_selection_and_clear() {
        const first = swatch(252, 36, 92)
        const second = swatch(52, 96, 204)
        graph.source = first
        checkSwatch(252, 36, 92)
        graph.source = second
        checkSwatch(52, 96, 204)
        graph.source = first
        graph.source = ""
        compare(graph.samples, null)
        waitForRendering(graph)
        compare(graph.samples, null)
        graph.source = second
        checkSwatch(52, 96, 204)
    }

    function test_all_four_quadrants() {
        const pixels = []
        for (let y = 0; y < 16; ++y) {
            for (let x = 0; x < 16; ++x) {
                if (y < 8) pixels.push(x < 8 ? "255 0 0" : "0 0 255")
                else pixels.push(x < 8 ? "0 255 0" : "255 255 255")
            }
        }
        graph.source = "data:image/x-portable-pixmap," + encodeURIComponent(
            "P3\n16 16\n255\n" + pixels.join(" ") + "\n")
        tryVerify(function() { return graph.samples && graph.samples.peak > 0 })
        for (let c = 0; c < 3; ++c) {
            const bins = graph.samples.channels[c]
            let total = 0
            for (let i = 0; i < bins.length; ++i) total += bins[i]
            // Half of each channel is dark, half bright, with a small resampling boundary.
            verify(bins[0] / total > 0.4 && bins[0] / total <= 0.51)
            verify(bins[63] / total > 0.4 && bins[63] / total <= 0.51)
        }
    }
}
