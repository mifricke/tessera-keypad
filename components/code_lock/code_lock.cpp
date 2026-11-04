#include "code_lock.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <string>

namespace esphome::code_lock {

using tessera::Font;
using tessera::Tone;
namespace ui = tessera::ui;

static constexpr uint32_t ICON_LOCK = 0xF033E, ICON_LOCK_OPEN = 0xF033F, ICON_CHECK = 0xF012C, ICON_CLOSE = 0xF0156;
static constexpr size_t MAX_DIGITS = 16;   // what the keypad takes

// The action the right code runs, by the entity's domain (named in the manifest's home_assistant_actions): a light, a
// switch and an input boolean are switched over, whatever they are now; a lock unlocks, a cover opens.
static const char *service_for(const char *entity) {
  static const struct { const char *domain, *service; } MAP[] = {
      {"lock.", "lock.unlock"},     {"cover.", "cover.open_cover"}, {"switch.", "switch.toggle"},
      {"input_boolean.", "input_boolean.toggle"}, {"light.", "light.toggle"}, {"script.", "script.turn_on"},
      {"scene.", "scene.turn_on"},   {"button.", "button.press"},
  };
  for (const auto &m : MAP)
    if (std::strncmp(entity, m.domain, std::strlen(m.domain)) == 0) return m.service;
  return nullptr;
}

// The action that closes the entity from what it is now, for a tap without the code (the option free_close): a lock
// that is unlocked locks, a cover that is open closes, a light, switch or input boolean that is on goes off. Nothing
// for an entity that is closed or off already, or that has no such state (a script, a scene, a button): those take
// the code.
static const char *closing_service(const char *entity, const char *state) {
  auto is = [&](const char *domain) { return std::strncmp(entity, domain, std::strlen(domain)) == 0; };
  auto now = [&](const char *word) { return std::strcmp(state, word) == 0; };
  if (is("lock.")) return now("unlocked") || now("open") ? "lock.lock" : nullptr;
  if (is("cover.")) return now("open") || now("opening") ? "cover.close_cover" : nullptr;
  if (!now("on")) return nullptr;
  if (is("light.")) return "light.turn_off";
  if (is("switch.")) return "switch.turn_off";
  if (is("input_boolean.")) return "input_boolean.turn_off";
  return nullptr;
}

// A text option as text: the code as the inspector keeps it, also when it came as a number.
static void option_text(JsonVariantConst value, char *out, size_t size) {
  out[0] = 0;
  if (value.is<const char *>()) std::strncpy(out, value.as<const char *>(), size - 1);
  else if (value.is<long>()) std::snprintf(out, size, "%ld", value.as<long>());
  out[size - 1] = 0;
  // Only digits count: a space the person typed around the code is left out.
  size_t n = 0;
  for (size_t i = 0; out[i]; ++i) if (out[i] >= '0' && out[i] <= '9') out[n++] = out[i];
  out[n] = 0;
}

// The code a helper in Home Assistant holds (an input_text or a text entity): its state when that is 1 to 16 digits,
// spaces around them left out; nothing while it is unavailable, unknown or anything else, so the keypad stays shut.
static void code_from_state(const char *state, char *out, size_t size) {
  out[0] = 0;
  size_t n = 0;
  for (const char *p = state ? state : ""; *p; ++p) {
    if (*p == ' ') continue;
    if (*p < '0' || *p > '9' || n + 1 >= size) {
      out[0] = 0;
      return;
    }
    out[n++] = *p;
  }
  out[n] = 0;
}

// The code of a tile's data when it comes from a helper: the state of the entity chosen as `code_entity`.
static void code_from_data(JsonObjectConst data, char *out, size_t size) {
  JsonVariantConst helper = data["entities"]["code_entity"];
  code_from_state(helper.isNull() ? "" : (helper["state"] | ""), out, size);
}

CodeLock::Tries &CodeLock::tries(const char *entity) {
  for (auto &t : tries_)
    if (std::strcmp(t.entity, entity) == 0) return t;
  Tries &t = tries_[next_];
  next_ = (next_ + 1) % 4;
  t = Tries();
  std::strncpy(t.entity, entity, sizeof(t.entity) - 1);
  return t;
}

// ---- The tile: the entity's icon, its name and what to do; a tap opens the keypad ----
class KeypadTile : public tessera::Tile {
 public:
  explicit KeypadTile(CodeLock *plugin) : plugin_(plugin) {}

  void create(const tessera::TileContext &c) override {
    width_ = c.width;
    height_ = c.height;
    tile_ = c.tile;
    std::strncpy(lock_.entity, c.options["target"] | "", sizeof(lock_.entity) - 1);
    // The code: from a helper in Home Assistant when one is chosen (it comes with the tile's data, on_state), else
    // the one typed into the tile's options.
    lock_.from_entity = (c.options["code_entity"] | "")[0] != 0;
    if (!lock_.from_entity) option_text(c.options["code"], lock_.code, sizeof(lock_.code));
    lock_.attempts = std::max(1, c.options["attempts"] | 3);
    lock_.auto_check = c.options["auto_check"] | true;
    lock_.free_close = c.options["free_close"] | false;
    lock_.wait_s = std::max(1, c.options["wait"] | 30);
    std::strncpy(name_, c.name ? c.name : "", sizeof(name_) - 1);

    icon_ = ui::label(c.parent, Font::ICON, theme::ACCENT);
    name_label_ = ui::label(c.parent, Font::TITLE, theme::INK);
    line_ = ui::label(c.parent, Font::BODY, theme::MUTED);
    for (lv_obj_t *l : {icon_, name_label_, line_}) {
      lv_obj_set_width(l, width_);
      lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0);
    }
    paint();
  }

  void on_state(JsonObjectConst data) override {
    // The chosen entity comes under entities.target (an option of kind entity), again at every change.
    JsonObjectConst target = data["entities"]["target"];
    if (!target["name"].isNull() && !name_[0]) std::strncpy(name_, target["name"] | "", sizeof(name_) - 1);
    std::strncpy(state_, target["state"] | "", sizeof(state_) - 1);
    // A helper's code follows Home Assistant: a new code there reaches the tile within seconds, without a build.
    if (lock_.from_entity) code_from_data(data, lock_.code, sizeof(lock_.code));
    paint();
  }

  void on_theme() override {
    ui::set_color(icon_, theme::ACCENT);
    ui::set_color(name_label_, theme::INK);
    ui::set_color(line_, theme::MUTED);
  }

  void on_tap() override {
    if (!ready()) return;
    // Closing needs no code where the tile says so: a tap locks, closes or switches off at once.
    const char *closing = lock_.free_close ? closing_service(lock_.entity, state_) : nullptr;
    if (closing) {
      const bool sent = tessera::action(closing, lock_.entity);
      ESP_LOGI("code_lock", "Without the code: %s on %s %s", closing, lock_.entity, sent ? "sent" : "NOT sent");
      (void) sent;
      return;
    }
    plugin_->chosen = lock_;
    tessera::open_card(plugin_->plugin_id(), "keys", lock_.entity, tile_);
  }

 private:
  // A code to check, and an entity the plugin knows how to act on.
  bool ready() const { return lock_.entity[0] && service_for(lock_.entity) && lock_.code[0]; }

  // What the entity is now, in the screen's words, or what to do.
  std::string line() const {
    if (!lock_.entity[0] || !service_for(lock_.entity)) return plugin_->text("choose");
    if (!lock_.code[0]) return plugin_->text(lock_.from_entity ? "no_helper_code" : "no_code");
    const char *s = state_;
    if (!std::strcmp(s, "unavailable")) return plugin_->text("unavailable");
    // Where a tap closes without the code, the line says what the tap does.
    if (lock_.free_close && closing_service(lock_.entity, s)) return plugin_->text(std::strncmp(lock_.entity, "lock.", 5) == 0 ? "tap_lock" : std::strncmp(lock_.entity, "cover.", 6) == 0 ? "tap_close" : "tap_off");
    if (!std::strcmp(s, "locked")) return plugin_->text("locked");
    if (!std::strcmp(s, "unlocked")) return plugin_->text("unlocked");
    if (!std::strcmp(s, "open")) return plugin_->text("open");
    if (!std::strcmp(s, "closed")) return plugin_->text("closed");
    if (!std::strcmp(s, "on")) return plugin_->text("on");
    if (!std::strcmp(s, "off") && std::strncmp(lock_.entity, "script.", 7)) return plugin_->text("off");
    return plugin_->text("tap");
  }

  void paint() {
    const bool open = !std::strcmp(state_, "unlocked") || !std::strcmp(state_, "open");
    ui::set_text(icon_, ui::icon(open ? ICON_LOCK_OPEN : ICON_LOCK));
    ui::set_text(name_label_, name_);
    ui::set_text(line_, line());
    // A large tile takes larger words, the name in HEADLINE and the line in BODY_LARGE, where they fit its width and
    // the three together take at most half its height.
    const bool roomy = ui::line_height(Font::ICON) + ui::px(4) + ui::line_height(Font::HEADLINE) +
                           ui::line_height(Font::BODY_LARGE) <= height_ / 2 &&
                       ui::text_width(name_, Font::HEADLINE) <= width_ && ui::text_width(line(), Font::BODY_LARGE) <= width_;
    const Font name_font = roomy ? Font::HEADLINE : Font::TITLE, line_font = roomy ? Font::BODY_LARGE : Font::BODY;
    ui::set_font(name_label_, name_font);
    ui::set_font(line_, line_font);
    // The icon, the name and the line under it, together in the middle; a low tile drops the icon, then the name.
    const int ih = ui::line_height(Font::ICON), nh = ui::line_height(name_font), lh = ui::line_height(line_font);
    const int gap = ui::px(4);
    const bool with_icon = ih + gap + nh + lh <= height_;
    const bool with_name = name_[0] && nh + lh <= height_;
    const int total = (with_icon ? ih + gap : 0) + (with_name ? nh : 0) + lh;
    int y = std::max(0, (height_ - total) / 2);
    if (with_icon) {
      lv_obj_remove_flag(icon_, LV_OBJ_FLAG_HIDDEN);
      lv_obj_set_pos(icon_, 0, y);
      y += ih + gap;
    } else {
      lv_obj_add_flag(icon_, LV_OBJ_FLAG_HIDDEN);
    }
    if (with_name) {
      lv_obj_remove_flag(name_label_, LV_OBJ_FLAG_HIDDEN);
      lv_obj_set_pos(name_label_, 0, y);
      y += nh;
    } else {
      lv_obj_add_flag(name_label_, LV_OBJ_FLAG_HIDDEN);
    }
    lv_obj_set_pos(line_, 0, y);
  }

  CodeLock *plugin_;
  Lock lock_;
  char name_[48] = "", state_[24] = "";
  int width_ = 0, height_ = 0, tile_ = -1;
  lv_obj_t *icon_{}, *name_label_{}, *line_{};
};

// ---- The keypad: Tessera's own code pad (the alarm panel's and the lock's) for a plugin ----
// The dots and a line of words above twelve round keys (1-9, clear, 0, OK), or beside them on wide glass, laid out as
// the core's keypad_layout lays its own: every key a finger's size at least, a key a finger and a half wide, the dots
// as a PIN field shows them (four at least, so they never tell the code's length).
class KeysCard : public tessera::Card {
 public:
  explicit KeysCard(CodeLock *plugin) : plugin_(plugin) {}

  void open(const tessera::CardContext &c) override {
    lock_ = plugin_->chosen;
    const bool large = ui::large();
    const int side = ui::px(large ? 20 : 12);
    int g = ui::px(large ? 12 : 6);
    text_h_ = ui::line_height(Font::BODY_LARGE);
    const int least = std::max(ui::mm(8), text_h_ + ui::px(8));
    int kh = std::max(ui::px(large ? 64 : 42), least);
    dot_max_ = ui::px(large ? 16 : 10);
    const int x = side, top = side, w = c.width - 2 * side, h = c.height - 2 * side;
    beside_ = w * 10 >= h * 13;
    const int info_h = dot_max_ + g + text_h_;
    const int keys_h = beside_ ? h : h - info_h - 2 * g;
    // Short glass gives up the space between the keys before a key gets smaller than a finger.
    if (4 * least + 3 * g > keys_h) g = std::max(2, (keys_h - 4 * least) / 3);
    if (4 * kh + 3 * g > keys_h) kh = std::max(least, (keys_h - 3 * g) / 4);
    // Glass too short even for a finger's keys (the CYD) gets smaller keys rather than keys off the glass.
    kh = std::max(1, std::min(kh, (keys_h - 3 * g) / 4));
    int kw = kh * 3 / 2;
    const int third = ((beside_ ? w * 64 / 100 : w) - 2 * g) / 3;
    kw = std::min(std::max(std::min(kw, third), least), third);
    const int pad_w = 3 * kw + 2 * g, pad_h = 4 * kh + 3 * g;
    const int info_w = beside_ ? std::min(w - pad_w - 2 * g, pad_w) : w;
    const int pair_x = beside_ ? x + (w - (info_w + 2 * g + pad_w)) / 2 : x;
    const int px0 = beside_ ? pair_x + info_w + 2 * g : x + (w - pad_w) / 2;
    const int py0 = beside_ ? top + (h - pad_h) / 2 : top + (h - (info_h + 2 * g + pad_h)) / 2 + info_h + 2 * g;
    key_w_ = kw;
    key_h_ = kh;
    gap_ = g;
    x0_ = px0;
    y0_ = py0;
    info_x_ = beside_ ? pair_x : x;
    info_w_ = info_w;
    const int info_y = beside_ ? top : py0 - 2 * g - info_h, info_hh = beside_ ? h : info_h + g;
    const int lines = beside_ ? 3 : 1;
    const int block = dot_max_ + g + lines * text_h_;
    dots_y_ = info_y + (info_hh - block) / 2;

    for (auto &d : dots_) {
      d = ui::block(c.parent, theme::CARD);
      lv_obj_set_style_radius(d, LV_RADIUS_CIRCLE, 0);
      lv_obj_add_flag(d, LV_OBJ_FLAG_HIDDEN);
    }
    line_ = lines > 1 ? ui::text(c.parent, Font::BODY_LARGE, theme::MUTED) : ui::label(c.parent, Font::BODY_LARGE, theme::MUTED);
    lv_obj_set_size(line_, info_w, lines * text_h_);
    lv_obj_set_pos(line_, info_x_, dots_y_ + dot_max_ + g);
    lv_obj_set_style_text_align(line_, LV_TEXT_ALIGN_CENTER, 0);

    // The digits in the largest of the screen's fonts that sits well in a key; the clear and OK keys their icons.
    Font digit = Font::BODY_LARGE;
    for (Font f : {Font::VALUE, Font::HEADLINE, Font::TITLE, Font::BODY_LARGE}) {
      digit = f;
      if (ui::line_height(f) <= kh * 6 / 10) break;
    }
    const Font icon = ui::line_height(Font::ICON) <= kh - ui::px(10) ? Font::ICON : Font::ICON_SMALL;
    for (int i = 0; i < 12; ++i) {
      lv_obj_t *k = ui::block(c.parent, i == OK ? theme::ACCENT : theme::CARD);
      lv_obj_set_style_pad_all(k, 0, 0);
      lv_obj_set_size(k, kw, kh);
      lv_obj_set_pos(k, px0 + (i % 3) * (kw + g), py0 + (i / 3) * (kh + g));
      lv_obj_set_style_radius(k, kh / 2, 0);
      lv_obj_set_style_border_width(k, i == OK ? 0 : 1, 0);
      lv_obj_set_style_border_color(k, ui::color(theme::LINE), 0);
      const Font face = i == OK || i == CLEAR ? icon : digit;
      lv_obj_t *l = ui::label(k, face, i == OK ? theme::ON_ACCENT : theme::INK);
      lv_obj_set_width(l, kw);
      lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0);
      lv_obj_set_pos(l, 0, (kh - ui::line_height(face)) / 2);
      if (i == CLEAR) ui::set_text(l, ui::icon(ICON_CLOSE));
      else if (i == OK) ui::set_text(l, ui::icon(ICON_CHECK));
      else ui::set_text(l, std::string(1, i == ZERO ? '0' : static_cast<char>('1' + i)));
      keys_[i] = k;
      labels_[i] = l;
    }
    paint();
  }

  // The tile's data again: a helper's new code counts at once, also while the keypad is open.
  void on_state(JsonObjectConst data) override {
    if (lock_.from_entity && !data["entities"].isNull()) code_from_data(data, lock_.code, sizeof(lock_.code));
  }

  void on_theme() override {
    for (int i = 0; i < 12; ++i) {
      ui::set_fill(keys_[i], Tone::NORMAL, i == OK ? theme::ACCENT : theme::CARD);
      lv_obj_set_style_border_color(keys_[i], ui::color(theme::LINE), 0);
      ui::set_color(labels_[i], i == OK ? theme::ON_ACCENT : theme::INK);
    }
    paint();
  }

  void on_tick(uint32_t) override {
    // A code that went out: the card closes a moment after it said so.
    if (said_ == Said::SENT && esphome::millis() - said_at_ >= 1500) {
      tessera::close_card();
      return;
    }
    paint();
  }

  void on_tap() override {
    if (said_ == Said::SENT || blocked()) return;
    const tessera::Point p = tessera::tap_point();
    const int key = key_at(p.x, p.y);
    if (key < 0) return;
    if (key == CLEAR) {
      clear();
      said_ = Said::NOTHING;
    } else if (key == OK) {
      if (typed_len_) submit();
    } else if (typed_len_ < MAX_DIGITS) {
      if (said_ == Said::WRONG) said_ = Said::NOTHING;
      typed_[typed_len_++] = key == ZERO ? '0' : static_cast<char>('1' + key);
      typed_[typed_len_] = 0;
      // The code is checked as its last digit comes, without OK; with auto_check off only OK checks it, so the keypad
      // never tells how long the code is.
      if (lock_.auto_check && lock_.code[0] && typed_len_ == std::strlen(lock_.code)) submit();
    }
    paint();
  }

 private:
  enum class Said : uint8_t { NOTHING, WRONG, SENT, FAILED, NO_CODE };
  static constexpr int CLEAR = 9, ZERO = 10, OK = 11;

  void clear() {
    std::memset(typed_, 0, sizeof(typed_));
    typed_len_ = 0;
  }

  bool blocked() const {
    const auto &t = plugin_->tries(lock_.entity);
    return t.blocked_until && static_cast<int32_t>(t.blocked_until - esphome::millis()) > 0;
  }

  // The key under a finger, with half a gap around each key as its own; -1 for none.
  int key_at(int x, int y) const {
    if (x < 0 || y < 0) return -1;
    const int half = gap_ / 2;
    for (int i = 0; i < 12; ++i) {
      const int kx = x0_ + (i % 3) * (key_w_ + gap_), ky = y0_ + (i / 3) * (key_h_ + gap_);
      if (x >= kx - half && x < kx + key_w_ + half && y >= ky - half && y < ky + key_h_ + half) return i;
    }
    return -1;
  }

  // The same length and the same digits, every digit looked at whatever the first difference.
  bool matches() const {
    const size_t n = std::strlen(lock_.code);
    unsigned diff = typed_len_ != n;
    for (size_t i = 0; i < n; ++i) diff |= static_cast<unsigned>(lock_.code[i] ^ typed_[i]);
    return diff == 0;
  }

  void submit() {
    auto &t = plugin_->tries(lock_.entity);
    said_at_ = esphome::millis();
    if (!lock_.code[0]) {
      // A helper that has no code now (unavailable, or not digits): nothing opens.
      clear();
      said_ = Said::NO_CODE;
      return;
    }
    if (!matches()) {
      clear();
      said_ = Said::WRONG;
      ESP_LOGI(TAG, "Wrong code for %s", lock_.entity);
      if (++t.wrong >= lock_.attempts) {
        t.wrong = 0;
        t.blocked_until = esphome::millis() + static_cast<uint32_t>(lock_.wait_s) * 1000u;
        if (!t.blocked_until) t.blocked_until = 1;
      }
      return;
    }
    t.wrong = 0;
    clear();
    const char *service = service_for(lock_.entity);
    const bool sent = service && tessera::action(service, lock_.entity);
    ESP_LOGI(TAG, "Right code: %s on %s %s", service ? service : "(no action)", lock_.entity, sent ? "sent" : "NOT sent");
    said_ = sent ? Said::SENT : Said::FAILED;
  }

  void paint() {
    const uint32_t now = esphome::millis();
    const bool held = blocked();
    // The dots: one filled per digit typed, and empty ones up to four, like a PIN field; smaller when many must fit.
    const int shown = std::max<int>(4, static_cast<int>(typed_len_));
    int dot = dot_max_;
    while (dot > 4 && shown * dot + (shown - 1) * (dot * 3 / 5) > info_w_) --dot;
    const int dgap = dot * 3 / 5, row = shown * dot + (shown - 1) * dgap;
    const int left = info_x_ + (info_w_ - row) / 2, y = dots_y_ + (dot_max_ - dot) / 2;
    const bool bad = said_ == Said::WRONG && now - said_at_ < 4000;
    for (int i = 0; i < static_cast<int>(MAX_DIGITS); ++i) {
      lv_obj_t *d = dots_[i];
      if (i >= shown) {
        lv_obj_add_flag(d, LV_OBJ_FLAG_HIDDEN);
        continue;
      }
      lv_obj_remove_flag(d, LV_OBJ_FLAG_HIDDEN);
      lv_obj_set_size(d, dot, dot);
      lv_obj_set_pos(d, left + i * (dot + dgap), y);
      const bool filled = i < static_cast<int>(typed_len_) || (bad && i < 4);
      if (filled) ui::set_fill(d, bad ? Tone::ALERT : Tone::NORMAL, theme::INK);
      else ui::set_fill(d, Tone::NORMAL, theme::CARD);
      lv_obj_set_style_border_width(d, filled ? 0 : std::max(1, dot / 7), 0);
      lv_obj_set_style_border_color(d, ui::color(theme::TICK), 0);
    }
    // The line under the dots: the wait after wrong tries, how it went for a few seconds, or what to do.
    std::string text = plugin_->text("enter");
    Tone tone = Tone::NORMAL;
    if (held) {
      const auto &tr = plugin_->tries(lock_.entity);
      text = tessera::format(plugin_->text("blocked"), (static_cast<int32_t>(tr.blocked_until - now) + 999) / 1000);
      tone = Tone::BUSY;
    } else if (said_ == Said::SENT) {
      text = plugin_->text("right");
      tone = Tone::GOOD;
    } else if (said_ == Said::FAILED && now - said_at_ < 4000) {
      text = plugin_->text("failed");
      tone = Tone::ALERT;
    } else if (said_ == Said::NO_CODE && now - said_at_ < 4000) {
      text = plugin_->text("no_helper_code");
      tone = Tone::ALERT;
    } else if (bad) {
      text = plugin_->text("wrong");
      tone = Tone::ALERT;
    }
    ui::set_text(line_, text);
    ui::set_tone(line_, tone, theme::MUTED);
    // Keys that do nothing now stand back: all of them while the keypad waits, clear and OK without a digit.
    for (int i = 0; i < 12; ++i) {
      const bool off = held || said_ == Said::SENT || ((i == CLEAR || i == OK) && !typed_len_);
      lv_obj_set_style_opa(keys_[i], off ? LV_OPA_40 : LV_OPA_COVER, 0);
    }
  }

  static constexpr const char *TAG = "code_lock";
  CodeLock *plugin_;
  Lock lock_;
  char typed_[MAX_DIGITS + 1] = "";   // what was typed while the card is open, gone with the card
  size_t typed_len_ = 0;
  Said said_ = Said::NOTHING;
  uint32_t said_at_ = 0;
  bool beside_ = false;
  int key_w_ = 0, key_h_ = 0, gap_ = 0, x0_ = 0, y0_ = 0, dot_max_ = 0, dots_y_ = 0, info_x_ = 0, info_w_ = 0, text_h_ = 0;
  lv_obj_t *keys_[12]{}, *labels_[12]{}, *dots_[MAX_DIGITS]{}, *line_{};
};

void CodeLock::setup() {
  add_tile("keypad", [this]() { return new KeypadTile(this); });
  add_card("keys", [this]() { return new KeysCard(this); });
}

}  // namespace esphome::code_lock
