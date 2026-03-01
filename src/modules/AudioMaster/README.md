# AudioMaster

AudioMaster is an audio device manager inside WinTools.

It gives you fast control over your sound devices and lets you fine-tune audio output without digging through Windows settings.

## What it does

- Provides a mixer-first layout inspired by SteelSeries Sonar.
- Includes five channels: Master, Game, Chat, Media, and Mic.
- Gives each channel a dedicated vertical volume slider.
- Adds a routing subpage where each channel can be linked to a real audio device.
- Supports WinTools hotkeys to rotate linked output and input devices.

## How it feels to use

You open AudioMaster and see every device plugged in, clearly labelled. Switching between a headset and speakers is one click. If you set up hotkeys you can flip between devices without touching the mouse at all.

The routing page lets you decide which output/input each AudioMaster channel controls.

## Everyday flow

1. Open AudioMaster from the WinTools launcher.
2. On the Mixer page, move Master/Game/Chat/Media/Mic sliders to adjust linked device volume.
3. On the Routing page, choose which real audio device each channel controls.
4. Set up hotkeys in WinTools settings for AudioMaster `rotate_output` and `rotate_input` actions.

## Quick note

Device switching uses the Windows audio API, so the change takes effect system-wide just like switching in the Windows taskbar tray.
