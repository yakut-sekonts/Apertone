#pragma once
#include "core/render/Sharpening.h"
#include <QImage>
namespace ux {
class Context;
// Independent document-to-pixels oracle for the small generated public fixtures.
// This checks UI/state coherence; renderer numerical correctness has separate tests.
QImage fixtureReference(Context &context,
                        luma::DetailDiagnostic diagnostic = luma::DetailDiagnostic::none);
} // namespace ux
