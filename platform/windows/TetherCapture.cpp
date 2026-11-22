#include "platform/windows/TetherCapture.h"
#include "apps/shared/PhotoDecoder.h"
#include "core/image/Bayer.h"
#include "core/io/Path.h"
#include <QSaveFile>
#include <QUuid>
#include <algorithm>
#include <array>
#include <chrono>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <set>
#include <utility>
// clang-format off
#include <windows.h>
#include <wtypes.h>
#include <initguid.h>
#include <portabledevice.h>
#include <portabledeviceapi.h>
// clang-format on

namespace luma::windows {
namespace {
template <typename Interface> struct ReleaseCOM {
    void operator()(Interface *pointer) const {
        pointer->Release();
    }
};
template <typename Interface> using COM = std::unique_ptr<Interface, ReleaseCOM<Interface>>;
struct FreeCOM {
    void operator()(wchar_t *pointer) const {
        CoTaskMemFree(pointer);
    }
};
using COMString = std::unique_ptr<wchar_t, FreeCOM>;

void require(HRESULT result, const char *message) {
    if (FAILED(result))
        throw std::runtime_error(message);
}

class Apartment {
  public:
    Apartment() {
        require(CoInitializeEx(nullptr, COINIT_MULTITHREADED),
                "Cannot initialize Windows camera control.");
    }
    ~Apartment() {
        CoUninitialize();
    }
    Apartment(const Apartment &) = delete;
    Apartment &operator=(const Apartment &) = delete;
};

template <typename Interface> COM<Interface> instance(REFCLSID id) {
    Interface *pointer = nullptr;
    require(CoCreateInstance(id, nullptr, CLSCTX_INPROC_SERVER, __uuidof(Interface),
                             reinterpret_cast<void **>(&pointer)),
            "Windows Portable Devices is unavailable.");
    return COM<Interface>(pointer);
}

COM<IPortableDevice> connectCamera(const wchar_t *id) {
    auto device = instance<IPortableDevice>(CLSID_PortableDeviceFTM);
    const auto settings = instance<IPortableDeviceValues>(CLSID_PortableDeviceValues);
    require(settings->SetStringValue(WPD_CLIENT_NAME, L"Apertone"),
            "Cannot configure camera access.");
    require(device->Open(id, settings.get()),
            "Cannot open the camera. Check its USB mode and other apps.");
    return device;
}

std::wstring captureTarget(IPortableDevice &device) {
    IPortableDeviceCapabilities *rawCaps = nullptr;
    require(device.Capabilities(&rawCaps), "Cannot read camera capabilities.");
    const COM<IPortableDeviceCapabilities> capabilities(rawCaps);
    IPortableDevicePropVariantCollection *rawObjects = nullptr;
    if (FAILED(capabilities->GetFunctionalObjects(WPD_FUNCTIONAL_CATEGORY_STILL_IMAGE_CAPTURE,
                                                  &rawObjects)))
        return {};
    const COM<IPortableDevicePropVariantCollection> objects(rawObjects);
    DWORD count = 0;
    if (FAILED(objects->GetCount(&count)) || !count)
        return {};
    PROPVARIANT target{};
    const auto status = objects->GetAt(0, &target);
    const std::wstring id =
        SUCCEEDED(status) && target.vt == VT_LPWSTR && target.pwszVal ? target.pwszVal : L"";
    PropVariantClear(&target);
    return id;
}

class CaptureEvents final : public IPortableDeviceEventCallback {
  public:
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID id, void **out) override {
        if (!out)
            return E_POINTER;
        *out = nullptr;
        if (id != IID_IUnknown && id != __uuidof(IPortableDeviceEventCallback))
            return E_NOINTERFACE;
        *out = static_cast<IPortableDeviceEventCallback *>(this);
        AddRef();
        return S_OK;
    }
    ULONG STDMETHODCALLTYPE AddRef() override {
        return ++references_;
    }
    ULONG STDMETHODCALLTYPE Release() override {
        const auto remaining = --references_;
        if (!remaining)
            delete this;
        return remaining;
    }
    HRESULT STDMETHODCALLTYPE OnEvent(IPortableDeviceValues *properties) override {
        if (!properties)
            return E_POINTER;
        try {
            GUID event{};
            if (FAILED(properties->GetGuidValue(WPD_EVENT_PARAMETER_EVENT_ID, &event)))
                return S_OK;
            if (event == WPD_EVENT_DEVICE_REMOVED) {
                std::lock_guard lock(mutex_);
                removed_ = true;
                updated_.notify_all();
            } else if (event == WPD_EVENT_OBJECT_ADDED) {
                wchar_t *raw = nullptr;
                if (SUCCEEDED(properties->GetStringValue(WPD_OBJECT_ID, &raw))) {
                    const COMString id(raw);
                    std::lock_guard lock(mutex_);
                    if (id && ids_.size() < 64)
                        ids_.emplace(id.get());
                    updated_.notify_all();
                }
            }
        } catch (...) {
            return E_FAIL;
        }
        return S_OK;
    }
    void clear() {
        std::lock_guard lock(mutex_);
        ids_.clear();
    }
    std::set<std::wstring> pending(CancellationToken stop) {
        std::unique_lock lock(mutex_);
        updated_.wait_for(lock, std::chrono::milliseconds(100),
                          [&] { return removed_ || !ids_.empty(); });
        checkCancellation(stop);
        if (removed_)
            throw std::runtime_error("The camera disconnected during capture.");
        return std::exchange(ids_, {});
    }

  private:
    std::atomic<ULONG> references_{1};
    std::mutex mutex_;
    std::condition_variable updated_;
    std::set<std::wstring> ids_;
    bool removed_ = false;
};

class Advice {
  public:
    Advice(IPortableDevice &device, CaptureEvents &events) : device_(device) {
        wchar_t *cookie = nullptr;
        require(device_.Advise(0, &events, nullptr, &cookie),
                "The camera does not report new photos to Windows.");
        cookie_.reset(cookie);
    }
    ~Advice() {
        if (cookie_)
            device_.Unadvise(cookie_.get());
    }
    Advice(const Advice &) = delete;
    Advice &operator=(const Advice &) = delete;

  private:
    IPortableDevice &device_;
    COMString cookie_;
};

void releaseShutter(IPortableDevice &device, const std::wstring &target) {
    const auto parameters = instance<IPortableDeviceValues>(CLSID_PortableDeviceValues);
    const auto &command = WPD_COMMAND_STILL_IMAGE_CAPTURE_INITIATE;
    require(parameters->SetGuidValue(WPD_PROPERTY_COMMON_COMMAND_CATEGORY, command.fmtid),
            "Cannot prepare camera capture.");
    require(parameters->SetUnsignedIntegerValue(WPD_PROPERTY_COMMON_COMMAND_ID, command.pid),
            "Cannot prepare camera capture.");
    require(parameters->SetStringValue(WPD_PROPERTY_COMMON_COMMAND_TARGET, target.c_str()),
            "Cannot prepare camera capture.");
    IPortableDeviceValues *raw = nullptr;
    require(device.SendCommand(0, parameters.get(), &raw),
            "The camera rejected the shutter command.");
    const COM<IPortableDeviceValues> response(raw);
    HRESULT status = E_FAIL;
    if (!response || FAILED(response->GetErrorValue(WPD_PROPERTY_COMMON_HRESULT, &status)))
        throw std::runtime_error("Windows did not return a capture result.");
    require(status, "The camera rejected the shutter command.");
}

std::filesystem::path downloadPhoto(IPortableDeviceContent &content, const std::wstring &id,
                                    const std::filesystem::path &folder, std::size_t index,
                                    CancellationToken stop) {
    IPortableDeviceProperties *rawProps = nullptr;
    require(content.Properties(&rawProps), "Cannot read the captured file information.");
    const COM<IPortableDeviceProperties> properties(rawProps);
    const auto keys = instance<IPortableDeviceKeyCollection>(CLSID_PortableDeviceKeyCollection);
    require(keys->Add(WPD_OBJECT_ORIGINAL_FILE_NAME), "Cannot read the captured file information.");
    require(keys->Add(WPD_OBJECT_NAME), "Cannot read the captured file information.");
    IPortableDeviceValues *rawValues = nullptr;
    if (FAILED(properties->GetValues(id.c_str(), keys.get(), &rawValues)))
        return {};
    const COM<IPortableDeviceValues> values(rawValues);
    wchar_t *rawName = nullptr;
    if (FAILED(values->GetStringValue(WPD_OBJECT_ORIGINAL_FILE_NAME, &rawName)) &&
        FAILED(values->GetStringValue(WPD_OBJECT_NAME, &rawName)))
        return {};
    const COMString filename(rawName);
    if (!filename)
        return {};
    const auto leaf = std::filesystem::path(filename.get()).filename();
    const auto name = QString::fromStdWString(leaf.native());
    if (name.size() > 180 || !PhotoDecoder::supportedExtension(leaf) ||
        std::ranges::any_of(name, [](QChar ch) {
            return ch.category() == QChar::Other_Control ||
                   QStringLiteral("\\/:*?\"<>|").contains(ch);
        }))
        return {};
    const auto path =
        folder / pathFromUtf8((QString::number(index + 1) + '-' + name).toStdString());
    IPortableDeviceResources *rawResources = nullptr;
    require(content.Transfer(&rawResources), "Cannot transfer the captured photo.");
    const COM<IPortableDeviceResources> resources(rawResources);
    DWORD bufferSize = 0;
    IStream *rawStream = nullptr;
    require(
        resources->GetStream(id.c_str(), WPD_RESOURCE_DEFAULT, STGM_READ, &bufferSize, &rawStream),
        "Cannot open the captured photo on the camera.");
    const COM<IStream> stream(rawStream);
    QSaveFile output(QString::fromStdWString(path.native()));
    if (!output.open(QIODevice::WriteOnly))
        throw std::runtime_error("Cannot write a captured photo to the selected folder.");
    std::vector<char> bytes(std::clamp<DWORD>(bufferSize, 65536, 1048576));
    std::uint64_t total = 0;
    for (;;) {
        checkCancellation(stop);
        ULONG read = 0;
        require(stream->Read(bytes.data(), static_cast<ULONG>(bytes.size()), &read),
                "Camera download was interrupted.");
        if (!read)
            break;
        total += read;
        if (read > bytes.size() || total > 1'300'000'000 ||
            output.write(bytes.data(), read) != read)
            throw std::runtime_error("Could not save the complete camera photo.");
    }
    checkCancellation(stop);
    if (!total || !output.commit())
        throw std::runtime_error("Could not save the complete camera photo.");
    return path;
}
} // namespace

std::vector<TetherCamera> tetherCameras(CancellationToken stop) {
    const Apartment com;
    const auto manager = instance<IPortableDeviceManager>(CLSID_PortableDeviceManager);
    require(manager->RefreshDeviceList(), "Cannot refresh connected cameras.");
    DWORD count = 0;
    require(manager->GetDevices(nullptr, &count), "Cannot list connected cameras.");
    if (!count)
        return {};
    if (count > 128)
        throw std::runtime_error("Too many Windows Portable Devices.");
    std::array<wchar_t *, 128> raw{};
    const auto status = manager->GetDevices(raw.data(), &count);
    std::array<COMString, 128> ids;
    std::ranges::transform(raw, ids.begin(), [](wchar_t *id) { return COMString(id); });
    require(status, "Cannot list connected cameras.");
    std::vector<TetherCamera> result;
    for (const auto &id : ids) {
        if (!id)
            continue;
        checkCancellation(stop);
        try {
            if (captureTarget(*connectCamera(id.get())).empty())
                continue;
        } catch (const std::runtime_error &) {
            continue;
        }
        std::wstring name = L"Windows Portable Camera";
        DWORD length = 0;
        if (SUCCEEDED(manager->GetDeviceFriendlyName(id.get(), nullptr, &length)) && length > 1 &&
            length <= 1024) {
            std::wstring label(length, L'\0');
            if (SUCCEEDED(manager->GetDeviceFriendlyName(id.get(), label.data(), &length))) {
                label.resize(std::char_traits<wchar_t>::length(label.c_str()));
                if (!label.empty())
                    name = std::move(label);
            }
        }
        result.push_back({id.get(), std::move(name)});
    }
    return result;
}

std::vector<std::filesystem::path> captureTetheredPhotos(const std::wstring &id,
                                                         const std::filesystem::path &destination,
                                                         CancellationToken stop) {
    using Clock = std::chrono::steady_clock;
    using namespace std::chrono_literals;
    if (id.empty() || id.size() > 4096 || !destination.is_absolute() ||
        !std::filesystem::is_directory(destination))
        throw std::runtime_error("Choose a connected camera and an existing capture folder.");
    checkCancellation(stop);
    const Apartment com;
    const auto device = connectCamera(id.c_str());
    const auto target = captureTarget(*device);
    if (target.empty())
        throw std::runtime_error("The camera does not expose shutter control to Windows.");
    IPortableDeviceContent *rawContent = nullptr;
    require(device->Content(&rawContent), "Cannot open the camera storage.");
    const COM<IPortableDeviceContent> content(rawContent);
    const COM<CaptureEvents> events(new CaptureEvents);
    const Advice advice(*device, *events);
    const auto folder =
        destination /
        pathFromUtf8("Capture-" + QUuid::createUuid().toString(QUuid::Id128).toStdString());
    if (!std::filesystem::create_directory(folder))
        throw std::runtime_error("Cannot create a folder for the captured files.");
    try {
        events->clear();
        checkCancellation(stop);
        releaseShutter(*device, target);
        std::vector<std::filesystem::path> photos;
        std::set<std::wstring> received;
        auto deadline = Clock::now() + 45s;
        while (Clock::now() < deadline && photos.size() < 8) {
            for (const auto &object : events->pending(stop)) {
                if (!received.insert(object).second)
                    continue;
                auto photo = downloadPhoto(*content, object, folder, photos.size(), stop);
                if (!photo.empty()) {
                    photos.push_back(std::move(photo));
                    deadline = Clock::now() + 2500ms;
                    if (photos.size() == 8)
                        break;
                }
            }
        }
        if (photos.empty())
            throw std::runtime_error("The camera did not deliver a supported photo after capture.");
        return photos;
    } catch (...) {
        std::error_code ignored;
        std::filesystem::remove(folder,
                                ignored); // Only removes an empty folder owned by this capture.
        throw;
    }
}
} // namespace luma::windows
