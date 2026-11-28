// blitz2 (Blitz: The League II) - ReXGlue Recompiled Project

#include "generated/default/blitz2_funcs.h"

#include <rex/ppc/hook.h>

#include <algorithm>
#include <atomic>
#include <bit>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <cstring>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <cerrno>
#include <time.h>
#endif
#include <immintrin.h>

#include "blitz_settings.h"

#include <rex/logging/log_category.h>

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

// ---------------------------------------------------------------------------
// Smart wait (blitz.smart_wait, blitz.wait_spin_us, blitz.wait_sleep_us).
//
// Two of the game's waits spin, which is free on the console (its db16cyc
// hint hands the core to the other hardware thread) but keeps a whole host
// core busy, about 2 of the 3.4 cores the game used in the attract demo
// (2026-11-27, laptop slowness reports 159199 / 159384):
//  - the main thread's wait for its jobs, sub_82636DA8, polls a few flags and
//    calls sub_827493B0 (NtYieldExecution) between polls; the host yield
//    returns at once when nothing else is ready to run;
//  - the D3D library's GPU waits (BlockOnFence and others) call sub_824AF628
//    on every pass: 32 db16cyc (nothing on the host), then the fence and hang
//    checks; it returns 1 to keep waiting.
// After spinning wait_spin_us, each further pass sleeps wait_sleep_us on a
// high-resolution timer. The yield wrapper has other callers; only calls
// whose return address is inside sub_82636DA8 back off. Idea from the
// Pirates of the Caribbean: At World's End and Guitar Hero: Warriors of Rock
// ReXGlue recomps (docs/cpu-usage.md, docs/fixes-from-gh5.md), written fresh.
// ---------------------------------------------------------------------------
namespace {
using WaitClock = std::chrono::steady_clock;

constexpr u32 kJobWaitBegin = 0x82636DA8;  // sub_82636DA8
constexpr u32 kJobWaitEnd = 0x82636F18;    // next function, sub_82636F18

// One wait in progress on this thread: polls closer together than this belong
// to the same wait (sleeps are at most 1 ms, the setting's maximum).
constexpr auto kSameWaitGap = std::chrono::microseconds(1500);

struct WaitState {
  u32 site = 0;  // the poll's return address or function
  WaitClock::time_point start{};
  WaitClock::time_point last{};
};
thread_local WaitState t_wait;

#if defined(_WIN32)
// This thread's high-resolution timer, closed when the thread exits.
struct WaitTimer {
  HANDLE handle = CreateWaitableTimerExW(nullptr, nullptr, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION,
                                         TIMER_ALL_ACCESS);
  ~WaitTimer() {
    if (handle) CloseHandle(handle);
  }
};
#endif

void SleepMicroseconds(i32 us) {
#if defined(_WIN32)
  thread_local WaitTimer wait_timer;
  const HANDLE timer = wait_timer.handle;
  if (timer) {
    LARGE_INTEGER due;
    due.QuadPart = -static_cast<LONGLONG>(us) * 10;  // relative, 100 ns units
    if (SetWaitableTimer(timer, &due, 0, nullptr, nullptr, FALSE)) {
      WaitForSingleObject(timer, INFINITE);
      return;
    }
  }
  Sleep(1);  // no high-resolution timer (Windows before 10 1803)
#else
  timespec ts{0, static_cast<long>(us) * 1000};
  while (nanosleep(&ts, &ts) == -1 && errno == EINTR) {
  }  // a signal cut it short: sleep the rest
#endif
}

// Called on each pass of a wait that is not over yet.
void BackOff(u32 site) {
  const auto now = WaitClock::now();
  if (t_wait.site != site || now - t_wait.last > kSameWaitGap) {
    t_wait.site = site;
    t_wait.start = now;
  }
  const auto& settings = blitz::Settings();
  if (now - t_wait.start < std::chrono::microseconds(settings.wait_spin_us.Get())) {
    _mm_pause();
  } else {
    SleepMicroseconds(settings.wait_sleep_us.Get());
  }
  t_wait.last = WaitClock::now();
}
}  // namespace

REX_HOOK_RAW(sub_827493B0) {
  const u32 caller = static_cast<u32>(ctx.lr);
  __imp__sub_827493B0(ctx, base);
  if (caller >= kJobWaitBegin && caller < kJobWaitEnd && blitz::Settings().smart_wait.Get()) {
    BackOff(caller);
  }
}

REX_HOOK_RAW(sub_824AF628) {
  __imp__sub_824AF628(ctx, base);
  if (ctx.r3.u32 == 1 && blitz::Settings().smart_wait.Get()) {
    BackOff(0x824AF628);
  }
}

// ---------------------------------------------------------------------------
// Audio barrier (sound stopping for good). sub_824D9538 is a rendezvous in the
// game's audio library: each audio thread sets its byte (index = its processor
// number, PCR byte 268) in a shared 64-bit word, then spins until the word is
// either all expected bytes (it then clears it) or zero (another thread cleared
// it). On the Xbox each thread owns a hardware thread and is never paused. On a
// PC, a thread paused after the word filled up can miss both states: the
// clearing thread clears, comes round again and sets its byte for the next
// round, and the paused thread then sees neither "full" nor "zero" while the
// other waits for its byte - both spin forever (seen in Blitz: The League 1 on
// 2026-11-27: two audio threads in its copy of this loop, sub_82469588 there).
// The host version keeps the barrier on the host: a mutex-guarded mask of the
// threads that arrived and a round counter. The last arrival clears the mask,
// ends the round and clears the game's word; the others wait for the round to
// change, so a paused thread always gets out of the round it was in. The game's
// word still gets each thread's byte and the final zero.
// ---------------------------------------------------------------------------
namespace {
struct AudioBarrier {
  std::mutex mutex;
  u64 arrived = 0;              // bytes in REX_LOAD_U64 order, guarded by mutex
  std::atomic<u32> round{0};
};

AudioBarrier& FindAudioBarrier(u32 word) {
  static std::mutex mutex;
  static std::unordered_map<u32, AudioBarrier> barriers;  // nodes never move
  std::lock_guard lock(mutex);
  return barriers[word];
}
}  // namespace

REX_HOOK_RAW(sub_824D9538) {
  if (!blitz::Settings().audio_barrier_fix.Get()) {
    __imp__sub_824D9538(ctx, base);
    return;
  }
  const u32 object = ctx.r3.u32;
  const u32 word = ctx.r4.u32;
  if (REX_LOAD_U32(object + 304) == 0) {
    return;
  }
  // One byte per hardware thread (byte k = slot at object + 308 + 4k); byte 0
  // is the top byte of the value REX_LOAD_U64 returns.
  u64 expected = 0;
  for (u32 k = 0; k < 6; ++k) {
    if (REX_LOAD_U32(object + 308 + 4 * k) != 0) {
      expected |= u64{1} << (56 - 8 * k);
    }
  }
  const u8 processor = REX_LOAD_U8(ctx.r13.u32 + 268);
  if (processor >= 8) {
    // Outside the 8-byte word: never seen; leave it to the game's own code.
    __imp__sub_824D9538(ctx, base);
    return;
  }
  AudioBarrier& barrier = FindAudioBarrier(word);
  u32 my_round;
  {
    std::lock_guard lock(barrier.mutex);
    REX_STORE_U8(word + processor, 1);
    barrier.arrived |= u64{1} << (56 - 8 * processor);
    if ((barrier.arrived & expected) == expected) {
      barrier.arrived = 0;
      REX_STORE_U64(word, 0);
      barrier.round.fetch_add(1, std::memory_order_release);
      return;
    }
    my_round = barrier.round.load(std::memory_order_relaxed);
  }
  for (u32 spins = 0; barrier.round.load(std::memory_order_acquire) == my_round; ++spins) {
    if (spins < 2000) {
      _mm_pause();
    } else {
      std::this_thread::yield();
    }
  }
}

// ---------------------------------------------------------------------------
// Frame rate: 60 (the game's own) or 30 mode (blitz.frame_mode).
//
// The game's speed is one fixed game step per main-loop pass, and each pass
// also runs the frame task sub_82331858, which renders and presents (the
// present waits for the 60 Hz vblank). Below 60 fps the game runs in slow
// motion. The frame task has the console's own frame skip, but a graphics
// flag ([[[0x82985B38]+8]+12], 1 from boot) bypasses it, and even forced on it
// only reaches 3 game steps per 2 presents (its 2-frame window counts the
// skipped, nearly free frame; Blitz 1 logs 2026-11-27; Blitz 2 has the
// same frame task and flag).
//
// 30 mode: the frame task renders on every other pass and skips the render on
// the passes between, so each drawn frame carries two game steps. A skipped
// pass waits until two guest vblanks have passed since the last one, so a
// fast GPU still runs 60 steps a second and not 120. Loading screens (game
// mode 17/18), the frame task's second early check and scenes the game itself
// locks to 30 fps keep the game's own path, which renders every pass, as the
// game does there. The cutscenes (players running on, cheerleaders, coaches,
// celebrations) present with a swap interval of 2: the graphics library's swap
// callback gets the interval with each swap, and 30 mode reads the last one,
// or those scenes would get a second game step and run at double speed.
//
// blitz.frame_diag (off, not saved): once a second, log frame-task passes,
// renders skipped, game steps (sub_82333680), presents (sub_824B33A0), the
// game mode and the early-check state.
// ---------------------------------------------------------------------------
namespace {
constexpr u32 kGameMode = 0x828834C0;

struct VblankClock {
  std::atomic<u64> count{0};
  std::mutex mutex;
  std::condition_variable cv;
};
VblankClock g_vblank;

struct FramePacer {  // main thread only
  bool active = false;      // in 30-mode pairs (false: 60 mode or the game's own path)
  bool render_next = true;
  u64 mark = 0;             // vblank count the next skipped pass waits two vblanks from
};
FramePacer g_pacer;

// Set while 30 mode draws a frame that carries two game steps (see the frame
// timer hook below).
bool g_pair_render = false;

// The scene update ran during this pass's game step (set by its hook, read and
// cleared by the frame task; same thread, the step runs first). In the game's
// loading modes 17/18, 30 mode leaves the frame task to the game until the
// scene update has run two passes in a row (the kickoff run-on starts while the
// mode still says loading), then keeps 30 mode until the mode changes.
bool g_scene_this_pass = false;
int g_scene_passes = 0;
bool g_loading_scene = false;

// The game's last present interval in vblanks (1 = 60 fps, 2 = locked 30).
std::atomic<u32> g_present_interval{0};  // 0 until the first swap

// A skipped pass: wait until two vblanks have passed since the last one.
void WaitSecondVblank() {
  const u64 target = g_pacer.mark + 2;
  if (g_vblank.count.load() < target) {
    std::unique_lock lock(g_vblank.mutex);
    // Bounded: vblanks always run, but never hang the game if they stop.
    g_vblank.cv.wait_for(lock, std::chrono::milliseconds(50),
                         [&] { return g_vblank.count.load() >= target; });
  }
  // A late pass starts a new grid here; missed vblanks are not made up.
  g_pacer.mark = g_vblank.count.load();
}

bool EarlyCheckB(u8* base) {
  if (u32 p = REX_LOAD_U32(0x82985B5C)) {
    if (u32 q = REX_LOAD_U32(p + 240)) return REX_LOAD_U8(q + 347) != 0;
  }
  return false;
}

bool EarlyCheckA(u8* base) {
  if (u32 a = REX_LOAD_U32(0x82985B38)) {
    if (u32 b = REX_LOAD_U32(a + 8)) return REX_LOAD_U32(b + 12) != 0;
  }
  return false;
}

struct FrameDiag {
  std::atomic<u32> frames{0}, early_a{0}, early_b{0}, locked30{0}, skipped{0}, renders{0}, steps{0},
      presents{0}, scene_updates{0}, halved{0};
  std::chrono::steady_clock::time_point last{};
  bool started = false;
};
FrameDiag g_frame_diag;

void FrameDiagTick(u8* base) {
  auto& d = g_frame_diag;
  const auto now = std::chrono::steady_clock::now();
  if (!d.started) {
    d.started = true;
    d.last = now;
    return;
  }
  if (now - d.last < std::chrono::seconds(1)) return;
  const double secs = std::chrono::duration<double>(now - d.last).count();
  REXLOG_INFO(
      "frame diag: {:.2f}s mode{} frames {} skipped {} renders {} steps {} presents {} | early a {} "
      "b {} | game mode {} vblanks {} | interval {} locked30 passes {} | scene updates {} halved {}",
      secs, blitz::FrameMode30() ? 30 : 60, d.frames.exchange(0), d.skipped.exchange(0),
      d.renders.exchange(0), d.steps.exchange(0), d.presents.exchange(0), d.early_a.exchange(0),
      d.early_b.exchange(0), u32{REX_LOAD_U32(kGameMode)}, g_vblank.count.load(),
      g_present_interval.load(), d.locked30.exchange(0), d.scene_updates.exchange(0),
      d.halved.exchange(0));
  d.last = now;
}
}  // namespace

// ---------------------------------------------------------------------------
// Scene time step in 30 mode. The game's frame timer (sub_824453D8, timer object
// [0x82985B28]) runs once per drawn frame from the render path: it stores the
// seconds since its previous call as a double at +200, plus the frame rate and
// a 64-frame frame-rate ring. The in-engine scenes (players running on, huddles,
// cheerleaders, coaches, celebrations) read that time step on every game step
// (sub_823A1F98). In 30 mode a drawn frame carries two game steps, so both read
// the whole frame's time and the scenes ran at double speed (found in Blitz 1,
// 2026-11-28; Blitz 2 has the same timer). When 30 mode draws such a frame, the
// time step is halved after the timer runs, so the two steps together add up to
// real time. It stays halved until the next drawn frame's timer call overwrites
// it; the frame rate and the ring (read by the render side) are left as measured.
// ---------------------------------------------------------------------------
constexpr u32 kFrameTimerPtr = 0x82985B28;

REX_HOOK_RAW(sub_824453D8) {
  const u32 timer = ctx.r3.u32;
  __imp__sub_824453D8(ctx, base);
  if (g_pair_render && timer != 0 && timer == REX_LOAD_U32(kFrameTimerPtr)) {
    const f64 step = std::bit_cast<f64>(u64{REX_LOAD_U64(timer + 200)});
    REX_STORE_U64(timer + 200, std::bit_cast<u64>(step * 0.5));
    if (blitz::Settings().frame_diag.Get()) g_frame_diag.halved++;
  }
}

// The scene update that reads the frame time step.
REX_HOOK_RAW(sub_823A1F98) {
  g_scene_this_pass = true;
  if (blitz::Settings().frame_diag.Get()) g_frame_diag.scene_updates++;
  __imp__sub_823A1F98(ctx, base);
}

// The graphics library's swap callback: the present interval of each swap is in
// the argument's bits 8-11.
REX_HOOK_RAW(sub_824B2D00) {
  g_present_interval.store((ctx.r3.u32 >> 8) & 0xF, std::memory_order_relaxed);
  __imp__sub_824B2D00(ctx, base);
}

// The game's graphics interrupt callback (registered with
// VdSetGraphicsInterruptCallback): r3 = source, 0 = vblank.
REX_HOOK_RAW(sub_824A8FD0) {
  if (ctx.r3.u32 == 0) {
    {
      std::lock_guard lock(g_vblank.mutex);
      g_vblank.count.fetch_add(1);
    }
    g_vblank.cv.notify_all();
  }
  __imp__sub_824A8FD0(ctx, base);
}

REX_HOOK_RAW(sub_82331858) {
  const bool mode30 = blitz::FrameMode30();
  const bool diag = blitz::Settings().frame_diag.Get();
  if (!mode30 && !diag) {
    g_pacer.active = false;
    g_scene_this_pass = false;
    g_scene_passes = 0;
    g_loading_scene = false;
    __imp__sub_82331858(ctx, base);
    return;
  }
  auto& d = g_frame_diag;
  const u32 renders_before = d.renders.load();
  if (diag) {
    if (EarlyCheckA(base)) d.early_a++;
    if (EarlyCheckB(base)) d.early_b++;
  }
  const u32 mode = REX_LOAD_U32(kGameMode);
  const bool scene_ran = g_scene_this_pass;
  g_scene_this_pass = false;
  if (mode == 17 || mode == 18) {
    g_scene_passes = scene_ran ? std::min(g_scene_passes + 1, 2) : 0;
    if (g_scene_passes >= 2) g_loading_scene = true;
  } else {
    g_scene_passes = 0;
    g_loading_scene = false;
  }
  const bool loading = (mode == 17 || mode == 18) && !g_loading_scene;
  const bool game_locked30 = g_present_interval.load(std::memory_order_relaxed) >= 2;
  if (diag && game_locked30) d.locked30++;
  if (!mode30) {
    g_pacer.active = false;
    __imp__sub_82331858(ctx, base);
  } else if (loading || game_locked30 || EarlyCheckB(base)) {
    // The game's own path (renders every pass here); pairs start again after.
    __imp__sub_82331858(ctx, base);
    g_pacer.active = false;
  } else if (!g_pacer.active || g_pacer.render_next) {
    // Entering 30 mode (start, a switch from 60, after a loading screen): anchor the
    // pacing at this render, so the first skipped pass also waits two vblanks.
    if (!g_pacer.active) {
      g_pacer.active = true;
      g_pacer.mark = g_vblank.count.load();
    }
    g_pair_render = true;
    sub_823310C0(ctx, base);  // the frame task's render call
    g_pair_render = false;
    g_pacer.render_next = false;
  } else {
    WaitSecondVblank();
    g_pacer.render_next = true;
  }
  if (diag) {
    d.frames++;
    if (d.renders.load() == renders_before) d.skipped++;
    FrameDiagTick(base);
  }
}

REX_HOOK_RAW(sub_823310C0) {
  if (blitz::Settings().frame_diag.Get()) g_frame_diag.renders++;
  __imp__sub_823310C0(ctx, base);
}

REX_HOOK_RAW(sub_82333680) {
  if (blitz::Settings().frame_diag.Get()) g_frame_diag.steps++;
  __imp__sub_82333680(ctx, base);
}

REX_HOOK_RAW(sub_824B33A0) {
  if (blitz::Settings().frame_diag.Get()) g_frame_diag.presents++;
  __imp__sub_824B33A0(ctx, base);
}
