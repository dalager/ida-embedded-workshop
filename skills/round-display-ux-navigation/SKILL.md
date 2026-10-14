---
name: round-display-ux-navigation
description: >
  UX and navigation foundations for a 360×360 round touch display with a turn-only knob,
  built on LVGL 9: a wrapping spine of surfaces walked by horizontal swipe or knob, shallow
  details with a chevron back, vertical lists scrolled by swipe or knob, tap as the only
  commit, a modal overlay layer for anything consequential, and three redundant routes home.
  Use when adding a screen, a list, a detail view, a settings page, a back button, an
  idle timeout, or any gesture handling; when the assistant proposes lv_tileview, an LVGL
  encoder group, edge-swipe back, or a "press the knob" interaction; or when a round-panel
  layout clips text or targets at the corners. Pairs with scaffold-knob-firmware.
---

# Round display UX and navigation

The reference firmware ships a navigation model that took several design rounds to get right.
Copy the model, not the screens. It gives a new app an "ok" UX in an hour and keeps every
later screen consistent, because there is exactly one way to move, one way to commit, one way
back, and one place for anything that must not be interrupted.

## 1. The constraint that shapes everything

The knob only turns. There is no press. So the two jobs a smartwatch crown does are split
permanently:

| Input | Role | LVGL source |
|---|---|---|
| Turn the knob | **motion only**: walk the spine, scroll a list, seek a value | `knob_init(on_step)` |
| Swipe | the same motion, for thumbs-only use | `LV_EVENT_GESTURE` on the screen |
| Tap | **the only commit**: open, select, toggle | `LV_EVENT_SHORT_CLICKED` on the target |
| Back chevron | up one level | a 40 px circle button at the top-left of the safe circle |
| Hold ~1 s | reserved for the one consequential action (start / stop) | `LV_EVENT_LONG_PRESSED` |

Rules that follow: the knob never commits, so a stray nudge can never confirm anything; every
knob motion has a swipe twin, so the device works without the knob; the knob gets **no LVGL
focus group** (`lv_indev_set_group` is never called), which removes the "am I scrolling or
editing?" ambiguity that encoder groups create.

## 2. Anchor, spine, depth, protected

- **Anchor.** Surface 0 is Home: the rest position, where the idle timer returns.
- **Spine.** A short, fixed ring of top-level surfaces (3 to 5; the reference uses 4). Swipe
  left or turn clockwise goes to the next, right or counter-clockwise to the previous. **It
  wraps**, so there is no dead end: keep going and you arrive home. Position dots near the
  top show where you are.
- **Depth ≤ 2.** A surface can open one detail (tap a row). A detail never opens another
  detail. A press-less back only stays graceful while the tree is this shallow.
- **Protected.** Anything consequential (recording, uploading, a destructive confirm) is a
  **mode on an overlay layer**, not a place on the spine. You enter it by intent or by event,
  leave it by intent or by resolution. Browsing underneath never touches it.

Three redundant routes home, none needing the knob: the chevron pops one level; keep
swiping and the wrap lands on Home; after ~20 s of no input a timer returns to Home.

## 3. The LVGL mapping (the part to copy)

**Surfaces are `lv_screen` objects, swapped with a slide.** Not `lv_tileview`: it does not
wrap and its momentum scroll reintroduces commit ambiguity.

```c
static lv_obj_t *s_surface[SURFACE_COUNT];
static int s_index;                 // which surface is showing
static int s_depth;                 // 0 = surface, 1 = detail
#define SLIDE_MS 200                // the one place a full-screen redraw is acceptable

static lv_obj_t *new_surface(int idx) {
    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr, lv_color_hex(HUD_BG), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    lv_obj_remove_flag(scr, LV_OBJ_FLAG_SCROLLABLE);   // the SCREEN never scrolls; lists do
    add_dots(scr, idx);
    wire_inputs(scr);                                  // gesture + click, see below
    return scr;
}

static void move_spine(int dir) {                      // +1 next, -1 previous; wraps
    s_index = (s_index + dir + SURFACE_COUNT) % SURFACE_COUNT;
    lv_screen_load_anim(s_surface[s_index],
        dir > 0 ? LV_SCR_LOAD_ANIM_OVER_LEFT : LV_SCR_LOAD_ANIM_OVER_RIGHT, SLIDE_MS, 0, false);
    lv_timer_reset(s_idle_timer);
}
```

**One gesture handler on every screen, central, depth-aware.**

```c
static void on_gesture(lv_event_t *e) {
    (void)e;
    if (overlay_active()) return;                      // a mode is up: swipes do nothing
    lv_indev_t *indev = lv_indev_active();
    lv_dir_t dir = lv_indev_get_gesture_dir(indev);
    if (s_depth == 1) {                                // inside a detail: right = back
        if (dir == LV_DIR_RIGHT) { nav_back(); lv_indev_wait_release(indev); }
        return;
    }
    if (dir == LV_DIR_LEFT)       { move_spine(+1); lv_indev_wait_release(indev); }
    else if (dir == LV_DIR_RIGHT) { move_spine(-1); lv_indev_wait_release(indev); }
}

static void wire_inputs(lv_obj_t *scr) {
    lv_obj_add_flag(scr, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(scr, on_gesture, LV_EVENT_GESTURE, NULL);
}
```

`lv_indev_wait_release()` matters: it swallows the rest of the touch so a swipe that began on
a clickable row does not also fire that row's click on release. Without it, swiping a list
opens whatever your finger landed on.

**The knob drives the same axis.** In the knob callback (it runs on the LVGL task): at depth
0 call `move_spine(dir)`; at depth 1, or on a surface that owns a list, scroll the list
(§4). Also call the "activity" hook so a turn resets the idle timer like a touch does.

**Details are pushed, never stacked deeper than one.**

```c
void nav_open_detail(lv_obj_t *content) {              // content is a screen with no dots
    lv_obj_t *back = lv_obj_create(content);           // 40 px circle, HUD_PANEL fill, 1 px HUD_LINE
    lv_obj_set_size(back, 40, 40);
    lv_obj_set_style_radius(back, LV_RADIUS_CIRCLE, 0);
    lv_obj_add_flag(back, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_ext_click_area(back, 8);
    lv_obj_align(back, LV_ALIGN_TOP_MID, -85, 62);     // inside the 340 px safe circle
    lv_obj_add_event_cb(back, on_back_click, LV_EVENT_CLICKED, NULL);
    lv_label_set_text(lv_label_create(back), LV_SYMBOL_LEFT);
    wire_inputs(content);
    lv_screen_load_anim(content, LV_SCR_LOAD_ANIM_OVER_LEFT, SLIDE_MS, 0, false);
    s_depth = 1;
}
void nav_back(void) {
    if (s_depth == 0) return;
    lv_screen_load_anim(s_surface[s_index], LV_SCR_LOAD_ANIM_OVER_RIGHT, SLIDE_MS, 0, true);
    s_depth = 0;                                       // auto_del = true frees the detail
}
```

A row that opens a detail builds its content screen and calls `nav_open_detail()` itself; no
per-feature callback plumbing. Mark a detail "keep" when the user reads or waits on it, so the
idle timer does not pop it mid-read; the chevron and swipe-right still leave.

**The idle timer, re-armed on every input.**

```c
static void to_home(lv_timer_t *t) {
    (void)t;
    if (s_depth == 1 && !s_detail_keep) nav_back();
    else if (s_index != 0) { s_index = 0; lv_screen_load_anim(s_surface[0], LV_SCR_LOAD_ANIM_OVER_RIGHT, SLIDE_MS, 0, false); }
    if (!overlay_active()) backlight_set_percent(BACKLIGHT_IDLE_PCT);   // the panel is the power draw
}
s_idle_timer = lv_timer_create(to_home, 20000, NULL);  // reset it in every gesture/click/knob handler
```

**The protected layer.** Build modal content on `lv_layer_top()`. It survives every
`lv_screen_load_anim` underneath, so "browse while a mode runs" and "idle timeout never ends
the mode" hold structurally: navigation has no code path into the mode. Show it with a fade,
not a slide, and gate the gesture handler on `overlay_active()` while it is up. The state
machine owns the mode; the overlay only mirrors it (direction is app → ui, never back).

## 4. Vertical navigation inside a surface

A surface that lists things is a vertical axis. The screen itself is not scrollable; one
container is.

```c
lv_obj_t *list = lv_obj_create(scr);
lv_obj_remove_style_all(list);
lv_obj_set_size(list, 260, 240);                      // inside the safe circle
lv_obj_center(list);
lv_obj_set_layout(list, LV_LAYOUT_FLEX);
lv_obj_set_flex_flow(list, LV_FLEX_FLOW_COLUMN);
lv_obj_set_scroll_dir(list, LV_DIR_VER);              // vertical only: horizontal stays the spine's
lv_obj_set_scrollbar_mode(list, LV_SCROLLBAR_MODE_OFF);
```

Rows: 260 × 42 px, flex row, space-between, a 1 px bottom hairline in `HUD_LINE`, a left label
and an optional right value. `LV_OBJ_FLAG_CLICKABLE` plus `LV_EVENT_SHORT_CLICKED` opens the
detail. Rows never scroll themselves (`lv_obj_remove_flag(row, LV_OBJ_FLAG_SCROLLABLE)`).

The knob scrolls the list in **detents, bounded, animated**, through one shared primitive so
every list feels the same:

```c
void scroll_by_detents(lv_obj_t *cont, int steps, int32_t unit_px) {
    if (!cont || steps == 0) return;
    lv_obj_scroll_by_bounded(cont, 0, -steps * unit_px, LV_ANIM_ON);   // negative dy = down
}
```

Pick `unit_px` per screen: one row height for a list, three line-heights for body text (a
readable nudge, not a pixel crawl, not a page jump). Hard stop at both ends, no wrap, no
overscroll: a list is not a ring. Facet pagers (Info / Transcript / Summary) are the one case
of horizontal paging inside a detail; then hide the chevron off the first page and let a
right-swipe on the first page be "back".

## 5. Round-panel layout rules

- **Safe circle 340 px** on a 360 px panel. Everything hittable or readable lives inside it;
  the outer ring is for arcs and glow only. Use `HUD_SAFE_DIA` from the tokens.
- **Nothing in the corners.** A 260 px wide column centred is safe at every height the list
  uses; a full-width label at the top or bottom is not.
- **Orientation rides the rim.** Position dots near the top, progress as an arc on the rim,
  a state colour on an arc or ring. No status bar, no corner buttons.
- **Colour = state.** One accent per screen from `theme_state_color()`, never a literal hex
  in a screen file. The state motion period from `theme_state_motion_ms()` drives the only
  animation on the screen.
- **Two typefaces, strict roles.** A humanist sans for labels; a mono for machine data
  (timestamps, sizes, codes). A faint, letter-spaced title at the top of each surface.
- **Flat tones.** RGB565 bands on gradients; use flat panel colours and one highlight arc.
- **Never silent.** A list with zero items shows the designed empty state with a route home,
  never a blank screen.
- **Animate only the deliberate move.** The 200 ms slide and a fade for the overlay. Steady
  state is dirty-rect; a full-screen animation on this QSPI panel tears (no TE line).

## 6. Adding a screen, step by step

1. Decide its plane: a surface on the spine, a detail under one, or a mode on the overlay.
2. Surface: add an enum entry, build it with `new_surface(idx)` and a faint title, add it to
   `s_surface[]`. Dots update themselves from `SURFACE_COUNT`.
3. Detail: write `lv_obj_t *screen_x_create(void)` returning a content screen; the opening
   row calls `nav_open_detail(screen_x_create())`. No back-button code in the screen.
4. List inside it: one vertical container, rows per §4, knob scroll through the shared
   primitive with this screen's `unit_px`.
5. Every input handler calls the activity hook and does state + render only. No I/O, no
   blocking, no network in an LVGL callback (hand off to a task, read results from atomics).
6. Flash, then check: swipe both ways wraps; chevron, wrap and timeout all reach Home;
   knob and swipe do the same thing on every screen; a swipe over a row does not open it;
   nothing is clipped at the corners; the overlay survives a surface change.

## 7. Pitfalls the reference firmware designed against

| Temptation | Why not | Do instead |
|---|---|---|
| `lv_tileview` for the spine | no wrap → press-less dead end; momentum scroll | screens + `lv_screen_load_anim` |
| Encoder indev with a focus group | scrolling vs editing ambiguity, knob can commit | knob = motion only, tap commits |
| Edge-swipe to go back | unreliable on the rim, collides with the spine axis | chevron + right-swipe in details |
| Tap to stop or pause a session | one stray tap ends it | hold → confirm → hold, on the overlay |
| Put the mode on the spine | navigation can reach and break it | `lv_layer_top()` overlay |
| Scrollable screen object | vertical and horizontal gestures fight | one scrollable container, `LV_DIR_VER` |
| Per-screen gesture recognisers | inconsistent thresholds and double handling | one central handler, depth-aware |
| Blank empty list | user thinks it is broken | designed empty state with a route home |
