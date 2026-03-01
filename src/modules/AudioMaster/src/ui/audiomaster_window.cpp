#include "modules/AudioMaster/src/ui/audiomaster_window.hpp"
#include "modules/AudioMaster/src/ui/vu_meter_widget.hpp"
#include "modules/AudioMaster/src/ui/virtual_devices_widget.hpp"
#include "logger/logger.hpp"

#if defined(Q_OS_WIN)
#include <windows.h>
#include <objbase.h>
#include <endpointvolume.h>
#include <mmdeviceapi.h>
#include <audiopolicy.h>

MIDL_INTERFACE("C02216F6-8C67-4B5B-9D00-D008E73E0064")
IAudioMeterInformation : public IUnknown {
    virtual HRESULT STDMETHODCALLTYPE GetPeakValue(float *pfPeak) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetMeteringChannelCount(UINT *pnChannelCount) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetChannelsPeakValues(UINT u32ChannelCount,
                                                            float *afPeakValues) = 0;
    virtual HRESULT STDMETHODCALLTYPE QueryHardwareSupport(DWORD *pdwHardwareSupportMask) = 0;
};

static const IID IID_IAudioMeterInformation =
    {0xC02216F6, 0x8C67, 0x4B5B, {0x9D, 0x00, 0xD0, 0x08, 0xE7, 0x3E, 0x00, 0x64}};

#endif

#include <QAudioDevice>
#include <QFileInfo>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QMediaDevices>
#include <QSet>
#include <QSignalBlocker>
#include <QVBoxLayout>

#include <array>

#include "common/themes/theme_listener.hpp"
#include "common/themes/theme_helper.hpp"

namespace wintools::audiomaster {

namespace {
constexpr const char* LogSource = "AudioMaster";

struct DeviceEntry {
    QString id;
    QString name;
};

constexpr std::array<const char*, 4> OutputChannelKeys = {"master", "game", "chat", "media"};
constexpr const char* MicChannelKey = "mic";

QString settingKeyForChannel(const QString& channelKey) {
    return QStringLiteral("routing/%1").arg(channelKey);
}

QString encodedId(const QString& id) {
    return QString::fromLatin1(id.toUtf8().toBase64(QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals));
}

QString cycleSettingKey(bool isInput, const QString& deviceId) {
    return QStringLiteral("cycle/%1/%2").arg(isInput ? QStringLiteral("input") : QStringLiteral("output"), encodedId(deviceId));
}

bool isDeviceIncludedInCycle(QSettings& settings, bool isInput, const QString& deviceId) {
    return settings.value(cycleSettingKey(isInput, deviceId), true).toBool();
}

void setDeviceIncludedInCycle(QSettings& settings, bool isInput, const QString& deviceId, bool included) {
    settings.setValue(cycleSettingKey(isInput, deviceId), included);
}

QVector<DeviceEntry> outputDevices() {
    QVector<DeviceEntry> devices;
    const auto outputs = QMediaDevices::audioOutputs();
    devices.reserve(outputs.size());
    for (const QAudioDevice& dev : outputs) {
        devices.push_back(DeviceEntry{QString::fromUtf8(dev.id()), dev.description()});
    }
    return devices;
}

QVector<DeviceEntry> inputDevices() {
    QVector<DeviceEntry> devices;
    const auto inputs = QMediaDevices::audioInputs();
    devices.reserve(inputs.size());
    for (const QAudioDevice& dev : inputs) {
        devices.push_back(DeviceEntry{QString::fromUtf8(dev.id()), dev.description()});
    }
    return devices;
}

QVector<DeviceEntry> enabledCycleDevices(QSettings& settings,
                                         const QVector<DeviceEntry>& allDevices,
                                         bool isInput) {
    QVector<DeviceEntry> filtered;
    filtered.reserve(allDevices.size());
    for (const DeviceEntry& device : allDevices) {
        if (isDeviceIncludedInCycle(settings, isInput, device.id)) {
            filtered.push_back(device);
        }
    }
    return filtered;
}

QString deviceNameForId(const QVector<DeviceEntry>& devices, const QString& id) {
    for (const DeviceEntry& device : devices) {
        if (device.id == id) return device.name;
    }
    return QStringLiteral("Not linked");
}

QString nextDeviceId(const QVector<DeviceEntry>& devices, const QString& currentId) {
    if (devices.isEmpty()) return {};

    int currentIndex = -1;
    for (int i = 0; i < devices.size(); ++i) {
        if (devices[i].id == currentId) {
            currentIndex = i;
            break;
        }
    }
    const int nextIndex = (currentIndex + 1) % devices.size();
    return devices[nextIndex].id;
}

#if defined(Q_OS_WIN)
QString processNameFromId(DWORD pid) {
    if (pid == 0) return QStringLiteral("System Audio");

    HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!process) {
        return QStringLiteral("PID %1").arg(pid);
    }

    wchar_t pathBuffer[MAX_PATH] = {};
    DWORD size = static_cast<DWORD>(std::size(pathBuffer));
    QString result;
    if (QueryFullProcessImageNameW(process, 0, pathBuffer, &size)) {
        result = QFileInfo(QString::fromWCharArray(pathBuffer)).fileName();
    }

    CloseHandle(process);

    if (result.isEmpty()) {
        result = QStringLiteral("PID %1").arg(pid);
    }
    return result;
}

QStringList activeAppNamesForEndpoint(const QString& deviceId) {
    QStringList result;
    if (deviceId.isEmpty()) return result;

    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    const bool shouldUninitialize = SUCCEEDED(hr);
    if (FAILED(hr) && hr != RPC_E_CHANGED_MODE) {
        return result;
    }

    IMMDeviceEnumerator* enumerator = nullptr;
    IMMDevice* endpoint = nullptr;
    IAudioSessionManager2* sessionManager = nullptr;
    IAudioSessionEnumerator* sessions = nullptr;
    QSet<QString> dedup;
    int count = 0;

    hr = CoCreateInstance(__uuidof(MMDeviceEnumerator),
                          nullptr,
                          CLSCTX_ALL,
                          __uuidof(IMMDeviceEnumerator),
                          reinterpret_cast<void**>(&enumerator));
    if (FAILED(hr) || !enumerator) goto cleanup;

    {
        const std::wstring wId = deviceId.toStdWString();
        hr = enumerator->GetDevice(wId.c_str(), &endpoint);
    }
    if (FAILED(hr) || !endpoint) goto cleanup;

    hr = endpoint->Activate(__uuidof(IAudioSessionManager2),
                            CLSCTX_ALL,
                            nullptr,
                            reinterpret_cast<void**>(&sessionManager));
    if (FAILED(hr) || !sessionManager) goto cleanup;

    hr = sessionManager->GetSessionEnumerator(&sessions);
    if (FAILED(hr) || !sessions) goto cleanup;

    hr = sessions->GetCount(&count);
    if (FAILED(hr) || count <= 0) goto cleanup;

    for (int i = 0; i < count; ++i) {
        IAudioSessionControl* control = nullptr;
        hr = sessions->GetSession(i, &control);
        if (FAILED(hr) || !control) continue;

        AudioSessionState state = AudioSessionStateInactive;
        control->GetState(&state);
        if (state != AudioSessionStateActive) {
            control->Release();
            continue;
        }

        IAudioSessionControl2* control2 = nullptr;
        hr = control->QueryInterface(__uuidof(IAudioSessionControl2), reinterpret_cast<void**>(&control2));
        if (SUCCEEDED(hr) && control2) {
            DWORD processId = 0;
            control2->GetProcessId(&processId);

            LPWSTR displayNameRaw = nullptr;
            QString displayName;
            if (SUCCEEDED(control2->GetDisplayName(&displayNameRaw)) && displayNameRaw) {
                displayName = QString::fromWCharArray(displayNameRaw).trimmed();
                CoTaskMemFree(displayNameRaw);
            }

            QString resolvedName = processNameFromId(processId);
            if (!displayName.isEmpty() && displayName.compare(resolvedName, Qt::CaseInsensitive) != 0) {
                resolvedName = QStringLiteral("%1 (%2)").arg(displayName, resolvedName);
            }

            if (!resolvedName.isEmpty()) {
                dedup.insert(resolvedName);
            }
            control2->Release();
        }

        control->Release();
    }

cleanup:
    if (sessions) sessions->Release();
    if (sessionManager) sessionManager->Release();
    if (endpoint) endpoint->Release();
    if (enumerator) enumerator->Release();
    if (shouldUninitialize) CoUninitialize();

    result = dedup.values();
    std::sort(result.begin(), result.end(), [](const QString& lhs, const QString& rhs) {
        return lhs.compare(rhs, Qt::CaseInsensitive) < 0;
    });
    return result;
}

bool setDefaultEndpointForAllRoles(const QString& deviceId) {
    if (deviceId.isEmpty()) return false;

    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    const bool shouldUninitialize = SUCCEEDED(hr);
    if (FAILED(hr) && hr != RPC_E_CHANGED_MODE) {
        return false;
    }

    static const CLSID CLSID_CPolicyConfigVistaClient = {
        0x870af99c, 0x171d, 0x4f9e, {0xaf,0x0d,0xe6,0x3d,0xf4,0x0c,0x2b,0xc9}
    };
    static const IID IID_IPolicyConfig = {
        0xf8679f50, 0x850a, 0x41cf, {0x9c,0x72,0x43,0x0f,0x29,0x02,0x90,0xc8}
    };

    struct IPolicyConfig : public IUnknown {
        virtual HRESULT STDMETHODCALLTYPE GetMixFormat() = 0;
        virtual HRESULT STDMETHODCALLTYPE GetDeviceFormat() = 0;
        virtual HRESULT STDMETHODCALLTYPE ResetDeviceFormat() = 0;
        virtual HRESULT STDMETHODCALLTYPE SetDeviceFormat() = 0;
        virtual HRESULT STDMETHODCALLTYPE GetProcessingPeriod() = 0;
        virtual HRESULT STDMETHODCALLTYPE SetProcessingPeriod() = 0;
        virtual HRESULT STDMETHODCALLTYPE GetShareMode() = 0;
        virtual HRESULT STDMETHODCALLTYPE SetShareMode() = 0;
        virtual HRESULT STDMETHODCALLTYPE GetPropertyValue() = 0;
        virtual HRESULT STDMETHODCALLTYPE SetPropertyValue() = 0;
        virtual HRESULT STDMETHODCALLTYPE SetDefaultEndpoint(LPCWSTR wszDeviceId, ERole eRole) = 0;
        virtual HRESULT STDMETHODCALLTYPE SetEndpointVisibility() = 0;
    };

    bool ok = false;
    IPolicyConfig* policy = nullptr;
    hr = CoCreateInstance(CLSID_CPolicyConfigVistaClient,
                          nullptr,
                          CLSCTX_ALL,
                          IID_IPolicyConfig,
                          reinterpret_cast<void**>(&policy));
    if (SUCCEEDED(hr) && policy) {
        const std::wstring wId = deviceId.toStdWString();
        const bool r1 = SUCCEEDED(policy->SetDefaultEndpoint(wId.c_str(), eConsole));
        const bool r2 = SUCCEEDED(policy->SetDefaultEndpoint(wId.c_str(), eMultimedia));
        const bool r3 = SUCCEEDED(policy->SetDefaultEndpoint(wId.c_str(), eCommunications));
        ok = r1 || r2 || r3;
        policy->Release();
    }

    if (shouldUninitialize) {
        CoUninitialize();
    }
    return ok;
}

bool setEndpointVolume(const QString& deviceId, float scalar) {
    if (deviceId.isEmpty()) return false;

    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    const bool shouldUninitialize = SUCCEEDED(hr);
    if (FAILED(hr) && hr != RPC_E_CHANGED_MODE) {
        return false;
    }

    IMMDeviceEnumerator* enumerator = nullptr;
    IMMDevice* endpoint = nullptr;
    IAudioEndpointVolume* endpointVolume = nullptr;
    bool ok = false;

    hr = CoCreateInstance(__uuidof(MMDeviceEnumerator),
                          nullptr,
                          CLSCTX_ALL,
                          __uuidof(IMMDeviceEnumerator),
                          reinterpret_cast<void**>(&enumerator));
    if (FAILED(hr) || !enumerator) goto cleanup;

    {
        const std::wstring wId = deviceId.toStdWString();
        hr = enumerator->GetDevice(wId.c_str(), &endpoint);
    }
    if (FAILED(hr) || !endpoint) goto cleanup;

    hr = endpoint->Activate(__uuidof(IAudioEndpointVolume),
                            CLSCTX_ALL,
                            nullptr,
                            reinterpret_cast<void**>(&endpointVolume));
    if (FAILED(hr) || !endpointVolume) goto cleanup;

    hr = endpointVolume->SetMasterVolumeLevelScalar(scalar, nullptr);
    ok = SUCCEEDED(hr);

cleanup:
    if (endpointVolume) endpointVolume->Release();
    if (endpoint) endpoint->Release();
    if (enumerator) enumerator->Release();
    if (shouldUninitialize) CoUninitialize();
    return ok;
}

float endpointVolume(const QString& deviceId, bool* okOut) {
    if (okOut) *okOut = false;
    if (deviceId.isEmpty()) return 0.0f;

    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    const bool shouldUninitialize = SUCCEEDED(hr);
    if (FAILED(hr) && hr != RPC_E_CHANGED_MODE) {
        return 0.0f;
    }

    IMMDeviceEnumerator* enumerator = nullptr;
    IMMDevice* endpoint = nullptr;
    IAudioEndpointVolume* endpointVolumePtr = nullptr;
    float value = 0.0f;

    hr = CoCreateInstance(__uuidof(MMDeviceEnumerator),
                          nullptr,
                          CLSCTX_ALL,
                          __uuidof(IMMDeviceEnumerator),
                          reinterpret_cast<void**>(&enumerator));
    if (FAILED(hr) || !enumerator) goto cleanup;

    {
        const std::wstring wId = deviceId.toStdWString();
        hr = enumerator->GetDevice(wId.c_str(), &endpoint);
    }
    if (FAILED(hr) || !endpoint) goto cleanup;

    hr = endpoint->Activate(__uuidof(IAudioEndpointVolume),
                            CLSCTX_ALL,
                            nullptr,
                            reinterpret_cast<void**>(&endpointVolumePtr));
    if (FAILED(hr) || !endpointVolumePtr) goto cleanup;

    hr = endpointVolumePtr->GetMasterVolumeLevelScalar(&value);
    if (SUCCEEDED(hr) && okOut) *okOut = true;

cleanup:
    if (endpointVolumePtr) endpointVolumePtr->Release();
    if (endpoint) endpoint->Release();
    if (enumerator) enumerator->Release();
    if (shouldUninitialize) CoUninitialize();
    return value;
}
bool setEndpointMute(const QString& deviceId, bool muted) {
    if (deviceId.isEmpty()) return false;

    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    const bool shouldUninitialize = SUCCEEDED(hr);
    if (FAILED(hr) && hr != RPC_E_CHANGED_MODE) return false;

    IMMDeviceEnumerator* enumerator = nullptr;
    IMMDevice* endpoint = nullptr;
    IAudioEndpointVolume* endpointVolumePtr = nullptr;
    bool ok = false;

    hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
                          __uuidof(IMMDeviceEnumerator), reinterpret_cast<void**>(&enumerator));
    if (FAILED(hr) || !enumerator) goto cleanup;

    {
        const std::wstring wId = deviceId.toStdWString();
        hr = enumerator->GetDevice(wId.c_str(), &endpoint);
    }
    if (FAILED(hr) || !endpoint) goto cleanup;

    hr = endpoint->Activate(__uuidof(IAudioEndpointVolume), CLSCTX_ALL, nullptr,
                            reinterpret_cast<void**>(&endpointVolumePtr));
    if (FAILED(hr) || !endpointVolumePtr) goto cleanup;

    hr = endpointVolumePtr->SetMute(muted ? TRUE : FALSE, nullptr);
    ok = SUCCEEDED(hr);

cleanup:
    if (endpointVolumePtr) endpointVolumePtr->Release();
    if (endpoint) endpoint->Release();
    if (enumerator) enumerator->Release();
    if (shouldUninitialize) CoUninitialize();
    return ok;
}

bool endpointMute(const QString& deviceId, bool* okOut) {
    if (okOut) *okOut = false;
    if (deviceId.isEmpty()) return false;

    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    const bool shouldUninitialize = SUCCEEDED(hr);
    if (FAILED(hr) && hr != RPC_E_CHANGED_MODE) return false;

    IMMDeviceEnumerator* enumerator = nullptr;
    IMMDevice* endpoint = nullptr;
    IAudioEndpointVolume* endpointVolumePtr = nullptr;
    BOOL muted = FALSE;

    hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
                          __uuidof(IMMDeviceEnumerator), reinterpret_cast<void**>(&enumerator));
    if (FAILED(hr) || !enumerator) goto cleanup;

    {
        const std::wstring wId = deviceId.toStdWString();
        hr = enumerator->GetDevice(wId.c_str(), &endpoint);
    }
    if (FAILED(hr) || !endpoint) goto cleanup;

    hr = endpoint->Activate(__uuidof(IAudioEndpointVolume), CLSCTX_ALL, nullptr,
                            reinterpret_cast<void**>(&endpointVolumePtr));
    if (FAILED(hr) || !endpointVolumePtr) goto cleanup;

    hr = endpointVolumePtr->GetMute(&muted);
    if (SUCCEEDED(hr) && okOut) *okOut = true;

cleanup:
    if (endpointVolumePtr) endpointVolumePtr->Release();
    if (endpoint) endpoint->Release();
    if (enumerator) enumerator->Release();
    if (shouldUninitialize) CoUninitialize();
    return (muted != FALSE);
}

float endpointPeakLevel(const QString& deviceId) {
    if (deviceId.isEmpty()) return 0.0f;

    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    const bool shouldUninitialize = SUCCEEDED(hr);
    if (FAILED(hr) && hr != RPC_E_CHANGED_MODE) return 0.0f;

    IMMDeviceEnumerator* enumerator = nullptr;
    IMMDevice* endpoint = nullptr;
    IAudioMeterInformation* meter = nullptr;
    float peak = 0.0f;

    hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
                          __uuidof(IMMDeviceEnumerator), reinterpret_cast<void**>(&enumerator));
    if (FAILED(hr) || !enumerator) goto cleanup;

    {
        const std::wstring wId = deviceId.toStdWString();
        hr = enumerator->GetDevice(wId.c_str(), &endpoint);
    }
    if (FAILED(hr) || !endpoint) goto cleanup;

    hr = endpoint->Activate(IID_IAudioMeterInformation, CLSCTX_ALL, nullptr,
                            reinterpret_cast<void**>(&meter));
    if (FAILED(hr) || !meter) goto cleanup;

    hr = meter->GetPeakValue(&peak);
    if (FAILED(hr)) peak = 0.0f;

cleanup:
    if (meter) meter->Release();
    if (endpoint) endpoint->Release();
    if (enumerator) enumerator->Release();
    if (shouldUninitialize) CoUninitialize();
    return qBound(0.0f, peak, 1.0f);
}
#else
QStringList activeAppNamesForEndpoint(const QString&) { return {}; }
bool setDefaultEndpointForAllRoles(const QString&) { return false; }
bool setEndpointVolume(const QString&, float) { return false; }
float endpointVolume(const QString&, bool* okOut) {
    if (okOut) *okOut = false;
    return 0.0f;
}
bool setEndpointMute(const QString&, bool) { return false; }
bool endpointMute(const QString&, bool* okOut) {
    if (okOut) *okOut = false;
    return false;
}
float endpointPeakLevel(const QString&) { return 0.0f; }
#endif

#if defined(Q_OS_WIN)
class DeviceNotificationClient : public IMMNotificationClient {
public:
    explicit DeviceNotificationClient(AudioMasterWindow* window)
        : m_window(window), m_ref(1) {}
    virtual ~DeviceNotificationClient() = default;

    ULONG STDMETHODCALLTYPE AddRef() override { return InterlockedIncrement(&m_ref); }
    ULONG STDMETHODCALLTYPE Release() override {
        ULONG ref = InterlockedDecrement(&m_ref);
        if (ref == 0) delete this;
        return ref;
    }
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject) override {
        if (riid == __uuidof(IUnknown) || riid == __uuidof(IMMNotificationClient)) {
            *ppvObject = static_cast<IMMNotificationClient*>(this);
            AddRef();
            return S_OK;
        }
        *ppvObject = nullptr;
        return E_NOINTERFACE;
    }

    HRESULT STDMETHODCALLTYPE OnDeviceStateChanged(LPCWSTR, DWORD) override { scheduleRefresh(); return S_OK; }
    HRESULT STDMETHODCALLTYPE OnDeviceAdded(LPCWSTR) override { scheduleRefresh(); return S_OK; }
    HRESULT STDMETHODCALLTYPE OnDeviceRemoved(LPCWSTR) override { scheduleRefresh(); return S_OK; }
    HRESULT STDMETHODCALLTYPE OnDefaultDeviceChanged(EDataFlow, ERole, LPCWSTR) override { scheduleRefresh(); return S_OK; }
    HRESULT STDMETHODCALLTYPE OnPropertyValueChanged(LPCWSTR, const PROPERTYKEY) override { return S_OK; }

private:
    void scheduleRefresh() {
        if (m_window) {
            QMetaObject::invokeMethod(m_window, "refreshDevices", Qt::QueuedConnection);
        }
    }
    AudioMasterWindow* m_window;
    LONG m_ref;
};
#endif

struct AppSessionInfo {
    QString name;
    quint32 pid{0};
    float volume{1.0f};
    bool muted{false};
    QString deviceId;
};

#if defined(Q_OS_WIN)
QVector<AppSessionInfo> appSessionsForEndpoint(const QString& deviceId) {
    QVector<AppSessionInfo> result;
    if (deviceId.isEmpty()) return result;

    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    const bool shouldUninitialize = SUCCEEDED(hr);
    if (FAILED(hr) && hr != RPC_E_CHANGED_MODE) return result;

    IMMDeviceEnumerator* enumerator = nullptr;
    IMMDevice* endpoint = nullptr;
    IAudioSessionManager2* sessionManager = nullptr;
    IAudioSessionEnumerator* sessions = nullptr;
    int count = 0;
    QSet<DWORD> seenPids;

    hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
                          __uuidof(IMMDeviceEnumerator), reinterpret_cast<void**>(&enumerator));
    if (FAILED(hr) || !enumerator) goto cleanup;

    {
        const std::wstring wId = deviceId.toStdWString();
        hr = enumerator->GetDevice(wId.c_str(), &endpoint);
    }
    if (FAILED(hr) || !endpoint) goto cleanup;

    hr = endpoint->Activate(__uuidof(IAudioSessionManager2), CLSCTX_ALL, nullptr,
                            reinterpret_cast<void**>(&sessionManager));
    if (FAILED(hr) || !sessionManager) goto cleanup;

    hr = sessionManager->GetSessionEnumerator(&sessions);
    if (FAILED(hr) || !sessions) goto cleanup;

    hr = sessions->GetCount(&count);
    if (FAILED(hr) || count <= 0) goto cleanup;

    for (int i = 0; i < count; ++i) {
        IAudioSessionControl* control = nullptr;
        hr = sessions->GetSession(i, &control);
        if (FAILED(hr) || !control) continue;

        AudioSessionState state = AudioSessionStateInactive;
        control->GetState(&state);
        if (state != AudioSessionStateActive) {
            control->Release();
            continue;
        }

        IAudioSessionControl2* control2 = nullptr;
        hr = control->QueryInterface(__uuidof(IAudioSessionControl2), reinterpret_cast<void**>(&control2));
        if (FAILED(hr) || !control2) {
            control->Release();
            continue;
        }

        DWORD pid = 0;
        control2->GetProcessId(&pid);

        if (seenPids.contains(pid)) {
            control2->Release();
            control->Release();
            continue;
        }
        seenPids.insert(pid);

        AppSessionInfo info;
        info.pid = pid;
        info.deviceId = deviceId;

        LPWSTR displayNameRaw = nullptr;
        if (SUCCEEDED(control2->GetDisplayName(&displayNameRaw)) && displayNameRaw) {
            QString dn = QString::fromWCharArray(displayNameRaw).trimmed();
            CoTaskMemFree(displayNameRaw);
            if (!dn.isEmpty()) info.name = dn;
        }
        if (info.name.isEmpty()) {
            info.name = processNameFromId(pid);
        }

        ISimpleAudioVolume* simpleVol = nullptr;
        hr = control->QueryInterface(__uuidof(ISimpleAudioVolume), reinterpret_cast<void**>(&simpleVol));
        if (SUCCEEDED(hr) && simpleVol) {
            simpleVol->GetMasterVolume(&info.volume);
            BOOL m = FALSE;
            simpleVol->GetMute(&m);
            info.muted = (m != FALSE);
            simpleVol->Release();
        }

        control2->Release();
        control->Release();
        result.push_back(info);
    }

cleanup:
    if (sessions) sessions->Release();
    if (sessionManager) sessionManager->Release();
    if (endpoint) endpoint->Release();
    if (enumerator) enumerator->Release();
    if (shouldUninitialize) CoUninitialize();

    std::sort(result.begin(), result.end(), [](const AppSessionInfo& a, const AppSessionInfo& b) {
        return a.name.compare(b.name, Qt::CaseInsensitive) < 0;
    });
    return result;
}

bool setSessionVolume(const QString& deviceId, DWORD targetPid, float volume) {
    if (deviceId.isEmpty()) return false;

    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    const bool shouldUninitialize = SUCCEEDED(hr);
    if (FAILED(hr) && hr != RPC_E_CHANGED_MODE) return false;

    IMMDeviceEnumerator* enumerator = nullptr;
    IMMDevice* endpoint = nullptr;
    IAudioSessionManager2* sessionManager = nullptr;
    IAudioSessionEnumerator* sessions = nullptr;
    int count = 0;
    bool ok = false;

    hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
                          __uuidof(IMMDeviceEnumerator), reinterpret_cast<void**>(&enumerator));
    if (FAILED(hr) || !enumerator) goto cleanup;

    {
        const std::wstring wId = deviceId.toStdWString();
        hr = enumerator->GetDevice(wId.c_str(), &endpoint);
    }
    if (FAILED(hr) || !endpoint) goto cleanup;

    hr = endpoint->Activate(__uuidof(IAudioSessionManager2), CLSCTX_ALL, nullptr,
                            reinterpret_cast<void**>(&sessionManager));
    if (FAILED(hr) || !sessionManager) goto cleanup;

    hr = sessionManager->GetSessionEnumerator(&sessions);
    if (FAILED(hr) || !sessions) goto cleanup;

    hr = sessions->GetCount(&count);
    if (FAILED(hr) || count <= 0) goto cleanup;

    for (int i = 0; i < count; ++i) {
        IAudioSessionControl* control = nullptr;
        hr = sessions->GetSession(i, &control);
        if (FAILED(hr) || !control) continue;

        IAudioSessionControl2* control2 = nullptr;
        hr = control->QueryInterface(__uuidof(IAudioSessionControl2), reinterpret_cast<void**>(&control2));
        if (SUCCEEDED(hr) && control2) {
            DWORD pid = 0;
            control2->GetProcessId(&pid);
            if (pid == targetPid) {
                ISimpleAudioVolume* simpleVol = nullptr;
                hr = control->QueryInterface(__uuidof(ISimpleAudioVolume), reinterpret_cast<void**>(&simpleVol));
                if (SUCCEEDED(hr) && simpleVol) {
                    ok = SUCCEEDED(simpleVol->SetMasterVolume(qBound(0.0f, volume, 1.0f), nullptr));
                    simpleVol->Release();
                }
            }
            control2->Release();
        }
        control->Release();
        if (ok) break;
    }

cleanup:
    if (sessions) sessions->Release();
    if (sessionManager) sessionManager->Release();
    if (endpoint) endpoint->Release();
    if (enumerator) enumerator->Release();
    if (shouldUninitialize) CoUninitialize();
    return ok;
}

bool setSessionMute(const QString& deviceId, DWORD targetPid, bool muted) {
    if (deviceId.isEmpty()) return false;

    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    const bool shouldUninitialize = SUCCEEDED(hr);
    if (FAILED(hr) && hr != RPC_E_CHANGED_MODE) return false;

    IMMDeviceEnumerator* enumerator = nullptr;
    IMMDevice* endpoint = nullptr;
    IAudioSessionManager2* sessionManager = nullptr;
    IAudioSessionEnumerator* sessions = nullptr;
    int count = 0;
    bool ok = false;

    hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
                          __uuidof(IMMDeviceEnumerator), reinterpret_cast<void**>(&enumerator));
    if (FAILED(hr) || !enumerator) goto cleanup;

    {
        const std::wstring wId = deviceId.toStdWString();
        hr = enumerator->GetDevice(wId.c_str(), &endpoint);
    }
    if (FAILED(hr) || !endpoint) goto cleanup;

    hr = endpoint->Activate(__uuidof(IAudioSessionManager2), CLSCTX_ALL, nullptr,
                            reinterpret_cast<void**>(&sessionManager));
    if (FAILED(hr) || !sessionManager) goto cleanup;

    hr = sessionManager->GetSessionEnumerator(&sessions);
    if (FAILED(hr) || !sessions) goto cleanup;

    hr = sessions->GetCount(&count);
    if (FAILED(hr) || count <= 0) goto cleanup;

    for (int i = 0; i < count; ++i) {
        IAudioSessionControl* control = nullptr;
        hr = sessions->GetSession(i, &control);
        if (FAILED(hr) || !control) continue;

        IAudioSessionControl2* control2 = nullptr;
        hr = control->QueryInterface(__uuidof(IAudioSessionControl2), reinterpret_cast<void**>(&control2));
        if (SUCCEEDED(hr) && control2) {
            DWORD pid = 0;
            control2->GetProcessId(&pid);
            if (pid == targetPid) {
                ISimpleAudioVolume* simpleVol = nullptr;
                hr = control->QueryInterface(__uuidof(ISimpleAudioVolume), reinterpret_cast<void**>(&simpleVol));
                if (SUCCEEDED(hr) && simpleVol) {
                    ok = SUCCEEDED(simpleVol->SetMute(muted ? TRUE : FALSE, nullptr));
                    simpleVol->Release();
                }
            }
            control2->Release();
        }
        control->Release();
        if (ok) break;
    }

cleanup:
    if (sessions) sessions->Release();
    if (sessionManager) sessionManager->Release();
    if (endpoint) endpoint->Release();
    if (enumerator) enumerator->Release();
    if (shouldUninitialize) CoUninitialize();
    return ok;
}

#else
QVector<AppSessionInfo> appSessionsForEndpoint(const QString&) { return {}; }
bool setSessionVolume(const QString&, quint32, float) { return false; }
bool setSessionMute(const QString&, quint32, bool) { return false; }
#endif
}

AudioMasterWindow::AudioMasterWindow(QWidget* parent)
    : QWidget(parent) {
    setWindowTitle(QStringLiteral("AudioMaster"));
    setWindowIcon(QIcon(QStringLiteral(":/icons/modules/audiomaster.svg")));
    setMinimumSize(1120, 640);

    channels_ = {
        ChannelWidgets{QStringLiteral("master"), QStringLiteral("Master"), false},
        ChannelWidgets{QStringLiteral("game"), QStringLiteral("Game"), false},
        ChannelWidgets{QStringLiteral("chat"), QStringLiteral("Chat"), false},
        ChannelWidgets{QStringLiteral("media"), QStringLiteral("Media"), false},
        ChannelWidgets{QStringLiteral("mic"), QStringLiteral("Mic"), true},
    };

    buildUi();

    m_themeListener_ = new wintools::themes::ThemeListener(this);
    connect(m_themeListener_, &wintools::themes::ThemeListener::themeChanged,
            this, &AudioMasterWindow::onThemeChanged);

    usageRefreshTimer_ = new QTimer(this);
    usageRefreshTimer_->setInterval(2000);
    connect(usageRefreshTimer_, &QTimer::timeout, this, [this]() {
        refreshAppUsage();
        if (pages_->currentWidget() == appsPage_)
            refreshAppSessions();
    });
    usageRefreshTimer_->start();

    meterTimer_ = new QTimer(this);
    meterTimer_->setInterval(50);
    connect(meterTimer_, &QTimer::timeout, this, [this]() {
        if (pages_->currentWidget() != mixerPage_) return;
        for (auto& ch : channels_) {
            if (!ch.vuMeter) continue;
            const QString devId = linkedDeviceId(ch);
            float peak = devId.isEmpty() ? 0.0f : endpointPeakLevel(devId);
            ch.vuMeter->setLevel(peak);
        }
    });
    meterTimer_->start();

    m_palette = wintools::themes::ThemeHelper::currentPalette();
    applyTheme(m_palette);
    refreshDevices();

#if defined(Q_OS_WIN)

    [this]() {
        HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
        if (FAILED(hr) && hr != RPC_E_CHANGED_MODE) return;

        IMMDeviceEnumerator* enumerator = nullptr;
        hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
                              __uuidof(IMMDeviceEnumerator), reinterpret_cast<void**>(&enumerator));
        if (FAILED(hr) || !enumerator) return;

        auto* client = new DeviceNotificationClient(this);
        hr = enumerator->RegisterEndpointNotificationCallback(client);
        if (SUCCEEDED(hr)) {
            m_deviceNotifier = client;
            m_deviceEnumerator = enumerator;
        } else {
            client->Release();
            enumerator->Release();
        }
    }();
#endif

    wintools::logger::Logger::log(QString::fromUtf8(LogSource),
                                  wintools::logger::Severity::Pass,
                                  QStringLiteral("AudioMaster initialized."));
}

AudioMasterWindow::~AudioMasterWindow() {
#if defined(Q_OS_WIN)
    if (m_deviceEnumerator && m_deviceNotifier) {
        auto* enumerator = static_cast<IMMDeviceEnumerator*>(m_deviceEnumerator);
        auto* client = static_cast<DeviceNotificationClient*>(m_deviceNotifier);
        enumerator->UnregisterEndpointNotificationCallback(client);
        client->Release();
        enumerator->Release();
        m_deviceNotifier = nullptr;
        m_deviceEnumerator = nullptr;
    }
#endif
}

void AudioMasterWindow::buildUi() {
    auto* rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(12, 12, 12, 12);
    rootLayout->setSpacing(10);

    auto* topBar = new QHBoxLayout();
    topBar->setSpacing(8);

    mixerTabButton_ = new QPushButton(QStringLiteral("Mixer"), this);
    mixerTabButton_->setCheckable(true);
    mixerTabButton_->setChecked(true);
    routingTabButton_ = new QPushButton(QStringLiteral("Routing"), this);
    routingTabButton_->setCheckable(true);
    appsTabButton_ = new QPushButton(QStringLiteral("Apps"), this);
    appsTabButton_->setCheckable(true);
    virtualTabButton_ = new QPushButton(QStringLiteral("Virtual Devices"), this);
    virtualTabButton_->setCheckable(true);
    refreshButton_ = new QPushButton(QStringLiteral("Refresh"), this);

    topBar->addWidget(mixerTabButton_);
    topBar->addWidget(routingTabButton_);
    topBar->addWidget(appsTabButton_);
    topBar->addWidget(virtualTabButton_);
    topBar->addStretch();
    topBar->addWidget(refreshButton_);

    pages_ = new QStackedWidget(this);
    mixerPage_ = new QWidget(this);
    routingPage_ = new QWidget(this);
    appsPage_ = new QWidget(this);
    virtualDevicesWidget_ = new VirtualDevicesWidget(this);
    pages_->addWidget(mixerPage_);
    pages_->addWidget(routingPage_);
    pages_->addWidget(appsPage_);
    pages_->addWidget(virtualDevicesWidget_);

    buildMixerPage();
    buildRoutingPage();
    buildAppsPage();

    rootLayout->addLayout(topBar);
    rootLayout->addWidget(pages_);

    auto uncheckAll = [this]() {
        mixerTabButton_->setChecked(false);
        routingTabButton_->setChecked(false);
        appsTabButton_->setChecked(false);
        virtualTabButton_->setChecked(false);
    };
    connect(mixerTabButton_, &QPushButton::clicked, this, [this, uncheckAll]() {
        uncheckAll();
        mixerTabButton_->setChecked(true);
        pages_->setCurrentWidget(mixerPage_);
    });
    connect(routingTabButton_, &QPushButton::clicked, this, [this, uncheckAll]() {
        uncheckAll();
        routingTabButton_->setChecked(true);
        pages_->setCurrentWidget(routingPage_);
    });
    connect(appsTabButton_, &QPushButton::clicked, this, [this, uncheckAll]() {
        uncheckAll();
        appsTabButton_->setChecked(true);
        pages_->setCurrentWidget(appsPage_);
        refreshAppSessions();
    });
    connect(virtualTabButton_, &QPushButton::clicked, this, [this, uncheckAll]() {
        uncheckAll();
        virtualTabButton_->setChecked(true);
        pages_->setCurrentWidget(virtualDevicesWidget_);
        virtualDevicesWidget_->populateVirtualDeviceControls();
    });
    connect(refreshButton_, &QPushButton::clicked, this, &AudioMasterWindow::refreshDevices);
}

void AudioMasterWindow::buildMixerPage() {
    auto* layout = new QHBoxLayout(mixerPage_);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(8);

    for (int i = 0; i < static_cast<int>(channels_.size()); ++i) {
        auto& channel = channels_[static_cast<size_t>(i)];

        channel.card = new QGroupBox(channel.title.toUpper(), mixerPage_);
        auto* cardLayout = new QVBoxLayout(channel.card);
        cardLayout->setContentsMargins(10, 14, 10, 10);
        cardLayout->setSpacing(8);

        channel.linkedDeviceLabel = new QLabel(QStringLiteral("Not linked"), channel.card);
        channel.linkedDeviceLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        channel.linkedDeviceLabel->setWordWrap(true);

        channel.slider = new QSlider(Qt::Vertical, channel.card);
        channel.slider->setRange(0, 100);
        channel.slider->setValue(50);
        channel.slider->setMinimumHeight(250);

        channel.valueLabel = new QLabel(QStringLiteral("50%"), channel.card);
        channel.valueLabel->setAlignment(Qt::AlignCenter);

        channel.muteButton = new QPushButton(QStringLiteral("\xF0\x9F\x94\x8A"), channel.card);
        channel.muteButton->setFixedSize(36, 36);
        channel.muteButton->setCheckable(true);
        channel.muteButton->setChecked(false);
        channel.muteButton->setToolTip(QStringLiteral("Mute"));

        channel.activateButton = new QPushButton(QStringLiteral("Use in Windows"), channel.card);

        channel.appsLabel = new QLabel(QStringLiteral("Apps: none"), channel.card);
        channel.appsLabel->setWordWrap(true);
        channel.appsLabel->setMinimumHeight(48);
        channel.appsLabel->setAlignment(Qt::AlignTop | Qt::AlignLeft);

        channel.vuMeter = new VuMeterWidget(channel.card);

        auto* sliderRow = new QHBoxLayout;
        sliderRow->setSpacing(6);
        sliderRow->addStretch();
        sliderRow->addWidget(channel.slider);
        sliderRow->addWidget(channel.vuMeter);
        sliderRow->addStretch();

        cardLayout->addWidget(channel.linkedDeviceLabel);
        cardLayout->addLayout(sliderRow, 1);
        cardLayout->addWidget(channel.valueLabel);
        cardLayout->addWidget(channel.muteButton, 0, Qt::AlignHCenter);
        cardLayout->addWidget(channel.activateButton);
        cardLayout->addWidget(channel.appsLabel);

        layout->addWidget(channel.card, 1);

        connect(channel.slider, &QSlider::valueChanged, this,
                [this, i](int value) { onSliderValueChanged(i, value); });
        connect(channel.activateButton, &QPushButton::clicked, this,
                [this, i]() { applyChannelActivation(i); });
        connect(channel.muteButton, &QPushButton::clicked, this, [this, i](bool checked) {
            auto& ch = channels_[static_cast<size_t>(i)];
            const QString devId = linkedDeviceId(ch);
            if (devId.isEmpty()) return;
            const bool ok = setEndpointMute(devId, checked);
            if (ok && ch.muteButton) {
                ch.muteButton->setText(checked ? QStringLiteral("\xF0\x9F\x94\x87") : QStringLiteral("\xF0\x9F\x94\x8A"));
                ch.muteButton->setToolTip(checked ? QStringLiteral("Unmute") : QStringLiteral("Mute"));
            }
        });
    }
}

void AudioMasterWindow::buildRoutingPage() {
    auto* layout = new QVBoxLayout(routingPage_);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(12);

    routingHintLabel_ = new QLabel(
        QStringLiteral("Link each AudioMaster channel to a real device, then pick which devices are in hotkey cycle."),
        routingPage_);
    routingHintLabel_->setWordWrap(true);

    auto* formCard = new QGroupBox(QStringLiteral("Channel Routing"), routingPage_);
    auto* form = new QFormLayout(formCard);
    form->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);
    form->setHorizontalSpacing(14);
    form->setVerticalSpacing(10);

    for (int i = 0; i < static_cast<int>(channels_.size()); ++i) {
        auto& channel = channels_[static_cast<size_t>(i)];
        channel.routingCombo = new QComboBox(formCard);
        channel.routingCombo->setMinimumWidth(360);
        form->addRow(QStringLiteral("%1 channel").arg(channel.title), channel.routingCombo);

        connect(channel.routingCombo,
                qOverload<int>(&QComboBox::currentIndexChanged),
                this,
                [this, i](int) { onRoutingChanged(i); });
    }

    auto* cycleRow = new QHBoxLayout();
    auto* outputCycleCard = new QGroupBox(QStringLiteral("Output Devices In Rotation"), routingPage_);
    auto* inputCycleCard = new QGroupBox(QStringLiteral("Input Devices In Rotation"), routingPage_);

    auto* outputCycleLayout = new QVBoxLayout(outputCycleCard);
    auto* inputCycleLayout = new QVBoxLayout(inputCycleCard);

    outputCycleList_ = new QListWidget(outputCycleCard);
    inputCycleList_ = new QListWidget(inputCycleCard);
    outputCycleList_->setSelectionMode(QAbstractItemView::NoSelection);
    inputCycleList_->setSelectionMode(QAbstractItemView::NoSelection);

    outputCycleLayout->addWidget(outputCycleList_);
    inputCycleLayout->addWidget(inputCycleList_);

    cycleRow->addWidget(outputCycleCard, 1);
    cycleRow->addWidget(inputCycleCard, 1);

    connect(outputCycleList_, &QListWidget::itemChanged, this, [this](QListWidgetItem* item) {
        if (updatingCycleLists_ || !item) return;
        const QString id = item->data(Qt::UserRole).toString();
        if (id.isEmpty()) return;
        setDeviceIncludedInCycle(settings_, false, id, item->checkState() == Qt::Checked);
    });
    connect(inputCycleList_, &QListWidget::itemChanged, this, [this](QListWidgetItem* item) {
        if (updatingCycleLists_ || !item) return;
        const QString id = item->data(Qt::UserRole).toString();
        if (id.isEmpty()) return;
        setDeviceIncludedInCycle(settings_, true, id, item->checkState() == Qt::Checked);
    });

    layout->addWidget(routingHintLabel_);
    layout->addWidget(formCard);
    layout->addLayout(cycleRow, 1);
}

void AudioMasterWindow::buildAppsPage() {
    auto* layout = new QVBoxLayout(appsPage_);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    auto* hintLabel = new QLabel(
        QStringLiteral("Per-application volume control. Adjust the volume or mute individual apps."),
        appsPage_);
    hintLabel->setWordWrap(true);
    hintLabel->setContentsMargins(4, 4, 4, 8);

    appsScrollArea_ = new QScrollArea(appsPage_);
    appsScrollArea_->setWidgetResizable(true);
    appsScrollArea_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    appsScrollArea_->setFrameShape(QFrame::NoFrame);

    appsScrollContent_ = new QWidget(appsScrollArea_);
    appsScrollLayout_ = new QVBoxLayout(appsScrollContent_);
    appsScrollLayout_->setContentsMargins(0, 0, 0, 0);
    appsScrollLayout_->setSpacing(6);
    appsScrollLayout_->addStretch();

    appsScrollArea_->setWidget(appsScrollContent_);

    layout->addWidget(hintLabel);
    layout->addWidget(appsScrollArea_, 1);
}

void AudioMasterWindow::refreshAppSessions() {

    for (const auto& row : appRows_) {
        if (row.slider && row.slider->isSliderDown()) return;
    }

    QSet<QString> seenDevices;
    QVector<AppSessionInfo> allSessions;

    for (const auto& channel : channels_) {
        const QString devId = linkedDeviceId(channel);
        if (devId.isEmpty() || seenDevices.contains(devId)) continue;
        seenDevices.insert(devId);
        allSessions.append(appSessionsForEndpoint(devId));
    }

    for (auto& row : appRows_) {
        if (row.container) {
            appsScrollLayout_->removeWidget(row.container);
            row.container->deleteLater();
        }
    }
    appRows_.clear();

    for (const auto& session : allSessions) {
        AppRow row;
        row.deviceId = session.deviceId;
        row.pid = session.pid;

        row.container = new QWidget(appsScrollContent_);
        row.container->setObjectName(QStringLiteral("appRow"));
        auto* rowLayout = new QHBoxLayout(row.container);
        rowLayout->setContentsMargins(12, 8, 12, 8);
        rowLayout->setSpacing(12);

        row.nameLabel = new QLabel(session.name, row.container);
        row.nameLabel->setMinimumWidth(180);
        row.nameLabel->setMaximumWidth(280);
        row.nameLabel->setWordWrap(true);
        auto nameFont = row.nameLabel->font();
        nameFont.setWeight(QFont::DemiBold);
        row.nameLabel->setFont(nameFont);

        row.muteButton = new QPushButton(row.container);
        row.muteButton->setFixedSize(32, 32);
        row.muteButton->setCheckable(true);
        row.muteButton->setChecked(session.muted);
        row.muteButton->setText(session.muted ? QStringLiteral("\xF0\x9F\x94\x87") : QStringLiteral("\xF0\x9F\x94\x8A"));
        row.muteButton->setToolTip(session.muted ? QStringLiteral("Unmute") : QStringLiteral("Mute"));

        row.slider = new QSlider(Qt::Horizontal, row.container);
        row.slider->setRange(0, 100);
        const int percent = qBound(0, static_cast<int>(session.volume * 100.0f + 0.5f), 100);
        row.slider->setValue(percent);
        row.slider->setMinimumWidth(200);

        row.valueLabel = new QLabel(QStringLiteral("%1%").arg(percent), row.container);
        row.valueLabel->setFixedWidth(40);
        row.valueLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

        rowLayout->addWidget(row.nameLabel);
        rowLayout->addWidget(row.muteButton);
        rowLayout->addWidget(row.slider, 1);
        rowLayout->addWidget(row.valueLabel);

        const int insertPos = appsScrollLayout_->count() > 0 ? appsScrollLayout_->count() - 1 : 0;
        appsScrollLayout_->insertWidget(insertPos, row.container);

        const QString devId = session.deviceId;
        const quint32 pid = session.pid;
        connect(row.slider, &QSlider::valueChanged, this, [this, devId, pid](int value) {

            for (auto& r : appRows_) {
                if (r.pid == pid && r.deviceId == devId && r.valueLabel) {
                    r.valueLabel->setText(QStringLiteral("%1%").arg(value));
                    break;
                }
            }
            setSessionVolume(devId, static_cast<DWORD>(pid), qBound(0.0f, value / 100.0f, 1.0f));
        });

        connect(row.muteButton, &QPushButton::clicked, this, [this, devId, pid](bool checked) {
            setSessionMute(devId, static_cast<DWORD>(pid), checked);
            for (auto& r : appRows_) {
                if (r.pid == pid && r.deviceId == devId && r.muteButton) {
                    r.muteButton->setText(checked ? QStringLiteral("\xF0\x9F\x94\x87") : QStringLiteral("\xF0\x9F\x94\x8A"));
                    r.muteButton->setToolTip(checked ? QStringLiteral("Unmute") : QStringLiteral("Mute"));
                    break;
                }
            }
        });

        appRows_.push_back(row);
    }

    if (appRows_.empty()) {
        auto* placeholder = new QWidget(appsScrollContent_);
        placeholder->setObjectName(QStringLiteral("appRow"));
        auto* placeholderLayout = new QHBoxLayout(placeholder);
        auto* placeholderLabel = new QLabel(QStringLiteral("No active audio sessions detected."), placeholder);
        placeholderLabel->setAlignment(Qt::AlignCenter);
        placeholderLayout->addWidget(placeholderLabel);

        AppRow row;
        row.container = placeholder;
        const int insertPos = appsScrollLayout_->count() > 0 ? appsScrollLayout_->count() - 1 : 0;
        appsScrollLayout_->insertWidget(insertPos, placeholder);
        appRows_.push_back(row);
    }
}

void AudioMasterWindow::onThemeChanged(bool) {
    applyTheme(wintools::themes::ThemeHelper::currentPalette());
}

void AudioMasterWindow::applyTheme(const wintools::themes::ThemePalette& palette) {
    m_palette = palette;

    const QString supplement = QStringLiteral(
        "QWidget { background-color: %1; color: %2; }"
        "QGroupBox { border: 1px solid %3; border-radius: 10px; margin-top: 10px; padding-top: 10px; background-color: %4; }"
        "QGroupBox::title { subcontrol-origin: margin; left: 8px; padding: 0 4px; color: %6; font-weight: 700; }"
        "QPushButton { background-color: %4; border: 1px solid %3; color: %2; padding: 6px 12px; border-radius: 6px; }"
        "QPushButton:hover { background-color: %5; }"
        "QPushButton:checked { background-color: %6; border-color: %6; color: %1; }"
        "QComboBox, QListWidget { background-color: %4; border: 1px solid %3; border-radius: 6px; padding: 6px; color: %2; }"
        "QListWidget::item { padding: 4px; }"
        "QSlider::groove:vertical { background: %3; width: 8px; border-radius: 4px; }"
        "QSlider::handle:vertical { background: %6; border: 1px solid %6; height: 14px; margin: -4px; border-radius: 4px; }"
        "QSlider::groove:horizontal { background: %3; height: 6px; border-radius: 3px; }"
        "QSlider::handle:horizontal { background: %6; border: 1px solid %6; width: 14px; margin: -5px 0; border-radius: 7px; }"
        "QSlider::sub-page:horizontal { background: %6; border-radius: 3px; }"
        "#appRow { background: %4; border: 1px solid %3; border-radius: 8px; }"
        "QLabel { color: %2; }"
        "QScrollArea { background: transparent; border: none; }"
    ).arg(
        palette.windowBackground.name(),
        palette.foreground.name(),
        palette.cardBorder.name(),
        palette.cardBackground.name(),
        palette.hoverBackground.name(),
        palette.accent.name());

    wintools::themes::ThemeHelper::applyThemeTo(this, supplement);
}

QString AudioMasterWindow::linkedDeviceId(const ChannelWidgets& channel) const {
    return settings_.value(settingKeyForChannel(channel.key)).toString();
}

void AudioMasterWindow::setLinkedDeviceId(const ChannelWidgets& channel, const QString& deviceId) {
    settings_.setValue(settingKeyForChannel(channel.key), deviceId);
}

void AudioMasterWindow::refreshCycleLists() {
    const QVector<DeviceEntry> outputs = outputDevices();
    const QVector<DeviceEntry> inputs = inputDevices();

    updatingCycleLists_ = true;

    auto fillList = [this](QListWidget* list, const QVector<DeviceEntry>& devices, bool isInput) {
        if (!list) return;
        list->clear();
        for (const DeviceEntry& device : devices) {
            auto* item = new QListWidgetItem(device.name, list);
            item->setData(Qt::UserRole, device.id);
            item->setFlags(item->flags() | Qt::ItemIsUserCheckable | Qt::ItemIsEnabled);
            item->setCheckState(isDeviceIncludedInCycle(settings_, isInput, device.id) ? Qt::Checked : Qt::Unchecked);
            list->addItem(item);
        }
        if (list->count() == 0) {
            auto* item = new QListWidgetItem(QStringLiteral("No devices found"), list);
            item->setFlags(Qt::NoItemFlags);
            list->addItem(item);
        }
    };

    fillList(outputCycleList_, outputs, false);
    fillList(inputCycleList_, inputs, true);

    updatingCycleLists_ = false;
}

void AudioMasterWindow::refreshRoutingControls() {
    const QVector<DeviceEntry> outputs = outputDevices();
    const QVector<DeviceEntry> inputs = inputDevices();

    for (auto& channel : channels_) {
        if (!channel.routingCombo) continue;

        const QVector<DeviceEntry>& source = channel.isInput ? inputs : outputs;
        const QString currentLinked = linkedDeviceId(channel);

        {
            QSignalBlocker blocker(channel.routingCombo);
            channel.routingCombo->clear();
            for (const DeviceEntry& device : source) {
                channel.routingCombo->addItem(device.name, device.id);
            }
            if (channel.routingCombo->count() == 0) {
                channel.routingCombo->addItem(QStringLiteral("No device available"), QString());
            }

            int selectedIndex = channel.routingCombo->findData(currentLinked);
            if (selectedIndex < 0 && channel.routingCombo->count() > 0) {
                selectedIndex = 0;
            }
            channel.routingCombo->setCurrentIndex(selectedIndex);
        }

        const QString selectedId = channel.routingCombo->currentData().toString();
        setLinkedDeviceId(channel, selectedId);
        if (channel.linkedDeviceLabel) {
            channel.linkedDeviceLabel->setText(deviceNameForId(source, selectedId));
        }
    }

    refreshCycleLists();
}

void AudioMasterWindow::refreshMixerValues() {
    const QVector<DeviceEntry> outputs = outputDevices();
    const QVector<DeviceEntry> inputs = inputDevices();

    for (auto& channel : channels_) {
        const QString linkedId = linkedDeviceId(channel);

        if (channel.linkedDeviceLabel) {
            channel.linkedDeviceLabel->setText(deviceNameForId(channel.isInput ? inputs : outputs, linkedId));
        }

        bool ok = false;
        const float scalar = endpointVolume(linkedId, &ok);
        const int percent = ok ? qBound(0, static_cast<int>(scalar * 100.0f + 0.5f), 100) : 0;

        if (channel.slider) {
            const QSignalBlocker blocker(channel.slider);
            channel.slider->setEnabled(ok);
            channel.slider->setValue(percent);
        }
        if (channel.valueLabel) {
            channel.valueLabel->setText(ok ? QStringLiteral("%1%").arg(percent) : QStringLiteral("N/A"));
        }

        bool muteOk = false;
        const bool muted = endpointMute(linkedId, &muteOk);
        if (channel.muteButton) {
            channel.muteButton->setEnabled(muteOk);
            const QSignalBlocker muteBlocker(channel.muteButton);
            channel.muteButton->setChecked(muted);
            channel.muteButton->setText(muted ? QStringLiteral("\xF0\x9F\x94\x87") : QStringLiteral("\xF0\x9F\x94\x8A"));
            channel.muteButton->setToolTip(muted ? QStringLiteral("Unmute") : QStringLiteral("Mute"));
        }
    }
}

void AudioMasterWindow::refreshAppUsage() {
    for (auto& channel : channels_) {
        if (!channel.appsLabel) continue;
        const QString linkedId = linkedDeviceId(channel);
        const QStringList apps = activeAppNamesForEndpoint(linkedId);
        if (apps.isEmpty()) {
            channel.appsLabel->setText(QStringLiteral("Apps: none"));
        } else {
            channel.appsLabel->setText(QStringLiteral("Apps: %1").arg(apps.join(QStringLiteral(", "))));
        }
    }
}

void AudioMasterWindow::refreshDevices() {
    refreshRoutingControls();
    refreshMixerValues();
    refreshAppUsage();
    refreshAppSessions();

    wintools::logger::Logger::log(QString::fromUtf8(LogSource),
                                  wintools::logger::Severity::Pass,
                                  QStringLiteral("Refreshed mixer devices."));
}

void AudioMasterWindow::onRoutingChanged(int channelIndex) {
    if (channelIndex < 0 || channelIndex >= static_cast<int>(channels_.size())) return;

    auto& channel = channels_[static_cast<size_t>(channelIndex)];
    if (!channel.routingCombo) return;

    setLinkedDeviceId(channel, channel.routingCombo->currentData().toString());
    refreshMixerValues();
    refreshAppUsage();
}

void AudioMasterWindow::onSliderValueChanged(int channelIndex, int value) {
    if (channelIndex < 0 || channelIndex >= static_cast<int>(channels_.size())) return;

    auto& channel = channels_[static_cast<size_t>(channelIndex)];
    if (channel.valueLabel) {
        channel.valueLabel->setText(QStringLiteral("%1%").arg(value));
    }

    const QString deviceId = linkedDeviceId(channel);
    if (deviceId.isEmpty()) return;

    const bool ok = setEndpointVolume(deviceId, qBound(0.0f, value / 100.0f, 1.0f));
    if (!ok) {
        wintools::logger::Logger::log(QString::fromUtf8(LogSource),
                                      wintools::logger::Severity::Warning,
                                      QStringLiteral("Failed to set volume for channel %1").arg(channel.title),
                                      deviceId);
    }
}

void AudioMasterWindow::applyChannelActivation(int channelIndex) {
    if (channelIndex < 0 || channelIndex >= static_cast<int>(channels_.size())) return;
    const auto& channel = channels_[static_cast<size_t>(channelIndex)];
    const QString deviceId = linkedDeviceId(channel);
    if (deviceId.isEmpty()) return;

    const bool ok = setDefaultEndpointForAllRoles(deviceId);
    wintools::logger::Logger::log(QString::fromUtf8(LogSource),
                                  ok ? wintools::logger::Severity::Pass : wintools::logger::Severity::Warning,
                                  ok
                                      ? QStringLiteral("Activated Windows device for %1").arg(channel.title)
                                      : QStringLiteral("Failed to activate Windows device for %1").arg(channel.title),
                                  deviceId);
}

void rotateLinkedOutputDevice() {
    const QVector<DeviceEntry> outputs = outputDevices();
    if (outputs.isEmpty()) {
        wintools::logger::Logger::log(QString::fromUtf8(LogSource),
                                      wintools::logger::Severity::Warning,
                                      QStringLiteral("No output devices found for rotation."));
        return;
    }

    QSettings settings(QStringLiteral("wintools"), QStringLiteral("audiomaster"));
    const QVector<DeviceEntry> cycleDevices = enabledCycleDevices(settings, outputs, false);
    if (cycleDevices.isEmpty()) {
        wintools::logger::Logger::log(QString::fromUtf8(LogSource),
                                      wintools::logger::Severity::Warning,
                                      QStringLiteral("No output devices enabled in cycle."));
        return;
    }

    const QString current = settings.value(settingKeyForChannel(QStringLiteral("master"))).toString();
    const QString next = nextDeviceId(cycleDevices, current);
    if (next.isEmpty()) return;

    for (const char* key : OutputChannelKeys) {
        settings.setValue(settingKeyForChannel(QString::fromUtf8(key)), next);
    }

    setDefaultEndpointForAllRoles(next);

    wintools::logger::Logger::log(QString::fromUtf8(LogSource),
                                  wintools::logger::Severity::Pass,
                                  QStringLiteral("Rotated output link to %1").arg(deviceNameForId(outputs, next)),
                                  next);
}

void rotateLinkedInputDevice() {
    const QVector<DeviceEntry> inputs = inputDevices();
    if (inputs.isEmpty()) {
        wintools::logger::Logger::log(QString::fromUtf8(LogSource),
                                      wintools::logger::Severity::Warning,
                                      QStringLiteral("No input devices found for rotation."));
        return;
    }

    QSettings settings(QStringLiteral("wintools"), QStringLiteral("audiomaster"));
    const QVector<DeviceEntry> cycleDevices = enabledCycleDevices(settings, inputs, true);
    if (cycleDevices.isEmpty()) {
        wintools::logger::Logger::log(QString::fromUtf8(LogSource),
                                      wintools::logger::Severity::Warning,
                                      QStringLiteral("No input devices enabled in cycle."));
        return;
    }

    const QString current = settings.value(settingKeyForChannel(QString::fromUtf8(MicChannelKey))).toString();
    const QString next = nextDeviceId(cycleDevices, current);
    if (next.isEmpty()) return;

    settings.setValue(settingKeyForChannel(QString::fromUtf8(MicChannelKey)), next);
    setDefaultEndpointForAllRoles(next);

    wintools::logger::Logger::log(QString::fromUtf8(LogSource),
                                  wintools::logger::Severity::Pass,
                                  QStringLiteral("Rotated input link to %1").arg(deviceNameForId(inputs, next)),
                                  next);
}

}
