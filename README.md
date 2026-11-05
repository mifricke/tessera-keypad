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
- **Code**: the digits that open.
- **Code only to open**: off (the default), every tap opens the keypad. On, a tap locks a lock, closes a cover or
  switches a light, switch or input boolean off at once, without the code; the tile says so ("Tap to lock").
  Unlocking, opening and switching on still take the code, and a script, a scene or a button always does.
- **Check the code by itself**: on (the default), the keypad checks the code as soon as its last digit is typed, so
  no check key is needed. Off, only the check key checks it: the keypad then never tells how long the code is.
- **Code from a helper**: instead of typing the code into the tile, choose a helper in Home Assistant that holds it
  (below). When a helper is chosen, its code counts and the **Code** above is not used.
- **Wrong tries** and **Wait after wrong tries**: after this many wrong codes in a row the keypad takes no code for a
  while and counts down.

## The code from a helper in Home Assistant

The code can live in Home Assistant instead of in Tessera, so it can be changed there, or by an automation, without
opening Tessera and without building the screen again.

1. In Home Assistant, open **Settings**, **Devices & services**, **Helpers**, and create a **Text** helper. Set its
   mode to **Password** so the code is hidden in the dashboard, and its maximum length to 16.
2. Type the code as its value: 4 to 16 digits.
3. In Tessera, choose that helper under **Code from a helper** on the tile.

A new value counts within seconds, also on a keypad that is open. While the helper is unavailable, or holds anything
but digits, the keypad takes no code at all and says so.

The Password mode only hides the code in Home Assistant's dashboard. The state itself is plain text: it is in the
history and the logbook, and everyone who can use Home Assistant or its API can read it. Leave the helper out of the
recorder (`recorder:` `exclude:` `entities:` in `configuration.yaml`). As with a code typed into the tile, the screen
checks the code, so it also reaches the screen.

## Tried on

- On a real screen: the Guition 4848S040 (4-inch, 480 x 480), with a light switched by the right code.

## Good to know

- The screen must be allowed to perform actions in Home Assistant (Tessera sets this up when it adds a screen).
- A light, a switch and an input boolean are switched over: off goes on, on goes off.
- The keypad closes by itself a moment after the right code. When the code could not be sent, it says so and stays
  open.
- The X key clears what was typed.
