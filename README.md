# Keypad

A keypad on your Tessera screen. Type the right code and the screen runs an action in Home Assistant: it unlocks a
lock, opens a garage door, switches a light or a switch, runs a script or a scene, or presses a button.

> The code is checked on the screen, so it is stored in the screen's layout and on the screen itself. Use it for
> convenience, such as letting family in, and not as the only protection of a door.

## Set up

1. In Tessera, open **Plugins**, choose **Add with a link** and paste `https://github.com/mifricke/tessera-keypad`.
2. Add the plugin to a screen. The screen builds once.
3. In **Layout**, place the **Keypad** tile from the library's Plugins group.
4. In the inspector, search for and choose the **Entity** the code switches, and fill in the **Code** (4 to 8 digits).
   The list offers only entities the code can switch.
5. Tap the tile on the screen, type the code, and the action runs.

## What the right code does

| Entity | Action |
|---|---|
| Lock | `lock.unlock` |
| Cover (a garage door, a gate) | `cover.open_cover` |
| Switch | `switch.toggle` |
| Input boolean | `input_boolean.toggle` |
| Light | `light.toggle` |
| Script | `script.turn_on` |
| Scene | `scene.turn_on` |
| Button | `button.press` |

To do something else, such as sending a notification or opening two locks, write a script in Home Assistant and choose
that script on the tile.

## Options

- **Entity**: what the right code switches, chosen from a list you can search.
- **Code**: the digits that open. The keypad checks the code as soon as the last digit is typed, so no OK is needed.
- **Wrong tries** and **Wait after wrong tries**: after this many wrong codes in a row the keypad takes no code for a
  while and counts down.

## Good to know

- The screen must be allowed to perform actions in Home Assistant (Tessera sets this up when it adds a screen).
- A light, a switch and an input boolean are switched over: off goes on, on goes off.
- The keypad closes by itself a moment after the right code. When the code could not be sent, it says so and stays
  open.
- The X key clears what was typed.
