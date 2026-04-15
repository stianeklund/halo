#!/usr/bin/env python3
"""Keyboard-to-virtual-Xbox-360-controller mapper for xemu (Windows).

Uses a global keyboard hook so it works even while xemu is focused.
May need to run as Administrator for the hook to take effect.

Requires: pip install vgamepad keyboard
Requires: ViGEmBus driver (https://github.com/nefarius/ViGEmBus/releases)

Controls:
    WASD       Left stick
    Arrows     D-pad
    J          A button
    K          B button
    I          X button
    L          Y button
    Q          Left shoulder
    E          Right shoulder
    Z          Left trigger
    X          Right trigger
    Enter      Start
    Backspace  Back
    Escape     Quit
"""

import sys
import time

import keyboard
import vgamepad as vg

BUTTON_MAP = {
    "j": vg.XUSB_BUTTON.XUSB_GAMEPAD_A,
    "k": vg.XUSB_BUTTON.XUSB_GAMEPAD_B,
    "i": vg.XUSB_BUTTON.XUSB_GAMEPAD_X,
    "l": vg.XUSB_BUTTON.XUSB_GAMEPAD_Y,
    "q": vg.XUSB_BUTTON.XUSB_GAMEPAD_LEFT_SHOULDER,
    "e": vg.XUSB_BUTTON.XUSB_GAMEPAD_RIGHT_SHOULDER,
    "enter": vg.XUSB_BUTTON.XUSB_GAMEPAD_START,
    "backspace": vg.XUSB_BUTTON.XUSB_GAMEPAD_BACK,
    "left": vg.XUSB_BUTTON.XUSB_GAMEPAD_DPAD_LEFT,
    "right": vg.XUSB_BUTTON.XUSB_GAMEPAD_DPAD_RIGHT,
    "up": vg.XUSB_BUTTON.XUSB_GAMEPAD_DPAD_UP,
    "down": vg.XUSB_BUTTON.XUSB_GAMEPAD_DPAD_DOWN,
}

STICK_KEYS = {"w", "a", "s", "d"}
TRIGGER_KEYS = {"z", "x"}

pressed = set()


def send_state(pad):
    pad.reset()

    for key, btn in BUTTON_MAP.items():
        if key in pressed:
            pad.press_button(button=btn)

    sx, sy = 0.0, 0.0
    if "a" in pressed:
        sx -= 1.0
    if "d" in pressed:
        sx += 1.0
    if "w" in pressed:
        sy -= 1.0
    if "s" in pressed:
        sy += 1.0
    pad.left_joystick_float(max(-1.0, min(1.0, sx)), max(-1.0, min(1.0, sy)))
    pad.left_trigger_float(1.0 if "z" in pressed else 0.0)
    pad.right_trigger_float(1.0 if "x" in pressed else 0.0)

    pad.update()


def on_event(event):
    if not event.name:
        return
    name = event.name.lower()
    if event.event_type == keyboard.KEY_DOWN:
        if name == "esc":
            print("Quitting.")
            keyboard.unhook_all()
            sys.exit(0)
        if name in pressed:
            return
        pressed.add(name)
        label = _label(name)
        print(f"  DOWN {name:12s} -> {label}")
        send_state(pad)
    elif event.event_type == keyboard.KEY_UP:
        if name not in pressed:
            return
        pressed.discard(name)
        label = _label(name)
        print(f"  UP   {name:12s} -> {label}")
        send_state(pad)


def _label(name):
    if name in BUTTON_MAP:
        return BUTTON_MAP[name].name
    if name in STICK_KEYS:
        return "left_stick"
    if name in TRIGGER_KEYS:
        return name + "_trigger"
    return name


def self_test(pad):
    print("Self-test: pressing virtual A for 0.5s ...")
    pad.press_button(button=vg.XUSB_BUTTON.XUSB_GAMEPAD_A)
    pad.update()
    time.sleep(0.5)
    pad.release_button(button=vg.XUSB_BUTTON.XUSB_GAMEPAD_A)
    pad.update()
    time.sleep(0.2)
    print("Self-test done. Check joy.cpl or xemu controller settings.\n")


if __name__ == "__main__":
    pad = vg.VX360Gamepad()

    print("Virtual Xbox 360 controller active.")
    print("Map it in xemu Settings > Controllers.")
    print("Press ESC to quit.\n")

    self_test(pad)

    keyboard.hook(on_event, suppress=False)
    try:
        keyboard.wait()
    except KeyboardInterrupt:
        pass
    finally:
        keyboard.unhook_all()
        del pad
