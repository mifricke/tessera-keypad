#include "code_lock.h"
#include "esphome/core/hal.h"
#include <algorithm>
#include <cstring>
#include <string>

namespace esphome::code_lock {

using tessera::Font;
using tessera::Tone;
namespace ui = tessera::ui;

static constexpr uint32_t ICON_LOCK = 0xF033E, ICON_LOCK_OPEN = 0xF033F, ICON_CHECK = 0xF012C, ICON_CLOSE = 0xF0156;
static constexpr size_t MAX_DIGITS = 16;   // what the keypad takes; the dots show at most DOTS
static constexpr int DOTS = 8;

// The action the right code runs, by the entity's domain (named in the manifest's home_assistant_actions).
static const char *service_for(const char *entity) {
  static const struct { const char *domain, *service; } MAP[] = {
      {"lock.", "lock.unlock"},         {"script.", "script.turn_on"},        {"scene.", "scene.turn_on"},
      {"button.", "button.press"},      {"input_button.", "input_button.press"}, {"switch.", "switch.turn_on"},
      {"automation.", "automation.trigger"}, {"cover.", "cover.open_cover"},
  };
  for (const auto &m : MAP)
    if (std::strncmp(entity, m.domain, std::strlen(m.domain)) == 0) return m.service;
  return nullptr;
}

// The largest of the screen's fonts whose line fits `room` pixels, for words (`digits` false) or for digits alone.
static Font fitting(int room, bool digits) {
  const Font faces[] = {Font::DIGITS, Font::VALUE, Font::HEADLINE, Font::TITLE, Font::BODY_LARGE, Font::BODY};
  for (Font f : faces) {
    if (f == Font::DIGITS && !digits) continue;
    if (ui::line_height(f) <= room) return f;
  }
  return Font::BODY;
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
    std::strncpy(lock_.entity, c.entity ? c.entity : "", sizeof(lock_.entity) - 1);
    std::strncpy(lock_.code, c.options["code"] | "", sizeof(lock_.code) - 1);
    lock_.pass_code = c.options["pass_code"] | false;
    lock_.attempts = std::max(1, c.options["attempts"] | 3);
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
    if (!data["name"].isNull() && !name_[0]) std::strncpy(name_, data["name"] | "", sizeof(name_) - 1);
    std::strncpy(state_, data["state"] | "", sizeof(state_) - 1);
    paint();
  }

  void on_theme() override {
    ui::set_color(icon_, theme::ACCENT);
    ui::set_color(name_label_, theme::INK);
    ui::set_color(line_, theme::MUTED);
  }

  void on_tap() override {
    if (!ready()) return;
    plugin_->chosen = lock_;
    tessera::open_card(plugin_->plugin_id(), "keys", lock_.entity, tile_);
  }

 private:
  // A code to check, or a lock that checks it itself; and an entity the plugin knows how to act on.
  bool ready() const {
    return lock_.entity[0] && service_for(lock_.entity) && (lock_.code[0] || lock_.pass_code);
  }

  // What the entity is now, in the screen's words, or what to do.
  std::string line() const {
    if (!lock_.entity[0] || !service_for(lock_.entity)) return plugin_->text("choose");
    if (!lock_.code[0] && !lock_.pass_code) return plugin_->text("no_code");
    const char *s = state_;
    if (!std::strcmp(s, "unavailable")) return plugin_->text("unavailable");
    if (!std::strcmp(s, "locked")) return plugin_->text("locked");
    if (!std::strcmp(s, "unlocked")) return plugin_->text("unlocked");
    if (!std::strcmp(s, "open")) return plugin_->text("open");
    if (!std::strcmp(s, "closed")) return plugin_->text("closed");
    if (!std::strncmp(lock_.entity, "switch.", 7) && !std::strcmp(s, "on")) return plugin_->text("on");
    if (!std::strncmp(lock_.entity, "switch.", 7) && !std::strcmp(s, "off")) return plugin_->text("off");
    return plugin_->text("tap");
  }

  void paint() {
    const bool open = !std::strcmp(state_, "unlocked") || !std::strcmp(state_, "open");
    ui::set_text(icon_, ui::icon(open ? ICON_LOCK_OPEN : ICON_LOCK));
    ui::set_text(name_label_, name_);
    ui::set_text(line_, line());
    // The icon, the name and the line under it, together in the middle; a low tile drops the icon, then the name.
    const int ih = ui::line_height(Font::ICON), nh = ui::line_height(Font::TITLE), lh = ui::line_height(Font::BODY);
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

// ---- The keypad: dots for what was typed, a line that says how it went, and twelve keys ----
class KeysCard : public tessera::Card {
 public:
  explicit KeysCard(CodeLock *plugin) : plugin_(plugin) {}

  void open(const tessera::CardContext &c) override {
    lock_ = plugin_->chosen;
    const int pad = ui::px(12), gap = ui::px(8);
    const int msg_h = ui::line_height(Font::BODY_LARGE);
    dot_ = std::max(ui::px(10), ui::mm(2));
    const int head = dot_ + ui::px(10) + msg_h + ui::px(14);
    // Twelve keys in four rows of three: as large as the room allows, never taller than wide, a finger's size at least
    // where the glass has it.
    key_w_ = (c.width - 2 * pad - 2 * gap) / 3;
    key_h_ = (c.height - 2 * pad - head - 3 * gap) / 4;
    key_h_ = std::min(key_h_, key_w_);
    key_w_ = std::min(key_w_, key_h_ * 16 / 10);
    gap_ = gap;
    const int grid_w = 3 * key_w_ + 2 * gap, grid_h = 4 * key_h_ + 3 * gap;
    x0_ = (c.width - grid_w) / 2;
    const int top = std::max(pad, (c.height - head - grid_h) / 2);
    y0_ = top + head;

    for (int i = 0; i < DOTS; ++i) {
      dots_[i] = ui::block(c.parent, theme::TRACK);
      lv_obj_set_size(dots_[i], dot_, dot_);
      lv_obj_set_style_radius(dots_[i], LV_RADIUS_CIRCLE, 0);
    }
    dots_y_ = top;
    width_ = c.width;
    msg_ = ui::label(c.parent, Font::BODY_LARGE, theme::MUTED);
    lv_obj_set_width(msg_, c.width - 2 * pad);
    lv_obj_set_pos(msg_, pad, top + dot_ + ui::px(10));
    lv_obj_set_style_text_align(msg_, LV_TEXT_ALIGN_CENTER, 0);

    const Font digit = fitting(key_h_ * 7 / 10, true);
    const Font icon = ui::line_height(Font::ICON) <= key_h_ * 8 / 10 ? Font::ICON : Font::ICON_SMALL;
    for (int i = 0; i < 12; ++i) {
      const int row = i / 3, col = i % 3;
      lv_obj_t *k = ui::block(c.parent, i == 11 ? theme::ACCENT : theme::RAISED);
      lv_obj_remove_flag(k, LV_OBJ_FLAG_CLICKABLE);
      lv_obj_remove_flag(k, LV_OBJ_FLAG_SCROLLABLE);
      lv_obj_set_style_pad_all(k, 0, 0);
      lv_obj_set_size(k, key_w_, key_h_);
      lv_obj_set_pos(k, x0_ + col * (key_w_ + gap), y0_ + row * (key_h_ + gap));
      const bool is_icon = i == 9 || i == 11;
      const Font face = is_icon ? icon : digit;
      lv_obj_t *l = ui::label(k, face, i == 11 ? theme::ON_ACCENT : theme::INK);
      lv_obj_set_width(l, key_w_);
      lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0);
      lv_obj_set_pos(l, 0, (key_h_ - ui::line_height(face)) / 2);
      if (i == 9) ui::set_text(l, ui::icon(ICON_CLOSE));
      else if (i == 11) ui::set_text(l, ui::icon(ICON_CHECK));
      else ui::set_text(l, std::string(1, i == 10 ? '0' : static_cast<char>('1' + i)));
      keys_[i] = k;
      labels_[i] = l;
    }
    paint();
  }

  void on_theme() override {
    for (int i = 0; i < 12; ++i) {
      ui::set_fill(keys_[i], Tone::NORMAL, i == 11 ? theme::ACCENT : theme::RAISED);
      ui::set_color(labels_[i], i == 11 ? theme::ON_ACCENT : theme::INK);
    }
    paint();
  }

  void on_tick(uint32_t) override {
    const uint32_t now = esphome::millis();
    // The right code: the card closes a moment after it said so.
    if (done_ && now - done_at_ >= 1500) {
      tessera::close_card();
      return;
    }
    paint();
  }

  void on_tap() override {
    if (done_ || blocked()) return;
    const tessera::Point p = tessera::tap_point();
    const int key = key_at(p.x, p.y);
    if (key < 0) return;
    said_ = Said::NOTHING;
    if (key == 9) {
      clear();
    } else if (key == 11) {
      if (typed_len_ || (lock_.pass_code && !lock_.code[0])) submit();
    } else if (typed_len_ < MAX_DIGITS) {
      typed_[typed_len_++] = key == 10 ? '0' : static_cast<char>('1' + key);
      typed_[typed_len_] = 0;
      // A code of known length is checked as its last digit comes, without OK.
      if (lock_.code[0] && typed_len_ == std::strlen(lock_.code)) submit();
    }
    paint();
  }

 private:
  enum class Said : uint8_t { NOTHING, WRONG, RIGHT, FAILED };

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
    if (lock_.code[0] && !matches()) {
      clear();
      said_ = Said::WRONG;
      if (++t.wrong >= lock_.attempts) {
        t.wrong = 0;
        t.blocked_until = esphome::millis() + static_cast<uint32_t>(lock_.wait_s) * 1000u;
        if (!t.blocked_until) t.blocked_until = 1;
      }
      return;
    }
    t.wrong = 0;
    const char *service = service_for(lock_.entity);
    bool sent = false;
    if (service) {
      if (lock_.pass_code && !std::strcmp(service, "lock.unlock") && typed_len_) {
        JsonDocument d;
        d["code"] = typed_;
        sent = tessera::action(service, lock_.entity, d);
      } else {
        sent = tessera::action(service, lock_.entity);
      }
    }
    clear();
    said_ = sent ? Said::RIGHT : Said::FAILED;
    if (sent) {
      done_ = true;
      done_at_ = esphome::millis();
    }
  }

  void paint() {
    // The dots: as many as the code has, filled as far as it is typed; without a known length one per digit typed.
    const size_t known = std::strlen(lock_.code);
    const int shown = std::min<int>(DOTS, known ? static_cast<int>(known) : static_cast<int>(typed_len_));
    const int filled = std::min<int>(shown, static_cast<int>(typed_len_));
    const int step = dot_ + ui::px(10);
    const int left = (width_ - (shown * step - ui::px(10))) / 2;
    const Tone tone = said_ == Said::WRONG ? Tone::ALERT : said_ == Said::RIGHT ? Tone::GOOD : Tone::ACCENT;
    for (int i = 0; i < DOTS; ++i) {
      if (i >= shown) {
        lv_obj_add_flag(dots_[i], LV_OBJ_FLAG_HIDDEN);
        continue;
      }
      lv_obj_remove_flag(dots_[i], LV_OBJ_FLAG_HIDDEN);
      lv_obj_set_pos(dots_[i], left + i * step, dots_y_);
      ui::set_fill(dots_[i], i < filled ? tone : Tone::NORMAL, theme::TRACK);
    }
    // The line under the dots: the wait after wrong tries, how it went, or what to do.
    std::string text;
    Tone t = Tone::NORMAL;
    if (blocked()) {
      const auto &tr = plugin_->tries(lock_.entity);
      const long left_s = (static_cast<int32_t>(tr.blocked_until - esphome::millis()) + 999) / 1000;
      text = tessera::format(plugin_->text("blocked"), left_s);
      t = Tone::BUSY;
    } else if (said_ == Said::WRONG) {
      text = plugin_->text("wrong");
      t = Tone::ALERT;
    } else if (said_ == Said::RIGHT) {
      text = plugin_->text("right");
      t = Tone::GOOD;
    } else if (said_ == Said::FAILED) {
      text = plugin_->text("failed");
      t = Tone::BUSY;
    } else if (!typed_len_) {
      text = plugin_->text("enter");
    }
    ui::set_text(msg_, text);
    ui::set_tone(msg_, t, theme::MUTED);
  }

  CodeLock *plugin_;
  Lock lock_;
  char typed_[MAX_DIGITS + 1] = "";   // what was typed while the card is open, gone with the card
  size_t typed_len_ = 0;
  Said said_ = Said::NOTHING;
  bool done_ = false;
  uint32_t done_at_ = 0;
  int key_w_ = 0, key_h_ = 0, gap_ = 0, x0_ = 0, y0_ = 0, dot_ = 0, dots_y_ = 0, width_ = 0;
  lv_obj_t *keys_[12]{}, *labels_[12]{}, *dots_[DOTS]{}, *msg_{};
};

void CodeLock::setup() {
  add_tile("keypad", [this]() { return new KeypadTile(this); });
  add_card("keys", [this]() { return new KeysCard(this); });
}

}  // namespace esphome::code_lock
