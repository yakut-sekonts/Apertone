#pragma once
#include "core/edits/EditDocument.h"
#include <QByteArray>
#include <QJsonObject>
#include <QString>
#include <deque>
#include <vector>

namespace ux {
class Context;
// Independent test oracle: a bounded timeline/cursor, not production EditHistory.
class HistoryChaosModel {
  public:
    const luma::EditDocument &current() const {
        return timeline_[cursor_];
    }
    int undoDepth() const {
        return static_cast<int>(cursor_);
    }
    int redoDepth() const {
        return static_cast<int>(timeline_.size() - cursor_ - 1);
    }
    void apply(const QString &op, const QString &control = {}, int delta = 0);

  private:
    std::deque<luma::EditDocument> timeline_{luma::EditDocument{}};
    std::size_t cursor_ = 0;
};
QString historyDocumentHash(const luma::EditDocument &document);
struct HistoryChaosPlan {
    quint64 seed = 0;
    int actions = 0;
    QJsonObject header;
    std::vector<QJsonObject> rows;
    QByteArray bytes;
    QString scheduleSHA256;
};
HistoryChaosPlan historyChaosPlan(quint64 seed, int actions, const QString &fixtures);
// Validate/copy the complete replay and fsync schedule+seed BEFORE Context/UI setup.
HistoryChaosPlan prepareHistoryChaosPlan(const QString &root, const QString &fixtures, quint64 seed,
                                         int actions, const QString &replay);
void runHistoryChaos(Context &context, const HistoryChaosPlan &plan);
} // namespace ux
