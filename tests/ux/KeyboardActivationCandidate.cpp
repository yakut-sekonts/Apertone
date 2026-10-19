#include "apps/shared/PresetStore.h"
#include "tests/ux/UxContext.h"
#include "tests/ux/UxPixels.h"
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QEvent>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QKeyEvent>
#include <QPointer>
#include <QQuickWindow>
#include <QSet>
#include <QSignalSpy>
#include <QStyleHints>
#include <QUuid>
#include <cstdio>
#include <exception>

// Bounded supplemental functional regression. Not a complete KEY015 adapter.
namespace ux {
namespace {
constexpr int maximumTabSteps = 96; // One bounded actual traversal, not a stress count.
constexpr auto candidateID = "CANDIDATE-KEYBOARD-SELECTED-ASSET-ACTIVATION";
constexpr auto keypadCandidateID = "CANDIDATE-KEYBOARD-KEYPAD-ASSET-ACTIVATION";
struct KeyboardNavigation {
    const Qt::TabFocusBehavior previous = QGuiApplication::styleHints()->tabFocusBehavior();
    KeyboardNavigation() {
        QGuiApplication::styleHints()->setTabFocusBehavior(Qt::TabFocusAllControls);
    }
    ~KeyboardNavigation() {
        QGuiApplication::styleHints()->setTabFocusBehavior(previous);
    }
};
QString digest(const QString &path) {
    QFile file(path);
    require(file.open(QIODevice::ReadOnly), "Cannot read keyboard activation fixture: " + path);
    QCryptographicHash hash(QCryptographicHash::Sha256);
    require(hash.addData(&file), "Cannot hash complete keyboard activation source");
    return QString::fromLatin1(hash.result().toHex());
}
QJsonObject fullDocument(const luma::EditDocument &document) {
    const auto encoded = QJsonDocument::fromJson(luma::encodePreset(
        {"Keyboard activation document", {}, document, luma::allSettingsGroups}));
    require(encoded.isObject(), "Cannot retain complete keyboard activation document");
    return encoded.object();
}
bool inside(QQuickItem *item, QQuickItem *ancestor) {
    if (!ancestor)
        return false;
    for (auto *parent = item; parent; parent = parent->parentItem())
        if (parent == ancestor)
            return true;
    return false;
}
bool centerVisible(QQuickItem *item) {
    if (!item || !item->isVisible() || !item->isEnabled() || !item->window() ||
        item->width() <= 0 || item->height() <= 0)
        return false;
    const auto center = item->mapToScene({item->width() / 2, item->height() / 2});
    if (!QRectF(0, 0, item->window()->width(), item->window()->height()).contains(center))
        return false;
    for (auto *ancestor = item; ancestor; ancestor = ancestor->parentItem())
        if (ancestor->clip() && !ancestor->boundingRect().contains(ancestor->mapFromScene(center)))
            return false;
    return true;
}
QJsonObject itemState(QQuickItem *item) {
    if (!item)
        return {{"present", false}};
    return {{"present", true},
            {"object_name", item->objectName()},
            {"class", QString::fromUtf8(item->metaObject()->className())},
            {"visible", item->isVisible()},
            {"enabled", item->isEnabled()},
            {"active_focus", item->hasActiveFocus()},
            {"active_focus_on_tab", item->activeFocusOnTab()},
            {"center_visible_under_clips", centerVisible(item)}};
}
// Observes only; never synthesizes input or throws from Qt dispatch.
class PointerAudit final : public QObject {
  public:
    explicit PointerAudit(QQuickWindow *caseWindow) : window(caseWindow) {
        QCoreApplication::instance()->installEventFilter(this);
    }
    ~PointerAudit() override {
        QCoreApplication::instance()->removeEventFilter(this);
    }
    QJsonArray events, keyEvents;
    int keypadPresses = 0, keypadRepeats = 0;

  protected:
    bool eventFilter(QObject *receiver, QEvent *event) override {
        if (event->type() == QEvent::KeyPress && receiver == window.data()) {
            const auto *key = static_cast<QKeyEvent *>(event);
            keyEvents.append(QJsonObject{{"key", key->key()},
                                         {"modifiers", static_cast<int>(key->modifiers())},
                                         {"auto_repeat", key->isAutoRepeat()}});
            if (key->key() == Qt::Key_Enter && key->modifiers() == Qt::KeypadModifier) {
                if (key->isAutoRepeat())
                    ++keypadRepeats;
                else
                    ++keypadPresses;
            }
        }
        switch (event->type()) {
        case QEvent::MouseButtonPress:
        case QEvent::MouseButtonRelease:
        case QEvent::MouseButtonDblClick:
        case QEvent::MouseMove:
        case QEvent::Wheel:
        case QEvent::TouchBegin:
        case QEvent::TouchUpdate:
        case QEvent::TouchEnd:
            events.append(QJsonObject{
                {"type", static_cast<int>(event->type())},
                {"receiver_name", receiver->objectName()},
                {"receiver_class", QString::fromUtf8(receiver->metaObject()->className())},
                {"spontaneous", event->spontaneous()}});
            break;
        default:
            break;
        }
        return false;
    }

  private:
    QPointer<QQuickWindow> window;
};
struct Fixture {
    QString publicSource, source, sha, id;
    QImage expectedPixels;
};
Fixture fixture(Context &c) {
    auto &d = *c.driver;
    require(c.library.count() == 0 && !c.editor.hasPhoto(),
            "Keyboard activation candidate requires a fresh private Context");
    Fixture value;
    value.publicSource = c.fixture + "/Photo-0000.png";
    value.sha = digest(value.publicSource);
    const auto directory = c.root + "/owned-input";
    const auto copy = directory + "/Keyboard-source.png";
    require(QDir().mkpath(directory) && !QFileInfo::exists(copy) &&
                QFile::copy(value.publicSource, copy) && !QFileInfo(copy).isSymLink() &&
                digest(copy) == value.sha,
            "Cannot create a new private generated keyboard activation source");
    value.source = QFileInfo(copy).canonicalFilePath();
    require(!value.source.isEmpty() &&
                value.source.startsWith(QFileInfo(c.root).canonicalFilePath() + '/'),
            "Keyboard activation original escaped the private case root");
    // All pointer inputs below belong ONLY to declared import fixture setup.
    // The keyboard acceptance boundary is recorded after setup has fully settled.
    d.log({{"action", "declared_keyboard_activation_import_fixture_begin"},
           {"pointer_setup_inputs", true},
           {"native_chooser_exercised", false}});
    require(d.object("library.pane")
                ->setProperty("sources",
                              QVariant::fromValue(QList<QUrl>{QUrl::fromLocalFile(value.source)})),
            "Declared file-chooser result fixture rejected its single owned source");
    d.click("library.importButton");
    d.wait([&] { return d.object("library.importDialog")->property("opened").toBool(); },
           "declared Import fixture opened");
    d.choose("library.importMode", 0);
    d.click("library.importStart");
    d.wait(
        [&] {
            return !c.library.importing() && !c.library.working() && !c.library.loading() &&
                   c.library.count() == 1;
        },
        "declared one-file Add fixture completed");
    d.wait([&] { return !d.object("library.importDialog")->property("visible").toBool(); },
           "declared Import fixture modal exit completed");
    const luma::Catalog catalog(c.catalogFile.toStdString());
    const auto ids = catalog.libraryIDs({});
    require(ids.size() == 1, "Declared Import fixture did not produce exactly one catalog UUID");
    value.id = QString::fromStdString(ids.front());
    const auto asset = catalog.assetByID(ids.front());
    require(!QUuid(value.id).isNull() && asset &&
                QString::fromStdString(asset->path.string()) == value.source &&
                asset->checksum == value.sha.toStdString() &&
                asset->edits == luma::EditDocument{} && c.library.selectedCount() == 0 &&
                !c.editor.hasPhoto() && c.library.error().isEmpty(),
            "Declared Import fixture has ambiguous UUID/source/SHA/default document/selection");
    d.wait(
        [&] {
            auto *card = d.find("library.card." + value.id);
            return card && card->property("assetID").toString() == value.id &&
                   card->property("filename").toString() == QFileInfo(value.source).fileName();
        },
        "GATE: actual Library card metadata appears without harness model.data retries");
    value.expectedPixels = fixtureReference(c, luma::EditDocument{}, value.publicSource);
    require(!value.expectedPixels.isNull(), "Keyboard activation source pixel oracle is empty");
    d.frame();
    return value;
}
void reproduce(Context &c, bool keypadOnly) {
    const KeyboardNavigation keyboardNavigation;
    const auto known = fixture(c);
    auto &d = *c.driver;
    QPointer<QQuickItem> card = d.item("library.card." + known.id);
    QPointer<QQuickItem> grid = d.item("library.grid");
    QSignalSpy opens(&c.library, &LibraryModel::openRequested);
    QSignalSpy loads(&c.editor, &EditorController::photoLoaded);
    QSignalSpy selected(d.object("library.selectAll"), SIGNAL(clicked()));
    QSignalSpy developed(d.object("main.developButton"), SIGNAL(clicked()));
    require(opens.isValid() && loads.isValid() && selected.isValid() && developed.isValid(),
            "Keyboard activation signals are not observable");
    QJsonArray trace, attempts;
    bool reachedCard = false, reachedGrid = false;
    bool forwardClosed = false, backwardClosed = false;
    QString phase = "acceptance_begin";
    const auto observe = [&](const QString &label, bool afterActualTab = false) {
        auto *focus = c.window->activeFocusItem();
        trace.append(QJsonObject{{"label", label},
                                 {"after_actual_tab", afterActualTab},
                                 {"focus", itemState(focus)},
                                 {"inside_expected_card", inside(focus, card.data())},
                                 {"grid_is_exact_focus", focus && focus == grid.data()},
                                 {"library_mode", c.window->property("libraryMode").toBool()},
                                 {"selected_count", c.library.selectedCount()},
                                 {"open_requested_count", opens.count()},
                                 {"photo_loaded_count", loads.count()}});
        reachedCard = reachedCard || (afterActualTab && inside(focus, card.data()));
        reachedGrid = reachedGrid || (afterActualTab && focus && focus == grid.data());
    };
    PointerAudit audit(c.window); // Installed only AFTER all declared setup pointer events.
    const auto evidence = [&](const QString &outcome) {
        auto *preview = d.item("develop.preview");
        writeJson(
            c.root + "/keyboard-activation.json",
            {{"candidate", keypadOnly ? keypadCandidateID : candidateID},
             {"outcome", outcome},
             {"phase", phase},
             {"official_KEY015_qualified", false},
             {"native_chooser_keyboard", false},
             {"fixture_import_uses_pointer", true},
             {"acceptance_allowed_inputs",
              keypadOnly ? "Tab/Shift+Tab/Space; modified Enter rejection; keypad Enter"
                         : "Tab/Shift+Tab/Return/Space only"},
             {"auto_repeat_probe",
              keypadOnly
                  ? "one public autoRepeat QKeyEvent at actual QQuickWindow; no OS cadence claim"
                  : "not exercised by this variant"},
             {"source", known.source},
             {"expected_uuid", known.id},
             {"expected_source_sha256", known.sha},
             {"actual_source_sha256", digest(known.source)},
             {"public_source_sha256", digest(known.publicSource)},
             {"card_reached_by_actual_tab", reachedCard},
             {"card_tab_observation", reachedCard ? "REACHED"
                                      : forwardClosed && backwardClosed
                                          ? "NOT_REACHED_IN_TWO_COMPLETE_CYCLES"
                                          : "INCOMPLETE"},
             {"grid_reached_by_actual_tab", reachedGrid},
             {"forward_cycle_closed", forwardClosed},
             {"backward_cycle_closed", backwardClosed},
             {"card", itemState(card.data())},
             {"grid", itemState(grid.data())},
             {"select_all_clicks", selected.count()},
             {"develop_clicks", developed.count()},
             {"open_requested_count", opens.count()},
             {"photo_loaded_count", loads.count()},
             {"actual_uuid", c.editor.photoID()},
             {"actual_source", c.editor.sourcePath()},
             {"has_photo", c.editor.hasPhoto()},
             {"editor_error", c.editor.error()},
             {"preview_status", preview->property("status").toInt()},
             {"preview_source", preview->property("source").toUrl().toString()},
             {"current_preview_url", c.editor.previewUrl()},
             {"provider_pixels_match_independent_default",
              c.provider->requestImage({}, nullptr, {}) == known.expectedPixels},
             {"actual_full_document", fullDocument(c.editor.currentSettings())},
             {"expected_full_document", fullDocument(luma::EditDocument{})},
             {"pointer_events_during_acceptance", audit.events},
             {"actual_window_key_presses", audit.keyEvents},
             {"keypad_primary_presses", audit.keypadPresses},
             {"keypad_repeat_presses", audit.keypadRepeats},
             {"route_attempts", attempts},
             {"actual_focus_trace", trace}});
    };
    const auto key = [&](Qt::Key value, Qt::KeyboardModifiers modifiers, const QString &label) {
        require(QGuiApplication::focusWindow() == c.window && c.window->isActive(),
                "GATE: keyboard input lost the actual case window; no focus setter fallback");
        d.key(value, modifiers);
        d.frame();
        observe(label, value == Qt::Key_Tab);
        require(audit.events.isEmpty(), "Unexpected pointer input during keyboard-only acceptance");
        require(opens.count() <= 1 && loads.count() <= 1,
                "Keyboard input duplicated selected-asset open/load");
    };
    const auto tabTo = [&](const QString &id) {
        QPointer<QQuickItem> target = d.item(id);
        for (int step = 0; target && step < maximumTabSteps; ++step) {
            if (target->hasActiveFocus() && c.window->activeFocusItem() == target.data())
                break;
            key(Qt::Key_Tab, Qt::NoModifier, "tab_to_" + id);
        }
        require(target && target->hasActiveFocus() &&
                    c.window->activeFocusItem() == target.data() && centerVisible(target.data()),
                "GATE: real Tab cannot reach required keyboard button " + id);
    };
    QSet<QString> tried;
    const auto tryCurrentAssetRoute = [&] {
        auto *focus = c.window->activeFocusItem();
        QString route;
        if (focus && focus == grid.data())
            route = "library_grid";
        else if (card && (focus == card.data() ||
                          (inside(focus, card.data()) && focus->inherits("QQuickMouseArea"))))
            route = "selected_photo_card";
        if (route.isEmpty() || tried.contains(route) || !centerVisible(focus) ||
            (keypadOnly && route != "selected_photo_card"))
            return;
        tried.insert(route);
        if (keypadOnly) {
            require(card && focus == card.data() && card->hasActiveFocus(),
                    "GATE: keypad acceptance requires the actual Tab-focused PhotoCard");
            const auto unchanged = [&] {
                require(opens.isEmpty() && loads.isEmpty() && !c.editor.hasPhoto() &&
                            c.editor.currentSettings() == luma::EditDocument{} &&
                            !c.editor.canUndo() && !c.editor.canRedo() &&
                            c.library.selectionIDs() ==
                                std::vector<std::string>{known.id.toStdString()} &&
                            card && c.window->activeFocusItem() == card.data() &&
                            card->hasActiveFocus() && audit.events.isEmpty(),
                        "Modified/repeated Enter changed focus, opened an asset or mutated state");
            };
            for (const auto modifier :
                 {Qt::ShiftModifier, Qt::ControlModifier, Qt::AltModifier, Qt::MetaModifier}) {
                key(Qt::Key_Enter, Qt::KeypadModifier | modifier, "reject_modified_keypad_enter");
                unchanged();
            }
            require(QGuiApplication::focusWindow() == c.window && c.window->isActive(),
                    "GATE: repeat probe lost the actual keyboard window");
            d.log({{"action", "single_keypad_enter_auto_repeat_input"},
                   {"delivery", "public QKeyEvent at actual QQuickWindow"},
                   {"native_os_repeat_cadence_exercised", false}});
            QKeyEvent repeat(QEvent::KeyPress, Qt::Key_Enter, Qt::KeypadModifier,
                             QString(QChar(0x0d)), true, 1);
            QCoreApplication::sendEvent(c.window, &repeat);
            QCoreApplication::processEvents();
            d.frame();
            observe("reject_auto_repeat_keypad_enter");
            unchanged();
            require(audit.keypadRepeats == 1 && audit.keypadPresses == 0,
                    "GATE: explicit repeat did not reach the actual window with its repeat flag");
            const auto before = itemState(c.window->activeFocusItem());
            key(Qt::Key_Enter, Qt::KeypadModifier, "asset_activation_keypad_enter");
            attempts.append(QJsonObject{{"route", route},
                                        {"key", static_cast<int>(Qt::Key_Enter)},
                                        {"modifiers", static_cast<int>(Qt::KeypadModifier)},
                                        {"focus_before", before},
                                        {"open_requested_count", opens.count()},
                                        {"photo_loaded_count", loads.count()}});
            require(audit.keypadPresses == 1 && opens.count() == 1,
                    "Keypad Enter did not dispatch once and request the expected asset once");
            return;
        }
        // Probe only the known asset surface, never arbitrary dialog, delete,
        // import, batch or file-picker controls encountered during traversal.
        for (const auto value : {Qt::Key_Return, Qt::Key_Space}) {
            const auto before = itemState(c.window->activeFocusItem());
            key(value, Qt::NoModifier, "asset_activation_" + route);
            attempts.append(QJsonObject{{"route", route},
                                        {"key", static_cast<int>(value)},
                                        {"focus_before", before},
                                        {"open_requested_count", opens.count()},
                                        {"photo_loaded_count", loads.count()}});
            if (!opens.isEmpty())
                return;
            require(!c.editor.hasPhoto() && c.editor.currentSettings() == luma::EditDocument{} &&
                        c.library.selectionIDs() ==
                            std::vector<std::string>{known.id.toStdString()},
                    "Unsupported asset key changed the selected identity or document");
        }
    };
    const auto traverse = [&](Qt::KeyboardModifiers modifiers, const QString &direction) {
        QPointer<QQuickItem> start = c.window->activeFocusItem();
        require(start, "GATE: keyboard traversal has no actual starting focus");
        for (int step = 0; step < maximumTabSteps; ++step) {
            key(Qt::Key_Tab, modifiers, direction + "_tab");
            auto *focus = c.window->activeFocusItem();
            require(focus && focus->isVisible() && focus->isEnabled(),
                    "GATE: Tab traversal reached absent/hidden/disabled focus");
            tryCurrentAssetRoute();
            if (!opens.isEmpty())
                return false; // Positive activation stops navigation immediately.
            if (c.window->activeFocusItem() == start.data())
                return true;
        }
        return false; // Bound reached is a gate failure, not proof of unreachability.
    };
    d.log({{"action", "keyboard_activation_acceptance_begin"},
           {"native_file_chooser_excluded", true},
           {"public_fixture_count", 1}});
    try {
        observe("acceptance_begin");
        phase = "keyboard_select_all";
        tabTo("library.selectAll");
        key(Qt::Key_Space, Qt::NoModifier, "select_all_space");
        d.wait([&] { return !c.library.selecting(); }, "keyboard Select all completed");
        require(selected.count() == 1 && c.library.selectedCount() == 1 &&
                    c.library.selectionIDs() == std::vector<std::string>{known.id.toStdString()} &&
                    !c.editor.hasPhoto() && opens.isEmpty() && loads.isEmpty(),
                "GATE: real keyboard Select all did not select only the expected imported UUID");
        phase = "forward_tab_asset_routes";
        forwardClosed = traverse(Qt::NoModifier, "forward");
        if (opens.isEmpty()) {
            require(forwardClosed,
                    "GATE: bounded forward Tab traversal did not close its actual cycle");
            phase = "backward_tab_asset_routes";
            backwardClosed = traverse(Qt::ShiftModifier, "backward");
            require(!opens.isEmpty() || backwardClosed,
                    "GATE: bounded backward Tab traversal did not close its actual cycle");
        }
        if (keypadOnly)
            require(!opens.isEmpty() && reachedCard,
                    "GATE: real Tab/card keypad activation was not reached; no Develop fallback");
        if (opens.isEmpty()) {
            phase = "selected_asset_develop_button";
            tabTo("main.developButton");
            key(Qt::Key_Space, Qt::NoModifier, "develop_button_space");
            require(developed.count() == 1 && !c.window->property("libraryMode").toBool(),
                    "GATE: real focused Develop button did not activate exactly once");
        }
        phase = "actual_selected_asset_activation";
        // Environment guard only. No more input/focus mutations while waiting.
        d.wait([&] { return !opens.isEmpty() || !loads.isEmpty() || !c.editor.error().isEmpty(); },
               "selected imported asset opens from actual tested keyboard routes", 5000);
        require(opens.count() == 1 && opens[0][0].toUrl().toLocalFile() == known.source &&
                    c.editor.error().isEmpty(),
                "Keyboard route opened the wrong source or did not emit one real openRequested");
        phase = "exact_default_document_ready_pixels";
        d.wait(
            [&] {
                auto *preview = d.item("develop.preview");
                return loads.count() == 1 && c.editor.hasPhoto() && c.editor.error().isEmpty() &&
                       !c.editor.busy() && !c.editor.saving() && c.editor.photoID() == known.id &&
                       c.editor.sourcePath() == known.source &&
                       c.editor.currentSettings() == luma::EditDocument{} && !c.editor.canUndo() &&
                       !c.editor.canRedo() && !c.editor.original() && !c.editor.presetPreview() &&
                       !c.window->property("libraryMode").toBool() && preview->isVisible() &&
                       preview->property("status").toInt() == 1 &&
                       preview->property("source").toUrl().toString() == c.editor.previewUrl() &&
                       c.provider->requestImage({}, nullptr, {}) == known.expectedPixels;
            },
            "keyboard-opened exact UUID/default document/current-source Ready pixels");
        require(loads[0][0].toString() == known.id && audit.events.isEmpty() &&
                    digest(known.source) == known.sha && digest(known.publicSource) == known.sha,
                "Keyboard activation crossed UUID/input/source integrity boundaries");
        const luma::Catalog catalog(c.catalogFile.toStdString());
        const auto active = catalog.activeAsset();
        require(catalog.libraryIDs({}) == std::vector<std::string>{known.id.toStdString()} &&
                    active && active->id == known.id.toStdString() &&
                    QString::fromStdString(active->path.string()) == known.source &&
                    active->checksum == known.sha.toStdString() &&
                    active->edits == luma::EditDocument{},
                "Keyboard activation changed membership or the durable source/full document");
        c.checkData();
        phase = "completed";
        evidence("CANDIDATE_ACTIVATION_REACHED_NOT_KEY015_QUALIFIED");
    } catch (...) {
        const auto failure = std::current_exception();
        try {
            evidence("FAILED_OR_PRECONDITION_GATE_NOT_QUALIFIED");
        } catch (const std::exception &error) {
            std::fprintf(stderr, "Keyboard activation diagnostic failed: %s\n", error.what());
        } catch (...) {
            std::fputs("Keyboard activation diagnostic failed; original failure retained.\n",
                       stderr);
        }
        std::rethrow_exception(failure);
    }
}
} // namespace
bool keyboardActivationCandidate(Context &c, const QString &id) {
    if (id != candidateID && id != keypadCandidateID)
        return false;
    reproduce(c, id == keypadCandidateID);
    return true;
}
} // namespace ux
