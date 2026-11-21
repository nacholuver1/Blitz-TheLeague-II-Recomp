// blitz2 (Blitz: The League II) - ReXGlue Recompiled Project

#include "generated/default/blitz2_funcs.h"

#include <rex/ppc/hook.h>

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
