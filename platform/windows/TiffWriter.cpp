#include "platform/windows/TiffWriter.h"
#include "core/image/Bayer.h"
#include <QColorSpace>
#include <QImage>
#include <memory>
#include <windows.h>
#include <shlwapi.h>
#include <initguid.h>
#include <wincodec.h>

namespace luma::windows {
namespace {
void require(HRESULT status, const char *operation) {
    if (FAILED(status))
        throw std::runtime_error(std::string(operation) + " (" +
                                 std::to_string(static_cast<std::uint32_t>(status)) + ").");
}

class ComSession {
  public:
    ComSession() : status_(CoInitializeEx(nullptr, COINIT_MULTITHREADED)) {
        if (status_ != RPC_E_CHANGED_MODE)
            require(status_, "Cannot initialize Windows image codecs");
    }
    ~ComSession() {
        if (SUCCEEDED(status_))
            CoUninitialize();
    }
  private:
    HRESULT status_;
};

struct Release {
    void operator()(IUnknown *object) const { object->Release(); }
};
template <class T> using ComPtr = std::unique_ptr<T, Release>;

template <class T, class Create> ComPtr<T> create(Create constructor, const char *operation) {
    T *object = nullptr;
    const auto status = constructor(&object);
    ComPtr<T> result(object);
    require(status, operation);
    return result;
}
} // namespace

void writeZipTiff(const QImage &image, const std::filesystem::path &path, CancellationToken stop) {
    checkCancellation(stop);
    if (image.format() != QImage::Format_RGBA64 && image.format() != QImage::Format_ARGB32)
        throw std::runtime_error("Unsupported input for Windows TIFF encoder.");
    ComSession com;
    const auto factory = create<IWICImagingFactory>([](auto **out) {
        return CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
                                IID_IWICImagingFactory, reinterpret_cast<void **>(out));
    }, "Cannot create Windows image codec factory");
    const auto stream = create<IStream>([&](auto **out) {
        return SHCreateStreamOnFileEx(path.c_str(), STGM_READWRITE | STGM_SHARE_DENY_NONE,
                                      FILE_ATTRIBUTE_NORMAL, FALSE, nullptr, out);
    }, "Cannot create TIFF stream");
    const auto encoder = create<IWICBitmapEncoder>([&](auto **out) {
        return factory->CreateEncoder(GUID_ContainerFormatTiff, nullptr, out);
    }, "Cannot create TIFF encoder");
    require(encoder->Initialize(stream.get(), WICBitmapEncoderNoCache), "Cannot start TIFF encode");
    IWICBitmapFrameEncode *framePointer = nullptr;
    IPropertyBag2 *propertyPointer = nullptr;
    const auto frameStatus = encoder->CreateNewFrame(&framePointer, &propertyPointer);
    ComPtr<IWICBitmapFrameEncode> frame(framePointer);
    ComPtr<IPropertyBag2> properties(propertyPointer);
    require(frameStatus, "Cannot create TIFF frame");
    if (!properties)
        throw std::runtime_error("Windows TIFF encoder does not expose compression options.");
    wchar_t compression[] = L"TiffCompressionMethod";
    PROPBAG2 property{};
    property.pstrName = compression;
    VARIANT value{};
    value.vt = VT_UI1;
    value.bVal = WICTiffCompressionZIP;
    require(properties->Write(1, &property, &value), "Cannot set TIFF ZIP compression");
    require(frame->Initialize(properties.get()), "Cannot initialize TIFF frame");
    require(frame->SetSize(image.width(), image.height()), "Cannot set TIFF dimensions");
    const auto expected = image.format() == QImage::Format_RGBA64 ? GUID_WICPixelFormat64bppRGBA
                                                                  : GUID_WICPixelFormat32bppBGRA;
    auto format = expected;
    require(frame->SetPixelFormat(&format), "Cannot set TIFF pixel format");
    if (!IsEqualGUID(format, expected))
        throw std::runtime_error("Windows TIFF encoder changed the requested depth or alpha.");
    auto profile = image.colorSpace().iccProfile();
    auto color = create<IWICColorContext>([&](auto **out) {
        return factory->CreateColorContext(out);
    }, "Cannot create TIFF color context");
    require(color->InitializeFromMemory(reinterpret_cast<BYTE *>(profile.data()),
                                        static_cast<UINT>(profile.size())), "Cannot prepare TIFF ICC");
    auto *context = color.get();
    require(frame->SetColorContexts(1, &context), "Cannot attach TIFF ICC");
    for (int y = 0; y < image.height(); ++y) {
        checkCancellation(stop);
        const auto stride = static_cast<UINT>(image.bytesPerLine());
        require(frame->WritePixels(1, stride, stride, const_cast<BYTE *>(image.constScanLine(y))),
                "Cannot write TIFF row");
    }
    require(frame->Commit(), "Cannot finish TIFF frame");
    require(encoder->Commit(), "Cannot finish TIFF container");
    require(stream->Commit(STGC_DEFAULT), "Cannot write the ZIP-compressed TIFF");
}
} // namespace luma::windows
