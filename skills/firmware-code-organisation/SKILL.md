---
name: firmware-code-organisation
description: >
  How to keep an ESP-IDF firmware small, well-organised and testable: one component per
  concern, files under 300 lines, pure logic separated from hardware so it runs under gcc +
  Unity on the host, documented headers, SOLID applied to C, and the Balanced Coupling rule for
  deciding what goes together. Use when structuring a new firmware, when a file is getting
  long, when deciding whether to split or merge components, when adding tests, when reviewing
  generated code, and before declaring any firmware task done (the checklist at the end).
---

# Firmware code organisation

Small firmware that an assistant and a human can both reason about has the same shape every
time: a thin hardware layer, pure logic beside it, and a state machine on top. The reference
firmware on this board grew to 13k lines of C and stayed workable only because of these rules.

## 1. Components, one per concern

```
components/
  board/     pins + capability flags as macros; header-only, no code
  ui/        display + touch + knob + backlight bring-up, LVGL screens
  theme/     colour = state policy from the design tokens; no LVGL dependency
  haptic/    DRV2605L driver
  net/       Wi-Fi, HTTP client, streamed upload
  app/       the state machine; the only module that knows about all the others
main/        app_main(): init order, nothing else
test/host/   gcc + Unity tests for pure modules
```

Dependency direction points inward: `app` → `ui`, `net`, `haptic`, `theme`; `ui` → `theme`,
`board`; `net` → `board` (for nothing but the name). `ui` never includes `net`. `theme` never
includes LVGL. `board` includes nothing.

Each component has `CMakeLists.txt`, `include/<name>.h` (public), and `.c` files. Private
headers stay next to the `.c` (`foo_priv.h`), not in `include/`.

## 2. Files under 300 lines

A 300-line ceiling keeps a file readable in one sitting and small enough for an assistant to
hold in context with its neighbours. When a file approaches it, split by **responsibility**,
not by line count:

- `knob.c` (GPIO + timer) and `knob_decode.c` (the detent state machine)
- `net_upload.c` (the protocol) and `net_upload_put.c` (one streamed PUT)
- `screen_detail.c` (layout) and `screen_detail_status.c` (status rendering)

If a split produces two files that always change together, merge them back and split along a
different seam.

## 3. Pure logic beside the hardware

Any code that makes a decision should compile on the host:

| Pure (host-tested) | Hardware wrapper around it |
|---|---|
| `knob_decode.c` — samples in, ±1 steps out | `knob.c` — GPIO read every 3 ms |
| `upload_plan.c` — file size → parts, offsets, lengths | `net_upload.c` — HTTP calls |
| `theme.c` — state → colour, motion period | screens call `lv_color_hex()` |
| `backlight_policy.c` — level, floor, dim → effective duty | `backlight.c` — LEDC |

Rules for a pure module: no `esp_*`, no FreeRTOS, no LVGL includes; state passed in as a
struct, zero-initialised means idle; no globals; no allocation. It is then a plain C file
that `gcc` compiles with Unity.

Test layout (see `templates/knob-starter/test/host/`):

```
test/host/unity/           vendored Unity (unity.c, unity.h, unity_internals.h)
test/host/stubs/           tiny stand-ins for sdkconfig.h and generated headers
test/host/test_knob_decode.c
test/host/test_upload_plan.c
test/host/test_theme.c
test/host/test_board.c     includes the board header, asserts no two signals share a GPIO
scripts/test.sh            compiles each test_*.c with its module and runs it
```

A test file: `#include "unity.h"`, `setUp`/`tearDown`, `TEST_ASSERT_EQUAL_INT`, a `main` with
`UNITY_BEGIN()` / `RUN_TEST` / `UNITY_END()`. Keep tests behavioural: "one detent yields exactly
one step", "a 17 MB file makes three 8 MiB parts, the last 1 MiB".

## 4. Headers are the documentation

Every public header opens with:

1. What the module owns (one sentence).
2. What it depends on and what it must never include.
3. Threading: which task calls what, what holds which lock.
4. Lifetime of pointers passed to callbacks ("valid only during the call; copy").

Then each function gets two or three lines: what it does, what it returns on failure, what it
does not do. Write the comment before the code; if it is hard to write, the function is doing
too much. Inside `.c` files comment the *why* (the lesson, the bug number, the datasheet
section), never the *what*.

## 5. SOLID, in C

- **Single responsibility:** one reason to change per file. `theme.c` changes when the design
  changes, `knob_decode.c` when the switch behaviour changes.
- **Open/closed:** extend behaviour through callbacks and function tables, not `#ifdef` forests.
  `net_set_result_cb(cb)` lets `app` react to uploads without `net` knowing `app` exists.
  Board differences are capability macros (`BOARD_HAS_HAPTIC`) checked in exactly one place.
- **Liskov:** a stubbed implementation (host test stub, `haptic` on a board without one) must
  honour the same contract: same return values, same "no-op is safe" guarantee.
- **Interface segregation:** small public headers. `net.h` exposes six functions; the
  multipart machinery is `net_priv.h`.
- **Dependency inversion:** high-level `app` depends on the `net.h` abstraction, not on
  `esp_http_client`; pure modules depend on nothing. Opaque state structs (`knob_decode_state_t`)
  keep callers from reaching inside.

## 6. Balanced Coupling, on one page

Coupling has three dimensions. Balance them and change stays cheap.

| Dimension | Meaning | Low | High |
|---|---|---|---|
| **Integration strength** | How much two modules know about each other | Shared value through a callback | Shared struct layout, shared globals |
| **Distance** | How far apart they live | Same file | Different component, different repo |
| **Volatility** | How often the reason to change arrives | Pins, panel init table | Screens, product rules |

The balance rule: **strong coupling is fine at short distance; across distance, coupling must
be weak**. Problems come from strong coupling at long distance (two components sharing a
struct) or from weak-but-chatty coupling between things that always change together (ten
callbacks between two files that should be one).

Applied to the template:

- `knob.c` ↔ `knob_decode.c`: strong, short distance, low volatility → fine.
- `app` ↔ `net`: one callback and three functions → weak across a component boundary → fine.
- `ui` ↔ `theme`: `ui` calls `theme_state_color()` and nothing else → weak → fine; the screens
  (high volatility) never leak into `theme` (low volatility).
- Smell: a screen file that includes `net.h` to fire a request. That is strong coupling across
  distance between a volatile and a stable module. Route it through `app`.

## 7. Low cyclomatic complexity

Cyclomatic complexity counts the independent paths through a function: one, plus one for every
`if`, `else if`, `case`, `&&`, `||`, `?:`, loop and early `return` inside a loop. It is the
best single predictor of where bugs hide and of what a reader (or an assistant) gets wrong.
Firmware grows it fast: error handling, state machines and hardware quirks all add branches.

Targets for the workshop, per function:

| Complexity | Verdict |
|---|---|
| 1–5 | Fine. Most functions in the template sit here. |
| 6–9 | Acceptable if the function is a flat dispatcher (one `switch`, one line per case). |
| ≥ 10 | Split before merging. Nesting deeper than three levels is the same signal. |

Tactics that work in C, all used in the template:

- **Guard clauses first.** `if (!cli) return false;` at the top instead of wrapping the body
  in `if (cli) { ... }`. Each guard is one branch, but nesting stays flat.
- **Tables instead of branches.** A state → colour map (`theme.c`), a state → text map, an
  array of handlers indexed by an enum. One lookup replaces a `switch` that grows per state.
- **Pure decision, separate effect.** `knob_decode_step()` returns `+1/-1/0`; the caller only
  acts on it. The decision function is where the branches live, and it is host-tested, so the
  effect function stays almost straight-line.
- **Extract the named predicate.** `if (resp_has_room(c, len))` beats
  `if (c->buf && c->len + len < c->cap - 1)`. The condition gets a name and its own test.
- **One retry loop, one attempt function.** `net_http_put_stream()` loops; `put_once()` does
  one attempt. Neither has to know about the other's branches.
- **No boolean parameters that pick a path.** Two functions beat one function with a flag
  that doubles its paths.

Measure, do not guess. Any of these works from the project root with no IDF:

```bash
pipx run lizard -C 10 components/ main/     # CCN per function, warns above 10 (or pip install lizard)
sudo apt install pmccabe && pmccabe components/*/*.c | sort -rn | head
```

Ask the assistant to run the tool on what it just wrote and to split anything flagged. It
will usually propose a table or a guard clause on its own once it sees the number.

## 8. Done checklist

Run this before saying a firmware change is finished.

- [ ] Builds in the Docker toolchain with zero warnings in your components.
- [ ] `scripts/check-board-pins.sh` passes: no GPIO literal outside `components/board`.
- [ ] Every pure module touched has a host test, and `scripts/test.sh` is green.
- [ ] No file over 300 lines; no function over ~60 lines or cyclomatic complexity 10.
- [ ] Each public header documents ownership, dependencies, threading and pointer lifetimes.
- [ ] No new `#include` crosses the dependency direction in §1.
- [ ] No blocking call inside an LVGL callback; no LVGL call outside the lock.
- [ ] Heap logged at the phases the change affects; internal minimum-free noted in the notes.
- [ ] Secrets (`wifi_secrets.h`) untracked; `wifi_secrets.example.h` updated if keys changed.
- [ ] Flashed and seen working on the device, or stated plainly that it was not.
