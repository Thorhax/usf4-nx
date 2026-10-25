# Ultra Street Fighter IV (Xbox 360): analysis notes

Facts about the game that the port relies on, each with where it came from. Addresses are guest addresses of
`Default.xex` unless stated otherwise.

## The disc

- Title ID `434307FC`, media ID `7E76B603`, version 0.0.0.5, disc 1 of 1, XGD3 (game partition at `0x02080000`).
- 1,122 files, 8.17 GB (`tools/extract_iso.py --list`). Besides `$SystemUpdate/` (dashboard update, unused):
  - `Default.xex` (12.7 MB) and `Capture.xex` (3.5 MB): the only code.
  - `.eaf` archives, 4.65 GB in 14 files (`archive/battle/battle.eaf` alone 2.8 GB; `dl0`-`dl2` are the
    Super/Arcade/Ultra content, `dvd_patch/` and `patch_ae2/` patch archives).
  - `.cpk` (CRI File System packs, 313 files), `.emz` (zlib containers, including the shaders in `archive/shader/`:
    `VsUserShader`, `PsUserShader`, `VsBgShader`, `PsBgShader`, `setting_technique`), `.gfx`/`.emb`/`.m4s`
    (Scaleform UI and its localisation).
  - 90 `.wmv` movies, 3.06 GB: ASF, **WMV9 Main@High (`WMV3`) + WMA 10 Professional 5.1** (stream headers).

## Default.xex

| | |
|---|---|
| Image base / size | `0x82000000` / `0x00DD0000` |
| Entry point | `0x82212AE0` |
| Encryption / compression | retail AES / basic (no LZX) |
| XDK | 2.0.21256 (all XDK libs); compiler 16.0.11886 (`C1`, `C2`), `LINK` 10.0.11886 |
| Code (`.text`) | `0x82170000`-`0x82A6CD14` (`REX_CODE_BASE`/`REX_CODE_SIZE` in the generated pch) |
| `.pdata` | 26,003 function entries |
| Prologues | 25,793 `mflr r12` (Xenon MSVC), 2 `mflr r0`: one compiler throughout |

Static libraries (XEX header `0x200FF`): XAPILIB, XBOXKRNL, XNET, XONLINE, LIBCPMT, XHV2, XPARTY, XAPOBA, XAUDIO2,
XMCORE, XHTTP, XAUTH, XMP, D3DX9, XGRAPHC, **D3D9LTCG**. Imports: 222 from `xam.xex`, 339 from `xboxkrnl.exe`.

- **Direct3D is the link-time-code-generation build (`D3D9LTCG`)**: its functions can be inlined into the game, so
  matching them by name (`tools/d3d_match.py`) is expected to find fewer clean hook points. To check in stage 4.
- Middleware (version strings in the image): CRI Audio 3.43.05, CRI File System 2.16.22, CRI ADX decoder 1.00.07,
  CRI AHX 1.90 (all "xdk21256.3", Jan 30 2014); Scaleform GFx; Lua 5.1.1.
- Movies are played by the XDK's WMV decoder linked into the game (paths
  `xdk\del\codecs\wmv\codecdsp-cayenne\video\wmv\decoder_c9e\dxva_pk\xenon\...` with GPU shaders `Shader_Deblock*`,
  `Shader_Mocomp*`). The SDK already carries an FFmpeg WMV3 configuration from Mass Effect
  (`sdk/thirdparty/FFmpeg/config_masseffect_wmv3.h`).

## Capture.xex

Image base `0x88000000`, module flags `0x9` (title DLL), entry `0x882D6218`, same XDK. The replay recorder for the
YouTube upload feature: `[EmCaptureXenon]` log strings, an ASF writer, the XAV encoder, XHTTP/XAUTH, and in
`Default.xex` "No youtube account linked to the authorised google account", `UPDATE:\capture.xex`. The service no
longer exists. Not translated; the plan is to keep the game from loading it (check at the first boot).

## setjmp / longjmp

Lua reports errors with `longjmp`. Neither function has a `.pdata` entry; found by shape (stores of the non-volatile
registers into r3's buffer and the mirrored loads) and set in `app/overrides.toml`:

- `setjmp` `0x8251C1E0`: if the pointer at `0x82C62A24` is set, tail-calls it (`bnectr`); else saves f14-f31
  (0..136), r13-r31 (152..296), the vector registers (stvx128), cr/lr (304/308), r1 (144), zeroes 312, returns 0.
- `longjmp` `0x82517AF0`: the mirror image through r7; lr from 308, r1 from 144, cr from 304, returns r6. With a
  non-zero 312 it takes an unwinding path (`0x82517DDC`).

## Code generation

First run (2026-10-11, rexglue 0.10.0 of the sa2-nx fork, Linux, clang 19.1.7 with `-march=x86-64-v2`):
4 "target not in any function" errors, all tail calls to small functions without `.pdata` entries, declared in
`app/overrides.toml`. Second run: 307 files, 39,960 functions, 101 s.

Jump tables (`tools/switch_check.py --code 0x82170000:0x82A6CD14`): 1,384 `bctr` (1,179 indirect, 108 byte, 63
halfword, 34 absolute tables); generated 198 switches and 803 indirect jumps; **0 truncated, 0 wrong targets,
0 missed**. Five without a bound in the code: three function-pointer tables (all entries are function starts, kept
as indirect calls) and two switches (`sub_82574850` at `0x82574D5C`, 17 cases; `sub_8262B390` at `0x8262BAB4`,
16 cases) whose index is not compared before the jump: unverified, a candidate if a "Switch case out of range" shows
up. Twelve odd shapes are indirect calls through tables in `.data`; `tools/pointer_audit.py` covers their targets.

Pointer-reached functions (`tools/pointer_fixpoint.py`, 2026-10-11): pass 1 over 20,493 unclaimed gaps found 170
candidates and accepted 166 (written to `app/overrides.toml` with where each pointer is stored). They exposed one
more tail-call target, `0x82950E80`. The next pass found 2 candidates and accepted none: fixpoint.
`tools/pointer_audit.py` afterwards: 40,392 registered functions; one vtable-like pointer to a non-start,
`0x82950F60` (pointer at `0x8212A5A8`), rejected by the fixpoint only because a one-word stub inside it had been
registered on its own; declared with `size = 0x40`. Also reported, not acted on yet (most are SEH/EH continuations or
resume addresses built with lis/addi; watch for them in boot crashes): 14 code addresses built in code, e.g.
`0x82510420`, `0x825122E8`, `0x8251C978` (`lwz r3,N(r31)` after a branch: SEH handler bodies) and `0x82534F5C`,
`0x8253CC84`, `0x8257C268`, `0x825CCD14` (`lis r11,0x8209` after a bctr). The rest are SEH scopes (204), jump-table
targets (190), EH state maps (38) and other data (160).

Gap classes not swept yet: `tools/gap_fixpoint.py` (code between functions that nothing reaches statically). Run it if
a boot dies in code that exists in the XEX but not in the C++.

## Tool changes in this tree (from sa2-nx)

- `tools/pointer_fixpoint.py`, `tools/gap_fixpoint.py`, `tools/pointer_audit.py`, `tools/pointer_scan.py`: the
  project name (`usf4_manifest.toml`), `Default.xex`, USF4's code range; the code range is read from the generated
  pch (`REX_CODE_BASE`), where this SDK writes it; on Linux the regeneration step runs `tools/codegen.sh`.
- `tools/codegen.sh` (new): the Linux codegen entry point. `tools/linux/boot_test.sh` (new): the boot test on a hidden
  Xvfb display.
- The host tools build on Linux with clang 19 (the SDK needs >= 18) and `-march=x86-64-v2` (without it
  `sdk/src/core/memory.cpp` fails: `_mm_shuffle_epi8` needs SSSE3); `libx11-xcb-dev` was the only missing package.

## First boots on the PC (stage 3, emulated GPU)

Linux, Intel HD 530 (Vulkan, Mesa ANV), hidden Xvfb display, `tools/linux/boot_test.sh`, 2026-10-11.

1. `librexruntimerd.so` not found: the SDK writes its shared libraries to `sdk/out/linux-amd64`; the harness sets
   `LD_LIBRARY_PATH`.
2. Died in audio init: SDL's `dummy` driver is not compiled into this SDL (only ALSA, PipeWire, PulseAudio). Muted
   output through the normal driver works.
3. `[FATAL] Call to invalid or unregistered function at guest address 0x8265BC98`: a qsort comparator whose address
   is built in code (declared in `app/overrides.toml`, evidence there).
4. **Runs** 45 s, then 180 s, no crash: CRIWARE and Scaleform logos, the opening movie (`movie\opening\opening.wmv`,
   decoded by the game's own WMV decoder and its GPU shaders, so no FFmpeg replacement was needed for it), the
   title logo, then the attract-mode demo fight (Ken against Crimson Viper, skate-park stage) fully drawn. Audio
   streams with no gaps (3,752 frames of data per 10 s). 197 graphics pipelines created in 3 minutes.
   Snapshots: `out/pc/captures/` (`usf4_snapshot_s`, read from the presenter's guest output; the X window itself
   shows black on Xvfb).

Seen in the log, harmless so far: `UPDATE:` paths fail ("device not found": the game looks for a title update first,
then reads the disc); `game:\test\...` debug files missing; `XamVoiceSetMicArrayIdleUsers` stub (voice chat).
`Capture.xex` was not requested in 3 minutes of boot and attract mode.

Not verified yet: input (menus, a match), the frame rate (nothing logs it), saving and loading, the other movies,
comparison against console footage.

## Input, menus and gameplay (stage 3, continued)

Test aids added to the app (2026-10-11):
- `usf4_input_script` / `usf4_input_script_file`: a scripted gamepad on player 1 (`app/src/usf4_script_input.cpp`;
  the SDK puts synthetic devices on player 1 and merges them with real pads). Every press/release is logged.
- `usf4_fps_log` (on by default): "[fps]" every 5 s, from a hook on the game's Direct3D swap `sub_821FAA00`, the
  only caller of the kernel's `VdSwap` (import thunk `0x82A6C924`).

Verified with scripts (snapshots in `out/pc/captures/`):
- START skips the opening movie; the title shows PRESS START.
- First start: "No saved data found. Create new saved data?". NO loops back to the prompt (the game re-lists the
  saves); YES creates `SF4_SaveData` through `XamContentCreate` (314 files, 16 MB, in the SDK's save folder
  `~/.local/share/usf4/<xuid>/434307FC/00000001/`), and the next boot opens it instead of asking.
- Main menu (Arcade, Versus, Xbox Live Battle, Challenge, Training, Player Data, Options, Xbox Games Store, Game
  Manual), Arcade settings with its info box, character select (44 characters, Ultra selection), the arcade
  prologue movie with subtitles (START skips it), the world map, the VS screen, both intros, ROUND/FIGHT, a fight
  with HUD, timer and hits landing ("FIRST ATTACK"): Juri vs Ibuki, Pitch-black Jungle.

Frame rate (i7-6700HQ, emulated GPU, RelWithDebInfo, classic codegen):
- Intel HD 530: 58-60 fps in boot, menus and movies; **12.5 fps in the fight**.
- GTX 960M (`VK_ICD_FILENAMES=/usr/share/vulkan/icd.d/nvidia_icd.json`): **30.0 fps in the fight** (exactly half:
  frames miss the 16.7 ms vblank; host presentation is IMMEDIATE, no host vsync), ~20 fps in menus.
- Where the time goes in the fight (perf, 8 s, NVIDIA): threads `Main XThread` 99.7 %, `GPU Command Processor`
  94 %, `EmMovie` 61 %. Of the main thread, ~95 % is **waiting for the emulated GPU**: `sub_821F54E0` 40 % (the
  XDK Direct3D spin wait: `cctpl`, 32 x `db16cyc`, compares the GPU read pointer at device+11024 with a fence,
  5000 ms timeout), `sub_821F8FA8` 22.5 % (waits for space in the command ring, same read pointer), the
  `savegprlr_29`/`restgprlr_29` helpers they call 23 %, `sub_8252C9D8` 6.7 % (time query for the timeout). The GPU
  command processor thread spends its time in kernel memory copies (`rep_movs_alternative`).
- So on this PC the limit is the emulated GPU, not the game's code; the game's own CPU cost per frame is not
  measured yet (it needs the native renderer, stage 4). `EmMovie` (`sub_82791438`, `sub_82790F78`, and the SDK's
  `rex::perf::IncrementCounter`) burns 61 % of a core during a fight, probably a polling loop: on the Switch's three
  cores that matters, to look at.

## Direct3D (stage 4)

No reference game with named XDK functions is available here (sa2-nx matched against banjo-nx's), so the functions
were found by what they write and where they sit (2026-10-11). The library block is about `0x821E0000`-`0x8220C000`
(`tools/d3d_surface.py --lo 0x821E0000 --hi 0x82212000`: 789 functions, 236 called from outside; from `0x8220E000`
on it is the CRT and XAPI: memcpy `0x8220EEA8`, memset `0x8220FFE8`, XAM wrappers).

The PM4 headers are built with the **predicate bit set** (`0xC0nn_oo01`), so a scan for `..00` headers misses them.

| Function | USF4 | Evidence |
|---|---|---|
| `D3DDevice_Swap` | `sub_821FAA00` | the only caller of `VdSwap` |
| `D3DDevice_DrawVertices` | `sub_821F4B88` | predicated `DRAW_INDX` (`0xC0012201`), auto-index source (`ori ...,128`), no inline data; 3 call sites |
| `D3DDevice_DrawIndexedVertices` | `sub_821F4FA0` | predicated `DRAW_INDX` n=4 (`0xC0032201`, index address and size), no inline data; 1 call site |
| `D3DDevice_DrawVerticesUP` | `sub_821F40A0` | like DrawVertices plus the inline-data allocator `sub_821F7EB0`; 3 call sites |
| `D3DDevice_DrawIndexedVerticesUP` | `sub_821F45A8` | like DrawIndexedVertices plus two inline allocations; 1 call site |
| draw-time shader load | `sub_82200438` | 3rd call of every Draw*'s flush; the only Direct3D writer of `IM_LOAD` (`0x2700`/`0x2701`); reads the bound **VS at device+13072, PS at device+13068**, then the VS object's +896, +872, +904: the same layout as sa2-nx's 2012 XDK |
| `D3DDevice_CreateVertexShader` | `sub_821E5D60` | r3 = container with magic `0x102A1101` (vertex): 255 calls in 40 s of boot (diagnostic hooks on all 182 entry points below `0x8220C000`, `out/diag`) |
| `D3DDevice_CreatePixelShader` | `sub_821E59B8` | r3 = container with magic `0x102A1100` (pixel): 392 calls; same 60-instruction shape as CreateVertexShader |
| container parse helper | `sub_821E5930` | 20 instructions: fills a descriptor (blob, microcode pointer, size) from a container; 18 calls |

The common Draw* flush calls, in order: `sub_821FF6A8` (twice), `sub_82200438` (shader load), `sub_821FEF80` (render
targets and depth: compares five surface pairs at device+12816.. against +13104.., writes `SET_BIN` packets), then
`sub_821FF310` (repeated), `sub_821FF550`, `sub_821FF3D0`, `sub_821F5830`, `sub_821F8EA8` (command buffer space),
`sub_821F7978`.

## Native renderer: first runs (2026-10-11)

Ported from sa2-nx without changes to its core (`app/src/me`, `app/src/me/native`, `app/src/cpu/sa2_enhancements*`,
`sa2_gpu_boost.cpp`, `app/src/profiler`; file and cvar names kept so sa2-nx fixes can be taken over). Not ported:
SA2's hooks, its native Direct3D layer (`sa2_api_*`), its census (`sa2_d3d_api.cpp`) and its GPU wait hook
(`sa2_gpu_wait.cpp`, on SA2's `0x8254F400`; USF4's equivalent spin is `sub_821F54E0`). New: `usf4_me_hooks.cpp` (the
addresses above), `usf4_api_stubs.cpp` (the dropped parts' "off" behaviour). Host tools: glslangValidator built
from `sdk/thirdparty/glslang` into `out/glslang`; DXC 1.9.2609 (Microsoft's Linux release) in `out/tools/dxc`;
`tools/build_shader_library.sh` takes them on Linux.

Shader package: one run through boot, menus and a fight with `usf4_dump_shaders = true` dumped 565 containers (223
VS, 342 PS); all 565 translated (XenosRecomp) and compiled (DXC), 0 failures (`out/shaders/sa2_shaders.mesp`, 15.8
MB). Direct3D's own shaders arrive as immediate loads: 1,766 "unidentified immediate" in 10 s of boot. Next: the
discovery loop (`SA2_SHADER_DISCOVERY`, `tools/wrap_discovered.py`).

Runs with `sa2_renderer_native = true` (GTX 960M): boot logos, opening movie, menus, character select, prologue
movie and a fight (Juri vs Ken, volcano stage, HUD, combos) all drawn, no crash in 200 s. Visible bugs in the fight:
horizontal stripes over the top-left HUD (player 1 health bar and name), a flat yellow sky.

Speed, and what limits it on this machine:
- Fixed: the draw-extent estimator asked `rex::memory::QueryProtect`, which on Linux reads all of `/proc/self/maps`
  (179 times in 4 s; sa2-nx had made the same change on Windows for `VirtualQuery`). Fight 10 -> 15 fps.
- The rest is the test setup, not the game: (1) the Vulkan driver presents to Xvfb by copying every frame through
  the X socket (`writev`, ~16 ms each, ~25 % of the ring thread; minimizing does not stop it without a window
  manager); (2) **the GeForce stays in P8, its idle state (232 MHz core, 405 MHz memory)**, at 40-56 % use: an
  Optimus laptop GPU rendering offscreen next to Xvfb is never boosted. The ring thread spins (`sched_yield`, ~17,000
  a second) waiting for it. So the fight's 15 fps (a quarter of 60: frames held to vblank multiples) measures
  neither the renderer nor the game.

## First Windows test (2026-10-11, the user's PC)

Windows 11, Visual Studio 2026 Community, LLVM 20.1.8, a desktop GPU: all build steps passed (codegen 24.6 s), the
native renderer plays boot, menus and fights at a steady **60 fps** (5-second windows 59-60 fps, worst 54.2 during a
load), and the user reports it plays perfectly and the PC stays cool. Keyboard input needed `mnk_mode = true` (the SDK
leaves it off); `tools/win/play.ps1` now sets it. `tools/win/env.ps1` now finds a VS install through vswhere when
the 2022 Build Tools folder is absent.

Graphics bug seen there and here: bright, horizontally smeared bands over the top-left HUD and across the screen at
waist height (Ryu vs Gen, Ha Long Bay stage; screenshot `usf4-logs/usf4-1.PNG` next to the project). It looks like
the glow/bloom post-process: the bands are horizontal streaks, and the log shows 1/16-size targets (96x45, 80x45:
"resolved logical crop 19410000"). Not caused by sa2-nx's enhancements: with `sa2_cpu_enhancements` and
`sa2_noncpu_enhancements` off the band is still there. Not a missing shader either: the discovery dump
(`sa2_shader_misses`) over a fight found only 9 Direct3D vertex shaders (and 1 rebuilt container); the repeated "PS
identity mismatch" lines (loaded `CAAF1EEB17A85076`, 252 words, and others) are rejected object pairings, after which
the load is identified by its code. Next: a single-frame trace of the post-process passes, against the emulated GPU's
frame.

## The bands: missing stage and character shaders (2026-10-11)

The first shader package came from one run (one stage, two characters). Other stages load their own shaders: the
renderer then paired loads with the nearest known shader ("PS identity mismatch", "variants" its repair tool rejects:
"NON-fetch ALU/CF differs") or left draws without shaders ("unresolved draw shader pairs": 85,896 draws with Direct3D's
own vertex shader `E35EA97ADB2AB935`). That drew the bands.

Fix: `tools/extract_game_shaders.py` reads every container from the game files (USF4 keeps them in EMB archives packed
as EMZ: `#EMZ`, checksum, unpacked size, data offset, raw deflate; 43,522 EMZ streams in 880 files): 456 containers,
290 with microcode the dump lacked (the other 165 differ only in header bytes the game rewrites at load). Plus
Direct3D's own shaders wrapped from the discovery dumps (9) and one rebuilt container: 865 shaders, all translated
and compiled. Result: fights on the Korean street (T.Hawk) and distillery (Gen) stages render cleanly; "unidentified
loads ... immediate 0", no unresolved draws. Still seen: a black vertical bar beside Gen during a hit (one frame);
18 vertex-shader variants dumped (Direct3D's rewrites).

Foliage (the user's emulated-GPU screenshots, jungle stage): leaves show the alpha-to-coverage dither pattern, which
shimmers in motion. USF4 draws its scene with 2x MSAA ("guest MSAA source 01"); the samples averaging at the resolve is
what smooths the dither on the console. The native renderer currently uses a "single-sample approximation" for these
transfers. To look at next.

## Black streaks on hits (native renderer only; 2026-10-11)

The user saw black lines where Juri's attacks land, with the native renderer only (`-Emulated` is clean). Caught
here with burst snapshots (`usf4_snapshot_s = "a-b@0.1"`) and a single-frame black-pixel detector: a thin black
diagonal streak growing from the impact point over 3-4 frames (counter hit on Makoto). Likely cause: Direct3D's
**interpolator relink**. The vertex-shader "variants" the renderer dumps differ from their templates in the vertex
fetches *and* in export instructions (`C80F8002` -> `C80F8003`: an output moved to the next slot; `C80F8002 E2020200`
-> `C8000000 02000000`: an output disabled), i.e. Direct3D renumbered the VS outputs to match the bound pixel shader.
`repair_vertex_variant_declarations` rejects those ("NON-fetch ALU/CF differs"). The dumped variants are whole
containers with the loaded microcode, and this port's XenosRecomp links interpolators by hardware register, so they
went into the package as they are (`out/pc/shaders_variants`, 8 of 9; `vs_vc43fc0b1422d5b97` is skipped by the
translator). Package v3: 873 shaders. After it: "FINAL VS selection audit: 1,213,958 proven, 0 unproven", and no
single-frame black streak in 601 snapshots of a fight with hits (not a proof: the streak needs a particular hit).

Still seen in that fight (bay stage, Juri vs Guy): a dark rectangle standing next to Juri (the same kind as the bar
next to Gen earlier) and a patch of white speckle noise (spray particles?). Next to look at, with the foliage dither.

Diagnostics added: `usf4_trace_until_s` (with `banjo_trace_frame_s`: trace every frame until then); note the log
rotates at 5 MB x 20 files (`log_max_file_size_mb`, `log_max_files`), so long traces need a larger limit.

## The black shapes on hits: reverse-subtract blending (fixed 2026-10-11)

Reproduced reliably in Training mode (`out/pc/training_script.txt`: menu -> TRAINING, Juri vs the dummy, walk in and
attack; the plain grey room makes dark shapes obvious). `banjo_native_skip_vs=434,78` removed them, so they came from
the hit-effect draws (PS n538 = texture x vertex colour to target 0, zeros to target 1). Their blend
`RB_BLENDCONTROL 01860186` is source alpha, **reverse subtract** (op 4), one: the effect darkens the scene a little.
The renderer drew op 4 as `VK_BLEND_OP_SUBTRACT` (source - destination, with a "subtract is used" warning, in both
`FillFixedState` and `OperationBlend` of `sa2_native_draws.cpp`): negative almost everywhere, clamped to black quads
and streaks. Now `VK_BLEND_OP_REVERSE_SUBTRACT`; the effects show as the intended faint grey swish.

Fixed on the way (`me_native_system.cpp`, `PairDraw`): with predicated tiling the GPU replays each command buffer once
per EDRAM tile (USF4 draws its 1280x720 2x MSAA scene in three tiles, scissors 0-544, 544-1088, 1088-1280). A draw
paired by address (`PairByAddress`, which consumes the Draw* record) did not store its pairing for the replays, so
tiles 2-3 drew with the ring's own pick: two library vertex shaders that differ only in fetch fields (n434 in tile
1, n78 in tiles 2-3). The pairing is now stored, and a replay takes the first tile's vertex shader when it still
matches. Trace now also shows target 1 (`rt1`, `blend1`).

## Stage 4 result on the user's PC (2026-10-11)

With update 2 (reverse subtract, tile-replay pairing) and shader package v3 (873 shaders), the native renderer
plays with no graphical glitch the user can see, at 60 fps (5-second windows 57.2-60.0; the dips in the first 30 s
come with 70-118 ms frames: pipelines compiled the first time a state is seen, which sa2-nx's pipeline cache
(`cache/`) removes on later runs). GPU time per 10 s, heaviest passes: the scene (VS249/PS168) 1.56 s, VS777/PS641
0.74 s. Open: the jungle foliage dither (seen with `-Emulated`; the native renderer draws alpha-to-coverage as an
alpha test at 0.5, `banjo_native_alpha_to_mask_test`), the white speckles seen once on the bay stage here.

## Stage 5: the first Switch build (2026-10-11)

Toolchain: devkitPro's Docker image `devkitpro/devkita64` (devkitA64 r29.2, GCC 15.2, binutils 2.45.1, libnx 4.12.0;
devkitpro.org's installer is behind a Cloudflare check that blocks scripted downloads). The build runs in the
container with the repository mounted at the same path; glslangValidator is built in the container
(`out/glslang-dkp`), the host's needs a newer glibc. First build without LTO (`-DUSF4_SWITCH_LTO=OFF`): 759 steps.

The Mesa driver:
- **Mesa 26.2.4** (`mesa-26.2.4-switch-unified-horizon-sdk.zip`, from the user): **unusable as shipped**. Its Rust
  parts (NAK, NIL) reference the `core` and `alloc` crates (hash `R2NHd66MWo`), but no archive of the package
  contains them (libvulkan.a bundles compiler_builtins, nak_rs and nil only; libnak_rs.a / libnil.a have std,
  panic_unwind, ... but no core/alloc): 4,944 undefined references at the link.
- **Mesa 26.2.2** (nfsmw-nx's build, the one sa2-nx shipped; it was in `/opt/devkitpro/portlibs/switch`): complete
  (nothing referenced is missing from the archive), but devkitA64's ld still left 4,939 Rust runtime symbols
  undefined, even for a program that only calls vkCreateInstance: the archive holds two Rust static libraries with
  their own runtime copies, 564 member names twice, and ld does not pull those members through the index. Passing the
  objects explicitly resolved them. `tools/switch/repack_archive.py` repacks the members in order under unique names
  with a fresh index (no object changed): the test program and the game link. The build uses
  `out/tools/mesa-26.2.2-repacked/switch` (the 26.2.2 SDK with the repacked libvulkan.a).

`tools/check_switch_build.py --elf out/nx/usf4 --nro out/nx/usf4.nro --sdk-vulkan ...`: `_start` at 0, Mesa 26.2.2 in
the NRO, 0 vector fused multiply-adds; "flavour" fails by design (classic codegen); three branch-to-self loops, all the
game's own: `sub_8229CCD8` (guest `b .` at 0x8229CCF8) and `sub_822D2A38` (guest `b .` at 0x822D2ADC) are halts,
`sub_82245530` is a list search whose "not found" path loops on registers only (the guest spins there too).

Settings compiled in: `app/usf4_switch_defaults.toml` (native renderer, logging on for the first tests, no asset-pack
build: the game folder is read directly, a 7.7 GB pack would not fit FAT32); the player's `usf4.toml` from
`app/usf4_switch.toml`. Package: `tools/linux/package_switch.sh` -> `out/nx/usf4-nx-switch.zip` (NRO 68 MB + shaders).

## First console test (0.1.0, 2026-10-11) and the profiling build (0.2.0)

0.1.0 (no LTO) on the user's Switch (Atmosphère, Status Monitor, an overclock: memory 2700 MHz): boots, menus and
fights with no visible glitch. Frame rate: boot 45-55, menus 39-52, **fights 10-24**, results ~59. A CPU overclock
(1785 MHz) changed nothing: the fight is **GPU-bound** (Status Monitor: GPU 1228.8 MHz, load 96.6 %, 24 fps;
renderer log: GPU busy ~6.05 s per 10 s in ~56 ms intervals, ~571 ms per second in fence waits; main thread 95 %,
which on PC was mostly Direct3D's spin wait). App memory: 3,281 of 3,285 MB.

Where the GPU time goes, from the PC's per-pass report (10 s at 60 fps, ~4.9 s in all): the scene tiles (pitch 560,
"320" category: VS249/PS168 1.56 s, VS777/PS641 0.74 s) ~2.3 s; EDRAM emulation ~2.1 s (9-pass stencil depth imports
0.63 s, colour aliases 0.69 s, depth imports 0.28 s, copies to textures 0.46 s). USF4's 2x MSAA, predicated tiling
and colour/depth aliasing in EDRAM make the mode-4 transfers far heavier than SA2's.

0.2.0: LTO (6 LTRANS jobs), `sa2_gpu_marks_categories` and `switch_profile_stacks` on, to break the console's GPU
and CPU time down. Checks: one more branch-to-self, `sub_822D43A0`: the guest halt of `sub_822D2A38` inlined into
its caller by LTO.

## 0.2.0 on the console, and what limits the fights (2026-10-11)

LTO made a large difference: fights ~30 fps (28-36, up to 40), menus 60 (user's run with clocks between CPU
1020-2703 MHz, GPU 460.8-1228.8 MHz, memory 2700). `sa2_render_height_* = 540` raised fights to ~37 fps.

GPU time per 10 s of fighting (~290 frames), `sa2_gpu_marks_categories`: EDRAM 9-pass stencil depth imports 1.28 s,
colour alias conversions 1.84 s, depth imports/exports 0.79 s, copies to textures 0.28 s, the scene itself 1.47 s:
**~70 % of the GPU is EDRAM emulation**. CPU (`switch_profile_stacks`): ring thread 88 % (CopyVertices, memcpy,
Fingerprint64: vertex uploads and their dedupe), main thread 69 %, mostly in Direct3D's waits (sub_821F54E0,
sub_821F8FA8).

The 9-pass imports are dominated by depth coming back from a 4x MSAA alias (pitch 280) to the 2x scene (pitch 560):
`D508:280x720:mx1my1 -> 508:560x720` 156,600 tiles and `D0E0 -> 0E0` 55,680 per 10 s (83 %). Those are Direct3D
Clears through the 4x alias (sa2-nx's notes, "How SA2 clears the screen"). The renderer redirects such clears into the
tiles' owners, but declined USF4's: "also writes color (color 0: ...)". With the colour planner's reasons logged
(`[usf4diag]`, first 12 of each): **owner unsupported**, the colour clear's tiles (colour base 000/4F8, 280x720 4x) are
owned by a *depth* view (0E0 or 060, D24S8/D24FS8 4x or 1x), which the planner does not handle; **rect not
tile-aligned**, clears along the predicated-tile seam (x 480-544, 544 not a multiple of 80) and thin strips (y 176-180).

Next (not done): let the colour half clear into depth owners (the colour word as depth = word >> 8, stencil = word &
0xFF, float24 for D24FS8), and handle seam/strip partial tiles. Caution: in these clears the colour surface (base 000,
630 tiles) and the depth surface (base 0E0) overlap in EDRAM, so the order of the two writes in the overlapped tiles
must match the hardware's; verify against the emulated GPU frame by frame before turning it on.

Test harness: `/dev/shm` filled up with `xenia_memory_*` files (4.8 GB each) left by killed runs, and the next start
died with SIGBUS; `tools/linux/boot_test.sh` now removes the ones its run created.

## Clears into depth owners (usf4_edram4_clear_into_depth, 2026-10-12)

`PlanClearColor0EDRAM4` now accepts tiles owned by a D24S8 / D24FS8 depth view: the clear's 32-bit EDRAM word
(byte c = colour channel c, as the depth redirect's colour owners assume) is that view's depth and stencil (depth =
word >> 8 as unorm24 or 20e4, stencil = word & 0xFF), cleared in place with `ClearDepthInPass`, with the same
bookkeeping as the depth redirect's in-place clears (`edram4_bits_stencil`, `StencilRealEDRAM4`, tile versions).
The colour half is committed after the depth half, as before.

Verification (the guide's A/B, on PC): lockstep runs (`vblank_per_swap`, the training script on the game-frame clock
`usf4_input_script_frames`, sa2-nx's recorder `sa2_video_s`/`sa2_video_frame`; `tools/linux/lockstep_run.sh`).
USF4 is not bit-exact between two identical runs (the fighters' animation phase differs: base1 vs base2 mean MAD
0.39, median 0.24, `tools/compare_raw.py --follow`), so the check is "only the fighters differ": base1 vs clr1 mean
MAD 0.34, median 0.20, the worst pairs differ on the fighters only, background and HUD identical. Tested on the
training stage only.

Effect per report (PC): 9-pass depth import tiles 76,110 -> 14,688 (-81 %); the 4x-alias imports `D0E0` (16,512)
and `D508` (46,440) are gone; the "owner unsupported" declines are gone. Still declined: "rect not tile-aligned"
(the predicated-tile seam at x 544 and thin strips), the next item.

## Crash-safe pipeline cache file (2026-10-12)

After a crash on the Switch (a screen recording), the next boot stayed on "Now Loading" until `cache/` was deleted:
`cache/sa2_native_pipelines.bin` had the right size but damaged contents, and the reader only checked sizes. The file
is now version 2 (`sa2_native_draws.cpp`, `kVersionFilePipelines`): its header carries an XXH3-64 of the list and the
cache, and the writer flushes and `fsync`s (`_commit` on Windows) the `.tmp` file before renaming it. On a mismatch
the reader logs `C6: pipeline file damaged (checksum): starting from scratch` and the session rebuilds the cache.
Verified on Linux: a version-1 file is rejected ("from another version"), a new version-2 file is saved after the
60 s timer, read back intact (6 pipelines), and the same file with one byte flipped in the middle is rejected.
The first boot of 0.4.0 therefore starts with an empty cache, once.

## Resolves that "nobody reads" (2026-10-12): not skipped

The copy report's `*** NOBODY READS IT ***` only counted draws that sample the texture. Checked before skipping any:

- **1F6F8000 1280x720 is the presented frame** (`sa2_diag_alias_frames`: every `PRESENT` is 1F6F8000). The Swap reads
  `resolved_` directly. The report now says `(presented by the Swap)` for it, and EDRAM restores from a resolved
  texture count as reads too.
- `sa2_native_lazy_front` (present from the render target) does not help USF4: of 4,156 deferred copies, 4,138 were
  recorded anyway before the target was drawn again; 18 were dropped.
- **19326000 / 19362000 (320x180) are read in some sessions and not in others** (Switch logs: 918 reads over 9
  reports, 4 reports with none), so learning "unread" per address would break the image later in a session.
- What is left: in fights the scene render target is resolved three times in a row (no draws in between) into
  three 1280x720 textures and the 544x720 tile halves; the first of each (Switch: 130AF000 / 13B77000; PC session:
  18328000 / 18DF0000, the addresses move with the stage) is never sampled in our runs. That is ~1 Mpixel per frame at
  540p, ~0.6-0.8 ms of a ~33 ms frame. Not skipped: our test fights never ran an Ultra or Super cinematic, the likely
  reader, and a wrong skip shows a stale image with no way to recover the content.

## Color-0 clears into their consumer (2026-10-12)

With `usf4_edram4_clear_into_depth` (0.3.0) the color half of Direct3D's Clear went to the depth views that owned its
tiles, and the color view that drew there next pulled it back through depth exports (Switch: category 11 from 210 to
~715 ms per 10 s). `usf4_edram4_clear_color_consumer` stamps the color-0 clear's tiles like the depth redirect does
(a site = drawn view + ordinal of its clears in the frame); the view that pulls them next, seen twice in a row, gets
the clear and the tiles (`PlanColor0ConsumerEDRAM4`). Report: `color 0 clears into their consumer`.

PC lockstep A/B (Training, Juri vs Hakan, frames 7000-7119): MAD 0.234 (baseline 0.2-0.4), differences only on the
fighters. Of ~870 color-0 clears per 10 s, 393 went to a color view; declined: 351 with no consumer seen twice (the
next reader alternates), 124 with tiles outside the consumer. GPU per unit of scene time (cat 4): depth exports
-45 %, color aliases -12 %, depth imports (cat 10 + 13) +13 %, EDRAM total -4 % (PC GPU at P8: indicative). The
depth view now imports back what it no longer owns, and 9-pass imports are dear on the Switch: judge it there at
fixed clocks. On in the 0.4.0 Switch defaults; `usf4_edram4_clear_color_consumer = false` in usf4.toml for the A run.

## 0.4.0 on the console, and the next limits (2026-10-12)

User's console, 540p, CPU 1785 / GPU 998.4 / memory 2700: fights ~38.4 fps (0.3.0: 30.8), visibly slowed down (the
game logic runs per frame). Both sides are now at the limit: GPU 26.2 ms per frame (EDRAM 15.7 ms of it: colour
aliases 7.8, 9-pass imports 4.4, imports 2.2, exports 1.3; scene tiles 7.5) and the ring thread 25.9 ms (22.7 us x
1,143 draws). PC timings on this box cannot measure the ring: under Xvfb 26 % of its time is the NVIDIA driver
writing frames to the X socket, and the GTX 960M stays at P8.

- Vertex dedupe content check (`usf4_dedupe_audit_each`, 16): fingerprinting was ~9 % of the console's ring thread and
  never found a stale range. Now one copied range in 16 is fingerprinted and every hit on it is checked; a mismatch
  logs a DIFFERENCE and checks every range for the session. PC fight: 19,204 -> 1,644 fingerprints per 10 s, same
  hits (30 %), no mismatch. Misses (new `C6 vertex dedupe misses` line): 5,395 not in the table, 11,090 copied in an
  older work slot, 11,708 forgotten at a guest wait.
- **Predicated tiling** is the structural cost: 45 % of all draws are tile replays (765,348 of 1,677,758), and the
  renderer already draws the guest's 2x MSAA targets with one sample, so the tiles buy nothing on screen.
  `BeginTiling` is `sub_821FDA68` (r4 flags 1, r5 count 3, r6 rects 0-544 / 544-1088 / 1088-1280, all 720 high),
  called from the engine's render-command handler `sub_82754260` (lr `0x827542DC`), which takes flags, count and
  rects from a prepared command (+4, +8, +12 word offset of the rects). Textures of 1280x720 / 544x720 are created
  through `sub_821F0B80` from `0x82792184`, `0x8253085C`, `0x822A4630`, `0x829FBF80`, `0x827579F4`, and
  `sub_821F0CA0` from `0x827926F4`, `0x82792604`, `0x827579B8`, `0x82757930` (the engine's render-target setup is
  around `0x8275xxxx`/`0x8279xxxx`). Diagnostic used: `out/diag/usf4_diag_tiling.cpp` (not in the build).

## One tile instead of three (`usf4_single_tile`, 2026-10-13)

How USF4 chooses its tiling (found with temporary hooks, `out/diag/usf4_diag_scan.cpp`, not in the build):

- `sub_82971F68` (render-target setup) reads the display height (engine config +2724) and picks a mode: 720 -> 0,
  768 -> 1, 576 -> 2, other -> 3. Mode 0 sets the tile preset r14 = 6 and the MSAA index r24 = 1 (2x).
- Tile presets: a table at `0x820DE8F0` of {count, rects} (index 1 = one tile 0,0-1280,720; 6 = three strips of
  544). `sub_82791BE0` copies the chosen preset into each render-target object (+40 count, +44 rects), and the
  render-command handler `sub_82754260` passes them to `BeginTiling` (`sub_821FDA68`). MSAA index -> type through
  `0x820DE654`; `sub_82792670` creates the EDRAM surface (`sub_821F0CA0`, CreateRenderTarget).
- The same function's second mode switch sizes the tile-local textures ("Main Tile Depth" and others): 720p r26 = 544.
- `sub_8296E028` copies the post-process strips as floats from `0x821295A4` into its object (+684 count, +696 rects).

Three `[[midasm_hook]]` entries (`app/overrides.toml`, `src/usf4_tiling.cpp`) turn this into one tile: preset 1 and
MSAA 0 at `0x82972060`, tile textures 1280 wide at `0x829728B4`, one post-process rect (0,0-1280,720) after
`0x8296E85C`. The game then lays out EDRAM as colour 0 at 0, colour 1 at 736, depth at 1472 (1x, 720 tiles each);
the depth runs to tile 2208, past the 2048 of real EDRAM, and wraps over colour 0's top 160 rows. No visible effect
and no significant transfer pair for it in the PC runs; a 4096-tile EDRAM model would remove it if the console shows
a cost.

PC results (Training fight, 150 frames each): draws per frame 394 -> 202 (-49 %), native draws 444,350 -> 242,620
per 10 s; colour alias operations 11,097 -> 0, depth exports 15,898 -> 976, depth imports 40,497 -> 19,983.
Lockstep image A/B (frames 7000-7119): no offset (best alignment 0,0), MAD 3.45 spread evenly: edges are a little
harder, because the 3-strip path's 2x MSAA did smooth them. GPU time can only be judged on the console.

## 0.5.0 on the console: the EDRAM wrap was the cost (2026-10-13)

0.5.0 with the one tile: GPU load down, but fights at 28-33 fps (CPU 2091, GPU 768 that day). The ring thread was
82 % busy, half of it in `PublishEDRAM4` and the lazy-stencil map lookups it makes: 30 M EDRAM tiles walked per second,
publish fast path 8 % (three strips: 0.36 M tiles, 95 %). The depth's last 144 tiles wrapped onto colour 0's first
rows (above), so the two took those tiles from each other on every draw, and each change of owner bumps the global
epoch that the fast path needs. Fix (0.5.1): the mode-4 EDRAM model has 4096 tiles (`kTilesEDRAM4` in
`sa2_native_targets.cpp`, `kOwnershipEdramTiles` in `me_edram_ownership.h`); a surface is still at most 2048 tiles,
the three strips (0..1918) never reach past 2048. PC one-tile fight: publish 34.2 M -> 1.8 M tiles per 10 s, sync
21.7 M -> 2.1 M.

0.5.1 on the console, one tile, CPU 2193 / GPU 998.4: **~52 fps in fights** (Status Monitor: GPU 67 %, cores 0-2
85-95 %). The limit is now the three CPU cores together: main thread 70 % (22 % of it the D3D spin wait), Vulkan
recording worker 68 % (NVK: `vk_dynamic_graphics_state_copy` 11 %, `nvk_CmdBeginRendering` 7 %, inline data 7 %), ring
thread 65 % (waits 2.8 s per 10 s for the worker in drains), game audio thread 21 %, four job workers 10 % each.
Ring per draw: uploads 5.3 us (VS constants re-uploaded 137,695 times per 10 s, all with new contents), textures
2.2 us, vertices/targets 2.3 us, pipeline 1.4 us, record 1.2 us.

## The D3D GPU wait sleeps (`usf4_cpu_gpu_wait_sleep`, 2026-10-13)

`src/usf4_gpu_wait.cpp`: sa2-nx's `sa2_gpu_wait.cpp` on USF4's copy of the XDK iteration function `sub_821F54E0`
(same record and device offsets: [0] device, [8] read pointer seen, [12] tick; device +11024 read-pointer word,
+11069 bit 0x02 hung; code hash checked, FNV 0x142E8261D1F076A1 over 0xD8 bytes). It sleeps until the ring thread
reports progress (at most `usf4_gpu_wait_max_us`, 1000) only when nothing can have changed since the previous
iteration. PC fight: ~8,000 sleeps and ~8.5 s asleep per 10 s on the main thread instead of spinning. The enhancement
use report (`[enh] uses in the last 10 s`) was not started in USF4; `usf4_app.h` now starts it.

## The frame end no longer waits for the recording worker (`usf4_deferred_async_submit`, 2026-10-13)

0.5.2 on the console (one tile, CPU 2193 / GPU 998.4): 48-50 fps; the GPU wait sleep took the main thread from 70 %
to 55 % but the frame rate stayed: the limit is the ring thread + Vulkan recording worker pair. The ring waited
2.6 s per 10 s for the worker (drains). A breakdown by cause (new report line "deferred recording drains by cause")
on PC: the waits are at `vkEndCommandBuffer` of the frame's one submission, then `vkResetCommandPool`; the other
drain-class calls (`vkBeginCommandBuffer`, `vkResetDescriptorPool`, `vkQueueSubmit`, `vkGetQueryPoolResults` through
the generic wrapper) found the queue already empty only because the end had just drained it.

With the cvar (Switch default from 0.5.3):
- `SendWork`: the upload and work buffers are ended and submitted by a job on the worker (`deferred::RunOnWorker`),
  in queue order; the slot keeps the job's ticket. Every fence status/wait on a work slot first waits for that ticket
  only (`deferred::WaitTicket`), never for the whole queue.
- `BeginRecording` and the upload buffer's begin: after the slot's ticket and fence, the pool resets and
  `vkBeginCommandBuffer` go straight to the driver (`deferred::Real()`); the commands after them are queued.
- `Present`: `RefreshGuestOutput` + `PaintOutput` run as a job on the worker after the frame's submission, with a
  copy of the source image (view and size); the presenter therefore never gets an output whose work is not submitted.
  `WaitOutputs` on the ring drains first. `presented_` counts the queued paints.
- Query results are read with the real `vkGetQueryPoolResults` (all reads follow the slot's fence).
- The arena is a ring when the cvar is on (markers per queued call, space returned as the worker passes them).

PC one-tile fight: 0 drains and 0 ticket waits per 10 s (before: ~170 drains, 34 with waits); no errors. Lockstep
capture (frames 7000-7119): every frame equals the reference's same or previous frame (MAD 0.00-0.08 in the first 30
frames); 33 captured frames repeat the previous one because the recorder samples the output at the guest's swap,
before the worker has painted it (the content is the same; the output reaches the presenter later).

## 0.5.3-0.5.5: queue length and merged tile copies (2026-10-13)

- 0.5.3 (async submit) on the console: 0 drains, but still ~46-49 fps; the ring thread was "working" 21.6 ms per
  21.7 ms frame and on the CPU only 68 % of it: it waited for room in the 1024-call queue (~1/5 of a frame's ~4,900
  calls). 0.5.4: `usf4_deferred_slots_log2` (Switch 12 = 4096 calls, 512 KB): ~55.5 fps at CPU 2193 / GPU 998.4,
  ~58.5-59 at CPU 2397 / GPU 921.6; queue full 0 times per 10 s once in the fight; all three cores ~100 %, GPU ~72 %.
- New report line "deferred recording, meta-path calls": PC fight ~127 `vkCmdCopyImage` per frame with ~9.4 regions
  each, almost all from `CopyTilesEDRAM4` (one region per 80x16 tile, two for depth). NVK records each colour region
  through vk_meta (`copy_image_region_gfx`: pipeline, temporary views, descriptor set, rendering, draw), and the
  command pool reset destroys the meta objects (`vk_meta_destroy_object`, `nvk_image_view_finish`, free/mutex:
  ~12 % of the ring thread on the console, which now resets pools itself). 0.5.5 (`usf4_copy_tiles_merged`): tiles that
  are neighbours in both images form one rectangle (row runs, then rows of the same span): 9.4 -> 2.5 regions per
  call. Lockstep: differences only on the fighters' outlines (animation phase), none tile-shaped.

## Optimised codegen, stage A (0.5.6, 2026-10-13)

0.5.5 on the console (CPU 2193 / GPU 921.6): ~50-55 fps; cores 250 % of 300 %, GPU 73 %; ring thread 13.2 ms per
frame (was 21.6), recording worker 39 % (was 87 %). The main thread (63 % busy, 42 % of its samples waiting in the
kernel) became the limit: its waits were the codegen's global lock (`REX_ENTER_GLOBAL_LOCK`, one process-wide recursive
mutex for every mfmsr/mtmsrd pair) in the XDK interlocked list pops (`sub_82966BA8`, via `sub_82967288`/`sub_828F62A8`/
`sub_82925FC8`) against the job workers and the audio thread, plus `NtPulseEvent` (the SDK's `PosixEvent::Pulse` takes a
mutex and sleeps).

USF4 used the classic codegen. Ported sa2-nx's optimised flavour, stage A (`app/perf_overrides.toml`; `tools/codegen.sh
--flavor optimised|classic` writes `app/codegen_flavor.toml`, which the manifest includes): cr/ctr/xer/reservation as
locals, skip_msr, dform_disp_split, fctiwz_inline, fp_moves_modeless; post steps (`tools/post_codegen.py`, also appended
to the build's own codegen command): no-volatile guest loads/stores + loop barriers (32,010 loop headers) + direct
calls (132,354 calls, 393 hooked addresses kept). Stage B (non_volatile_as_local, elide_lr_stores,
non_argument_as_local) needs the read-before-write and non-argument audits first. PC: boots and fights normally.

## 0.5.6 on the console: job workers starved the main thread (0.5.7 `usf4_cpu_job_pop_yield`, 2026-10-14)

0.5.6 menus ran at ~58 fps, but fights fell to 10-12 fps with all three cores at 100 % and the GPU at 17 %. The profile:
the four Job Workers spent 80-90 % of their samples in the XDK job-list pop `sub_82966BA8` (via `sub_82967288`, the
worker loop `sub_82966C28`: "while the batch flag [+56] is set: pop a job and run it"). The main thread
(`sub_82966C90` publishes a batch, sets [+56] = 1, runs jobs itself, clears it at the end) was 92 % waiting. All of
them run at Horizon priority 0x3B (59). Horizon only switches between threads of equal priority when one yields (or
at its 10 ms rotation of priority-59 threads). So a woken main thread waited for a core that a spinning worker held.
In 0.5.5 each interrupt-off section took the codegen's global recursive mutex, and the contended mutex slept, which
hid this; `skip_msr` removed that. The PC never showed it: Linux time-slices (and fights there were always 10-13 fps
from the P8 GPU, so `out/pc/fight` fps cannot show this class of bug).
Fix: `src/usf4_job_pop_yield.cpp` hooks the pop; an empty pop yields (`svcSleepThread(YieldType_WithCoreMigration)`).
Code check: FNV 0xA74D6392CF5F4057 over 0x58 bytes.

## Column imports: the shadow-map clear strips (`usf4_edram4_import_columns`, 0.6.0, 2026-10-14)

Console GPU per Swap in a 60 fps fight (0.5.8, 998.4 MHz): ~12.6 ms busy, of which depth imports 5.2 ms
(edram_import9 3.3, edram_import 1.9); then scene 2.6, copies 1.6, 320 1.2, scene_no_z 1.1, alias 0.5. ~80
single-tile 9-pass imports per frame (~25 us each) plus ~49 late stencil fetches.
Cause: Direct3D clears the shadow maps (512x512 in view 0E0, pitch 560; 1024x1024 in view 340, pitch 1040) with
VS777/PS641 rectangles in 32/64-pixel strips. The last strip leaves the right edge of the last tile column uncovered,
so each tile row of that column was imported alone (SynchronizeEDRAM4 runs are per row), and its stencil was later
fetched alone again (FetchStencilDeferredEDRAM4). Each import of a tile is a draw of up to 9 passes.
Fix: runs of one sync / one fetch that form a column (same source, same tile offset, same x range, touching rows)
become one ImportColorDepthEDRAM4 over the column's rectangle; the shader maps tiles linearly from the start and the
rectangle limits rasterization to the column, while the bookkeeping (stencil sources, synchronized versions,
stencil-only clears) walks the original runs. PC with the Switch settings, 10 s of fight: 9-pass intervals 7014 ->
823, depth-import intervals 7187 -> 1034, late fetches 4306 -> 625 (PC fight fps 15 -> 21 on this GPU-bound box).

Console 0.6.0 (2026-10-15, CPU 1785, memory 2700, 540p): column imports active (per 10 s ~4180 imports replacing
~74600 runs, 0 failed). At GPU 998.4: busy ~12.4 ms per Swap (edram_import9 2.7, edram_import 1.7, scene 3.1,
copies 1.6, scene_no_z 1.1, 320 0.9, alias 0.5), gap ~4.4 ms, 59.5-59.9 fps. The saving was only ~0.8 ms: the
remaining ~7 import9 intervals per frame still average ~0.36 ms each, so their cost is per pass (barriers,
render pass switches, the copy-engine stencil path), not per tile.
At GPU 768 (stock docked maximum): busy ~14.8 ms, gap ~2 ms, 59.8-59.9 fps; holds 60 apart from transition hitches.
On a stage with many background characters at 998.4, fights ran 55-58 fps while the GPU had a ~5 ms gap per frame,
so that slowdown comes from the CPU side (ring thread / game threads at 1785), not from the GPU.

## Save-open hitches (0.6.1, 2026-10-15)

Console 0.6.0 log (CPU 1785 / GPU 768): the longest transition hitches (183, 217, 417 ms) each sit next to an
`[io] mounted '...SF4_SaveData'` line. USF4 opens its save (314 files, ~16 MB) before and after fights; every
XamContentCreate mounted it again with PopulateEntry -> ListFiles, which stats each file (~0.7 ms each on the SD:
216-570 ms per mount), and the `[save] open` / `[save] close` log lines listed the folder the same way
(~350 ms each). The ring's "GPU fence" wait in those [hitch] lines spans exactly that time.
Fix: writable HostPathDevices are built from the directory listing alone (vfs_index::ListLight: fsDirRead
gives name, type and size, 64 entries per IPC; cvar `vfs_light_scan_writable`, default true). Every entry gets
the folder's timestamps, as an STFS package gives every entry its root's. The save log lines use ListLight too.
PC: same 314 files and 16115897 bytes as before.
Console 0.6.1 (1785 / 768): save mount 5.2-6.2 ms (was 216-570); no hitch next to the save opens any more. Fights
59.9 fps, Status Monitor GPU load 88 %, cores 72 / 91 / 89 %. Hitches left: boot (300-480 ms) and the versus and
stage loading screens (66-350 ms), while the game loads its own data.

## Vertex ranges kept across frames (`usf4_vertex_cache`, 0.6.2, 2026-10-15)

Console 0.6.1 fight (1785 / 768): CPU threads add up to ~260 % of the 3 game cores; the ring thread (75 %) spent
~18 % of its samples in CopyVertices / memcpy / NEON loads and stores. The same-frame dedupe hits only ~3 %;
~16,000 lookups per second missed because the range was "copied in an older work slot": the same bytes as the
frame before, copied again because the upload buffer belongs to a work slot.
`app/src/me/native/usf4_vertex_cache.h`: a 24 MB buffer (cvar `usf4_vertex_cache_mb`) in the upload memory type,
handed out as a ring. A binding that misses the same-frame dedupe is looked up by (address, order, bytes); the guest
bytes are hashed (XXH3-64) and an equal hash binds the kept copy; a different hash copies to a new place (never in
place). Within a frame a range already checked is not hashed again until a guest synchronization (the dedupe's own
rule). An allocation is reused only when the last frame that bound it has finished (UseSlot follows the wait for
that slot's fence; fences signal in submission order); one still in flight at the front of the ring is retired
(no new draw gets it) and frees itself a few frames later. Ranges that change on 3 copies in a row are left out for
120 frames. Bindings from the cache use the buffer per binding (the bind check compares buffers too; the postponed
sky keeps them).
PC with the Switch settings, 10 s of fight: 950 MB bound from the cache (16,829 hash checks + 2,130 same-frame
reuses), 1.2 MB copied into it, 1.1 MB left on the per-frame path (was all of it).
PC lockstep (120 frames of fight, Switch settings) with the cache off and on: bit-identical (same frame hashes).
Console 0.6.2 (1785 / 768, 2026-10-15): buffer created; ~3.1-3.7 GB per 10 s bound from the cache (~6 MB per frame),
only 20-40 MB per 10 s still on the per-frame path. Fights 59-60 fps, Status Monitor cores 75 / 94 / 83 %, GPU 89 %.
But the ring's work per Swap did not drop (~12.5 ms, 27 us per draw): XXH3 over the 5.6 MB checked per frame was
~10 % of the ring's samples (its NEON loop plus an out-of-line XXH_memcpy). 0.6.3 hashes with the CRC fingerprint
(sa2_crc_fingerprint.h) on the Switch.

### 0.6.4: vertex ranges trusted between checks; pipelines saved sooner (2026-10-16)

Switch 0.6.3 at CPU 1785 / GPU 768 (Ryu vs Adon, night street; Ryu vs Guile): fights 57-58 / 59.9 fps, ring work per
Swap 13.7-14.3 ms (500+ draws) / 12.5 ms (~410 draws), 27 us per draw as before. The cache reused 3-4.5 GB per 10 s but
hashed all of it: Fingerprint64 was 15.8 % of the ring thread's samples (XXH3 in 0.6.2 ~10 %), so the faster hash did
not help; reading ~6 MB of guest memory per frame costs about what the copy did.

- usf4_vertex_cache_interval_max (8): a range that kept the same bytes in 2 checks in different frames is trusted
  without a hash for 2, 4, then 8 frames (jittered), only while it is bound every frame; a gap (new round, stage,
  loading) means a check first. A range found changed after being trusted is checked every frame (late_change).
- usf4_vertex_cache_audit (test): trusted ranges are hashed anyway and differences counted. PC training fight, 150 s:
  ~80,000 audited, 0 changed; hashed bytes 91 MB per 10 s against 954 MB reused.
- PC lockstep is no longer usable for this: two runs with the same settings diverge in game state (gauges, poses),
  probably since a fight run and a lockstep run shared the save folder (and out/pc/boot.log) concurrently. Do not run
  fight_run.sh and lockstep_run.sh at the same time.
- Pipelines: saved 15 s after any new one (was 60). In 0.6.3 Guile's special move compiled new pipelines (783 ms
  frame) 20 s before the user quit, after the last save.

### 0.6.5: video recording off (2026-10-16)

Switch 0.6.4 at 1785 / 768: fights 59.3-59.8 fps; on ~500-draw scenes the ring did 12.2 ms per Swap (24.4 us per draw;
0.6.3: 14.0 ms, 27.3 us). Fingerprint64 fell from 15.8 % to 5.1 % of the ring thread; ~3.4 GB per 10 s trusted
without a hash, ~0.55 GB hashed. The user: "much smoother yet again". The user also reported that holding Capture to
record a clip crashed the game, "too often" (the log just stops; application memory 3281 of 3285 MB). 0.6.5 calls
appletSetGamePlayRecordingState(false) at start-up (cvar switch_video_recording, default false; screenshots still
work).

Remaining CPU in fights (0.6.4 profile): ring 71 % of a core (Draw 10.9 %, memcpy 5.4 %, Fingerprint64 5.1 %,
WriteConstantRunRaw 3.1 %, PM4 parsing ~5 %, EDRAM4 bookkeeping ~5.5 %, malloc/free/mutex ~7 %); main game thread 71 %
(guest code, spread over many functions; largest sub_8220FFE8 7 %). The test builds also run the 1 ms stack sampler
(switch_profile_stacks) and per-category GPU timestamps (sa2_gpu_marks_categories), both diagnostic costs.

### 0.6.6: the game's memcpy routines run as the host's memcpy (2026-10-16)

In the 0.6.4 console profile the main game thread (71 % of a core) spent ~13 % in the Xbox 360 C library's copy
routines: sub_8220FFE8 7.1 % (with its VMX helper sub_822140C0 2.3 %) and sub_82514750 3.6 % (also 3-4 % of the job
workers). Note: the "memset 0x8220FFE8" in the Direct3D section above is wrong; 0x8220FFE8 is a memcpy (r3 dst, r4
src, r5 bytes, returns r3) and 0x8220EEA8 is the memset.

- app/src/usf4_native_copy.cpp, CPU enhancement usf4_cpu_native_memcpy (on): both routines copy with the host memcpy
  when the ranges do not overlap and do not wrap; otherwise the original runs. Guest code checked by FNV-1a
  (760 / 1160 bytes). tools/direct_calls.py --redo makes the 376 + 2,119 call sites call the hooks.
- usf4_cpu_native_memcpy_audit (test): PC boot + training fight, ~14.5 M copies compared after the original ran,
  0 differences; 9,391 overlapping calls of sub_82514750 (left to the original).
- PC native run: 150 s training fight, ~190,000 native copies per second, frames look correct (lockstep frames only
  differ by the known game-state drift).
- Switch fault handling: the host memcpy uses only LDP/STP/LDR/STR/LDUR/STUR, all decoded by rex_decode.h.

Console result of 0.6.6 (2026-10-17, 1785 / 768): both routines "native" at start-up, ~0.98 M native copies per 10 s.
The memcpy share of the main game thread fell from ~13 % to 3.5 %. The ~520-draw India stage holds 59.8 fps (0.6.4:
58.1-59.7 on ~500-draw scenes); the ring now waits for the game 2-2.7 ms per Swap there (0.6.4: 0.5-1.4), i.e. the game
thread hands frames over earlier. Cores in the screenshots 76 / 80 / 93 % (the ring core), GPU 92 %. User: "small
improvements again". The SwitchWindowedAppContext::Initialize log lines (incl. "video recording off") come before the
log file opens, so the recording switch cannot be confirmed from the log.

### 0.6.7: fewer allocations on the ring thread (2026-10-17)

From the 0.6.6 console profile's stacks, the ring thread's heap traffic came from: per-call std::vectors in
SynchronizeEDRAM4 (spans, merged), CopyTilesEDRAM4 (regions, rects, stacked) and RedirectClearDepthEDRAM4 (regions,
merged), each an operator new + malloc's recursive lock (rmutexLock); and paired_packets_ in me_native_system.cpp, an
unordered_map that got a node for every new command-buffer address and was never emptied.

- KeptList<T> (sa2_native_targets.cpp): one kept vector per nesting level and element type (the EDRAM functions can
  run inside each other), cleared on entry and exit.
- paired_packets_: an 8192-slot linear-probing table; entries of earlier frames count as empty (only the current
  frame's were ever used).
- NativeDrawExtentEstimator::PhysicalReadable: the Switch now uses the physical heap's page table, like PC/Windows,
  instead of RexGmQueryProtect (~1 % of the ring thread).
- PC checks: tiled fight (pairing path) 48,480 replays reusing a pairing, 0.327 dropped by predicate per replay as in
  earlier tiled runs; Switch-settings fight, 0 draw failures; lockstep frames match 0.6.6 apart from a hit spark.
- Not changed: NVK's vk_meta object frees at command-buffer reset (from vkCmdCopyImage through vk_meta).

Console result of 0.6.7 (2026-10-17, 1785 / 768): ~500-draw scenes (Yang / Ryu street stage) 10.9-11.2 ms of ring
work per Swap at 21.2-22.0 us per draw (0.6.6: ~12.2 ms, ~24 us); ring thread 68 % of a core in the profile (0.6.4:
71 %). Fights 59.4-59.9 fps; Status Monitor cores 76 / 77 / 94 % and GPU 95 % on that stage, 63 / 87 / 83 % and GPU 94 %
on Honda's stage (~315 draws, ring 8.5 ms, main game thread 85 % of a core there, now the busiest thread). The
deferred-recording worker (core 0) takes ~43 %. Hitches only at boot / loading (up to 600 ms) and one 174 ms frame at a
stage change. User: "this felt good. Seems like we're gaining some headroom".

### 0.6.8: profiling measurements off in the Switch build (2026-10-17)

The 0.6.7 console log showed the GPU using the whole frame on the busiest stage (16.7 ms per Swap, Status Monitor 95 %),
with 1.7 ms of "gap" between marks. The Switch defaults still had the profiling build's two measurements on since
0.2.0: sa2_gpu_marks_categories (a timestamp at every change of pass category, ~14 categories, many per frame) and
switch_profile_stacks (every thread interrupted every 1 ms). 0.6.8 turns both off in usf4_switch_defaults.toml. The
10-second report (per-thread CPU, ring and GPU totals per Swap) stays; only the per-category GPU split and the stack
samples go. Set both true again for a profiling build.

Console result of 0.6.8 (2026-10-17, 1785 / 768): Dudley's stage (~440 draws) 59.5-59.9 fps, Status Monitor GPU 89 %,
cores 71 / 92 / 81 %; Guile's stage GPU 81 %, cores 60 / 80 / 77 % (0.6.7: GPU 94-95 % on the ~500-draw street stage and
Honda's). Ring 10.1-10.3 ms per Swap at ~440 draws (22.9 us per draw, as in 0.6.7), ring thread 59.5 % of a core,
recording worker 39 % (0.6.7: 43 %), main game thread 66 %. CPU total in fights 217-242 % (0.6.7: 240-255 %). With the
category marks off, "GPU per Swap" only spans first mark to first mark (= the frame interval, 16.8 ms), so it no longer
shows GPU load; use Status Monitor. User: "cpu load seemed lower".

### 0.6.9: edge tiles of stencil-replace clears skip their old stencil (2026-10-17)

0.6.8 profiling build on the console: edram_import9 (depth imports with the 8 stencil-bit passes) 3.4-3.5 ms of GPU per
Swap in fights, scene 3.0-3.6, edram_import 2.2, copies 1.7. PC trace (Switch settings): every 9-pass import came from
one draw kind, VS n777 / PS n641, a one-rectangle clear with RB_DEPTHCONTROL 00008777 (Z ALWAYS + write, stencil ALWAYS,
zpass REPLACE, ref 0, write mask 0xFF) over 1024x1024 (D340, pitch 1040), 512x512 (D0E0, pitch 560) and 320x180 (D060 /
D528, pitch 320) targets. The proven stencil-replace rectangle covers each target's used area, but the right / bottom tile
of every row is only partly inside (padding to whole 80x16 tiles), so those tiles got their old stencil first: deferred
fetches merged into column imports (the "tiles" in the 9-pass list are column spans; ~140 real tiles per frame).

- usf4_edram4_stencil_edges_skip (on): tiles only partly inside the replace rectangle count as replaced (lazy-stencil
  record cleared at the draw; import with only the depth step). The padding pixels keep the view's stale stencil.
- PC: 9-pass imports none, 0 draw failures; lockstep frames vs 0.6.7 differ only by the usual animation drift
  (median 1.33 mean abs vs 1.04 between two earlier normal builds; background 0.07 % of pixels).
- Console test (profiling build, 2026-10-17): edram_import9 3.4 -> 0.3 ms (the remaining copy-engine stencil import),
  GPU "gap" 2.1 -> 4.5 ms per Swap, Status Monitor GPU 74-76 % in fights, no glitches seen. User: "load seems to be
  lowered".

Console result of 0.6.9 (normal build, 2026-10-17, 1785 / 768): fights 59.5-59.7 fps (longest frame 17.6 ms outside
round changes), ~400-420 draws, ring 9.1-9.4 ms per Swap. Status Monitor in fights: GPU 70-73 % (0.6.8: 81-89 %),
cores 64 / 85 / 83 %. CPU total 227-244 %. User: "it runs great".

### 0.7.0 test: the main thread sleeps while its jobs run (2026-10-17)

0.6.9 profiling build with per-caller yield counts (`switch_perf.cpp`, profiling builds only): in fights 0.65-0.92 million
yields per second came from one chain, KeDelayExecutionThread <- SleepEx (sub_8220FB70) <- Sleep (sub_8220FC00) <-
sub_82966C90, the main thread's "wait for my job batch" loop (collect finished jobs, run one if the list has one,
Sleep(0), look again); 10-17 % of the main thread's samples. Second source: the deferred-recording worker's 5 us naps
(30-48k per second, BeginCommands worker lambda, `sa2_native_deferred_recording_naps` 64), left as they are.

- usf4_cpu_job_wait_block (on, `src/usf4_job_wait.cpp`): the Sleep(0) at return address 0x82966E40 returns at once when
  a job finished since the last look, else yields for `usf4_job_wait_spin_us` (20) and then sleeps on a condition
  variable bumped by every sub_82967288 that popped a job (2 ms safety timeout). Hooks on sub_8220FC00 and sub_82967288
  (their direct `__imp__` calls in the generated code turned back into hookable calls; the addresses are in the sources,
  so tools/direct_calls.py keeps them). Code checks: sub_82966C90 488 bytes, sub_82967288 120 bytes.
- PC fight (Switch settings): code checks pass, ring 45 ms per Swap as before, ~46 sleeps per second.
- Console (normal build, 1785 / 768): fights 59.4-60.1 fps, Status Monitor GPU 74-76 %, cores 66-67 / 84-85 / 87-91 %,
  CPU total 234-236 %. Yields 0.45-0.64 million per second (0.6.9: 0.74-0.98), main thread 80.7 % of a core (0.6.9
  profiling: 88 %), ~130 condition-variable sleeps per second (~28 ms asleep per second): most waits end inside the
  20 us spin. User: "load is definitely down".
- Spin tests (usf4.toml, 2026-10-17): usf4_job_wait_spin_us 5 and 0 changed little (fights 59.7-60.0 fps, yields
  0.55-0.78 million per second, condition-variable sleeps 120-180 per second; the same single 44-45 ms frames as with
  20). User felt dropped frames with 5. Kept 20. The remaining yields are not this loop's spin:

### 0.7.1 test: the game's frame limiter sleeps (2026-10-17)

sub_82756DE8 (7 % of the main thread's samples in the 0.6.9 profile, via SleepEx) is the frame limiter: while the
seconds since the last frame (QueryPerformanceCounter delta x 1.0 / frequency, constant at 0x820EBABC) are below the
period [obj+176] (0.016667), Sleep(0) + read the clock. usf4_cpu_frame_limiter_sleep (on): its Sleep(0) (return address
0x82756ED0) sleeps for the missing time minus usf4_frame_limiter_margin_us (1000), then the loop spins as before. Code
check sub_82756DE8 336 bytes 0x18A88DF8143CF8A3. PC: the call site is reached with f13 = 0.016667 and f0 = 0.0006 at
the first look (the PC fight never runs ahead, so it only sleeps in menus).
- Console (0.7.1 normal build with usf4_job_wait_spin_us = 0 in usf4.toml, 2026-10-17, 1785 / 768): fights 59.9-60.3
  fps; yields 0.15-0.20 million per second (0.7.0: 0.45-0.64), clock reads (UpdateGuestClock) 0.13 million (0.7.0:
  0.5-1.0); limiter sleeps ~58 per second (one per frame), 0.22-0.55 s asleep per second. CPU total in fights 197-216 %
  (0.7.0: 234-236 %); main thread 54 % of a core (0.7.0: 81 %), job workers 6.5 % each (9.4 %), ring 53 %, recording
  worker 37 %. Status Monitor cores 62 / 79-89 / 56-70 % (0.7.0: 66 / 84 / 88 %), GPU 69-71 %. Single 43-52 ms frames
  in some 5 s windows (59.1-59.5 fps there), as in 0.7.0 (44-59 ms); no pipelines created, they come with new
  shader loads ("VS word diff" / "PS identity mismatch" lines) and round changes.

### 0.7.2: the recording worker sleeps at once on an empty queue (2026-10-17)

Console tests of v0.7.1 with `sa2_native_deferred_recording_naps` in usf4.toml (1785 / 768, fights):
- 16: naps 34-44k per second (64: ~50k), otherwise as 0.7.1.
- 0: naps 0.9-7k per second (what is left is the ring thread's WAIT_REG_MEM sleeps), recording worker 32.6 % of a core
  (36.5 %), CPU total 199-203 %, fights 60.0-60.4 fps, longest frames 17.1-17.6 ms apart from one 46 ms (round change).
  Status Monitor cores 57-62 / 56-66 / 80-87 % (0.7.1: 62 / 79-89 / 56-70 %), GPU 72 %. Ring 9.5-9.6 ms per Swap at
  ~410 draws (23.3 us per draw; 0.7.1 9.1-9.2 ms, 21-21.5 us): the condition-variable wake at each publish.
Switch default now 0 (usf4_switch_defaults.toml).

### 0.7.2: narrow sampler-cache invalidation for resolved crops (2026-10-17)

The console's per-fetch sampler cache missed 48 % of its lookups, 99 % of them "by generation". PC counters on every
InvalidateTextures / ForgetImage site: in fights only ResolvedWritten fires (~2 per frame): a resolve into a texture
that has crops (resolved_clips_) bumped the whole cache's generation. Crops are only sampled by fetches whose base is
the crop's address (the crop contract needs base_page << 12 == address), so usf4_resolved_crops_invalidate_narrow (on)
drops only the register / per-fetch entries whose fetch dword_1 base (bits 12-31) is that address
(InvalidateTexturesAt). PC: misses 50 -> 22 %; lockstep frames vs the old path median 0.09 mean abs diff (noise level).

Console (0.7.2 test, 2026-10-17, 1785 / 768, Elena's jungle stage, ~300-340 draws): misses 14 % (none by generation),
textures stage 2.4 -> 1.3-1.5 us per draw; fights 60.0-60.3 fps, CPU total 178-187 %, main thread 43.5 %, ring 41 %,
recording worker 29 %, Status Monitor cores 53 / 56 / 73 %, GPU 79 %. User: no glitches. (Fewer draws than the earlier
stages, so the totals are not directly comparable.) Remaining draw stages per draw: uploads 2.8-3.2 us, targets /
indices / vertices 2.4-2.9, record 1.9, framing 1.5.

### 0.7.3 diagnostics: what the fight hitches are (2026-10-17)

`[hitch]` line (usf4_frame_counter.cpp, frames over usf4_hitch_log_ms = 25): the main thread's wall time split into GPU
ring waits (usf4_gpu_wait iteration), job waits (sub_82966C90's Sleep(0)), the frame limiter, and the rest. The
profiler's hitch windows now start at 30 ms (were 45). Console run (0.7.3 profiling build, 2026-10-17, ~2 min of fights
at 60-63 fps): in fights 1 frame of 42-50 ms every ~15-30 s (the user noticed none). All but one are "the rest": the
game's own work on the main thread, not a wait on the renderer (e.g. 44.2 ms: GPU 0.0, jobs 0.7, rest 43.6). One at the
end of a match was 35.5 ms of waiting for the game's jobs while it opened its save data (XamContentCreate right after).
Frames right after a long one sometimes wait 18-23 ms for the GPU ring (the ring catching up: a 66 ms ring frame with
60 new textures, 14 MB, at a new effect). The stack samples caught nothing inside single ~45 ms windows (too few
samples per window). Not pursued: they are the game's own loads / effect set-up.

### 0.7.3: constants up to the last declared register (2026-10-19)

Console 0.7.3 test (C6 substages 19-23, ~400 draws, ring 9.1 ms per Swap): uploads 2.9 us per draw = textures 0.05,
vertices 1.4-1.6, indices 0.4, VS constants ~1.0, PS constants 0.37; indices stage "indices" 1.4. PC: vertices 1.0,
VS constants 0.4. VS constants were uploaded again for ~85 % of draws, and every shader whose constant table has an
array or matrix (RegisterCount > 1) counted as reading all 256 registers (4 KB copy), because XenosRecomp clamps
relative reads of an array to register 255. usf4_cpu_constants_exact_extent (on): without relative operands (no ALU
instruction with dword 1 bit 30/31, checked on every ALU instruction) the extent is the end of the last declared
Float4 constant. PC: 242 shaders narrowed; lockstep frames vs off median 0.23 mean abs diff (the same path in two
builds: 0.43).
- Console 0.7.3 test2 (2026-10-19, 1785 / 768, ~330-440 draws): uploads 2.9 -> 2.5-2.6 us per draw, VS constants
  1.0 -> 0.56-0.65; ring 9.2-9.4 ms per Swap at 410-440 draws (21.4-22.1 us per draw); fights 59.9-60.2 fps, CPU total
  202-209 %. User: "felt good", one dip at the end of the fight with Sakura: at the match end the game saves (opens
  savedata:\30x, LIST, save.dat, save-swan.dat, replays-swan.dat, 18-31 ms each on the SD, ~100 ms in all, main-thread
  frame 42.9 ms) and ~12 s later loads the next scene (174 ms frame, "the rest", announcer RYU_NML_J.cpk open 16 ms).

### 0.7.4: faster save-file opens (2026-10-19)

0.7.3 console: at a match end the game opens savedata:\30x, LIST, save.dat, save-swan.dat and replays-swan.dat with
FILE_OVERWRITE_IF (disposition 5), 18-31 ms each on the SD. The VFS overwrite was stat (stale-entry check) + delete +
create (fopen "wb", fclose, stat) + open.
- 0.7.4 test: an existing file is truncated in place (HostPathEntry::Truncate, the old fallback); delete + recreate is
  now the fallback. Console: 12-21 ms per file; the new "[io] open steps" line: check (the stat) 2-8 ms, replace
  (truncate) 5-12, open 0.5-2.4. Save loaded fine after a restart.
- 0.7.4 test2: FILE_OVERWRITE_IF on a file skips the stat (the truncation recreates a file that vanished).
- Fight hitches (0.7.4 test, user: "at the same points" each fight): single 44-46 ms frames, all "the rest" (game work
  on the main thread), e.g. 22:05:29.67 and 22:05:59.64 (30.0 s apart, as 22:39:12 / 22:39:42 / 22:40:12 on
  2026-10-17). Profiling builds now sample the "Main XThread" at every tick (1 ms) besides the round robin, and game
  frames over 30 ms register hitch windows (usf4_frame_counter.cpp), so the hitch section shows its stacks.
- 0.7.4 profiling test2 on the console (2026-10-19): save opens now "check 0.0 ms", truncate 9-23 ms, open 0.4-4.8:
  the FAT truncation (freeing the old clusters) is what is left. Main thread sampled every ms: in the fight hitches
  (single 43-50 ms frames, "the rest") its samples are, besides the frame limiter and the job wait, the game's own job
  work run on the main thread: sub_82966C90 (batch wait) -> sub_82967288 (run a job) -> sub_8219B6E0 ->
  sub_82925E40 -> sub_8292B7A8 -> sub_828BB588 -> sub_829CD8D0 (682 lines; calls sub_82183D60, sub_828B9280,
  sub_828B88B8, sub_828B72D8). Game logic at specific moments (round start, KO, REVERSAL / SUPER pop-ups, some 30 s
  apart): not a renderer wait; not pursued further (it would mean rewriting game code natively).

## Public release: NRO + installer page (2026-10-19/20)

User decision (2026-10-19): the repository goes public, releases carry the NRO (never game files or shaders), and a
browser installer makes the rest from the player's own disc, like sa2-nx's. Author name rewritten to Thorhax in all 57
commits and 8 tags (filter-branch, trees verified identical, force-pushed).
- `tools/installer_recipe.py`: per container of the package input (out/shaders/in, 874), a source in the player's
  files (EMZ stream slice, game file slice or default.xex image slice) plus patch runs; 847 as they are (6 game files
  and the image), 27 patched (3.1 KB), 15 Direct3D builds at run time carried inline (5.1 KB). `--check`: 0 mismatches.
- sa2-nx-installer's xex.js + LZX wasm decompress Default.xex to exactly out/default_image.bin.
- WebAssembly tools built from this repository with Emscripten 6.0.12 (`shaders/wasm/build_wasm_tools.sh`, translator
  now with -fwasm-exceptions: the one container the translator rejects, vs_vc43fc0b1422d5b97, is skipped as natively):
  HLSL identical for 873 of 873. DXC: nfsmw-nx-installer's v2025.1 wasm (same file as sa2-nx's); its SPIR-V differs
  from the PC's DXC 1.9 for 767 of 873 shaders. Library from the ISO in Node (usf4-nx-installer test/release_node.mjs):
  sha256 201e0bf7..., identical to the one built from out/shaders/in. PC lockstep with it: 0 draw failures, frames
  differ from the native library only on the moving characters (animation drift; median 1.19 this run, background and
  HUD identical).
- usf4-nx-installer (local repo): lib/xdvdfs.js (ISO in place), lib/shaders.js (recipe, EMZ via DecompressionStream
  'deflate-raw'), lib/installer.js; full usf4-nx.zip from the ISO in Node: 8.1 GB, 976 entries, 159 s, CRCs fine,
  Default.xex and battle.eaf byte-identical.
- The Switch NRO had libnx's default icon (USF4_NRO_ICON defaulted to empty when icon_local.jpg was absent): fixed to
  app/icon.jpg. Release NRO built without icon_local.jpg: its icon is app/icon.jpg byte for byte.

## Thorhax intro video (0.7.5, 2026-10-20)

User decision: every Thorhax port starts with the Thorhax intro (app/thorhax_intro, self-contained for other ports).
The first 3 s cannot be skipped, then any button ends it; - and + held together end it at any time. There is
deliberately no toml or cvar to turn it off (redistributors would).
- MPEG-1 (pl_mpeg, q3, no B-frames, 3.46 MB) + raw 48 kHz stereo PCM, linked with .incbin; libnx framebuffer on the
  default window, sound through audout, its clock from the played sample count. Player on core 2, decoder on core 1.
- It plays while the game boots: RexSwitchHoldOutput keeps the window without a surface, the audio output silent but
  consuming in real time and the pads untouched (until all buttons are up after the release). The release reconnects
  the surface on the UI thread and opens the audio device.
- Console (handheld): 169 of 169 frames, 0 late, decode 6.3 ms average / 11.5 ms slowest. The game reads its files
  from about 2 s into the intro and is at its normal frame pacing when it ends; its swapchain comes up about 4 ms
  after the intro (v1, intro before the boot: 1.5 s of GPU-clock check plus 2-4 s of loading on black).

## Controllers for two players (0.7.6, 2026-10-22)

Tester reports (Sphaira forwarder, Joy-Cons attached plus two Pro-style controllers): Start from a controller did
nothing, controller 2 only worked once Versus asked for player 2, and it sometimes "turned off".
- Players were numbered by slot order (handheld first), so attached Joy-Cons took player 1 even when unused. Now a
  slot only becomes a device after its first real button press: players are numbered in the order they are used.
- The quiet-until-released guard counted the sticks' pseudo-buttons, so a drifting stick kept a pad quiet; it now
  looks at real buttons only (bits 0-15, SL/SR).
- xam_input: XUSER_INDEX_ANY / XINPUT_FLAG_ANY_USER merge users 0-3 (state, keystrokes, capabilities) instead of
  pinning user 0; XamUserGetDeviceContext succeeds for every user with a controller. USF4 itself polls users 0-3
  with XINPUT_FLAG_GAMEPAD (logged once per user/flags: "input: the game asks ...").
- Console (tester, usf4_018-027): Start works from controller 1; controller 2 becomes player 2 on its first press.
  As on the 360, only the player who pressed Start drives the menus; player 2 joins at Versus character select.
- Controller 2 dropping out happened only with the Ultrahand overlay open (clock changes): HID reports style 0 with
  no focus change; a button press reconnects it and it is player 2 again.
- Status Monitor shows the output size (1280x720 / 1920x1080), not the render size: "[res] ... render 960x540"
  confirms sa2_render_height_* = 540 is applied.
- Default render height is now 540 handheld and docked (built-in defaults and the usf4.toml written at first start);
  an existing usf4.toml without these keys gets 540 too. The installer still ships no usf4.toml (no overwrite).

## 0.8.0: Renut on deko3d on the Switch (2026-10-23)

The full case study, written for other ports: [deko3d-renderer-usf4.md](deko3d-renderer-usf4.md).

The Switch release draws with Renut's Direct3D-level renderer on deko3d (`docs/renut-census.md`), built with
`-DUSF4_RENUT_RENDERER=ON -DSA2_RENUT_RELEASE_SHADERS=ON -DREXGLUE_SWITCH_NO_NVK=ON` (`out/nx-release`; Mesa's NVK is
not linked: 56.3 MB instead of 66.7 MB). The NRO holds no translated game shader: `deko3d_glsl.bin` beside it is made
by the installer page from the player's disc (the page's `lib/deko3d.js`, from sa2-nx-installer; its translator is
this repository's `app/thirdparty/reblue-XenosRecomp` with `tools/renut_hlsl_frontend.cpp`). 720p in both modes (Renut
has no 540p).

Console measurements (V1, docked, Training fight, 2 minutes each):

| | NVK 540p (0.7.6) | NVK 720p | Renut 720p |
|---|---|---|---|
| fps | 59.9 | 53.5 | 59.9 |
| GPU per frame | 6.9 ms | 9.3 ms | 8.4 ms |
| ring / render thread | 7.4 ms | 18.6 ms | 2.8 ms |
| CPU total (of 400 %) | 167 % | 155 % | 110 % |

Renut at GPU 768 MHz: Training 59.9 fps (GPU 8.8 ms); attract-mode fights on four stages 59.2-59.9 fps (GPU 8-11 ms,
render thread 4.7-8.2 ms). Lesson from the first comparison: without `sa2_shaders.mesp` beside the NRO the native
renderer draws nothing (every draw "no VS entry, no PS entry", sys-botbase screenshots black) and still reports 60 fps;
its first numbers were of an empty frame.

Also in 0.8.0:

- **Maxwell clips to the smallest bound target:** USF4 leaves a 320x180 depth target bound while it draws full-screen
  passes; a draw (or colour clear) that neither tests nor writes depth no longer binds a smaller depth target
  (`render/draw.cpp`). Before, menus and fights showed only the top-left 320x180 on the console.
- **Linear textures:** a linear texture unchanged at 60 full checks gets the tiled textures' sampled hash each frame
  and its full hash every 16 frames (`usf4_renut_static_linear_textures`): the render thread hashed 11 GB every 10 s in a
  fight; 6.4 -> 2.8 ms a frame.
- **Late input** counts the game's frame limiter wait (`usf4_late_input_limiter_margin_us`, 3 ms more margin): input ->
  display 34 -> ~31 ms with no extra dropped frames; the native renderer has the same (`src/usf4_late_input.cpp`).
- **Switch button icons** (`usf4_button_icons`, `src/usf4_button_icons.cpp`): USF4's prompts are 32x32 textures of their
  own (A, B, X balls, RB, BACK in two forms), known by a hash of their bytes and redrawn at upload, for both renderers.
- **Log network sink:** non-blocking socket; a stalled listener had frozen the game behind spdlog's sink mutex.
- **Wait tracker** (`sdk/src/system/wait_watch.cpp`) and the `[waits]` stall report (`usf4_stall_report_s`).
- First play with empty caches: the first fight's load compiles ~90 shaders (~7.6 s once; kept in `deko3d_shaders.bin`).

Updating from 0.7.x: `sa2_shaders.mesp`, `sa2_shaders.mesp.idx` and `cache/sa2_native_pipelines.bin` are no longer used
and can be deleted; `usf4.toml`'s 540 render height draws at 720.

## 0.8.1: Switch button names on the controller settings screen (2026-10-25)

- **Button names** (`src/usf4_button_names.cpp`, with `usf4_button_icons = "switch"`): the controller settings screen
  listed actions with Xbox button names (X, Y, A, B, LB, LT, RB, RT), reported by a player. They are text in the game's
  "#M4S" string tables (ENG, FRA, GER, ITA, JPN, KOR, SPA; EMZ streams of `archive/ui/ui.eaf` and `dl2/ae2.eaf`), not
  textures, so 0.8.0's icons did not reach them. A hook on the game's zlib inflate (`sub_82776738`, FNV-checked)
  rewrites the eight names as a table is inflated: by position (`input_xbox_layout`, the default) Y X B A, by letter X
  Y A B; L ZL R ZR. Each new name fits in the old one's units, so no table offset moves.
- **The game checks each EMZ stream's CRC-32:** the first build changed only the names and the whole table was refused
  (every text of the screen showed its id, `ID_OPT_1108`...). The two units the shorter names free get values that keep
  the CRC-32 as it was (solved on the PC per layout, checked on the seven tables). Console: Controls shows
  Y X R L B A ZR ZL for the default layout, and pressing the Switch's B assigns the row named B.
- Installer page: the shader step shows its progress (it takes about a minute on a fast PC, several on a slow one, and
  had looked stuck), and the page unpacks the game's archives with fflate instead of the browser's
  `DecompressionStream`. The shaders are unchanged: updating from 0.8.0 only replaces `usf4.nro`.
