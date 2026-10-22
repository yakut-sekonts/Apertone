#include "tests/ux/UxContext.h"
#include <QJsonArray>
#include <QSignalSpy>
#include <algorithm>
#include <cmath>

namespace ux {
namespace {
struct VisibleCard {
    QString selector, asset;
    int row;
    QRectF box;
};
std::vector<VisibleCard> visibleCards(Context &c) {
    auto *grid = c.driver->item("library.grid");
    std::vector<VisibleCard> cards;
    std::function<void(QQuickItem *)> visit = [&](QQuickItem *parent) {
        for (auto *item : parent->childItems()) {
            if (item->objectName().startsWith("library.card.")) {
                const auto box = item->mapRectToItem(grid, item->boundingRect());
                const auto asset = item->property("assetID").toString();
                if (item->isVisible() && box.intersects(grid->boundingRect()))
                    cards.push_back(
                        {item->objectName(), asset, item->property("index").toInt(), box});
            } else
                visit(item);
        }
    };
    visit(grid);
    std::sort(cards.begin(), cards.end(),
              [](const auto &a, const auto &b) { return a.row < b.row; });
    return cards;
}
void settled(Context &c) {
    c.driver->wait(
        [&] {
            return !c.library.loading() && !c.library.working() && !c.library.selecting() &&
                   !c.driver->object("library.grid")->property("moving").toBool();
        },
        "Library selection and scroll settled");
    require(c.library.error().isEmpty(), c.library.error());
    c.driver->frame();
}
void scrollTo(Context &c, double fraction) {
    auto &d = *c.driver;
    auto *bar = d.item("library.scrollBar");
    auto *thumb =
        qobject_cast<QQuickItem *>(qvariant_cast<QObject *>(bar->property("contentItem")));
    require(thumb && thumb->height() > 0, "Library scroll has no usable scrollbar thumb");
    const auto center = thumb->mapToItem(bar, QPointF(thumb->width() / 2, thumb->height() / 2));
    const double travel = bar->property("availableHeight").toDouble() - thumb->height();
    require(travel > 0 && bar->width() > 0 && bar->height() > 0, "Library cannot scroll");
    const double target =
        bar->property("topPadding").toDouble() + travel * fraction + thumb->height() / 2;
    d.drag("library.scrollBar", {center.x() / bar->width(), center.y() / bar->height()},
           {.5, target / bar->height()});
    settled(c);
    auto *grid = d.item("library.grid");
    const double maximum = grid->property("contentHeight").toDouble() - grid->height();
    require(maximum > 0 && std::abs(grid->property("contentY").toDouble() / maximum - fraction) <=
                               2 / travel + 1e-4,
            "Actual scrollbar input did not reach the requested deep viewport");
    d.wait(
        [&] {
            const auto cards = visibleCards(c);
            return !cards.empty() && std::all_of(cards.begin(), cards.end(), [&](const auto &card) {
                return !card.asset.isEmpty() &&
                       c.library.data(c.library.index(card.row), LibraryModel::AssetID)
                               .toString() == card.asset;
            });
        },
        "Deep Library metadata pages ready");
}
QJsonObject snapshot(Context &c) {
    const auto cards = visibleCards(c);
    require(!cards.empty() && !cards.front().asset.isEmpty(),
            "Library scroll oracle has no populated anchor");
    const auto first = cards.front();
    auto *grid = c.driver->item("library.grid");
    return {{"anchor", first.asset},
            {"anchor_row", first.row},
            {"anchor_offset", first.box.y()},
            {"content_y", grid->property("contentY").toDouble()},
            {"origin_y", grid->property("originY").toDouble()},
            {"height", grid->height()},
            {"count", c.library.count()}};
}
VisibleCard clickTarget(Context &c, bool last) {
    auto cards = visibleCards(c);
    auto *grid = c.driver->item("library.grid");
    std::erase_if(cards,
                  [&](const auto &card) { return !grid->boundingRect().contains(card.box); });
    require(cards.size() >= 2, "Library regression needs two fully visible click targets");
    return last ? cards.back() : cards.front();
}
void verify(Context &c, const QJsonObject &before, const QString &action, QJsonArray &checks) {
    settled(c);
    const auto after = snapshot(c);
    QJsonObject entry{{"action", action}, {"before", before}, {"after", after}};
    checks.append(entry);
    c.driver->log(entry);
    require(before["anchor"] == after["anchor"] && before["count"] == after["count"] &&
                std::abs(before["anchor_offset"].toDouble() - after["anchor_offset"].toDouble()) <=
                    1 &&
                std::abs(before["content_y"].toDouble() - after["content_y"].toDouble()) <= 1 &&
                std::abs(before["height"].toDouble() - after["height"].toDouble()) <= 1,
            "Photo selection/open/return changed Library's visible asset anchor or offset");
}
} // namespace

bool libraryScrollCandidate(Context &c, const QString &id) {
    if (id != "CANDIDATE-LIBRARY-SELECTION-SCROLL")
        return false;
    constexpr int count = 176;
    require(count > LibraryModel::pageSize, "Fixture must cross a production metadata page");
    c.import(count);
    auto &d = *c.driver;
    QJsonArray checks;
    try {
        for (int phase = 0; phase < 3; ++phase) {
            const QString search = phase == 1 ? "Photo-01" : "";
            if (phase > 0) {
                d.type("library.search", search);
                d.wait(
                    [&] {
                        return !c.library.loading() &&
                               c.library.count() == (phase == 1 ? 76 : count);
                    },
                    "Actual Library filter completed");
            }
            scrollTo(c, phase == 0 ? .88 : .6);
            const auto before = snapshot(c);
            require(before["content_y"].toDouble() > before["height"].toDouble(),
                    "Selection fixture is not beyond the first viewport");
            if (phase == 0)
                require(before["anchor_row"].toInt() >= LibraryModel::pageSize,
                        "Selection fixture did not reach the second metadata page");
            QSignalSpy resets(&c.library, &QAbstractItemModel::modelReset);
            QSignalSpy activation(&c.library, &LibraryModel::openRequested);
            QString chosen;
            for (bool last : {false, true}) {
                const auto target = clickTarget(c, last);
                chosen = target.asset;
                auto *content = qobject_cast<QQuickItem *>(
                    qvariant_cast<QObject *>(d.object("library.grid")->property("contentItem")));
                require(content, "Library scroll observer has no content item");
                const double initialY = content->y();
                QJsonArray positions{initialY};
                bool overflow = false;
                QObject observer;
                QObject::connect(content, &QQuickItem::yChanged, &observer, [&] {
                    if (positions.size() < 64)
                        positions.append(content->y());
                    else
                        overflow = true;
                });
                d.click(target.selector);
                verify(c, before, "single-click", checks);
                checks.append(QJsonObject{{"action", "single-click-motion"},
                                          {"content_item_y", positions},
                                          {"overflow", overflow}});
                const auto selected = c.library.selectionIDs();
                require(selected.size() == 1 && selected.front() == chosen.toStdString() &&
                            activation.isEmpty(),
                        "Single Library click selected the wrong asset or opened Develop");
                require(!overflow && std::all_of(positions.begin(), positions.end(),
                                                 [&](const auto &value) {
                                                     return std::isfinite(value.toDouble()) &&
                                                            std::abs(value.toDouble() - initialY) <=
                                                                1;
                                                 }),
                        "Single click transiently moved the Library viewport by more than one "
                        "logical pixel");
            }
            const auto target = clickTarget(c, true);
            d.doubleClick(target.selector);
            c.loaded();
            require(c.editor.photoID() == chosen && activation.count() == 1 &&
                        !c.window->property("libraryMode").toBool(),
                    "Deep Library double click did not open exactly its selected asset");
            d.click("main.libraryButton");
            verify(c, before, "open-and-return", checks);
            require(resets.isEmpty(), "Opening an already loaded asset reset the Library model");
            // Mouse retention must not disable the view's ordinary arrow-key
            // reveal of a current item outside the viewport.
            d.click(clickTarget(c, true).selector);
            auto *grid = d.item("library.grid");
            const int previousRow = grid->property("currentIndex").toInt();
            const int columns =
                static_cast<int>(grid->width() / grid->property("cellWidth").toDouble());
            const double previousY = grid->property("contentY").toDouble();
            d.key(Qt::Key_Down);
            d.wait([&] { return grid->property("contentY").toDouble() > previousY + 1; },
                   "Arrow-key current-item reveal completed", 5000);
            settled(c);
            checks.append(QJsonObject{{"action", "keyboard-down"},
                                      {"before_row", previousRow},
                                      {"after_row", grid->property("currentIndex").toInt()},
                                      {"before_y", previousY},
                                      {"after", snapshot(c)}});
            require(columns > 0 &&
                        grid->property("currentIndex").toInt() == previousRow + columns &&
                        grid->property("contentY").toDouble() > previousY + 1,
                    "Mouse viewport retention broke arrow-key current-item reveal");
        }
        writeJson(c.root + "/library-selection-scroll.json",
                  {{"status", "PASS"},
                   {"photos", count},
                   {"checks", checks},
                   {"native_file_chooser", "fixture-boundary"},
                   {"sort", "default filename/ID"}});
    } catch (...) {
        writeJson(c.root + "/library-selection-scroll.json",
                  {{"status", "FAIL"}, {"checks", checks}});
        throw;
    }
    return true;
}
} // namespace ux
