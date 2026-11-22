#include "platform/windows/TetherCapture.h"
#include "apps/shared/PhotoDecoder.h"
#include "core/image/Bayer.h"
#include "core/io/Path.h"
#include <QBuffer>
#include <QImageReader>
#include <QSaveFile>
#include <QUuid>
#include <algorithm>
#include <array>
#include <chrono>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <set>
#include <thread>
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

// WpdMtpExtensions.h is absent from the MinGW SDK used by this build.
// Keep the published WPD MTP command category and property IDs together here.
namespace mtp {
constexpr GUID category{
    0x4d545058, 0x1a2e, 0x4106, {0xa3, 0x57, 0x77, 0x1e, 0x08, 0x19, 0xfc, 0x56}};
constexpr PROPERTYKEY opcode{category, 1001}, arguments{category, 1002}, response{category, 1003},
    vendorCodes{category, 1005}, context{category, 1006}, dataSize{category, 1007},
    bytesToRead{category, 1008}, bytesRead{category, 1009}, bytesToWrite{category, 1010},
    bytesWritten{category, 1011}, buffer{category, 1012}, optimalBuffer{category, 1013};
constexpr DWORD getVendorCodes = 11, execute = 12, beginRead = 13, beginWrite = 14, read = 15,
                write = 16, end = 17;
} // namespace mtp

constexpr std::uint16_t getDeviceInfo = 0x1001, initiateCapture = 0x100e,
                        getPropertyDescriptor = 0x1014, setPropertyValue = 0x1016;
constexpr std::uint16_t nikonStartLiveView = 0x9201, nikonEndLiveView = 0x9202,
                        nikonLiveViewImage = 0x9203, nikonCaptureToMedia = 0x9207;
constexpr std::array<std::uint16_t, 5> exposureCodes{0x5007, 0x500d, 0x500f, 0x5010, 0xd100};

class PtpRejected : public std::runtime_error {
  public:
    explicit PtpRejected(DWORD value)
        : std::runtime_error("The camera rejected the requested PTP operation."), code(value) {}
    DWORD code;
};

class PtpDevice {
  public:
    explicit PtpDevice(IPortableDevice &device) : device_(device) {}

    std::vector<std::uint16_t> vendorOperations() {
        const auto response = send(*command(mtp::getVendorCodes));
        IPortableDevicePropVariantCollection *raw = nullptr;
        require(response->GetIPortableDevicePropVariantCollectionValue(mtp::vendorCodes, &raw),
                dataError);
        const COM<IPortableDevicePropVariantCollection> codes(raw);
        DWORD count = 0;
        if (!codes || FAILED(codes->GetCount(&count)) || count > 4096)
            throw std::runtime_error(dataError);
        std::vector<std::uint16_t> operations;
        for (DWORD i = 0; i < count; ++i) {
            PROPVARIANT item{};
            const auto status = codes->GetAt(i, &item);
            if (SUCCEEDED(status) && item.vt == VT_UI4 && item.ulVal <= 0xffff)
                operations.push_back(static_cast<std::uint16_t>(item.ulVal));
            PropVariantClear(&item);
        }
        return operations;
    }

    std::u16string manufacturer() {
        IPortableDeviceContent *rawContent = nullptr;
        require(device_.Content(&rawContent), dataError);
        const COM<IPortableDeviceContent> content(rawContent);
        IPortableDeviceProperties *rawProperties = nullptr;
        require(content->Properties(&rawProperties), dataError);
        const COM<IPortableDeviceProperties> props(rawProperties);
        const auto keys = instance<IPortableDeviceKeyCollection>(CLSID_PortableDeviceKeyCollection);
        require(keys->Add(WPD_DEVICE_MANUFACTURER), dataError);
        IPortableDeviceValues *raw = nullptr;
        require(props->GetValues(WPD_DEVICE_OBJECT_ID, keys.get(), &raw), dataError);
        const COM<IPortableDeviceValues> values(raw);
        LPWSTR label = nullptr;
        require(values->GetStringValue(WPD_DEVICE_MANUFACTURER, &label), dataError);
        const COMString owner(label);
        return owner ? QString::fromWCharArray(owner.get()).toStdU16String() : std::u16string{};
    }

    QByteArray read(std::uint16_t opcode, const std::initializer_list<DWORD> &parameters = {},
                    CancellationToken stop = {}) {
        auto input = command(mtp::beginRead);
        setOperation(*input, opcode, parameters);
        const auto result = send(*input);
        Transfer transfer(*this, *result);
        ULONGLONG size = 0;
        require(result->GetUnsignedLargeIntegerValue(mtp::dataSize, &size), dataError);
        constexpr auto maximum = 12 * 1024 * 1024;
        const bool unknown = size == 0xffffffffULL || size == 0xffffffffffffffffULL;
        if (!unknown && size > maximum)
            throw std::runtime_error(dataError);
        DWORD preferred = 256 * 1024;
        result->GetUnsignedIntegerValue(mtp::optimalBuffer, &preferred);
        preferred = std::clamp<DWORD>(preferred, 1024, 1024 * 1024);
        QByteArray buffer(static_cast<qsizetype>(preferred), '\0');
        QByteArray data;
        while (unknown || static_cast<ULONGLONG>(data.size()) < size) {
            checkCancellation(stop);
            const auto length = unknown
                                    ? preferred
                                    : static_cast<DWORD>(std::min<ULONGLONG>(
                                          preferred, size - static_cast<ULONGLONG>(data.size())));
            const auto values = transfer.packet(mtp::read);
            require(values->SetUnsignedIntegerValue(mtp::bytesToRead, length), dataError);
            require(values->SetBufferValue(mtp::buffer, reinterpret_cast<BYTE *>(buffer.data()),
                                           length),
                    dataError);
            const auto response = send(*values);
            DWORD count = 0, bufferLength = 0;
            BYTE *bytes = nullptr;
            require(response->GetUnsignedIntegerValue(mtp::bytesRead, &count), dataError);
            if (!count && unknown)
                break;
            const auto status = response->GetBufferValue(mtp::buffer, &bytes, &bufferLength);
            const std::unique_ptr<BYTE, decltype(&CoTaskMemFree)> owner(bytes, CoTaskMemFree);
            require(status, dataError);
            if (!bytes || count == 0 || count > length || count > bufferLength ||
                static_cast<qsizetype>(count) > maximum - data.size())
                throw std::runtime_error(dataError);
            data.append(reinterpret_cast<const char *>(bytes), static_cast<qsizetype>(count));
            if (unknown && count < length)
                break;
        }
        transfer.finish();
        return data;
    }
    void write(std::uint16_t opcode, const std::initializer_list<DWORD> &parameters,
               std::vector<std::uint8_t> data) {
        const auto input = command(mtp::beginWrite);
        setOperation(*input, opcode, parameters);
        const auto size = static_cast<DWORD>(data.size());
        require(input->SetUnsignedLargeIntegerValue(mtp::dataSize, size), dataError);
        const auto result = send(*input);
        Transfer transfer(*this, *result);
        const auto chunk = transfer.packet(mtp::write);
        require(chunk->SetUnsignedIntegerValue(mtp::bytesToWrite, size), dataError);
        require(chunk->SetBufferValue(mtp::buffer, data.data(), size), dataError);
        const auto response = send(*chunk);
        DWORD written = 0;
        require(response->GetUnsignedIntegerValue(mtp::bytesWritten, &written), dataError);
        if (written != size)
            throw std::runtime_error(dataError);
        transfer.finish();
    }
    void execute(std::uint16_t opcode, const std::initializer_list<DWORD> &parameters = {}) {
        const auto input = command(mtp::execute);
        setOperation(*input, opcode, parameters);
        checkResponse(*send(*input));
    }

  private:
    static constexpr const char *dataError = "The camera's Windows PTP transfer failed.";
    IPortableDevice &device_;

    static COM<IPortableDeviceValues> command(DWORD id) {
        auto values = instance<IPortableDeviceValues>(CLSID_PortableDeviceValues);
        require(values->SetGuidValue(WPD_PROPERTY_COMMON_COMMAND_CATEGORY, mtp::category),
                dataError);
        require(values->SetUnsignedIntegerValue(WPD_PROPERTY_COMMON_COMMAND_ID, id), dataError);
        return values;
    }
    static void setOperation(IPortableDeviceValues &values, DWORD opcode,
                             const std::initializer_list<DWORD> &parameters) {
        const auto collection = instance<IPortableDevicePropVariantCollection>(
            CLSID_PortableDevicePropVariantCollection);
        for (const auto parameter : parameters) {
            PROPVARIANT entry{};
            entry.vt = VT_UI4;
            entry.ulVal = parameter;
            require(collection->Add(&entry), dataError);
        }
        require(values.SetUnsignedIntegerValue(mtp::opcode, opcode), dataError);
        require(
            values.SetIPortableDevicePropVariantCollectionValue(mtp::arguments, collection.get()),
            dataError);
    }
    COM<IPortableDeviceValues> send(IPortableDeviceValues &input) {
        IPortableDeviceValues *raw = nullptr;
        const auto result = device_.SendCommand(0, &input, &raw);
        COM<IPortableDeviceValues> output(raw);
        require(result, dataError);
        if (!output)
            throw std::runtime_error(dataError);
        HRESULT status = E_FAIL;
        require(output->GetErrorValue(WPD_PROPERTY_COMMON_HRESULT, &status), dataError);
        require(status, dataError);
        return output;
    }
    static void checkResponse(IPortableDeviceValues &result) {
        DWORD response = 0;
        require(result.GetUnsignedIntegerValue(mtp::response, &response), dataError);
        if (response != 0x2001)
            throw PtpRejected(response);
    }
    class Transfer {
      public:
        Transfer(PtpDevice &device, IPortableDeviceValues &response) : device_(device) {
            LPWSTR token = nullptr;
            require(response.GetStringValue(mtp::context, &token), dataError);
            context_.reset(token);
            if (!context_ || !*context_)
                throw std::runtime_error(dataError);
        }
        ~Transfer() {
            if (active_)
                try {
                    finish();
                } catch (...) {
                    // The device can be unplugged while ending an in-flight read.
                }
        }
        COM<IPortableDeviceValues> packet(DWORD action) {
            auto result = command(action);
            require(result->SetStringValue(mtp::context, context_.get()), dataError);
            return result;
        }
        void finish() {
            active_ = false;
            checkResponse(*device_.send(*packet(mtp::end)));
        }

      private:
        PtpDevice &device_;
        COMString context_;
        bool active_ = true;
    };
};

std::span<const std::uint8_t> dataView(const QByteArray &data) {
    return {reinterpret_cast<const std::uint8_t *>(data.constData()),
            static_cast<std::size_t>(data.size())};
}

QByteArray displayLiveFrame(const QByteArray &data) {
    const auto marker = QByteArray::fromHex("ffd8ff");
    const auto start = data.indexOf(marker);
    if (start < 0 || start > 16384)
        throw std::runtime_error("The camera did not return a JPEG Live View frame.");
    QBuffer input;
    input.setData(data.sliced(start));
    input.open(QIODevice::ReadOnly);
    QImageReader reader(&input, "jpeg");
    const auto size = reader.size();
    if (!size.isValid() || size.width() > 4096 || size.height() > 4096)
        throw std::runtime_error("The camera returned an unsupported Live View frame size.");
    if (size.width() > 1280 || size.height() > 960)
        reader.setScaledSize(size.scaled(1280, 960, Qt::KeepAspectRatio));
    const auto frame = reader.read();
    QByteArray jpeg;
    QBuffer output(&jpeg);
    if (frame.isNull() || !output.open(QIODevice::WriteOnly) || !frame.save(&output, "JPEG", 85))
        throw std::runtime_error("Cannot decode the camera's Live View frame.");
    return jpeg;
}

ptp::DeviceInfo describeCamera(PtpDevice &device, CancellationToken stop) {
    try {
        return ptp::deviceInfo(dataView(device.read(getDeviceInfo, {}, stop)));
    } catch (const std::runtime_error &) {
        checkCancellation(stop);
    }
    // Some MTP drivers answer GetDeviceInfo themselves and only expose the
    // vendor operation list to clients. Probe only known Nikon controls then.
    ptp::DeviceInfo result;
    try {
        result.manufacturer = device.manufacturer();
        if (!result.nikon())
            return {};
        result.operations = device.vendorOperations();
    } catch (const std::runtime_error &) {
        checkCancellation(stop);
        return result;
    }
    for (const auto code : exposureCodes) {
        checkCancellation(stop);
        try {
            (void)ptp::exposureProperty(dataView(device.read(getPropertyDescriptor, {code}, stop)),
                                        code);
            result.properties.push_back(code);
        } catch (const std::runtime_error &) {
            checkCancellation(stop);
        }
    }
    if (!result.properties.empty()) {
        result.operations.push_back(getPropertyDescriptor);
        result.operations.push_back(setPropertyValue);
    }
    return result;
}

bool supportsExposure(const ptp::DeviceInfo &info) {
    return info.supports(getPropertyDescriptor) &&
           std::ranges::any_of(exposureCodes, [&](auto code) { return info.hasProperty(code); });
}

bool supportsLive(const ptp::DeviceInfo &info) {
    return info.nikon() && info.supports(nikonStartLiveView) && info.supports(nikonEndLiveView) &&
           info.supports(nikonLiveViewImage);
}

class CameraLiveView {
  public:
    explicit CameraLiveView(PtpDevice &camera) : camera_(camera) {}
    ~CameraLiveView() {
        if (active_)
            try {
                set(false);
            } catch (...) {
                // The connection can already be gone when the user closes the dialog.
            }
    }
    bool active() const {
        return active_;
    }
    void set(bool live) {
        if (active_ != live) {
            camera_.execute(live ? nikonStartLiveView : nikonEndLiveView);
            active_ = live;
        }
    }

  private:
    PtpDevice &camera_;
    bool active_ = false;
};

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
            const auto camera = connectCamera(id.get());
            if (captureTarget(*camera).empty()) {
                PtpDevice ptp(*camera);
                const auto info = describeCamera(ptp, stop);
                if (!supportsLive(info) && !supportsExposure(info))
                    continue;
            }
        } catch (const std::runtime_error &) {
            checkCancellation(stop);
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

namespace {
std::vector<std::filesystem::path> captureConnectedCamera(IPortableDevice &device, PtpDevice &ptp,
                                                          const ptp::DeviceInfo &info,
                                                          const std::filesystem::path &destination,
                                                          CancellationToken stop) {
    using Clock = std::chrono::steady_clock;
    using namespace std::chrono_literals;
    if (!destination.is_absolute() || !std::filesystem::is_directory(destination))
        throw std::runtime_error("Choose a connected camera and an existing capture folder.");
    checkCancellation(stop);
    const auto target = captureTarget(device);
    const bool nikon = info.nikon() && info.supports(nikonCaptureToMedia);
    if (target.empty() && !nikon && !info.supports(initiateCapture))
        throw std::runtime_error("The camera does not expose shutter control to Windows.");
    IPortableDeviceContent *rawContent = nullptr;
    require(device.Content(&rawContent), "Cannot open the camera storage.");
    const COM<IPortableDeviceContent> content(rawContent);
    const COM<CaptureEvents> events(new CaptureEvents);
    const Advice advice(device, *events);
    const auto folder =
        destination /
        pathFromUtf8("Capture-" + QUuid::createUuid().toString(QUuid::Id128).toStdString());
    if (!std::filesystem::create_directory(folder))
        throw std::runtime_error("Cannot create a folder for the captured files.");
    try {
        events->clear();
        checkCancellation(stop);
        if (!target.empty())
            releaseShutter(device, target);
        else if (nikon)
            ptp.execute(nikonCaptureToMedia, {0xfffffffeUL, 0}); // Focus, write to the camera card.
        else
            ptp.execute(initiateCapture, {0, 0});
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
} // namespace

std::vector<std::filesystem::path> captureTetheredPhotos(const std::wstring &id,
                                                         const std::filesystem::path &destination,
                                                         CancellationToken stop) {
    if (id.empty() || id.size() > 4096)
        throw std::runtime_error("Choose a connected camera and an existing capture folder.");
    checkCancellation(stop);
    const Apartment com;
    const auto device = connectCamera(id.c_str());
    PtpDevice ptp(*device);
    const auto info =
        captureTarget(*device).empty() ? describeCamera(ptp, stop) : ptp::DeviceInfo{};
    return captureConnectedCamera(*device, ptp, info, destination, stop);
}

void TetherControlRequests::submit(TetherControlCommand request) {
    const std::scoped_lock lock(mutex_);
    std::erase_if(queue_, [&](const auto &pending) {
        return pending.action == request.action && pending.property == request.property;
    });
    queue_.push_back(std::move(request));
}
std::optional<TetherControlCommand> TetherControlRequests::next() {
    const std::scoped_lock lock(mutex_);
    if (queue_.empty())
        return std::nullopt;
    auto request = std::move(queue_.front());
    queue_.pop_front();
    return request;
}

std::vector<std::filesystem::path>
controlTetheredCamera(const std::wstring &id, TetherControlRequests &requests,
                      const std::function<void(const TetherControlState &)> &state,
                      const std::function<void(const QByteArray &)> &liveFrame,
                      const std::function<void(const char *)> &warning, CancellationToken stop) {
    using namespace std::chrono_literals;
    if (id.empty() || id.size() > 4096)
        throw std::runtime_error("Choose a connected camera.");
    checkCancellation(stop);
    const Apartment apartment;
    const auto device = connectCamera(id.c_str());
    PtpDevice camera(*device);
    const auto info = describeCamera(camera, stop);
    TetherControlState controls;
    controls.liveAvailable = supportsLive(info);
    if (!controls.liveAvailable && !supportsExposure(info))
        throw std::runtime_error(
            "This camera driver does not expose Live View or exposure controls.");
    CameraLiveView live(camera);
    const auto descriptor = [&](std::uint16_t code) {
        return ptp::exposureProperty(dataView(camera.read(getPropertyDescriptor, {code}, stop)),
                                     code);
    };
    const auto refresh = [&] {
        controls.exposure.clear();
        if (info.supports(getPropertyDescriptor))
            for (const auto code : exposureCodes) {
                if (!info.hasProperty(code) || (code == 0xd100 && !info.nikon()) ||
                    (code == 0x500d && info.nikon() && info.hasProperty(0xd100)))
                    continue;
                try {
                    auto setting = descriptor(code);
                    setting.writable = setting.writable && info.supports(setPropertyValue);
                    controls.exposure.push_back(std::move(setting));
                } catch (const std::runtime_error &) {
                    checkCancellation(stop);
                }
            }
        controls.live = live.active();
        state(controls);
    };
    refresh();
    unsigned missedFrames = 0;
    while (!stop.isCancelled()) {
        if (const auto request = requests.next()) {
            try {
                switch (request->action) {
                case TetherControlCommand::Action::capture:
                    live.set(false);
                    return captureConnectedCamera(*device, camera, info, request->destination,
                                                  stop);
                case TetherControlCommand::Action::live:
                    if (!controls.liveAvailable)
                        throw std::runtime_error("This camera driver does not expose Live View.");
                    live.set(request->value != 0);
                    missedFrames = 0;
                    if (!live.active())
                        liveFrame({});
                    break;
                case TetherControlCommand::Action::exposure: {
                    const auto property = std::ranges::find(controls.exposure, request->property,
                                                            &ptp::ExposureProperty::id);
                    if (property == controls.exposure.end() || !info.supports(setPropertyValue))
                        throw std::runtime_error(
                            "The selected exposure setting is unavailable on the camera.");
                    camera.write(setPropertyValue, {property->id},
                                 ptp::exposureValue(descriptor(property->id), request->value));
                    break;
                }
                case TetherControlCommand::Action::refresh:
                    break;
                }
                refresh();
            } catch (const std::runtime_error &error) {
                checkCancellation(stop);
                warning(error.what());
                refresh();
            }
        }
        if (live.active()) {
            try {
                liveFrame(displayLiveFrame(camera.read(nikonLiveViewImage, {}, stop)));
                missedFrames = 0;
            } catch (const std::runtime_error &error) {
                checkCancellation(stop);
                if (++missedFrames >= 25) {
                    live.set(false);
                    liveFrame({});
                    warning(error.what());
                    refresh();
                }
            }
        }
        std::this_thread::sleep_for(live.active() ? 170ms : 80ms);
    }
    return {};
}
} // namespace luma::windows
