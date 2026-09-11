# M5 Remote — the actual Rotation screen

This builds the real screen, superseding the "not built yet" note in
`M5-REMOTE-ROTATION-INTEGRATION.md`. Written directly against the cloned
`ortlof/OSSM-M5-Remote` source (SquareLine Studio 1.0.5 / LVGL 8.2 export),
mirroring the exact construction style already used for the Pattern and
Torque screens - not hand-rolled LVGL from scratch. If you later open this
project in SquareLine Studio itself, these are the same calls it would have
generated for equivalent widgets; editing visually afterward should re-sync
fine as long as the object names below are kept.

Layout: header + battery (every screen has these) → "Rotation" title →
speed slider (encoder 1, mirrors Home's speed row) → pattern roller
(encoder 4, mirrors the Pattern screen's roller) → a "Reverse" checkbox
(touch only, mirrors Settings' `ui_lefty` checkbox) → the usual 3-button
footer (L = Menu, M = Start/Stop, R = Save). Matches how every other screen
in this UI re-purposes the same 4 encoders + 3 buttons per screen - see the
"how will the user control..." discussion for why that's the right model
here rather than fighting Home for its encoders.

**2026-09-11 update:** the rotation pattern roller (section 3c below) now
lists all 17 rotation pattern names, not the original 5 - see
`PATTERN-INTEGRATION-NOTES.md` section 1 for why this list must stay in
sync with `RotationPattern.h`'s `rotationPatternTable[]` order.

## 1. `src/language.h` — two new strings

```cpp
#define T_MENUE             "Menu"
#define T_ROTATION          "Rotation"   // <-- add
#define T_REVERSE           "Reverse"    // <-- add
```

Everything else (title, speed label, pattern-select label, save/menu/home
button text) reuses existing constants (`T_SPEED`, `T_SELECT_PATTERN`,
`T_SAVE`, `T_HOME`) - no need to duplicate strings that already say the
right thing on another screen.

## 2. `src/ui/ui.h` — new extern declarations

Add near the other screen-object externs (after the `ui_Torqe` block is a
reasonable spot):

```cpp
extern lv_obj_t * ui_Rotation;
extern lv_obj_t * ui_Logo7;
extern lv_obj_t * ui_RotationButtonL;
extern lv_obj_t * ui_RotationButtonLText;
extern lv_obj_t * ui_RotationButtonM;
extern lv_obj_t * ui_RotationButtonMText;
extern lv_obj_t * ui_RotationButtonR;
extern lv_obj_t * ui_RotationButtonRText;
extern lv_obj_t * ui_Batt7;
extern lv_obj_t * ui_BattValue7;
extern lv_obj_t * ui_Battery7;
extern lv_obj_t * ui_RotationTitle;
extern lv_obj_t * ui_RotationSpeedL;
extern lv_obj_t * ui_rotationspeedslider;
extern lv_obj_t * ui_rotationspeedvalue;
extern lv_obj_t * ui_RotationPatternTitle;
extern lv_obj_t * ui_RotationPatternS;
extern lv_obj_t * ui_rotationdirection;
```

No `ui_Rotation_screen_init` prototype needed here - like the other screen
init functions, it's only ever called from `ui_init()` in the same file
(`ui.c`), so C's same-translation-unit visibility covers it.

## 3. `src/ui/ui.c`

### 3a. Global object-pointer definitions, alongside the others near the top of the file (after the `ui_Torqe`-related block, ~line 100)

```cpp
lv_obj_t * ui_Rotation;
lv_obj_t * ui_Logo7;
lv_obj_t * ui_RotationButtonL;
lv_obj_t * ui_RotationButtonLText;
lv_obj_t * ui_RotationButtonM;
lv_obj_t * ui_RotationButtonMText;
lv_obj_t * ui_RotationButtonR;
lv_obj_t * ui_RotationButtonRText;
lv_obj_t * ui_Batt7;
lv_obj_t * ui_BattValue7;
lv_obj_t * ui_Battery7;
lv_obj_t * ui_RotationTitle;
lv_obj_t * ui_RotationSpeedL;
lv_obj_t * ui_rotationspeedslider;
lv_obj_t * ui_rotationspeedvalue;
lv_obj_t * ui_RotationPatternTitle;
lv_obj_t * ui_RotationPatternS;
lv_obj_t * ui_rotationdirection;
```

### 3b. Event handlers, alongside the others in the `///// FUNCTIONS /////` block (right after `ui_event_TorqeButtonR`, ~line 345)

```cpp
static void ui_event_Rotation(lv_event_t * e)
{
    lv_event_code_t event = lv_event_get_code(e);
    lv_obj_t * ta = lv_event_get_target(e);
    if(event == LV_EVENT_SCREEN_LOADED) {
        screenmachine(e);
    }
}
static void ui_event_RotationButtonL(lv_event_t * e)
{
    lv_event_code_t event = lv_event_get_code(e);
    lv_obj_t * ta = lv_event_get_target(e);
    if(event == LV_EVENT_CLICKED) {
        _ui_screen_change(ui_Menue, LV_SCR_LOAD_ANIM_FADE_ON, 20, 0);
    }
}
static void ui_event_RotationButtonM(lv_event_t * e)
{
    lv_event_code_t event = lv_event_get_code(e);
    lv_obj_t * ta = lv_event_get_target(e);
    if(event == LV_EVENT_CLICKED) {
        rotationbuttonmevent(e);
    }
}
static void ui_event_RotationButtonR(lv_event_t * e)
{
    lv_event_code_t event = lv_event_get_code(e);
    lv_obj_t * ta = lv_event_get_target(e);
    if(event == LV_EVENT_CLICKED) {
        saverotationpattern(e);
        _ui_screen_change(ui_Home, LV_SCR_LOAD_ANIM_FADE_ON, 20, 0);
    }
}
static void ui_event_rotationdirection(lv_event_t * e)
{
    lv_event_code_t event = lv_event_get_code(e);
    lv_obj_t * ta = lv_event_get_target(e);
    if(event == LV_EVENT_VALUE_CHANGED) {
        saverotationdirection(e);
    }
}
```

`rotationbuttonmevent`, `saverotationpattern`, and `saverotationdirection`
are defined in `main.cpp` (section 4 below) - same split as the existing
`homebuttonmevent`/`savepattern` functions the Home and Pattern screens
call into. The direction checkbox sends on every tap (`LV_EVENT_VALUE_
CHANGED`) rather than waiting for the R button - it's a single discrete
toggle, not something you'd want to batch like the pattern roller's
multi-step scroll.

### 3c. The screen itself - new function, placed after `ui_Torqe_screen_init` and before `ui_EJECTSettings_screen_init` (~line 1675)

```cpp
void ui_Rotation_screen_init(void)
{

    // ui_Rotation

    ui_Rotation = lv_obj_create(NULL);

    lv_obj_clear_flag(ui_Rotation, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_add_event_cb(ui_Rotation, ui_event_Rotation, LV_EVENT_ALL, NULL);

    // ui_Logo7

    ui_Logo7 = lv_label_create(ui_Rotation);

    lv_obj_set_width(ui_Logo7, LV_SIZE_CONTENT);
    lv_obj_set_height(ui_Logo7, LV_SIZE_CONTENT);

    lv_obj_set_y(ui_Logo7, -103);
    lv_obj_set_x(ui_Logo7, lv_pct(0));

    lv_obj_set_align(ui_Logo7, LV_ALIGN_CENTER);

    lv_label_set_text(ui_Logo7, T_HEADER);

    lv_obj_set_style_text_font(ui_Logo7, &lv_font_montserrat_16, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_outline_color(ui_Logo7, lv_color_hex(0x83277B), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_outline_opa(ui_Logo7, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_outline_width(ui_Logo7, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_outline_pad(ui_Logo7, 5, LV_PART_MAIN | LV_STATE_DEFAULT);

    // ui_Batt7

    ui_Batt7 = lv_label_create(ui_Rotation);

    lv_obj_set_width(ui_Batt7, 85);
    lv_obj_set_height(ui_Batt7, 30);

    lv_obj_set_x(ui_Batt7, 115);
    lv_obj_set_y(ui_Batt7, -103);

    lv_obj_set_align(ui_Batt7, LV_ALIGN_CENTER);

    lv_label_set_text(ui_Batt7, T_BATT);

    // ui_BattValue7

    ui_BattValue7 = lv_label_create(ui_Batt7);

    lv_obj_set_width(ui_BattValue7, LV_SIZE_CONTENT);
    lv_obj_set_height(ui_BattValue7, LV_SIZE_CONTENT);

    lv_obj_set_x(ui_BattValue7, 0);
    lv_obj_set_y(ui_BattValue7, -7);

    lv_obj_set_align(ui_BattValue7, LV_ALIGN_RIGHT_MID);

    lv_label_set_text(ui_BattValue7, T_BLANK);

    // ui_Battery7

    ui_Battery7 = lv_bar_create(ui_Batt7);
    lv_bar_set_range(ui_Battery7, 0, 100);

    lv_obj_set_width(ui_Battery7, 80);
    lv_obj_set_height(ui_Battery7, 10);

    lv_obj_set_x(ui_Battery7, 0);
    lv_obj_set_y(ui_Battery7, 10);

    lv_obj_set_align(ui_Battery7, LV_ALIGN_CENTER);

    lv_obj_set_style_bg_color(ui_Battery7, lv_color_hex(0xD591D5), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_Battery7, 255, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_set_style_bg_color(ui_Battery7, lv_color_hex(0x83247B), LV_PART_INDICATOR | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_Battery7, 255, LV_PART_INDICATOR | LV_STATE_DEFAULT);

    // ui_RotationTitle

    ui_RotationTitle = lv_label_create(ui_Rotation);

    lv_obj_set_width(ui_RotationTitle, LV_SIZE_CONTENT);
    lv_obj_set_height(ui_RotationTitle, LV_SIZE_CONTENT);

    lv_obj_set_x(ui_RotationTitle, 0);
    lv_obj_set_y(ui_RotationTitle, -78);

    lv_obj_set_align(ui_RotationTitle, LV_ALIGN_CENTER);

    lv_label_set_text(ui_RotationTitle, T_ROTATION);

    lv_obj_set_style_text_font(ui_RotationTitle, &lv_font_montserrat_20, LV_PART_MAIN | LV_STATE_DEFAULT);

    // ui_RotationSpeedL (row container, mirrors Home's ui_SpeedL)

    ui_RotationSpeedL = lv_label_create(ui_Rotation);

    lv_obj_set_width(ui_RotationSpeedL, lv_pct(95));
    lv_obj_set_height(ui_RotationSpeedL, LV_SIZE_CONTENT);

    lv_obj_set_x(ui_RotationSpeedL, 0);
    lv_obj_set_y(ui_RotationSpeedL, -48);

    lv_obj_set_align(ui_RotationSpeedL, LV_ALIGN_CENTER);

    lv_label_set_text(ui_RotationSpeedL, T_SPEED);

    lv_obj_set_style_text_font(ui_RotationSpeedL, &lv_font_montserrat_20, LV_PART_MAIN | LV_STATE_DEFAULT);

    // ui_rotationspeedslider

    ui_rotationspeedslider = lv_slider_create(ui_RotationSpeedL);
    lv_slider_set_range(ui_rotationspeedslider, 0, 100);

    lv_obj_set_width(ui_rotationspeedslider, 130);
    lv_obj_set_height(ui_rotationspeedslider, 10);

    lv_obj_set_x(ui_rotationspeedslider, -15);
    lv_obj_set_y(ui_rotationspeedslider, 0);

    lv_obj_set_align(ui_rotationspeedslider, LV_ALIGN_RIGHT_MID);

    lv_obj_set_style_bg_color(ui_rotationspeedslider, lv_color_hex(0xD691D0), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_rotationspeedslider, 255, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_set_style_bg_color(ui_rotationspeedslider, lv_color_hex(0x83277B), LV_PART_INDICATOR | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_rotationspeedslider, 255, LV_PART_INDICATOR | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_color(ui_rotationspeedslider, lv_color_hex(0x83277B), LV_PART_INDICATOR | LV_STATE_DEFAULT);

    lv_obj_set_style_bg_color(ui_rotationspeedslider, lv_color_hex(0x83277B), LV_PART_KNOB | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_rotationspeedslider, 255, LV_PART_KNOB | LV_STATE_DEFAULT);

    // ui_rotationspeedvalue

    ui_rotationspeedvalue = lv_label_create(ui_RotationSpeedL);

    lv_obj_set_width(ui_rotationspeedvalue, LV_SIZE_CONTENT);
    lv_obj_set_height(ui_rotationspeedvalue, LV_SIZE_CONTENT);

    lv_obj_set_x(ui_rotationspeedvalue, 80);
    lv_obj_set_y(ui_rotationspeedvalue, 0);

    lv_obj_set_align(ui_rotationspeedvalue, LV_ALIGN_LEFT_MID);

    lv_label_set_text(ui_rotationspeedvalue, T_BLANK);

    // ui_RotationPatternTitle

    ui_RotationPatternTitle = lv_label_create(ui_Rotation);

    lv_obj_set_width(ui_RotationPatternTitle, LV_SIZE_CONTENT);
    lv_obj_set_height(ui_RotationPatternTitle, LV_SIZE_CONTENT);

    lv_obj_set_x(ui_RotationPatternTitle, 0);
    lv_obj_set_y(ui_RotationPatternTitle, -20);

    lv_obj_set_align(ui_RotationPatternTitle, LV_ALIGN_CENTER);

    lv_label_set_text(ui_RotationPatternTitle, T_SELECT_PATTERN);

    lv_obj_set_style_text_font(ui_RotationPatternTitle, &lv_font_montserrat_20, LV_PART_MAIN | LV_STATE_DEFAULT);

    // ui_RotationPatternS

    ui_RotationPatternS = lv_roller_create(ui_Rotation);
    lv_roller_set_options(ui_RotationPatternS,
                          "Steady\nSweep\nEscalation\nRandom Bursts\nScrew Sync\n"
                          "Stutter Sync\nBottom Hold Sync\nDepth Surge\n"
                          "Threshold Burst\nTempo Match\nSnap Reverse\n"
                          "Alternating Sweep\nRandom Direction Bursts\n"
                          "Metronome Flip\nDrunken Walk\nEscalating Reversal\n"
                          "Depth Trigger Spin",
                          LV_ROLLER_MODE_NORMAL);

    lv_obj_set_height(ui_RotationPatternS, 55);
    lv_obj_set_width(ui_RotationPatternS, lv_pct(95));

    lv_obj_set_x(ui_RotationPatternS, 0);
    lv_obj_set_y(ui_RotationPatternS, 15);

    lv_obj_set_align(ui_RotationPatternS, LV_ALIGN_CENTER);

    lv_obj_set_style_bg_color(ui_RotationPatternS, lv_color_hex(0xB481AC), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_RotationPatternS, 255, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_set_style_bg_color(ui_RotationPatternS, lv_color_hex(0x83247B), LV_PART_SELECTED | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_RotationPatternS, 255, LV_PART_SELECTED | LV_STATE_DEFAULT);

    // ui_rotationdirection (mirrors Settings' ui_lefty checkbox)

    ui_rotationdirection = lv_checkbox_create(ui_Rotation);
    lv_checkbox_set_text(ui_rotationdirection, T_REVERSE);

    lv_obj_set_width(ui_rotationdirection, LV_SIZE_CONTENT);
    lv_obj_set_height(ui_rotationdirection, LV_SIZE_CONTENT);

    lv_obj_set_x(ui_rotationdirection, 0);
    lv_obj_set_y(ui_rotationdirection, 65);

    lv_obj_set_align(ui_rotationdirection, LV_ALIGN_CENTER);

    lv_obj_add_flag(ui_rotationdirection, LV_OBJ_FLAG_SCROLL_ON_FOCUS);

    lv_obj_add_event_cb(ui_rotationdirection, ui_event_rotationdirection, LV_EVENT_ALL, NULL);

    lv_obj_set_style_text_font(ui_rotationdirection, &lv_font_montserrat_20, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_set_style_border_color(ui_rotationdirection, lv_color_hex(0x83247B), LV_PART_INDICATOR | LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(ui_rotationdirection, 255, LV_PART_INDICATOR | LV_STATE_DEFAULT);

    // ui_RotationButtonL

    ui_RotationButtonL = lv_btn_create(ui_Rotation);

    lv_obj_set_width(ui_RotationButtonL, 100);
    lv_obj_set_height(ui_RotationButtonL, 30);

    lv_obj_set_y(ui_RotationButtonL, 100);
    lv_obj_set_x(ui_RotationButtonL, lv_pct(-33));

    lv_obj_set_align(ui_RotationButtonL, LV_ALIGN_CENTER);

    lv_obj_add_flag(ui_RotationButtonL, LV_OBJ_FLAG_SCROLL_ON_FOCUS);
    lv_obj_clear_flag(ui_RotationButtonL, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_add_event_cb(ui_RotationButtonL, ui_event_RotationButtonL, LV_EVENT_ALL, NULL);
    lv_obj_set_style_bg_color(ui_RotationButtonL, lv_color_hex(0x83277B), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_RotationButtonL, 255, LV_PART_MAIN | LV_STATE_DEFAULT);

    // ui_RotationButtonLText

    ui_RotationButtonLText = lv_label_create(ui_RotationButtonL);

    lv_obj_set_width(ui_RotationButtonLText, LV_SIZE_CONTENT);
    lv_obj_set_height(ui_RotationButtonLText, LV_SIZE_CONTENT);

    lv_obj_set_x(ui_RotationButtonLText, lv_pct(0));
    lv_obj_set_y(ui_RotationButtonLText, lv_pct(0));

    lv_obj_set_align(ui_RotationButtonLText, LV_ALIGN_CENTER);

    lv_label_set_text(ui_RotationButtonLText, T_MENUE);

    lv_obj_set_style_text_color(ui_RotationButtonLText, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_RotationButtonLText, 255, LV_PART_MAIN | LV_STATE_DEFAULT);

    // ui_RotationButtonM

    ui_RotationButtonM = lv_btn_create(ui_Rotation);

    lv_obj_set_width(ui_RotationButtonM, 100);
    lv_obj_set_height(ui_RotationButtonM, 30);

    lv_obj_set_y(ui_RotationButtonM, 100);
    lv_obj_set_x(ui_RotationButtonM, lv_pct(0));

    lv_obj_set_align(ui_RotationButtonM, LV_ALIGN_CENTER);

    lv_obj_add_flag(ui_RotationButtonM, LV_OBJ_FLAG_SCROLL_ON_FOCUS);
    lv_obj_clear_flag(ui_RotationButtonM, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_add_event_cb(ui_RotationButtonM, ui_event_RotationButtonM, LV_EVENT_ALL, NULL);
    lv_obj_set_style_bg_color(ui_RotationButtonM, lv_color_hex(0x83277B), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_RotationButtonM, 255, LV_PART_MAIN | LV_STATE_DEFAULT);

    // ui_RotationButtonMText

    ui_RotationButtonMText = lv_label_create(ui_RotationButtonM);

    lv_obj_set_width(ui_RotationButtonMText, LV_SIZE_CONTENT);
    lv_obj_set_height(ui_RotationButtonMText, LV_SIZE_CONTENT);

    lv_obj_set_x(ui_RotationButtonMText, lv_pct(0));
    lv_obj_set_y(ui_RotationButtonMText, lv_pct(0));

    lv_obj_set_align(ui_RotationButtonMText, LV_ALIGN_CENTER);

    lv_label_set_text(ui_RotationButtonMText, T_START);

    lv_obj_set_style_text_color(ui_RotationButtonMText, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_RotationButtonMText, 255, LV_PART_MAIN | LV_STATE_DEFAULT);

    // ui_RotationButtonR

    ui_RotationButtonR = lv_btn_create(ui_Rotation);

    lv_obj_set_width(ui_RotationButtonR, 100);
    lv_obj_set_height(ui_RotationButtonR, 30);

    lv_obj_set_y(ui_RotationButtonR, 100);
    lv_obj_set_x(ui_RotationButtonR, lv_pct(33));

    lv_obj_set_align(ui_RotationButtonR, LV_ALIGN_CENTER);

    lv_obj_add_flag(ui_RotationButtonR, LV_OBJ_FLAG_SCROLL_ON_FOCUS);
    lv_obj_clear_flag(ui_RotationButtonR, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_add_event_cb(ui_RotationButtonR, ui_event_RotationButtonR, LV_EVENT_ALL, NULL);
    lv_obj_set_style_bg_color(ui_RotationButtonR, lv_color_hex(0x83277B), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_RotationButtonR, 255, LV_PART_MAIN | LV_STATE_DEFAULT);

    // ui_RotationButtonRText

    ui_RotationButtonRText = lv_label_create(ui_RotationButtonR);

    lv_obj_set_width(ui_RotationButtonRText, LV_SIZE_CONTENT);
    lv_obj_set_height(ui_RotationButtonRText, LV_SIZE_CONTENT);

    lv_obj_set_x(ui_RotationButtonRText, lv_pct(0));
    lv_obj_set_y(ui_RotationButtonRText, lv_pct(0));

    lv_obj_set_align(ui_RotationButtonRText, LV_ALIGN_CENTER);

    lv_label_set_text(ui_RotationButtonRText, T_SAVE);

    lv_obj_set_style_text_color(ui_RotationButtonRText, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_RotationButtonRText, 255, LV_PART_MAIN | LV_STATE_DEFAULT);

}
```

`T_START` already exists in `language.h` ("Start/Stop") - reused as-is for
this button, same label Home's own start/stop button uses, no new string
needed.

### 3d. Add "Rotation" as a 6th item in the Menu screen (`ui_Menue_screen_init`, section on `ui_EJECTSettingButton`)

The Menu screen's `EJECTSettingButton` is currently full-width and
disabled (a stub for an unbuilt feature) - shrink it to half-width and add
Rotation alongside it, same pairing style already used for `ui_SPattern`/
`ui_STorqe` one row up:

```cpp
    // ui_EJECTSettingButton

    ui_EJECTSettingButton = lv_btn_create(ui_Menue);

    lv_obj_set_height(ui_EJECTSettingButton, 30);
    lv_obj_set_width(ui_EJECTSettingButton, lv_pct(38));     // was lv_pct(80)

    lv_obj_set_y(ui_EJECTSettingButton, 60);
    lv_obj_set_x(ui_EJECTSettingButton, lv_pct(-20));        // was lv_pct(1)

    lv_obj_set_align(ui_EJECTSettingButton, LV_ALIGN_CENTER);
    ... (rest of ui_EJECTSettingButton and ui_EJECTSettingButtonText unchanged) ...

    // ui_SRotation   <-- new, mirrors ui_STorqe's construction exactly

    ui_SRotation = lv_btn_create(ui_Menue);

    lv_obj_set_height(ui_SRotation, 30);
    lv_obj_set_width(ui_SRotation, lv_pct(38));

    lv_obj_set_y(ui_SRotation, 60);
    lv_obj_set_x(ui_SRotation, lv_pct(21));

    lv_obj_set_align(ui_SRotation, LV_ALIGN_CENTER);

    lv_obj_add_flag(ui_SRotation, LV_OBJ_FLAG_SCROLL_ON_FOCUS);
    lv_obj_clear_flag(ui_SRotation, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_add_event_cb(ui_SRotation, ui_event_SRotation, LV_EVENT_ALL, NULL);
    lv_obj_set_style_bg_color(ui_SRotation, lv_color_hex(0x83277B), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_SRotation, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui_SRotation, lv_color_hex(0x5b0353), LV_PART_MAIN | LV_STATE_FOCUSED);

    // ui_SRotationL

    ui_SRotationL = lv_label_create(ui_SRotation);

    lv_obj_set_width(ui_SRotationL, LV_SIZE_CONTENT);
    lv_obj_set_height(ui_SRotationL, LV_SIZE_CONTENT);

    lv_obj_set_x(ui_SRotationL, lv_pct(0));
    lv_obj_set_y(ui_SRotationL, lv_pct(0));

    lv_obj_set_align(ui_SRotationL, LV_ALIGN_CENTER);

    lv_label_set_text(ui_SRotationL, T_ROTATION);

    lv_obj_set_style_text_color(ui_SRotationL, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_SRotationL, 255, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_g_menue = lv_group_create();
    lv_group_add_obj(ui_g_menue, ui_SDepthI);
    lv_group_add_obj(ui_g_menue, ui_SDepthF);
    lv_group_add_obj(ui_g_menue, ui_SPattern);
    lv_group_add_obj(ui_g_menue, ui_STorqe);
    lv_group_add_obj(ui_g_menue, ui_EJECTSettingButton);
    lv_group_add_obj(ui_g_menue, ui_SRotation);   // <-- add

}
```

New object-pointer declarations (section 3a) and event handler (section
3b) needed for this button:

```cpp
// 3a additions:
lv_obj_t * ui_SRotation;
lv_obj_t * ui_SRotationL;

// 3b addition, alongside ui_event_STorqe:
static void ui_event_SRotation(lv_event_t * e)
{
    lv_event_code_t event = lv_event_get_code(e);
    lv_obj_t * ta = lv_event_get_target(e);
    if(event == LV_EVENT_CLICKED) {
        _ui_screen_change(ui_Rotation, LV_SCR_LOAD_ANIM_FADE_ON, 90, 0);
    }
}
```

Also add the corresponding extern to `ui.h`: `extern lv_obj_t * ui_SRotation;`
and `extern lv_obj_t * ui_SRotationL;` (section 2).

### 3e. Register the new screen in `ui_init()` (bottom of `ui.c`)

```cpp
void ui_init(void)
{
    lv_disp_t * dispp = lv_disp_get_default();
    lv_theme_t * theme = lv_theme_default_init(dispp, lv_color_hex(0x83247B), lv_color_hex(0xD591D5), dark_mode, LV_FONT_DEFAULT);
    lv_disp_set_theme(dispp, theme);
    ui_Start_screen_init();
    ui_Home_screen_init();
    ui_Menue_screen_init();
    ui_Pattern_screen_init();
    ui_Torqe_screen_init();
    ui_Rotation_screen_init();     // <-- add
    ui_EJECTSettings_screen_init();
    ui_Settings_screen_init();
    lv_disp_load_scr(ui_Start);
}
```

## 4. `src/main.cpp`

### 4a. New screen state, in the `#define ST_UI_*` block (~line 46-54)

```cpp
#define ST_UI_EJECTSETTINGS 13
#define ST_UI_ROTATION 15     // <-- add (14 left open in case eject settings
                               //     ever needs a sub-state; 15 avoids it)

#define ST_UI_SETTINGS 20
```

### 4b. New command IDs, alongside the existing `#define SPEED 1` block (~line 84-104) - must match the mainboard exactly

```cpp
#define REBOOT 14
#define ROT_ON 30
#define ROT_OFF 31
#define ROT_SPEED 32
#define ROT_PATTERN 33
#define ROT_DIR 34

#define CONNECT 88
#define HEARTBEAT 99
```

### 4c. Mirror `struct_message` exactly (~line 187-199) - same struct, same order, same rules as before

```cpp
typedef struct struct_message {
  float esp_speed;
  float esp_depth;
  float esp_stroke;
  float esp_sensation;
  float esp_pattern;
  bool esp_rstate;
  bool esp_connected;
  bool esp_heartbeat;
  int esp_command;
  float esp_value;
  int esp_target;
  float esp_rotation_speed;
  float esp_rotation_pattern;
  float esp_rotation_direction;   // <-- add (mirrors mainboard section 6b)
} struct_message;
```

### 4d. New state variables, alongside `pattern`/`patternstr`/`OSSM_On` (~line 114-234)

```cpp
float rotationSpeed = 0;
int rotationPattern = 0;
char rotationpatternstr[20];
bool rotationDirection = false;
bool RotationOn = false;
```

### 4e. New functions, alongside `savepattern`/`homebuttonmevent` (~line 504-520)

```cpp
void saverotationpattern(lv_event_t * e){
  rotationPattern = lv_roller_get_selected(ui_RotationPatternS);
  lv_roller_get_selected_str(ui_RotationPatternS, rotationpatternstr, 0);
  LogDebug(rotationPattern);
  float rp = rotationPattern;
  SendCommand(ROT_PATTERN, rp, OSSM_ID);
}

void saverotationdirection(lv_event_t * e){
  rotationDirection = lv_obj_has_state(ui_rotationdirection, LV_STATE_CHECKED);
  SendCommand(ROT_DIR, rotationDirection ? 1.0 : 0.0, OSSM_ID);
}

void rotationbuttonmevent(lv_event_t * e){
  LogDebug("RotationButton");
  if(RotationOn == false){
    SendCommand(ROT_ON, 0.0, OSSM_ID);
  } else if(RotationOn == true){
    SendCommand(ROT_OFF, 0.0, OSSM_ID);
  }
}
```

Mirrors `savepattern`/`homebuttonmevent` exactly - same shape, new names,
new command IDs. `RotationOn` gets flipped by the mainboard's echo, same
round-trip as `OSSM_On` (section 4g below), not set directly here - that's
what makes the button correct even if the command is dropped in transit.

### 4f. `screenmachine()` - one more branch (~line 340-372 area, the function that sets `st_screens` from `lv_scr_act()`)

```cpp
  } else if (lv_scr_act() == ui_Torqe){
    st_screens = ST_UI_Torqe;
    torqe_f = lv_slider_get_value(ui_outtroqeslider);
    torqe_f_enc = fscale(50, 200, 0, Encoder_MAP, torqe_f, 0);
    encoder1.setCount(torqe_f_enc);

    torqe_r = lv_slider_get_value(ui_introqeslider);
    torqe_r_enc = fscale(20, 200, 0, Encoder_MAP, torqe_r, 0);
    encoder4.setCount(torqe_r_enc);

  } else if (lv_scr_act() == ui_Rotation){          // <-- add this whole branch
    st_screens = ST_UI_ROTATION;
    rotationSpeed = lv_slider_get_value(ui_rotationspeedslider);
    lv_roller_set_selected(ui_RotationPatternS, rotationPattern, LV_ANIM_OFF);
    if (rotationDirection) {
      lv_obj_add_state(ui_rotationdirection, LV_STATE_CHECKED);
    } else {
      lv_obj_clear_state(ui_rotationdirection, LV_STATE_CHECKED);
    }

  } else if (lv_scr_act() == ui_EJECTSettings){
```

Syncs the screen's widgets to current state on every load, same idea as
the Torque branch already does for its two sliders/encoders.

### 4g. Handle `ROT_ON`/`ROT_OFF` echoes and sync rotation state on connect (the function around line 335-372 that handles incoming ESP-NOW state)

```cpp
    maxdepthinmm = incomingcontrol.esp_depth;
    LogDebug(maxdepthinmm);
    pattern = incomingcontrol.esp_pattern;
    LogDebug(pattern);
    rotationPattern = incomingcontrol.esp_rotation_pattern;      // <-- add
    rotationDirection = (incomingcontrol.esp_rotation_direction != 0.0);  // <-- add
    outgoingcontrol.esp_target = OSSM_ID;

    ...

  switch(incomingcontrol.esp_command)
    {
    case OFF:
    {
    OSSM_On = false;
    }
    break;
    case ON:
    {
    OSSM_On = true;
    }
    break;
    case ROT_OFF:          // <-- add
    {
    RotationOn = false;
    }
    break;
    case ROT_ON:            // <-- add
    {
    RotationOn = true;
    }
    break;
    }
```

### 4h. New polling case, alongside `ST_UI_Torqe`/`ST_UI_EJECTSETTINGS` in the big screen-state switch (~line 956-1031)

```cpp
      case ST_UI_ROTATION:
      {
        if(lv_obj_has_state(ui_lefty, LV_STATE_CHECKED) == 1){
          touch_disabled = true;
        }

        //
        // Encoder 1 - Rotation Speed (mirrors Home's speed block exactly, 0-100 bounds)
        //
        if(lv_slider_is_dragged(ui_rotationspeedslider) == false){
          changed = false;
          lv_slider_set_value(ui_rotationspeedslider, rotationSpeed, LV_ANIM_OFF);

          if (encoder1.getCount() >= 2){
            changed = true;
            rotationSpeed += rampValue;
            encoder1.setCount(0);
            rampMs = millis();
            encId = 1;
          } else if (encoder1.getCount() <= -2){
            changed = true;
            rotationSpeed -= rampValue;
            encoder1.setCount(0);
            rampMs = millis();
            encId = 1;
          }

          if (rotationSpeed < 0){
            changed = true;
            rotationSpeed = 0;
          }
          if (rotationSpeed > 100){
            changed = true;
            rotationSpeed = 100;
          }

          if (changed) {
            SendCommand(ROT_SPEED, rotationSpeed, OSSM_ID);
          }

        } else if (lv_slider_get_value(ui_rotationspeedslider) != rotationSpeed){
            rotationSpeed = lv_slider_get_value(ui_rotationspeedslider);
            SendCommand(ROT_SPEED, rotationSpeed, OSSM_ID);
        }
        char rotationspeed_v[7];
        dtostrf(rotationSpeed, 6, 0, rotationspeed_v);
        lv_label_set_text(ui_rotationspeedvalue, rotationspeed_v);

        //
        // Encoder 4 - Pattern roller (mirrors ST_UI_PATTERN's block exactly)
        //
        if(encoder4.getCount() > encoder4_enc + 2){
          uint32_t t = LV_KEY_DOWN;
          lv_obj_send_event(ui_RotationPatternS, LV_EVENT_KEY, &t);
          encoder4_enc = encoder4.getCount();
        } else if(encoder4.getCount() < encoder4_enc - 2){
          uint32_t t = LV_KEY_UP;
          lv_obj_send_event(ui_RotationPatternS, LV_EVENT_KEY, &t);
          encoder4_enc = encoder4.getCount();
        }

        if(click2_short_waspressed == true){
         lv_obj_send_event(ui_RotationButtonL, LV_EVENT_CLICKED, NULL);
        } else if(mxclick_short_waspressed == true){
         lv_obj_send_event(ui_RotationButtonM, LV_EVENT_CLICKED, NULL);
        } else if(click3_short_waspressed == true){
         lv_obj_send_event(ui_RotationButtonR, LV_EVENT_CLICKED, NULL);
        }
      }
      break;
```

Insert this `case` block anywhere among the other screen cases in that
switch (order doesn't matter to the compiler; placing it right after
`ST_UI_Torqe`'s block keeps it near its Menu-screen sibling for readability).

### 4i. Wire the new battery indicator into `loop()` (~line 636-647) - every screen's battery widgets get refreshed here every frame

```cpp
     lv_bar_set_value(ui_Battery5, BatteryLevel, LV_ANIM_OFF);
     lv_label_set_text(ui_BattValue5, battVal);
     lv_bar_set_value(ui_Battery7, BatteryLevel, LV_ANIM_OFF);   // <-- add
     lv_label_set_text(ui_BattValue7, battVal);                  // <-- add
```

Without this, `ui_Battery7`/`ui_BattValue7` exist as widgets but never
update - same state `ui_Battery6` (the EjectSettings screen's battery
indicator) is apparently already in, as cloned; that one's a pre-existing
gap, not something this session caused or fixes.

## Not covered here

- **No live "Start"/"Stop" label swap.** The M button always reads
  "Start/Stop" regardless of `RotationOn` - matches Home's own start
  button, which has the same limitation today. If you want the label (or
  a status dot) to actually reflect run-state, that's a small addition
  (an `lv_label_set_text` call keyed off `RotationOn`, e.g. in the
  `ST_UI_ROTATION` case above) worth doing for both screens together
  rather than just this one.
- **Roller labels vs. `Stroker`'s actual pattern names**: while building
  this I noticed the *existing* stroke Pattern screen's roller options
  (`"SimpleStroke\nTeasingPounding\n...Knot"`, 8 entries) don't match
  `StrokeEngine`'s actual 9-pattern table (ends in `JackHammer`/
  `StrokeNibbler`, not `Knot`) - a pre-existing drift, not something this
  session touched or caused. See `PATTERN-INTEGRATION-NOTES.md` section 6
  for the corrected 10-entry stroke roller string (post Depth Pulse).
