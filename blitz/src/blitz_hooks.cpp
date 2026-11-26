// blitz2 (Blitz: The League II) - ReXGlue Recompiled Project

#include "generated/default/blitz2_funcs.h"

#include <rex/ppc/hook.h>

#include <bit>
#include <cmath>
#include <cstring>

#include "blitz_settings.h"

// ---------------------------------------------------------------------------
// Optional intro skip (blitz.skip_intro).
//
// sub_825A9A48 is CRI Sofdec's mwPlyStartFname(handle, fname) (found by its
// "mwPlyStartFname: handle is invalid" / "fname is NULL" error strings). At boot
// the game plays d:\movies\X_LOGO_W.sfd through it. When that file can't be
// opened the game simply moves on to loading, so the skip renames the request
// in place to a file that doesn't exist (same length). Every other movie
// (injuries, cutscenes, trailers) plays as normal.
// ---------------------------------------------------------------------------
REX_DETOUR(sub_825A9A48, void, (u32 handle, char* fname)) {
  if (fname && blitz::Settings().skip_intro.Get()) {
    if (char* hit = std::strstr(fname, "X_LOGO_W.")) hit[7] = 'Z';  // X_LOGO_Z.sfd
  }
  original(handle, fname);
}

// ---------------------------------------------------------------------------
// No lock-wrapper fix needed.
//
// Blitz 1 hung a second into boot because its recursive-lock unlock cleared
// the owner field after RtlLeaveCriticalSection (fixed there by a hook). Blitz 2
// ships the corrected unlock (lock_enter 0x826F4BB8, lock_leave 0x826F4C20):
// the owner is reset, or the count decremented, before the critical section is
// released.
// ---------------------------------------------------------------------------

// ---------------------------------------------------------------------------
// Offline user-created content.
//
// sub_82491900 is XAPI's XUserCheckPrivilege(user, privilege, &result) (exact
// match of Blitz 1's sub_82429490). Blitz 1's campaign create-a-team save
// required privilege 247 (XPRIVILEGE_USER_CREATED_CONTENT), which a local,
// non-Live profile never has. Grant only that privilege; communications (252)
// and multiplayer (254) still go through the original.
// ---------------------------------------------------------------------------
namespace {
constexpr u32 kPrivilegeUserCreatedContent = 247;
}

REX_DETOUR(sub_82491900, u32, (u32 user_index, u32 privilege, be_u32* result)) {
  if (privilege == kPrivilegeUserCreatedContent && result) {
    *result = 1;
    return 0;  // ERROR_SUCCESS
  }
  return original(user_index, privilege, result);
}

// ---------------------------------------------------------------------------
// Ultrawide (blitz.aspect_ratio).
//
// The game cameras (sub_82338A10, sub_8228E690, ...) fill a view command
// (vtable 0x82058CA8) with a horizontal size at +164 and a vertical size at
// +168 = horizontal * 0x828834B4, the height / width ratio start-up sets to
// 9/16 (sub_8232DF78), and hand it to sub_822A4EA8, which copies it into the
// render queue. Widening only the horizontal size there gives Hor+: the same
// vertical view, more to the sides (the picture keeps its pixels and is
// presented at the chosen shape). Changing the ratio global instead crops the
// height, and the 2D layout reads it too: its menus and HUD then reach past
// the 16:9 edges, where the game parks unused pieces (a white dash in play,
// a "00" box on the matchup screen). Turning the 16:9 the view setters
// sub_8243A8A8 / sub_8242F210 (view+732) receive into the wider shape does the
// same to the 2D layout, so those are left alone too: the 2D layout stays 16:9
// and is stretched with the picture (tested 2026-11-26).
// ---------------------------------------------------------------------------
namespace {
constexpr u32 kViewCommandVtable = 0x82058CA8;
constexpr u32 kViewHeightOverWidth = 0x828834B4;

bool Widened() { return blitz::AspectRatio() > 16.0 / 9.0 + 0.01; }
}  // namespace

REX_HOOK_RAW(sub_822A4EA8) {
  const u32 command = ctx.r4.u32;
  if (command && Widened() && REX_LOAD_U32(command) == kViewCommandVtable) {
    const f32 ratio = std::bit_cast<f32>(REX_LOAD_U32(kViewHeightOverWidth));
    const f32 horizontal = std::bit_cast<f32>(REX_LOAD_U32(command + 164));
    const f32 vertical = std::bit_cast<f32>(REX_LOAD_U32(command + 168));
    // A camera view: the vertical size was derived from the horizontal one.
    if (ratio > 0.0f && horizontal > 0.0f &&
        std::fabs(vertical - horizontal * ratio) <= 1e-4f * horizontal) {
      const f32 widen = static_cast<f32>(blitz::AspectRatio() * ratio);  // 4/3 for 64:27
      REX_STORE_U32(command + 164, std::bit_cast<u32>(horizontal * widen));
    }
  }
  __imp__sub_822A4EA8(ctx, base);
}

