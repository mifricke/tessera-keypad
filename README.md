# Code lock

A keypad on your Tessera screen. Type the right code and the screen runs an action in Home Assistant: it unlocks a
lock, opens a garage door, runs a script or a scene, or presses a button.

> The code is checked on the screen, so it is stored in the screen's layout and on the screen itself. Use it for
> convenience, such as letting family in, and not as the only protection of a door. For a lock that has a code of its
> own in Home Assistant, turn on **Pass the code to the lock** and Home Assistant checks the code instead.

## Set up

1. In Tessera, open **Plugins**, choose **Add with a link** and paste `https://github.com/mifricke/tessera-keypad`.
2. Add the plugin to a screen. The screen builds once.
3. In **Layout**, place the **Code lock** tile from the library's Plugins group.
4. In the inspector, choose the entity the code acts on, and fill in the **Code** (4 to 8 digits).
5. Tap the tile on the screen, type the code, and the action runs.

## What the right code does

| Entity | Action |
|---|---|
| Lock | `lock.unlock` |
| Cover (a garage door, a gate) | `cover.open_cover` |
| Script | `script.turn_on` |
| Scene | `scene.turn_on` |
| Button, input button | `button.press`, `input_button.press` |
| Switch | `switch.turn_on` |
| Automation | `automation.trigger` |

To do something else, such as sending a notification or opening two locks, write a script in Home Assistant and choose
that script on the tile.

## Options

- **Code**: the digits that open. The keypad checks the code as soon as the last digit is typed, so no OK is needed.
- **Pass the code to the lock**: for a lock only. The typed code goes along with `lock.unlock` as its `code`. With
  **Code** left empty, Home Assistant alone decides whether the code is right; the keypad then needs OK after the last
  digit.
- **Wrong tries** and **Wait after wrong tries**: after this many wrong codes in a row the keypad takes no code for a
  while and counts down.

## Good to know

- The screen must be allowed to perform actions in Home Assistant (Tessera sets this up when it adds a screen).
- The keypad closes by itself a moment after the right code.
- The X key clears what was typed.
