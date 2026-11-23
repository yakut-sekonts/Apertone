#include "core/catalog/Catalog.h"
#include "core/io/Path.h"
#include "platform/macos/FileHasher.h"
#include "platform/macos/SemanticEmbedding.h"
#include <QCoreApplication>
#include <QImage>
#include <QTemporaryDir>
#include <algorithm>
#include <iostream>

namespace {
void require(bool okay, const char *message) {
    if (!okay)
        throw std::runtime_error(message);
}
} // namespace

int main(int argc, char **argv) {
    const QCoreApplication app(argc, argv);
    try {
        require(app.arguments().size() == 2, "Expected the photo fixture directory.");
        const auto fixtures = luma::pathFromUtf8(app.arguments().at(1).toStdString());
        const auto provider = luma::macos::makeSemanticEmbedding();
        require(provider && provider->location() == luma::ai::ExecutionLocation::local,
                "Bundled category models are missing.");

        const QTemporaryDir workspace;
        require(workspace.isValid(), "Cannot create a category test workspace.");
        luma::Catalog catalog(
            luma::pathFromUtf8(workspace.filePath("catalog.sqlite").toStdString()));
        const auto index = [&](const char *name) {
            const auto path = fixtures / name;
            const auto image = QImage(QString::fromStdWString(path.native()))
                                   .convertToFormat(QImage::Format_RGBA8888);
            require(!image.isNull(), "Cannot load the category photo fixture.");
            luma::PhotoMetadata metadata;
            metadata.width = image.width();
            metadata.height = image.height();
            const auto checksum = luma::macos::sha256File(path);
            const auto asset = catalog.addAndSelect(path, checksum, metadata);
            const auto vector = provider->imageEmbedding(
                {image.width(),
                 image.height(),
                 {image.constBits(), static_cast<std::size_t>(image.sizeInBytes())}},
                {});
            std::cout << name << ": people=" << vector[0] << ", sky=" << vector[1] << '\n';
            catalog.saveSemanticEmbedding(asset.id, checksum, provider->identifier(), vector);
            return asset.id;
        };
        const auto portrait = index("portrait.png");
        index("landscape.png");
        index("night.png");

        const auto search = [&](const char *category) {
            return catalog.semanticSearch(provider->identifier(),
                                          provider->textEmbedding(category, {}), 100);
        };
        const auto people = search("people");
        require(people.size() == 1 && people.front().id == portrait,
                "People search did not isolate the portrait.");
        const auto sky = search("sky");
        require(sky.size() == 2 &&
                    std::ranges::none_of(sky, [&](const auto &hit) { return hit.id == portrait; }),
                "Sky search did not isolate both outdoor photos.");

        catalog.clearSemanticIndex();
        require(search("people").empty() && search("sky").empty(),
                "Category results remained after clearing the index.");
        return 0;
    } catch (const std::exception &failure) {
        std::cerr << failure.what() << '\n';
        return 1;
    }
}
