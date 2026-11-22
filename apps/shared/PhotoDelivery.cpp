#include "apps/shared/PhotoDelivery.h"
#include "apps/shared/SessionService.h"
#include "core/image/Bayer.h"
#include "core/io/FileSystem.h"
#include "core/io/Path.h"
#include "core/io/StagingFile.h"
#include "core/render/CpuRenderer.h"
#include <QColorSpace>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFontDatabase>
#include <QGuiApplication>
#include <QPainter>
#include <QPdfWriter>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <algorithm>

namespace luma {
namespace {
QString asQString(const std::filesystem::path &path) {
    return QString::fromStdString(pathUtf8(path));
}
QString photoName(const AssetRecord &asset) {
    auto name = asQString(asset.path.filename());
    if (!asset.copyName.empty())
        name += QStringLiteral(" · ") + QString::fromStdString(asset.copyName);
    return name;
}
QColorSpace readProof(const std::filesystem::path &path) {
    if (path.empty())
        return {};
    QFile file(asQString(path));
    if (!file.open(QIODevice::ReadOnly) || file.size() < 128 || file.size() > 16 * 1024 * 1024)
        throw std::runtime_error("Cannot read the proof ICC profile.");
    const auto profile = QColorSpace::fromIccProfile(file.readAll());
    if (!profile.isValid() || profile.colorModel() != QColorSpace::ColorModel::Rgb)
        throw std::runtime_error("Choose a valid RGB ICC profile for this proof.");
    return profile;
}

QImage proofPreview(const SessionService &session, const AssetRecord &asset, int edge,
                    const QColorSpace &profile, CancellationToken stop) {
    const auto decoded = session.fullResolution(asset, stop);
    const auto input = standardPreview(decoded->image, edge, stop);
    const double scale = static_cast<double>(input.width) / decoded->image.width;
    auto output = renderCPU(input, asset.edits, asset.metadata.orientation, {}, stop,
                            decoded->calibration, DetailDiagnostic::none, scale, asset.metadata, {},
                            nullptr, {}, 4, DisplayMapping::adaptiveHDR);
    QImage srgb(output.rgba.data(), output.width, output.height, output.width * 4,
                QImage::Format_RGBA8888);
    srgb.setColorSpace(QColorSpace::SRgb);
    if (!profile.isValid())
        return srgb.copy();
    auto proofed = srgb.convertedToColorSpace(profile, QImage::Format_RGBA8888);
    if (proofed.isNull())
        throw std::runtime_error("Cannot preview this proof profile.");
    proofed.convertToColorSpace(QColorSpace::SRgb, QImage::Format_RGBA8888);
    return proofed;
}
QFont pdfFont(QFont font) {
#ifdef _WIN32
    if (QGuiApplication::platformName() == QStringLiteral("offscreen")) {
        // Qt's Windows offscreen plugin uses FreeType. Unlike the native
        // integration, its font database doesn't discover installed fonts.
        static const auto family = [] {
            const QDir dir(QStandardPaths::writableLocation(QStandardPaths::FontsLocation));
            for (const auto name : {"segoeui.ttf", "arial.ttf"}) {
                const auto path = dir.absoluteFilePath(QString::fromLatin1(name));
                if (!QFileInfo(path).isFile())
                    continue;
                const auto id = QFontDatabase::addApplicationFont(path);
                if (id >= 0) {
                    const auto families = QFontDatabase::applicationFontFamilies(id);
                    if (!families.isEmpty())
                        return families.front();
                }
            }
            throw std::runtime_error("Cannot load a Windows font for PDF captions.");
        }();
        font.setFamily(family);
    }
#endif
    return font;
}
void writePdf(const SessionService &session, const std::vector<AssetRecord> &assets,
              const std::filesystem::path &catalog, const std::filesystem::path &destination,
              const QString &title, bool book, const QColorSpace &proof, CancellationToken stop,
              const std::function<void(double)> &progress) {
    StagingFile temporary(destination.parent_path(), ".luma-export-");
    QFile file(asQString(temporary.path()));
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        throw std::runtime_error("Cannot create the PDF file.");
    {
        QPdfWriter pdf(&file);
        pdf.setPageSize(QPageSize(QPageSize::A4));
        pdf.setPageMargins({10, 10, 10, 10}, QPageLayout::Millimeter);
        pdf.setResolution(300);
        pdf.setTitle(title);
        QPainter painter(&pdf);
        if (!painter.isActive())
            throw std::runtime_error("Cannot start the PDF layout.");
        const int columns = book ? 1 : 3, rows = book ? 1 : 4;
        const int perPage = columns * rows;
        const int gap = 36, header = 86, footer = 60;
        const int cellWidth = (pdf.width() - (columns - 1) * gap) / columns;
        const int cellHeight = (pdf.height() - header - footer - (rows - 1) * gap) / rows;
        painter.setRenderHint(QPainter::SmoothPixmapTransform);
        auto font = pdfFont(painter.font());
        for (std::size_t i = 0; i < assets.size(); ++i) {
            checkCancellation(stop);
            const int slot = static_cast<int>(i % perPage);
            if (slot == 0) {
                if (i != 0 && !pdf.newPage())
                    throw std::runtime_error("Cannot add a PDF page.");
                font.setPointSize(15);
                painter.setFont(font);
                painter.setPen(Qt::black);
                painter.drawText(
                    QRect(0, 0, pdf.width(), header), Qt::AlignTop | Qt::AlignLeft,
                    painter.fontMetrics().elidedText(title, Qt::ElideRight, pdf.width()));
                font.setPointSize(8);
                painter.setFont(font);
                painter.drawText(QRect(0, pdf.height() - footer, pdf.width(), footer),
                                 Qt::AlignBottom | Qt::AlignRight,
                                 QString::number(i / perPage + 1));
            }
            font.setPointSize(9);
            painter.setFont(font);
            const QRect cell((slot % columns) * (cellWidth + gap),
                             header + (slot / columns) * (cellHeight + gap), cellWidth, cellHeight);
            const int caption = painter.fontMetrics().height() + 15;
            const QRect space = cell.adjusted(0, 0, 0, -caption);
            const auto image = proofPreview(session, assets[i], book ? 3200 : 960, proof, stop);
            const auto fitted = image.size().scaled(space.size(), Qt::KeepAspectRatio);
            QRect rectangle(QPoint{}, fitted);
            rectangle.moveCenter(space.center());
            painter.drawImage(rectangle, image);
            painter.drawText(
                cell.adjusted(0, cellHeight - caption, 0, 0), Qt::AlignCenter,
                painter.fontMetrics().elidedText(photoName(assets[i]), Qt::ElideMiddle, cellWidth));
            if (progress)
                progress(static_cast<double>(i + 1) / assets.size());
        }
        if (!painter.end())
            throw std::runtime_error("Cannot finish the PDF layout.");
    }
    if (!file.flush())
        throw std::runtime_error("Cannot write the PDF file.");
    file.close();
    if (files::syncFile(temporary.path()) != 0)
        throw std::runtime_error("Cannot sync the PDF file.");
    Catalog(catalog, CatalogOpenMode::existingOnly).installExport(destination, stop, [&] {
        checkCancellation(stop);
        if (files::publish(temporary.path(), destination) != 0)
            throw std::runtime_error("Cannot install the PDF at the selected path.");
    });
}

void writeGallery(const SessionService &session, const std::vector<AssetRecord> &assets,
                  const std::filesystem::path &destination, const QString &title, bool slideshow,
                  CancellationToken stop, const std::function<void(double)> &progress) {
    QTemporaryDir staging(asQString(destination.parent_path() / ".apertone-gallery-XXXXXX"));
    if (!staging.isValid())
        throw std::runtime_error("Cannot create a temporary delivery folder.");
    const auto temporary = pathFromUtf8(staging.path().toStdString());
    const auto escaped = title.toHtmlEscaped();
    QString html = QStringLiteral(R"html(<!doctype html>
<html lang="en"><meta charset="utf-8"><meta name="viewport" content="width=device-width, initial-scale=1">
<title>%1</title><style>
body {font:16px system-ui,sans-serif;background:#202124;color:#f1f2f3;margin:2rem}
main {display:grid;grid-template-columns:repeat(auto-fit,minmax(min(100%,17rem),1fr));gap:1.5rem}
figure {margin:0} img {width:100%;display:block} figcaption {padding:.5rem 0;overflow-wrap:anywhere}
a {color:inherit} body.slideshow main {display:block} body.slideshow figure:not(.current) {display:none}
body.slideshow img {width:100%;height:74vh;object-fit:contain} button {padding:.5rem .9rem;margin:0 .5rem .7rem 0}
</style><body class="%2"><h1>%1</h1><nav hidden aria-label="Slideshow controls">
<button id="previous">Previous</button><button id="play">Play</button><button id="next">Next</button>
<span id="position" aria-live="polite"></span></nav><main>
)html")
                       .arg(escaped, slideshow ? QStringLiteral("slideshow") : QString{});
    for (std::size_t i = 0; i < assets.size(); ++i) {
        checkCancellation(stop);
        ExportOptions options;
        options.resize = ResizeMode::longEdge;
        options.resizeValue = 2400;
        const auto name = QStringLiteral("photo-%1.jpg").arg(i + 1, 4, 10, QLatin1Char('0'));
        auto target = temporary / pathFromUtf8(name.toStdString());
        const auto &asset = assets[i];
        session.exportPhoto(asset, asset.edits, target, options, stop);
        html += QStringLiteral("<figure%1><a href=\"%2\"><img alt=\"%3\" "
                               "%4src=\"%2\"></a><figcaption>%3</figcaption></figure>\n")
                    .arg(i == 0 ? QStringLiteral(" class=\"current\"") : QString{}, name,
                         photoName(asset).toHtmlEscaped(),
                         i == 0 ? QString{} : QStringLiteral("loading=\"lazy\" "));
        if (progress)
            progress(static_cast<double>(i + 1) / assets.size());
    }
    html += QStringLiteral(R"html(</main><script>
if (document.body.classList.contains('slideshow')) {
  const slides = [...document.querySelectorAll('figure')];
  const play = document.getElementById('play');
  let current = 0, timer;
  document.querySelector('nav').hidden = false;
  function show(index) {
    slides[current].classList.remove('current');
    current = (index + slides.length) % slides.length;
    slides[current].classList.add('current');
    document.getElementById('position').textContent = `${current + 1} / ${slides.length}`;
  }
  document.getElementById('previous').onclick = () => show(current - 1);
  document.getElementById('next').onclick = () => show(current + 1);
  play.onclick = () => {
    if (timer) { clearInterval(timer); timer = undefined; play.textContent = 'Play'; }
    else { timer = setInterval(() => show(current + 1), 3000); play.textContent = 'Pause'; }
  };
  window.addEventListener('keydown', event => {
    if (event.key === 'ArrowLeft') show(current - 1);
    else if (event.key === 'ArrowRight') show(current + 1);
  });
  show(0);
}
</script></body></html>
)html");
    const auto utf8 = html.toUtf8();
    QFile index(asQString(temporary / "index.html"));
    if (!index.open(QIODevice::WriteOnly) || index.write(utf8) != utf8.size() || !index.flush())
        throw std::runtime_error("Cannot write the gallery index.");
    index.close();
    checkCancellation(stop);
    if (files::move(temporary, destination, false) != 0)
        throw std::runtime_error("Cannot publish the gallery folder. Choose a new folder name.");
    staging.setAutoRemove(false);
}
} // namespace

void deliverPhotos(const std::filesystem::path &catalog, const std::vector<std::string> &ids,
                   const std::filesystem::path &destination, DeliveryFormat format,
                   const std::string &title, const std::filesystem::path &proofProfile,
                   CancellationToken stop, const std::function<void(double)> &progress) {
    if (ids.empty() || ids.size() > 1000 || title.empty() || title.size() > 1024 ||
        format < DeliveryFormat::contactSheet || format > DeliveryFormat::slideshow)
        throw std::runtime_error("Choose a delivery title and 1 to 1,000 photos.");
    const auto target = std::filesystem::absolute(destination).lexically_normal();
    if (std::filesystem::exists(target) || !std::filesystem::is_directory(target.parent_path()))
        throw std::runtime_error("Choose a new output path in an existing folder.");
    const Catalog db(catalog, CatalogOpenMode::existingOnly);
    const SessionService session(catalog);
    std::vector<AssetRecord> assets;
    assets.reserve(ids.size());
    for (const auto &id : ids) {
        checkCancellation(stop);
        auto photo = db.assetByID(id);
        if (!photo)
            throw std::runtime_error("A selected photo is no longer in the catalog.");
        assets.push_back(std::move(*photo));
    }
    std::ranges::stable_sort(assets, [](const auto &left, const auto &right) {
        return photoName(left).compare(photoName(right), Qt::CaseInsensitive) < 0;
    });
    const auto displayTitle = QString::fromStdString(title);
    if (format == DeliveryFormat::contactSheet || format == DeliveryFormat::photoBook) {
        if (asQString(target.extension()).compare(QStringLiteral(".pdf"), Qt::CaseInsensitive) != 0)
            throw std::runtime_error("Choose a PDF filename.");
        writePdf(session, assets, catalog, target, displayTitle,
                 format == DeliveryFormat::photoBook, readProof(proofProfile), stop, progress);
    } else {
        if (!proofProfile.empty())
            throw std::runtime_error("ICC proofing is available for PDF output.");
        writeGallery(session, assets, target, displayTitle, format == DeliveryFormat::slideshow,
                     stop, progress);
    }
}
} // namespace luma
