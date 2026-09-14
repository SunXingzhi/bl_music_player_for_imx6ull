/**
 * @file lyrics_demo.c
 * @brief AMLL (Apple Music-like Lyrics) style UI demo for LVGL v9
 *
 * Layout:  left = album art + song info + controls
 *          right = scrollable lyrics with animated current-line highlight
 *
 * Structure:
 *  - colors & shared styles  -> ui_theme.c/h (theme, centralized)
 *  - view data (obj refs)    -> view_data_t (model of the view)
 *  - per-component create fns: create_button_ui / create_album_art_ui /
 *    create_left_panel / create_right_panel
 *  - logic (cbs, timer)      -> bottom half of this file
 */

#include <stdio.h>
#include <string.h>
#include <lvgl/lvgl.h>

#include "include/ui_theme.h"

/**********************
 *  CONFIG
 **********************/
#define LINE_ANIM_TIME 350 /* ms, line highlight animation */
#define LINE_STEP_MS 2600  /* ms, auto advance per line */
#define BUTTON_NUMBER 3

typedef enum {
        BUTTON_PREV_SONG,
        BUTTON_PLAY_PAUSE,
        BUTTON_NEXT_SONG,
        // BUTTON_BACK,
        // BUTTON_SETTING,
} button_type_t;

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
 *  VIEW DATA (object refs only - no styling here)
 **********************/
typedef struct {
        button_type_t button_type;
        lv_obj_t * button_obj;
        lv_obj_t * button_label;
        const lv_style_t * button_style;
} button_t;

typedef struct {
        lv_obj_t * lyric_list_obj;
        lv_obj_t * lines_view[LYRIC_CNT];
} lyric_view_t;

/* page view data: every object reference in one place */
typedef struct {
        lyric_view_t lyric_view;
        lv_obj_t * progress_bar;
        lv_obj_t * time_label;
} view_data_t;



/**********************
 *  STATE (runtime, not styling)
 **********************/
typedef struct {
        lv_obj_t * scr;
        int32_t cur_line;
        bool playing;
        int32_t line_elapsed; /* ms elapsed in current line */
} ui_ctx_t;

static ui_ctx_t ui;
/* all buttons of the page */
static button_t ui_button[BUTTON_NUMBER];
static view_data_t view_data;

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
        lv_label_set_text(ui_button[BUTTON_PLAY_PAUSE].button_label,
                          ui.playing ? LV_SYMBOL_PAUSE : LV_SYMBOL_PLAY);
}

static void scale_anim_cb(void * var, int32_t v)
{
        lv_obj_set_style_transform_scale((lv_obj_t *)var, v, 0);
}

static void apply_line_styles(int32_t prev, int32_t cur)
{
        lv_obj_t ** lines = view_data.lyric_view.lines_view;

        /* dim the previous line */
        if(prev >= 0) {
                lv_obj_remove_style(lines[prev], &style_lyric_act, 0);
                lv_obj_add_style(lines[prev], &style_lyric_dim, 0);
                lv_obj_set_style_transform_scale(lines[prev], 256, 0); /* 100% */
        }

        /* highlight the current line */
        lv_obj_add_style(lines[cur], &style_lyric_act, 0);

        /* spring-like pop-in: scale 0.9 -> 1.0 using overshoot path */
        lv_anim_t a;
        lv_anim_init(&a);
        lv_anim_set_var(&a, lines[cur]);
        lv_anim_set_exec_cb(&a, scale_anim_cb);
        lv_anim_set_values(&a, 230, 256); /* ~90% -> 100% */
        lv_anim_set_path_cb(&a, lv_anim_path_overshoot);
        lv_anim_set_time(&a, LINE_ANIM_TIME);
        lv_anim_set_delay(&a, 60);
        lv_anim_start(&a);
}

static void scroll_to_line(int32_t idx)
{
        lv_obj_t * line = view_data.lyric_view.lines_view[idx];
        // scroll so the current line sits at ~1/3 from top of the list
        int32_t list_h = lv_obj_get_height(view_data.lyric_view.lyric_list_obj);

	// get
        int32_t line_y = lv_obj_get_y(line);
        int32_t target = line_y - list_h / 3;

        lv_obj_scroll_to_y(view_data.lyric_view.lyric_list_obj, target, LV_ANIM_ON);
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

        /* 模拟歌词, 数量固定 */
        if(ui.cur_line >= (int32_t)LYRIC_CNT) ui.cur_line = 0;

        apply_line_styles(prev, ui.cur_line);
        scroll_to_line(ui.cur_line);

        /* update progress bar & time (fake 4-min song) */
        int32_t pct = ui.cur_line * 100 / (int32_t)(LYRIC_CNT - 1);
        lv_bar_set_value(view_data.progress_bar, pct, LV_ANIM_ON);

        int32_t total_s = pct * 240 / 100;
        char buf[16];
        lv_snprintf(buf, sizeof(buf), "%d:%02d", total_s / 60, total_s % 60);
        lv_label_set_text(view_data.time_label, buf);
}

/**********************
 *  UI BUILD
 **********************/
static const char * button_symbol_of(button_type_t type)
{
        switch(type) {
                case BUTTON_PREV_SONG:  return LV_SYMBOL_PREV;
                case BUTTON_PLAY_PAUSE: return LV_SYMBOL_PAUSE;
                case BUTTON_NEXT_SONG:  return LV_SYMBOL_NEXT;
                // case BUTTON_BACK:       return LV_SYMBOL_BACKSPACE;
                // case BUTTON_SETTING:    return LV_SYMBOL_SETTINGS;
                default:                return NULL;
        }
}

/**
 * @Return successfully for 0, and failed for 1
 */
static int create_button_ui(lv_obj_t * container)
{
	
	// create a child container to place the button, in order to make the button flex row.
	lv_obj_t* button_container	= lv_obj_create(container);
	// drop default theme look (white bg / border), "inherit" parent appearance
	lv_obj_remove_style_all(button_container);
	lv_obj_add_style(button_container, &style_button_container, 0);
	// match parent width so pct-sized children work as expected
	lv_obj_set_size(button_container, LV_PCT(100), LV_SIZE_CONTENT);

        for(int i = 0; i < BUTTON_NUMBER; i++) {
                button_t * btn = &ui_button[i];
                btn->button_type  = (button_type_t)i;
                btn->button_style = &style_ctrl_button;

                btn->button_obj   = lv_button_create(button_container);
                
                lv_obj_set_size(btn->button_obj, LV_PCT(15), 44);

                const char * symbol = button_symbol_of(btn->button_type);
                if(symbol == NULL) {
                        LV_LOG_ERROR("Failed to find a button symbol.");
                        return 1;
                }
                btn->button_label = lv_label_create(btn->button_obj);
                lv_label_set_text(btn->button_label, symbol);
                lv_obj_center(btn->button_label);
                if(btn->button_type == BUTTON_PLAY_PAUSE) {
                        lv_obj_add_style(btn->button_obj, btn->button_style, 0);
                }
        }

	/* play/pause: wired to logic (all buttons share the same size/style) */
	button_t * play = &ui_button[BUTTON_PLAY_PAUSE];
	lv_obj_add_event_cb(play->button_obj, play_pause_cb, LV_EVENT_CLICKED, NULL);

        return 0;
}

static lv_obj_t * create_album_art_ui(lv_obj_t * parent)
{
        /* rounded gradient square as fake "album cover" */
        lv_obj_t * art = lv_obj_create(parent);
        lv_obj_add_style(art, &style_album_art, 0);
        lv_obj_set_size(art, 200, 200);

        lv_obj_t * note = lv_label_create(art);
        lv_label_set_text(note, LV_SYMBOL_AUDIO);
        lv_obj_set_style_text_font(note, &lv_font_montserrat_28, 0);
        lv_obj_set_style_text_color(note, COL_TXT, 0);
        lv_obj_center(note);

        return art;
}

static lv_obj_t * create_left_panel(lv_obj_t * parent)
{
        lv_obj_t * panel = lv_obj_create(parent);
        lv_obj_remove_style_all(panel);

	// Style panel is column flow
        lv_obj_add_style(panel, &style_panel, 0);
        lv_obj_set_size(panel, LV_PCT(40), LV_PCT(100));

        // Album art
        create_album_art_ui(panel);

        // Title 
        lv_obj_t * title = lv_label_create(panel);
        lv_label_set_text(title, "A Whole New World");
        lv_obj_set_style_text_color(title, COL_TXT, 0);
        lv_obj_set_style_text_font(title, &lv_font_montserrat_28, 0);

        // Artist
        lv_obj_t * artist = lv_label_create(panel);
        lv_label_set_text(artist, "Peabo Bryson & Regina Belle");
        lv_obj_set_style_text_color(artist, COL_LINE_DIM, 0);

        // Progress bar
        view_data.progress_bar = lv_bar_create(panel);
        lv_obj_set_width(view_data.progress_bar, LV_PCT(100));
        lv_obj_add_style(view_data.progress_bar, &style_bar_track, 0);
        lv_obj_add_style(view_data.progress_bar, &style_bar_ind, LV_PART_INDICATOR);
        lv_bar_set_range(view_data.progress_bar, 0, 100);
        lv_bar_set_value(view_data.progress_bar, 0, LV_ANIM_OFF);

        view_data.time_label = lv_label_create(panel);
        lv_label_set_text(view_data.time_label, "0:00");
        lv_obj_set_style_text_color(view_data.time_label, COL_LINE_DIM, 0);

        /* control buttons */
        if(create_button_ui(panel) != 0) {
                LV_LOG_ERROR("Failed to create button UI.");
        }

        return panel;
}

static lv_obj_t * create_right_panel(lv_obj_t * parent)
{
        lv_obj_t * list = lv_obj_create(parent);
        lv_obj_set_size(list, LV_PCT(1), LV_PCT(100)); /* flex-grow */
        lv_obj_set_flex_grow(list, 1);
        lv_obj_add_style(list, &style_lyric_list, 0);
        lv_obj_set_scrollbar_mode(list, LV_SCROLLBAR_MODE_OFF);
        /* top padding: shift first render position of lines */
        lv_obj_set_style_pad_top(list, lv_obj_get_height(parent) / 3, 0);
        lv_obj_scroll_to_y(list, 0, LV_ANIM_OFF);

        /* lyric lines */
        for(int32_t i = 0; i < (int32_t)LYRIC_CNT; i++) {
                lv_obj_t * lbl = lv_label_create(list);
                lv_label_set_text(lbl, lyrics[i]);
                lv_label_set_long_mode(lbl, LV_LABEL_LONG_WRAP);
                lv_obj_set_width(lbl, LV_PCT(92));

                lv_obj_add_style(lbl, &style_lyric_dim, 0);
                lv_obj_set_clickable(lbl, true);
                lv_obj_add_event_cb(lbl, lyric_line_click_cb, LV_EVENT_CLICKED, (void *)(intptr_t)i);

                view_data.lyric_view.lines_view[i] = lbl;
        }

        view_data.lyric_view.lyric_list_obj = list;
        return list;
}

/**********************
 *  PUBLIC
 **********************/
void lyrics_demo_create(void)
{
        ui_theme_init();

        ui.scr = lv_obj_create(NULL);
        lv_obj_add_style(ui.scr, &style_screen, 0);

        /* root flex: left panel + right panel */
        lv_obj_set_flex_flow(ui.scr, LV_FLEX_FLOW_ROW);

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
