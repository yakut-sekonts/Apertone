#pragma once
#include "core/edits/EditDocument.h"
#include "core/render/Sharpening.h"
#include <QImage>
#include <QString>
class QQuickItem;
namespace luma {
struct AssetSnapshot;
}
namespace ux {
class Context;
// Independent document-to-pixels oracle for the small generated public fixtures.
// This checks UI/state coherence; renderer numerical correctness has separate tests.
QImage fixtureReference(Context &context,
                        luma::DetailDiagnostic diagnostic = luma::DetailDiagnostic::none);
QImage fixtureReference(Context &context, const luma::EditDocument &document,
                        const QString &sourcePath,
                        luma::DetailDiagnostic diagnostic = luma::DetailDiagnostic::none);
// Exact saved-document JPEG cache plus actual Image-node scene readback.
// The URL generation is an invalidation counter, not the asset edit revision.
void thumbnailCardPixels(Context &context, QQuickItem *card, const luma::AssetSnapshot &saved);
} // namespace ux
