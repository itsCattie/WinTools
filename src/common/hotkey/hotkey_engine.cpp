#include "common/hotkey/hotkey_engine.hpp"

#include "logger/logger.hpp"

#include <QCoreApplication>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

namespace wintools::hotkeys {

namespace {
constexpr const char* LogSource = "HotkeyEngine";
}

HotkeyEngine::HotkeyEngine(QObject* parent)
    : QObject(parent) {
    qApp->installNativeEventFilter(this);
    wintools::logger::Logger::log(LogSource, wintools::logger::Severity::Pass,
                                  "Hotkey engine started.");
}

HotkeyEngine::~HotkeyEngine() {
    unregisterAll();
    qApp->removeNativeEventFilter(this);
}

void HotkeyEngine::applyBindings(const QList<HotkeyBinding>& bindings) {
    unregisterAll();
    m_bindings = bindings;
    registerAll();
}

const QList<HotkeyBinding>& HotkeyEngine::bindings() const {
    return m_bindings;
}

void HotkeyEngine::registerAll() {
    for (auto& binding : m_bindings) {
        if (!binding.enabled) continue;

        binding.vk      = parseVirtualKey(binding.keyString);
        binding.winMods = parseModifiers(binding.modifiers);

        if (binding.vk == 0) {
            wintools::logger::Logger::log(
                LogSource, wintools::logger::Severity::Warning,
                QString("Unknown key '%1' – binding '%2' skipped.")
                    .arg(binding.keyString, binding.action.id));
            continue;
        }

#ifdef Q_OS_WIN
        const int id = m_nextId++;
        if (RegisterHotKey(nullptr, id, binding.winMods | MOD_NOREPEAT, binding.vk)) {
            m_idToAction.insert(id, binding.action);
            wintools::logger::Logger::log(
                LogSource, wintools::logger::Severity::Pass,
                QString("Registered '%1' → %2.%3")
                    .arg(displayString(binding), binding.action.module, binding.action.action));
        } else {
            wintools::logger::Logger::log(
                LogSource, wintools::logger::Severity::Warning,
                QString("Failed to register '%1' (Win32 error %2). "
                        "It may already be claimed by another app.")
                    .arg(displayString(binding))
                    .arg(GetLastError()));
        }
#else

        const int id = m_nextId++;
        m_idToAction.insert(id, binding.action);
        wintools::logger::Logger::log(
            LogSource, wintools::logger::Severity::Warning,
            QString("Global hotkeys are not yet supported on this platform. "
                    "Binding '%1' was saved but will not be active.")
                .arg(displayString(binding)));
#endif
    }
}

void HotkeyEngine::unregisterAll() {
#ifdef Q_OS_WIN
    for (auto it = m_idToAction.cbegin(); it != m_idToAction.cend(); ++it) {
        UnregisterHotKey(nullptr, it.key());
    }
#endif
    m_idToAction.clear();
}

bool HotkeyEngine::nativeEventFilter(const QByteArray& eventType,
                                     void*             message,
                                     qintptr*          ) {
#ifdef Q_OS_WIN
    if (eventType != "windows_generic_MSG") return false;

    const auto* msg = static_cast<const MSG*>(message);
    if (msg->message != WM_HOTKEY) return false;

    const int id = static_cast<int>(msg->wParam);
    const auto it = m_idToAction.constFind(id);
    if (it != m_idToAction.constEnd()) {
        emit hotkeyTriggered(it.value());
    }
#else
    Q_UNUSED(eventType);
    Q_UNUSED(message);
#endif
    return false;
}

unsigned int HotkeyEngine::parseVirtualKey(const QString& key) {
    const QString k = key.toUpper().trimmed();

    if (k.startsWith('F') && k.size() > 1) {
        bool ok = false;
        const int n = k.mid(1).toInt(&ok);
        if (ok && n >= 1 && n <= 24)
            return static_cast<unsigned int>(0x6F + n);
    }

    if (k.size() == 1 && k[0] >= 'A' && k[0] <= 'Z')
        return static_cast<unsigned int>(k[0].unicode());

    if (k.size() == 1 && k[0] >= '0' && k[0] <= '9')
        return static_cast<unsigned int>(k[0].unicode());

#ifdef Q_OS_WIN
    static const QHash<QString, unsigned int> named = {
        {"SPACE",         VK_SPACE},
        {"ENTER",         VK_RETURN},
        {"RETURN",        VK_RETURN},
        {"TAB",           VK_TAB},
        {"ESCAPE",        VK_ESCAPE},
        {"ESC",           VK_ESCAPE},
        {"BACKSPACE",     VK_BACK},
        {"DELETE",        VK_DELETE},
        {"DEL",           VK_DELETE},
        {"INSERT",        VK_INSERT},
        {"INS",           VK_INSERT},
        {"HOME",          VK_HOME},
        {"END",           VK_END},
        {"PAGEUP",        VK_PRIOR},
        {"PGUP",          VK_PRIOR},
        {"PAGEDOWN",      VK_NEXT},
        {"PGDN",          VK_NEXT},
        {"UP",            VK_UP},
        {"DOWN",          VK_DOWN},
        {"LEFT",          VK_LEFT},
        {"RIGHT",         VK_RIGHT},
        {"NUMPAD0",       VK_NUMPAD0},
        {"NUMPAD1",       VK_NUMPAD1},
        {"NUMPAD2",       VK_NUMPAD2},
        {"NUMPAD3",       VK_NUMPAD3},
        {"NUMPAD4",       VK_NUMPAD4},
        {"NUMPAD5",       VK_NUMPAD5},
        {"NUMPAD6",       VK_NUMPAD6},
        {"NUMPAD7",       VK_NUMPAD7},
        {"NUMPAD8",       VK_NUMPAD8},
        {"NUMPAD9",       VK_NUMPAD9},
        {"MULTIPLY",      VK_MULTIPLY},
        {"ADD",           VK_ADD},
        {"SUBTRACT",      VK_SUBTRACT},
        {"DECIMAL",       VK_DECIMAL},
        {"DIVIDE",        VK_DIVIDE},
        {"CAPSLOCK",      VK_CAPITAL},
        {"NUMLOCK",       VK_NUMLOCK},
        {"SCROLLLOCK",    VK_SCROLL},
        {"PAUSE",         VK_PAUSE},
        {"PRINTSCREEN",   VK_SNAPSHOT},
        {"APPS",          VK_APPS},
        {"SEMICOLON",     VK_OEM_1},
        {"EQUALS",        VK_OEM_PLUS},
        {"COMMA",         VK_OEM_COMMA},
        {"MINUS",         VK_OEM_MINUS},
        {"PERIOD",        VK_OEM_PERIOD},
        {"SLASH",         VK_OEM_2},
        {"TILDE",         VK_OEM_3},
        {"LBRACKET",      VK_OEM_4},
        {"BACKSLASH",     VK_OEM_5},
        {"RBRACKET",      VK_OEM_6},
        {"QUOTE",         VK_OEM_7},
        {"MEDIA_PLAY",    VK_MEDIA_PLAY_PAUSE},
        {"MEDIA_PAUSE",   VK_MEDIA_PLAY_PAUSE},
        {"MEDIA_NEXT",    VK_MEDIA_NEXT_TRACK},
        {"MEDIA_PREV",    VK_MEDIA_PREV_TRACK},
        {"VOLUME_UP",     VK_VOLUME_UP},
        {"VOLUME_DOWN",   VK_VOLUME_DOWN},
        {"VOLUME_MUTE",   VK_VOLUME_MUTE},
    };
#else

    static const QHash<QString, unsigned int> named = {
        {"SPACE",         0x20}, {"ENTER",         0x0D}, {"RETURN",        0x0D},
        {"TAB",           0x09}, {"ESCAPE",        0x1B}, {"ESC",           0x1B},
        {"BACKSPACE",     0x08}, {"DELETE",        0x2E}, {"DEL",           0x2E},
        {"INSERT",        0x2D}, {"INS",           0x2D}, {"HOME",          0x24},
        {"END",           0x23}, {"PAGEUP",        0x21}, {"PGUP",          0x21},
        {"PAGEDOWN",      0x22}, {"PGDN",          0x22}, {"UP",            0x26},
        {"DOWN",          0x28}, {"LEFT",          0x25}, {"RIGHT",         0x27},
        {"NUMPAD0",       0x60}, {"NUMPAD1",       0x61}, {"NUMPAD2",       0x62},
        {"NUMPAD3",       0x63}, {"NUMPAD4",       0x64}, {"NUMPAD5",       0x65},
        {"NUMPAD6",       0x66}, {"NUMPAD7",       0x67}, {"NUMPAD8",       0x68},
        {"NUMPAD9",       0x69}, {"MULTIPLY",      0x6A}, {"ADD",           0x6B},
        {"SUBTRACT",      0x6D}, {"DECIMAL",       0x6E}, {"DIVIDE",        0x6F},
        {"CAPSLOCK",      0x14}, {"NUMLOCK",       0x90}, {"SCROLLLOCK",    0x91},
        {"PAUSE",         0x13}, {"PRINTSCREEN",   0x2C}, {"APPS",          0x5D},
        {"SEMICOLON",     0xBA}, {"EQUALS",        0xBB}, {"COMMA",         0xBC},
        {"MINUS",         0xBD}, {"PERIOD",        0xBE}, {"SLASH",         0xBF},
        {"TILDE",         0xC0}, {"LBRACKET",      0xDB}, {"BACKSLASH",     0xDC},
        {"RBRACKET",      0xDD}, {"QUOTE",         0xDE}, {"MEDIA_PLAY",    0xB3},
        {"MEDIA_PAUSE",   0xB3}, {"MEDIA_NEXT",    0xB0}, {"MEDIA_PREV",    0xB1},
        {"VOLUME_UP",     0xAF}, {"VOLUME_DOWN",   0xAE}, {"VOLUME_MUTE",   0xAD},
    };
#endif

    return named.value(k, 0u);
}

unsigned int HotkeyEngine::parseModifiers(const QList<QString>& mods) {
    unsigned int result = 0;
    for (const QString& m : mods) {
        const QString mu = m.toUpper().trimmed();
#ifdef Q_OS_WIN
        if      (mu == "CTRL"  || mu == "CONTROL") result |= MOD_CONTROL;
        else if (mu == "ALT")                       result |= MOD_ALT;
        else if (mu == "SHIFT")                     result |= MOD_SHIFT;
        else if (mu == "WIN"   || mu == "WINDOWS")  result |= MOD_WIN;
#else

        if      (mu == "CTRL"  || mu == "CONTROL") result |= 0x0002;
        else if (mu == "ALT")                       result |= 0x0001;
        else if (mu == "SHIFT")                     result |= 0x0004;
        else if (mu == "WIN"   || mu == "WINDOWS")  result |= 0x0008;
#endif
    }
    return result;
}

QString HotkeyEngine::displayString(const HotkeyBinding& binding) {
    QStringList parts(binding.modifiers);
    parts.append(binding.keyString);
    return parts.join('+');
}

}
