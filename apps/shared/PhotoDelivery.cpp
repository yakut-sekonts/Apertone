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
#include <QFontDatabase>
#include <QGuiApplication>
#include <QPageRanges>
#include <QPainter>
#include <QPdfWriter>
#include <QPrinter>
#include <QScopeGuard>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <algorithm>
#include <cmath>

namespace luma {
namespace {
constexpr int contactColumns = 3;
constexpr int contactRows = 4;
constexpr int layoutDpi = 300;

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
QFont captionFont(QFont font) {
#ifdef _WIN32
    if (QGuiApplication::platformName() == QStringLiteral("offscreen")) {
        // The Windows offscreen plugin uses FreeType with Qt's fonts directory.
        // Register the installed fonts so mixed-script captions can use fallback.
        static const auto family = [] {
            const auto location = QStandardPaths::writableLocation(QStandardPaths::FontsLocation);
            if (!location.isEmpty()) {
                for (const auto &file : QDir(location).entryInfoList({"*.ttf", "*.ttc", "*.otf"},
                                                                     QDir::Files | QDir::Readable))
                    QFontDatabase::addApplicationFont(file.absoluteFilePath());
            }
            const auto families = QFontDatabase::families();
            for (const auto name : {"Segoe UI", "Arial"})
                if (families.contains(QLatin1String(name), Qt::CaseInsensitive))
                    return QString::fromLatin1(name);
            if (families.isEmpty())
                throw std::runtime_error("Cannot load a Windows font for PDF captions.");
            return families.front();
        }();
        font.setFamily(family);
    }
#endif
    return font;
}
void paintPhotoPages(QPagedPaintDevice &device, QPrinter *printer, const SessionService &session,
                     const std::vector<AssetRecord> &assets, const QString &title,
                     DeliveryFormat format, const QColorSpace &proof, CancellationToken stop,
                     const std::function<void(double)> &progress) {
    const bool book = format == DeliveryFormat::photoBook;
    const int columns = book ? 1 : contactColumns, rows = book ? 1 : contactRows;
    const int perPage = columns * rows;
    const auto range = printer ? printer->pageRanges() : QPageRanges{};
    std::vector<int> pages;
    std::size_t photos = 0;
    const int pageCount = photoDeliveryPageCount(format, assets.size());
    for (int page = 1; page <= pageCount; ++page) {
        if (range.isEmpty() || range.contains(page)) {
            pages.push_back(page);
            photos += std::min<std::size_t>(perPage, assets.size() - (page - 1) * perPage);
        }
    }
    if (pages.empty())
        throw std::runtime_error("Choose a page range within the document.");
    if (printer && printer->pageOrder() == QPrinter::LastPageFirst)
        std::ranges::reverse(pages);
    checkCancellation(stop);
    QPainter painter(&device);
    if (!painter.isActive())
        throw std::runtime_error(printer ? "Cannot start printing."
                                         : "Cannot start the PDF layout.");
    try {
        const auto scaleX = [&](int pixels) {
            return static_cast<int>(std::lround(pixels * device.logicalDpiX() / double(layoutDpi)));
        };
        const auto scaleY = [&](int pixels) {
            return static_cast<int>(std::lround(pixels * device.logicalDpiY() / double(layoutDpi)));
        };
        const int width = device.width(), height = device.height();
        const int gapX = scaleX(36), gapY = scaleY(36), header = scaleY(86), footer = scaleY(60);
        const int cellWidth = (width - (columns - 1) * gapX) / columns;
        const int cellHeight = (height - header - footer - (rows - 1) * gapY) / rows;
        painter.setRenderHint(QPainter::SmoothPixmapTransform);
        painter.setPen(Qt::black);
        auto font = captionFont(painter.font());
        std::size_t current = 0;
        for (int page : pages) {
            checkCancellation(stop);
            if (current != 0 && !device.newPage())
                throw std::runtime_error(printer ? "Cannot add a printed page."
                                                 : "Cannot add a PDF page.");
            font.setPointSize(15);
            painter.setFont(font);
            painter.drawText(QRect(0, 0, width, header), Qt::AlignTop | Qt::AlignLeft,
                             painter.fontMetrics().elidedText(title, Qt::ElideRight, width));
            font.setPointSize(8);
            painter.setFont(font);
            painter.drawText(QRect(0, height - footer, width, footer),
                             Qt::AlignBottom | Qt::AlignRight, QString::number(page));
            font.setPointSize(9);
            painter.setFont(font);
            const auto metrics = painter.fontMetrics();
            const int caption = metrics.height() + scaleY(15);
            if (cellWidth <= 0 || cellHeight <= caption)
                throw std::runtime_error("The paper is too small for this photo layout.");
            const auto first = static_cast<std::size_t>((page - 1) * perPage);
            const auto end = std::min(first + perPage, assets.size());
            for (auto index = first; index < end; ++index) {
                checkCancellation(stop);
                const int slot = static_cast<int>(index - first);
                const QRect cell((slot % columns) * (cellWidth + gapX),
                                 header + (slot / columns) * (cellHeight + gapY), cellWidth,
                                 cellHeight);
                const QRect space = cell.adjusted(0, 0, 0, -caption);
                const auto &asset = assets[index];
                const auto image = proofPreview(session, asset, book ? 3200 : 960, proof, stop);
                QRect rectangle(QPoint{}, image.size().scaled(space.size(), Qt::KeepAspectRatio));
                rectangle.moveCenter(space.center());
                painter.drawImage(rectangle, image);
                painter.drawText(cell.adjusted(0, cellHeight - caption, 0, 0), Qt::AlignCenter,
                                 metrics.elidedText(photoName(asset), Qt::ElideMiddle, cellWidth));
                ++current;
                if (progress)
                    progress(static_cast<double>(current) / photos);
            }
        }
        checkCancellation(stop);
        if (!painter.end() || (printer && printer->printerState() == QPrinter::Error))
            throw std::runtime_error(printer ? "Cannot finish printing."
                                             : "Cannot finish the PDF layout.");
    } catch (...) {
        if (printer && painter.isActive())
            printer->abort();
        throw;
    }
}

void installPdf(const std::filesystem::path &catalog, const std::filesystem::path &staged,
                const std::filesystem::path &destination, CancellationToken stop) {
    if (files::syncFile(staged) != 0)
        throw std::runtime_error("Cannot sync the PDF file.");
    Catalog(catalog, CatalogOpenMode::existingOnly).installExport(destination, stop, [&] {
        checkCancellation(stop);
        if (files::move(staged, destination, false) != 0)
            throw std::runtime_error("Cannot install the PDF at the selected path.");
    });
}

void writePdf(const SessionService &session, const std::vector<AssetRecord> &assets,
              const std::filesystem::path &catalog, const std::filesystem::path &destination,
              const QString &title, DeliveryFormat format, const QColorSpace &proof,
              CancellationToken stop, const std::function<void(double)> &progress) {
    StagingFile temporary(destination.parent_path(), ".luma-export-");
    QFile file(asQString(temporary.path()));
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        throw std::runtime_error("Cannot create the PDF file.");
    {
        QPdfWriter pdf(&file);
        pdf.setPageSize(QPageSize(QPageSize::A4));
        pdf.setPageMargins({10, 10, 10, 10}, QPageLayout::Millimeter);
        pdf.setResolution(layoutDpi);
        pdf.setTitle(title);
        paintPhotoPages(pdf, nullptr, session, assets, title, format, proof, stop, progress);
    }
    if (!file.flush())
        throw std::runtime_error("Cannot write the PDF file.");
    file.close();
    installPdf(catalog, temporary.path(), destination, stop);
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
void validateDelivery(const std::vector<std::string> &ids, DeliveryFormat format,
                      const std::string &title) {
    if (ids.empty() || ids.size() > 1000 || title.empty() || title.size() > 1024 ||
        format < DeliveryFormat::contactSheet || format > DeliveryFormat::slideshow)
        throw std::runtime_error("Choose a delivery title and 1 to 1,000 photos.");
}

std::filesystem::path newDestination(const std::filesystem::path &destination) {
    const auto target = std::filesystem::absolute(destination).lexically_normal();
    if (std::filesystem::exists(target) || !std::filesystem::is_directory(target.parent_path()))
        throw std::runtime_error("Choose a new output path in an existing folder.");
    return target;
}

std::vector<AssetRecord> deliveryPhotos(const std::filesystem::path &catalog,
                                        const std::vector<std::string> &ids,
                                        CancellationToken stop) {
    const Catalog db(catalog, CatalogOpenMode::existingOnly);
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
    return assets;
}
} // namespace

int photoDeliveryPageCount(DeliveryFormat format, std::size_t photoCount) {
    if (photoCount == 0 || photoCount > 1000)
        return 0;
    if (format == DeliveryFormat::photoBook)
        return static_cast<int>(photoCount);
    if (format == DeliveryFormat::contactSheet) {
        const int perPage = contactColumns * contactRows;
        return static_cast<int>((photoCount + perPage - 1) / perPage);
    }
    return 0;
}

void printPhotos(QPrinter &printer, const std::filesystem::path &catalog,
                 const std::vector<std::string> &ids, DeliveryFormat format,
                 const std::string &title, const std::filesystem::path &proofProfile,
                 CancellationToken stop, const std::function<void(double)> &progress) {
    validateDelivery(ids, format, title);
    if (!photoDeliveryPageCount(format, ids.size()))
        throw std::runtime_error("Choose a contact sheet or photo book to print.");
    if (!printer.isValid())
        throw std::runtime_error("No printer is available.");
    const auto assets = deliveryPhotos(catalog, ids, stop);
    const SessionService session(catalog);
    const auto displayTitle = QString::fromStdString(title);
    const auto proof = readProof(proofProfile);
    if (printer.outputFormat() == QPrinter::PdfFormat) {
        const auto output = printer.outputFileName();
        if (output.isEmpty())
            throw std::runtime_error("Choose a PDF filename.");
        const auto target = newDestination(pathFromUtf8(output.toStdString()));
        StagingFile temporary(target.parent_path(), ".luma-export-");
        printer.setOutputFileName(asQString(temporary.path()));
        const auto resetOutput = qScopeGuard([&] { printer.setOutputFileName(output); });
        paintPhotoPages(printer, &printer, session, assets, displayTitle, format, proof, stop,
                        progress);
        installPdf(catalog, temporary.path(), target, stop);
    } else {
        paintPhotoPages(printer, &printer, session, assets, displayTitle, format, proof, stop,
                        progress);
    }
}

void deliverPhotos(const std::filesystem::path &catalog, const std::vector<std::string> &ids,
                   const std::filesystem::path &destination, DeliveryFormat format,
                   const std::string &title, const std::filesystem::path &proofProfile,
                   CancellationToken stop, const std::function<void(double)> &progress) {
    validateDelivery(ids, format, title);
    const auto target = newDestination(destination);
    const auto assets = deliveryPhotos(catalog, ids, stop);
    const SessionService session(catalog);
    const auto displayTitle = QString::fromStdString(title);
    if (format == DeliveryFormat::contactSheet || format == DeliveryFormat::photoBook) {
        if (asQString(target.extension()).compare(QStringLiteral(".pdf"), Qt::CaseInsensitive) != 0)
            throw std::runtime_error("Choose a PDF filename.");
        writePdf(session, assets, catalog, target, displayTitle, format, readProof(proofProfile),
                 stop, progress);
    } else {
        if (!proofProfile.empty())
            throw std::runtime_error("ICC proofing is available for PDF output.");
        writeGallery(session, assets, target, displayTitle, format == DeliveryFormat::slideshow,
                     stop, progress);
    }
}
} // namespace luma
