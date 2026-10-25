// Ultra Street Fighter IV: the controller settings screen names Switch buttons (usf4_button_icons = "switch", 0.8.1).
//
// WHAT THE GAME DOES
//   The button layout screen (character select's Controls, the options' Controller Settings) lists each action with
//   the Xbox button it is on, as text: X, Y, A, B, LB, LT, RB, RT. The texts are entries of the game's string tables
//   ("#M4S" + language: ENG, FRA, GER, ITA, JPN, KOR, SPA; archive/ui/ui.eaf and the Ultra update's dl2/ae2.eaf), EMZ
//   streams the game inflates with its own zlib (inflate is sub_82776738). Each table holds the eight names once, one
//   after the other: UTF-16BE strings, each ending in a 0 unit, after the 0 that ends "NO FUNCTION" (or its
//   translation). The usf4_button_icons textures (usf4_button_icons.cpp) do not reach them: they are text in the font.
//
// WHAT THIS DOES
//   inflate runs as before; then, when the stream's output so far is one of those tables, the eight names are
//   rewritten in place to the Switch buttons: by position (input_xbox_layout, the default) X Y A B become Y X B A, by
//   letter they stay; LB LT RB RT become L ZL R ZR. Every new name fits in the old one's units (a shorter one ends
//   with an extra 0), so no string moves and every table offset stays right. A stream is followed from its first
//   inflate call while its output stays one block (next_out - total_out does not change), and dropped once patched,
//   at its end, or when its output moves; nothing outside what inflate wrote is read.
//   The game checks each EMZ stream's CRC-32 (its header) against what it inflated: a table with the names changed
//   and nothing else was refused whole (every text of the screen showed its id, ID_OPT_1108..., console 2026-10-25).
//   So the two units the shorter names free (after L and after R, never read: each string ends at its 0) get the
//   values that keep the CRC-32 of the 21 units, and so of the whole table, what it was (CRC is linear: what a change
//   inside the run does to the CRC does not depend on where the table ends; solved once for each layout on the PC,
//   checked on the seven tables). The names are only rewritten when inflate wrote the whole run in this call, so the
//   game's check cannot have read part of it already.
//
// CHECKS
//   inflate's code is compared with the code this was written from (FNV-1a of its bytes) at the first call; a
//   different game executable leaves the names alone. "xbox" (usf4_button_icons) leaves them alone too.
#include <array>
#include <atomic>
#include <cstdint>
#include <cstring>
#include <mutex>
#include <string>

#include <rex/cvar.h>
#include <rex/hook.h>
#include <rex/logging.h>
#include <rex/ppc.h>

#include "cpu/sa2_enhancements.h"

REXCVAR_DECLARE(std::string, usf4_button_icons);

namespace {

constexpr uint32_t kInflate = 0x82776738;
constexpr uint32_t kInflateBytes = 0x15A8;
constexpr uint64_t kInflateFnv = 0x50712898296D2DC7ull;

// z_stream (zlib 1.2, 32-bit, big-endian guest)
constexpr uint32_t kNextOut = 12, kTotalOut = 20;

// The eight names as the tables hold them, with the 0 before them: UTF-16 units, each name ended by 0.
constexpr std::array<uint16_t, 21> kXbox = {0, 'X', 0, 'Y', 0, 'A', 0, 'B', 0, 'L', 'B', 0, 'L', 'T', 0, 'R', 'B', 0,
                                            'R', 'T', 0};

uint32_t Load32(const uint8_t *base, uint32_t address) {
  uint32_t v;
  std::memcpy(&v, base + address, 4);
  return __builtin_bswap32(v);
}

uint16_t Load16(const uint8_t *base, uint32_t address) {
  uint16_t v;
  std::memcpy(&v, base + address, 2);
  return __builtin_bswap16(v);
}

void Store16(uint8_t *base, uint32_t address, uint16_t value) {
  value = __builtin_bswap16(value);
  std::memcpy(base + address, &value, 2);
}

bool Enabled(const uint8_t *base) {
  static const bool on = [base] {
    if (REXCVAR_GET(usf4_button_icons) == "xbox") return false;
    return sa2::enh::GuestCodeMatches(base, kInflate, kInflateBytes, kInflateFnv,
                                      "usf4_button_icons (button names, sub_82776738)");
  }();
  return on;
}

// A table being inflated: its z_stream and where its output starts.
struct Followed {
  uint32_t stream = 0, start = 0;
};
std::mutex g_mutex;
std::array<Followed, 8> g_followed;
std::atomic<uint32_t> g_following{0};  // entries in use (read without the lock as a hint)

// Looks for the names in [start, end) (whole UTF-16 units from start), written from `fresh` on (this inflate call),
// and rewrites them. Returns whether they were.
bool Rewrite(uint8_t *base, uint32_t start, uint32_t fresh, uint32_t end) {
  for (uint32_t at = start; at + kXbox.size() * 2 <= end; at += 2) {
    if (at < fresh) continue;
    bool same = true;
    for (uint32_t i = 0; i < kXbox.size() && same; ++i) same = Load16(base, at + i * 2) == kXbox[i];
    if (!same) continue;
    // By position (input_xbox_layout, the default) the Xbox button's name becomes the Switch button in its place.
    // Units 11 and 17 (after L's 0 and R's 0) keep the CRC-32 as it was.
    static const bool by_position = rex::cvar::GetFlagByName("input_xbox_layout") != "false";
    const std::array<uint16_t, kXbox.size()> names =
        by_position ? std::array<uint16_t, 21>{0, 'Y', 0, 'X', 0, 'B', 0, 'A', 0, 'L', 0, 0xC74E, 'Z', 'L', 0, 'R', 0,
                                               0x43DA, 'Z', 'R', 0}
                    : std::array<uint16_t, 21>{0, 'X', 0, 'Y', 0, 'A', 0, 'B', 0, 'L', 0, 0x52EA, 'Z', 'L', 0, 'R', 0,
                                               0x81AA, 'Z', 'R', 0};
    for (uint32_t i = 0; i < names.size(); ++i) Store16(base, at + i * 2, names[i]);
    static std::atomic<bool> logged{false};
    if (!logged.exchange(true))
      REXLOG_INFO("[usf4] button names: the string tables name Switch buttons ({})",
                  by_position ? "by position" : "by letter");
    return true;
  }
  return false;
}

void AfterInflate(uint8_t *base, uint32_t stream, uint32_t next_out_before, uint32_t total_before) {
  const uint32_t next_out = Load32(base, stream + kNextOut);
  const uint32_t total = Load32(base, stream + kTotalOut);
  const uint32_t start = next_out - total;
  std::lock_guard lock(g_mutex);
  Followed *slot = nullptr;
  for (Followed &f : g_followed)
    if (f.stream == stream) slot = &f;
  if (total_before == 0 && next_out - next_out_before == total && total >= 8 && !slot) {
    // a stream's first output: a string table?
    if (std::memcmp(base + start, "#M4S#", 5) != 0) return;
    for (Followed &f : g_followed)
      if (f.stream == 0) {
        slot = &f;
        break;
      }
    if (!slot) return;
    *slot = {stream, start};
    ++g_following;
  }
  if (!slot) return;
  if (slot->start != start || Rewrite(base, start, next_out_before, next_out) || total > (1u << 20)) {
    *slot = {};
    --g_following;
  }
}

}  // namespace

REX_EXTERN(__imp__sub_82776738);
REX_HOOK_RAW(sub_82776738) {
  const uint32_t stream = ctx.r3.u32;
  if (stream == 0 || !Enabled(base)) {
    __imp__sub_82776738(ctx, base);
    return;
  }
  const uint32_t next_out_before = Load32(base, stream + kNextOut);
  const uint32_t total_before = Load32(base, stream + kTotalOut);
  __imp__sub_82776738(ctx, base);
  const int32_t result = int32_t(ctx.r3.u32);
  if (result < 0 && result != -5) {
    // an error (Z_BUF_ERROR, -5, only means no progress): a followed stream is dropped
    std::lock_guard lock(g_mutex);
    for (Followed &f : g_followed)
      if (f.stream == stream) {
        f = {};
        --g_following;
      }
    return;
  }
  if (total_before == 0 || g_following != 0) AfterInflate(base, stream, next_out_before, total_before);
  if (result == 1) {
    // Z_STREAM_END: the stream is done whatever it was
    std::lock_guard lock(g_mutex);
    for (Followed &f : g_followed)
      if (f.stream == stream) {
        f = {};
        --g_following;
      }
  }
}
