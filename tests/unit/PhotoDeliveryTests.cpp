#include "apps/shared/PhotoDelivery.h"
#include "apps/shared/SessionService.h"
#include "core/io/Path.h"
#include <QColorSpace>
#include <QFile>
#include <QGuiApplication>
#include <QImage>
#include <QPrinter>
#include <QPrinterInfo>
#include <QRegularExpression>
#include <QTemporaryDir>
#include <future>
#include <iostream>

namespace {
void require(bool condition, const char *message) {
    if (!condition)
        throw std::runtime_error(message);
}

std::filesystem::path path(const QString &value) {
    return luma::pathFromUtf8(value.toStdString());
}

void verifyPdf(const QString &fileName, int pageCount) {
    QFile file(fileName);
    require(file.open(QIODevice::ReadOnly), "PDF was not created");
    const auto contents = file.readAll();
    require(contents.startsWith("%PDF") && contents.contains("/ToUnicode"),
            "PDF does not include searchable captions");
    const QRegularExpression page(QStringLiteral(R"(/Type\s*/Page\b)"));
    auto matches = page.globalMatch(QString::fromLatin1(contents));
    for (int current = 0; current < pageCount; ++current) {
        require(matches.hasNext(), "PDF contains fewer pages than expected");
        matches.next();
    }
    require(!matches.hasNext(), "PDF contains more pages than expected");
}
} // namespace

int main(int argc, char **argv) {
    QGuiApplication app(argc, argv);
    try {
        using luma::DeliveryFormat;
        QTemporaryDir temporary;
        require(temporary.isValid(), "Cannot create print test directory");
        const auto catalog = path(temporary.filePath("catalog.sqlite"));
        const luma::SessionService session(catalog);
        std::vector<std::string> ids;
        for (int index = 0; index < 13; ++index) {
            const auto fileName = temporary.filePath(QStringLiteral("Фото-%1.jpg").arg(index));
            QImage image(64, 48, QImage::Format_RGB32);
            image.fill(QColor(32, index * 12, 200));
            require(image.save(fileName), "Cannot save a JPEG test photo");
            ids.push_back(session.open(path(fileName)).asset.id);
        }
        require(luma::photoDeliveryPageCount(DeliveryFormat::contactSheet, ids.size()) == 2 &&
                    luma::photoDeliveryPageCount(DeliveryFormat::photoBook, ids.size()) == 13,
                "Print dialog page counts do not match photo layouts");

        const auto title = QStringLiteral("Проверка Apertone").toStdString();
        const auto exported = temporary.filePath("export.pdf");
        luma::deliverPhotos(catalog, ids, path(exported), DeliveryFormat::contactSheet, title);
        verifyPdf(exported, 2);

        const auto profileName = temporary.filePath("proof.icc");
        QFile profile(profileName);
        const auto icc = QColorSpace(QColorSpace::AdobeRgb).iccProfile();
        require(!icc.isEmpty() && profile.open(QIODevice::WriteOnly) &&
                    profile.write(icc) == icc.size(),
                "Cannot write the proof ICC profile");
        profile.close();

        QPrinter printer(QPrinter::HighResolution);
        printer.setOutputFormat(QPrinter::PdfFormat);
        printer.setResolution(150);
        printer.setPageSize(QPageSize(QPageSize::A5));
        printer.setPageOrientation(QPageLayout::Landscape);
        const auto ranged = temporary.filePath("ranged-print.pdf");
        printer.setOutputFileName(ranged);
        QPageRanges pages;
        pages.addPage(2);
        printer.setPageRanges(pages);
        std::vector<double> progress;
        std::async(std::launch::async, [&] {
            luma::printPhotos(printer, catalog, ids, DeliveryFormat::contactSheet, title,
                              path(profileName), {},
                              [&](double value) { progress.push_back(value); });
        }).get();
        verifyPdf(ranged, 1);
        require(progress == std::vector<double>{1.0}, "Print rendered pages outside the range");

        const auto book = temporary.filePath("book-print.pdf");
        printer.setOutputFileName(book);
        printer.setPageRanges({});
        ids.resize(3);
        luma::printPhotos(printer, catalog, ids, DeliveryFormat::photoBook, title);
        verifyPdf(book, 3);

        const auto canceled = temporary.filePath("canceled-print.pdf");
        printer.setOutputFileName(canceled);
        luma::CancellationSource cancellation;
        bool stopped = false;
        try {
            luma::printPhotos(printer, catalog, ids, DeliveryFormat::photoBook, title, {},
                              cancellation.token(), [&](double) { cancellation.cancel(); });
        } catch (const std::exception &) {
            stopped = cancellation.isCancelled();
        }
        require(stopped && !QFile::exists(canceled), "Canceled print published an incomplete PDF");
#ifdef _WIN32
        if (app.arguments().contains(QStringLiteral("--windows-virtual"))) {
            const auto name = QStringLiteral("Microsoft Print to PDF");
            const auto info = QPrinterInfo::printerInfo(name);
            require(!info.isNull(), "Microsoft Print to PDF is unavailable");
            QPrinter native(info, QPrinter::HighResolution);
            require(native.printerName() == name && native.outputFormat() == QPrinter::NativeFormat,
                    "The specified virtual printer was not selected");
            const auto spooled = temporary.filePath("native-print.prn");
            // A .pdf suffix would cause Qt to bypass the Windows print driver.
            native.setOutputFileName(spooled);
            std::async(std::launch::async, [&] {
                luma::printPhotos(native, catalog, ids, DeliveryFormat::contactSheet, title);
            }).get();
            QFile output(spooled);
            require(output.open(QIODevice::ReadOnly) && output.read(4) == "%PDF",
                    "Microsoft Print to PDF did not produce a PDF");
        }
#endif
        return 0;
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
