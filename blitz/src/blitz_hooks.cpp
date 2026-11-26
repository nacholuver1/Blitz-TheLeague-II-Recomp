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
// sub_8232DF78 (game start-up) stores the view's height / width ratio at
// 0x828834B4: 9/16 = 0.5625 with a widescreen video mode, 0.75 without. The
// camera projection functions (sub_8228E690, sub_82291400, ...) read it, and
// sub_8232DF40 returns 1 / ratio as the aspect ratio. Views also get an aspect
// ratio through two setters (sub_8243A8A8, sub_8242F210: view+732, dirty bit),
// which sub_8228BEE8 calls with the 16:9 constant directly. With a wider screen
// shape chosen, the ratio is set to match and the setters turn 16:9 into it, so
// the 3D view widens (the picture keeps its pixels and is presented at the
// chosen shape). The game lays its 2D overlay out for the shape too: menus
// stay 16:9 in the middle and the HUD moves to the sides (tested 2026-11-26).
// ---------------------------------------------------------------------------
namespace {
constexpr u32 kViewHeightOverWidth = 0x828834B4;

bool Widened() { return blitz::AspectRatio() > 16.0 / 9.0 + 0.01; }

f32 WidenAspect(f32 aspect) {
  return Widened() && std::fabs(aspect - 16.0f / 9.0f) < 0.01f
             ? static_cast<f32>(blitz::AspectRatio())
             : aspect;
}
}  // namespace

REX_HOOK_RAW(sub_8232DF78) {
  __imp__sub_8232DF78(ctx, base);
  if (Widened()) {
    REX_STORE_U32(kViewHeightOverWidth,
                  std::bit_cast<u32>(static_cast<f32>(1.0 / blitz::AspectRatio())));
  }
}

REX_DETOUR(sub_8243A8A8, void, (u32 view, f32 aspect)) { original(view, WidenAspect(aspect)); }

REX_DETOUR(sub_8242F210, void, (u32 view, f32 aspect)) { original(view, WidenAspect(aspect)); }
