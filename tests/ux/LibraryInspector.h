#pragma once
#include "tests/ux/UxDriver.h"

namespace ux {
inline void revealLibraryControl(Driver &driver, const QString &id) {
    auto *target = driver.item(id);
    auto *viewport = driver.item("library.inspectorScroll");
    driver.frame();
    target->forceActiveFocus(Qt::TabFocusReason);
    driver.wait(
        [&] {
            const auto rect = target->mapRectToItem(viewport, target->boundingRect());
            return rect.top() >= -1 && rect.bottom() <= viewport->height() + 1;
        },
        "Library inspector revealed " + id);
}

inline void clickLibraryControl(Driver &driver, const QString &id) {
    revealLibraryControl(driver, id);
    driver.click(id);
}
} // namespace ux
