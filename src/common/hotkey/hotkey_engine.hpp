#pragma once

// WinTools: hotkey engine manages shared infrastructure.

#include <QAbstractNativeEventFilter>
#include <QHash>
#include <QList>
#include <QObject>
#include <QString>

namespace wintools::hotkeys {

struct HotkeyAction {
    QString id;
    QString module;
    QString action;
};

struct HotkeyBinding {
    HotkeyAction   action;
    QString        keyString;
    QList<QString> modifiers;
    bool           enabled = true;

    unsigned int vk      = 0;
    unsigned int winMods = 0;
};

class HotkeyEngine : public QObject, public QAbstractNativeEventFilter {
    Q_OBJECT
public:
    explicit HotkeyEngine(QObject* parent = nullptr);
    ~HotkeyEngine() override;

    void applyBindings(const QList<HotkeyBinding>& bindings);

    const QList<HotkeyBinding>& bindings() const;

    static unsigned int parseVirtualKey(const QString& key);

    static unsigned int parseModifiers(const QList<QString>& mods);

    static QString displayString(const HotkeyBinding& binding);

signals:

    void hotkeyTriggered(const wintools::hotkeys::HotkeyAction& action);

protected:
    bool nativeEventFilter(const QByteArray& eventType,
                           void*             message,
                           qintptr*          result) override;

private:
    void unregisterAll();
    void registerAll();

    QList<HotkeyBinding>     m_bindings;
    QHash<int, HotkeyAction> m_idToAction;
    int                      m_nextId = 1;
};

}
