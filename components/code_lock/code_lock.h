#pragma once
#include "esphome/core/component.h"
#include "esphome/components/smart_display/plugin_api.h"

namespace esphome::code_lock {

// What a tile hands the keypad it opens: the code, the entity and how to act on it. The card is made after the tile's
// tap and reads it from the plugin, as it has no options of its own.
struct Lock {
  char code[17] = "";       // the right code, "" while none is known
  bool from_entity = false; // the code is a helper's state in Home Assistant, not the tile's option
  char entity[96] = "";     // what the right code acts on
  int attempts = 3;         // wrong codes in a row before the keypad waits
  bool auto_check = true;   // check the code as its last digit comes, without OK
  bool free_close = false;  // lock, close and switch off with a tap, the code only to open and switch on
  int wait_s = 30;          // how long it waits
};

// The plugin: a tile that shows the entity and opens the keypad, and the keypad card. It keeps the wrong tries per
// entity across the card's openings, so closing and opening the keypad does not reset the wait.
class CodeLock : public Component, public tessera::Plugin {
 public:
  void setup() override;
  float get_setup_priority() const override { return setup_priority::DATA; }

  Lock chosen;

  // Wrong tries of one entity (the last few entities seen; a screen has only a handful of code locks).
  struct Tries {
    char entity[96] = "";
    int wrong = 0;
    uint32_t blocked_until = 0;   // millis(), 0 for not blocked
  };
  Tries &tries(const char *entity);

 private:
  Tries tries_[4];
  uint8_t next_ = 0;
};

}  // namespace esphome::code_lock
