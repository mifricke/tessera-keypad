# Changelog

## 0.5.1
- The tile's example line in the editor no longer speaks of unlocking only: it fits every entity the code can switch.

## 0.5.0
- New option **Check the code by itself**, on by default as before. Turned off, only the check key checks the code,
  so the keypad never tells how long the code is.
- The tile takes any size the screen's grid offers, up to 9 x 9; a large tile shows its name and state in larger
  words.

## 0.4.0
- New option **Code from a helper**: the code can come from an input_text or text helper in Home Assistant instead of
  the tile. A new code there counts within seconds, without building the screen again. A code typed into the tile
  keeps working as before when no helper is chosen.

## 0.3.2
- The plugin and its tile are called Keypad now, in every language.

## 0.3.1
- On the smallest screens (the 2.8-inch CYD) the keypad's keys fit the glass: before, the lowest row was cut off.

## 0.3.0
- The right code switches a light, a switch or an input boolean over, whatever it is now. Before, it only turned them
  on, so a light that was already on did nothing.
- The keypad looks like Tessera's own: round keys, OK in the accent colour, and dots like a PIN field that never show
  how long the code is. On wide glass the dots stand beside the keys.
- The keypad says whether the code went out ("Sent") or could not be sent, and keys that do nothing now stand back.

## 0.2.0
- The entity is chosen from a list you can search, and the list offers only entities the code can switch: locks,
  covers, switches, input booleans, lights, scripts, scenes and buttons. Choose the entity of each tile again.
- Gone: the option to pass the code to the lock. The screen always checks the code.

## 0.1.0
- First version: a tile that opens a keypad, and the right code unlocks a lock or runs a script, a scene, a button,
  a switch, an automation or a cover.
