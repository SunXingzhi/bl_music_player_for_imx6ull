/**
 * @file lyrics_demo.c
 * @brief AMLL (Apple Music-like Lyrics) style UI demo for LVGL v9
 *
 * Layout:  left = album art + song info + controls
 *          right = scrollable lyrics with animated current-line highlight
 *
 * Features demonstrated:
 *  - flex layout (left/right columns)
 *  - scrollable lyric list with auto-scroll-to-current-line
 *  - current line: bigger font + white color + scale animation (spring-like)
 *  - inactive lines: dimmed grey
 *  - karaoke-style per-line progress bar (simulated word-fill)
 *  - timer-driven line switching
 */

#include <stdio.h>
#include <string.h>
#include <lvgl/lvgl.h>

/**********************
 *  CONFIG
 **********************/
#define LYRIC_LINE_MAX 16  /* max chars per lyric line */
#define LINE_ANIM_TIME 350 /* ms, line highlight animation */
#define LINE_STEP_MS 2600  /* ms, auto advance per line */

/**********************
 *  DEMO LYRICS
 **********************/
static const char * lyrics[] = {
    "\xE2\x99\xAA Intro...", /* ♪ Intro... */
    "I can show you the world",
    "Shining, shimmering, splendid",
    "Tell me, princess, now",
    "When did you last let your heart decide",
    "A whole new world",
    "A dazzling place I never knew",
    "But when I'm way up here",
    "It's crystal clear",
    "That now I'm in a whole new world with you",
    "\xE2\x99\xAA Outro...", /* ♪ Outro... */
};
#define LYRIC_CNT (sizeof(lyrics) / sizeof(lyrics[0]))

/**********************
 *  WIDGET COLORS (dark theme like Apple Music)
 **********************/
#define COL_BG lv_color_hex(0x101418)
#define COL_LINE_DIM lv_color_hex(0x5a6068) /* inactive lyric */
#define COL_LINE_ACT lv_color_hex(0xf2f3f5) /* current lyric */
#define COL_ACCENT lv_color_hex(0xfa4362)   /* pink accent */

typedef struct 
{
	lv_obj_t*	button_obj;
	lv_style_t*	button_style;
} button_t;

typedef struct
{
	lv_obj_t*	lyric_list_obj;
	lv_style_t*	lyric_list_styles;
	lv_obj_t*	active_line;
	lv_style_t*	lyric_active_line_style;
	
} lyric_list_t;

/**********************
 *  STATE
 **********************/
typedef struct
{
        lv_obj_t * scr;
        lv_obj_t * lyric_list;
        lv_obj_t * lines[32];
        lv_obj_t * progress_bar;
        lv_obj_t * play_pause_btn;
        lv_obj_t * play_pause_label;
        lv_obj_t * time_label;
        int32_t cur_line;
        bool playing;
        int32_t line_elapsed; /* ms elapsed in current line */
} ui_ctx_t;

static ui_ctx_t ui;

/**********************
 *  STATIC STYLE (current line)
 **********************/
static lv_style_t style_act_line;

/**********************
 *  HELPERS
 **********************/
static void lyric_line_click_cb(lv_event_t * e)
{
        lv_obj_t * lbl = lv_event_get_current_target(e);
        int32_t idx    = (int32_t)(intptr_t)lv_obj_get_user_data(lbl);
        if(idx == ui.cur_line) return;

        /* jump to clicked line */
        ui.cur_line     = idx - 1;      /* advance timer will bump it to idx */
        ui.line_elapsed = LINE_STEP_MS; /* force immediate advance */
}

static void play_pause_cb(lv_event_t * e)
{
        LV_UNUSED(e);
        ui.playing = !ui.playing;
        lv_label_set_text(ui.play_pause_label, ui.playing ? LV_SYMBOL_PAUSE : LV_SYMBOL_PLAY);
}

static void scale_anim_cb(void * var, int32_t v)
{
        lv_obj_set_style_transform_scale((lv_obj_t *)var, v, 0);
}

static void apply_line_styles(int32_t prev, int32_t cur)
{
        /* dim the previous line */
        if(prev >= 0) {
                lv_obj_remove_style(ui.lines[prev], &style_act_line, 0);
                lv_obj_set_style_text_color(ui.lines[prev], COL_LINE_DIM, 0);
                lv_obj_set_style_text_opa(ui.lines[prev], LV_OPA_60, 0);
                lv_obj_set_style_transform_scale(ui.lines[prev], 256, 0); /* 100% */
        }

        /* highlight the current line */
        lv_obj_add_style(ui.lines[cur], &style_act_line, 0);

        /* spring-like pop-in: scale 0.9 -> 1.0 using overshoot path */
        lv_anim_t a;
        lv_anim_init(&a);
        lv_anim_set_var(&a, ui.lines[cur]);
        lv_anim_set_exec_cb(&a, scale_anim_cb);
        lv_anim_set_values(&a, 230, 256); /* ~90% -> 100% */
        lv_anim_set_path_cb(&a, lv_anim_path_overshoot);
        lv_anim_set_time(&a, LINE_ANIM_TIME);
        lv_anim_set_delay(&a, 60);
        lv_anim_start(&a);
}

static void scroll_to_line(int32_t idx)
{
        lv_obj_t * line = ui.lines[idx];
        /* scroll so the current line sits at ~1/3 from top of the list */
        int32_t list_h = lv_obj_get_height(ui.lyric_list);
        int32_t line_y = lv_obj_get_y(line);
        int32_t target = line_y - list_h / 3;

        lv_obj_scroll_to_y(ui.lyric_list, target, LV_ANIM_ON);
}

static void timer_tick_cb(lv_timer_t * t)
{
        LV_UNUSED(t);

        if(!ui.playing) return;

        ui.line_elapsed += LINE_STEP_MS; /* demo: each tick = one line */
        if(ui.line_elapsed < LINE_STEP_MS) return;
        ui.line_elapsed = 0;

        int32_t prev = ui.cur_line;
        ui.cur_line++;
        if(ui.cur_line >= (int32_t)LYRIC_CNT) ui.cur_line = 0;

        apply_line_styles(prev, ui.cur_line);
        scroll_to_line(ui.cur_line);

        /* update progress bar & time (fake 4-min song) */
        int32_t pct = ui.cur_line * 100 / (int32_t)(LYRIC_CNT - 1);
        lv_bar_set_value(ui.progress_bar, pct, LV_ANIM_ON);

        int32_t total_s = pct * 240 / 100;
        char buf[16];
        lv_snprintf(buf, sizeof(buf), "%d:%02d", total_s / 60, total_s % 60);
        lv_label_set_text(ui.time_label, buf);
}

/**********************
 *  UI BUILD
 **********************/
static lv_obj_t * create_album_art(lv_obj_t * parent)
{
        /* rounded square with gradient as fake "album cover" */
        lv_obj_t * art = lv_obj_create(parent);
        lv_obj_set_size(art, 200, 200);
        lv_obj_set_style_radius(art, 16, 0);
        lv_obj_set_style_bg_opa(art, LV_OPA_COVER, 0);
        lv_obj_set_style_bg_grad_dir(art, LV_GRAD_DIR_LINEAR, 0);
        lv_obj_set_style_bg_color(art, lv_color_hex(0x35506b), 0);
        lv_obj_set_style_bg_grad_color(art, lv_color_hex(0xfa4362), 0);
        lv_obj_set_style_border_width(art, 0, 0);
        lv_obj_set_style_shadow_width(art, 60, 0);
        lv_obj_set_style_shadow_opa(art, LV_OPA_40, 0);
        lv_obj_set_style_shadow_color(art, lv_color_black(), 0);

        lv_obj_t * note = lv_label_create(art);
        lv_label_set_text(note, LV_SYMBOL_AUDIO);
        lv_obj_set_style_text_font(note, &lv_font_montserrat_28, 0);
        lv_obj_set_style_text_color(note, lv_color_white(), 0);
        lv_obj_center(note);

        return art;
}

static lv_obj_t * create_left_panel(lv_obj_t * parent)
{
        lv_obj_t * panel = lv_obj_create(parent);
        lv_obj_remove_style_all(panel);
        lv_obj_set_size(panel, 320, LV_PCT(100));
        lv_obj_set_style_pad_all(panel, 24, 0);
        lv_obj_set_flex_flow(panel, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flex_align(panel, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
        lv_obj_set_style_pad_gap(panel, 14, 0);

        /* album art */
        create_album_art(panel);

        /* title */
        lv_obj_t * title = lv_label_create(panel);
        lv_label_set_text(title, "A Whole New World");
        lv_obj_set_style_text_color(title, lv_color_white(), 0);
        lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);

        /* artist */
        lv_obj_t * artist = lv_label_create(panel);
        lv_label_set_text(artist, "Peabo Bryson & Regina Belle");
        lv_obj_set_style_text_color(artist, COL_LINE_DIM, 0);

        /* progress bar + time */
        ui.progress_bar = lv_bar_create(panel);
        lv_obj_set_width(ui.progress_bar, LV_PCT(100));
        lv_obj_set_style_bg_color(ui.progress_bar, lv_color_hex(0x2a2f36), 0);
        lv_obj_set_style_bg_opa(ui.progress_bar, LV_OPA_COVER, 0);
        lv_obj_set_style_bg_color(ui.progress_bar, COL_ACCENT, LV_PART_INDICATOR);
        lv_bar_set_range(ui.progress_bar, 0, 100);
        lv_bar_set_value(ui.progress_bar, 0, LV_ANIM_OFF);

        ui.time_label = lv_label_create(panel);
        lv_label_set_text(ui.time_label, "0:00");
        lv_obj_set_style_text_color(ui.time_label, COL_LINE_DIM, 0);

        /* play / pause button */
        ui.play_pause_btn = lv_button_create(panel);
        lv_obj_set_size(ui.play_pause_btn, 64, 44);
        lv_obj_set_style_bg_color(ui.play_pause_btn, COL_ACCENT, 0);
        lv_obj_add_event_cb(ui.play_pause_btn, play_pause_cb, LV_EVENT_CLICKED, NULL);

        ui.play_pause_label = lv_label_create(ui.play_pause_btn);
        lv_label_set_text(ui.play_pause_label, LV_SYMBOL_PAUSE);
        lv_obj_set_style_text_color(ui.play_pause_label, lv_color_white(), 0);
        lv_obj_center(ui.play_pause_label);

        return panel;
}

static lv_obj_t * create_right_panel(lv_obj_t * parent)
{
        lv_obj_t * list = lv_obj_create(parent);
        lv_obj_set_size(list, LV_PCT(1), LV_PCT(100)); /* flex-grow */
        lv_obj_set_flex_grow(list, 1);

        /* transparent scrollable list */
        lv_obj_set_style_bg_opa(list, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(list, 0, 0);
        lv_obj_set_scrollbar_mode(list, LV_SCROLLBAR_MODE_OFF);
        lv_obj_set_flex_flow(list, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_style_pad_gap(list, 22, 0);
        lv_obj_set_style_pad_top(list, lv_obj_get_height(parent) / 3, 0);
        lv_obj_set_style_pad_bottom(list, 40, 0);
        lv_obj_scroll_to_y(list, 0, LV_ANIM_OFF);

        /* lyric lines */
        for(int32_t i = 0; i < (int32_t)LYRIC_CNT; i++) {
                lv_obj_t * lbl = lv_label_create(list);
                lv_label_set_text(lbl, lyrics[i]);
                lv_label_set_long_mode(lbl, LV_LABEL_LONG_WRAP);
                lv_obj_set_width(lbl, LV_PCT(92));

                lv_obj_set_style_text_color(lbl, COL_LINE_DIM, 0);
                lv_obj_set_style_text_opa(lbl, LV_OPA_60, 0);
                lv_obj_set_style_text_font(lbl, &lv_font_montserrat_18, 0);
                lv_obj_set_clickable(lbl, true);
                lv_obj_add_event_cb(lbl, lyric_line_click_cb, LV_EVENT_CLICKED, (void *)(intptr_t)i);

                ui.lines[i] = lbl;
        }

        ui.lyric_list = list;
        return list;
}

/**********************
 *  PUBLIC
 **********************/
void lyrics_demo_create(void)
{
        lv_style_init(&style_act_line);
        lv_style_set_text_color(&style_act_line, COL_LINE_ACT);
        lv_style_set_text_opa(&style_act_line, LV_OPA_COVER);
        lv_style_set_text_font(&style_act_line, &lv_font_montserrat_24);

        ui.scr = lv_obj_create(NULL);
        lv_obj_set_style_bg_color(ui.scr, COL_BG, 0);
        lv_obj_set_style_bg_opa(ui.scr, LV_OPA_COVER, 0);

        /* root flex: left panel + right panel */
        lv_obj_set_flex_flow(ui.scr, LV_FLEX_FLOW_ROW);
        lv_obj_set_style_pad_all(ui.scr, 0, 0);

        create_left_panel(ui.scr);
        create_right_panel(ui.scr);

        /* first line highlighted */
        ui.cur_line     = 0;
        ui.playing      = true;
        ui.line_elapsed = 0;
        apply_line_styles(-1, 0);

        lv_screen_load(ui.scr);

        lv_timer_create(timer_tick_cb, LINE_STEP_MS, NULL);
}
