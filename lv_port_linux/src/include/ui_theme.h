/**
 * @file ui_theme.h
 * @brief Centralized theme: colors & shared lv_style_t objects
 *
 * Every color/artistic parameter of the app lives here.
 * View files only call lv_obj_add_style() with these styles.
 */
#ifndef UI_THEME_H
#define UI_THEME_H

#include <lvgl/lvgl.h>

/**********************
 *  PALETTE
 **********************/
#define COL_BG       lv_color_hex(0x101418)
#define COL_BG_PANEL lv_color_hex(0x2a2f36)
#define COL_LINE_DIM lv_color_hex(0x5a6068)
#define COL_LINE_ACT lv_color_hex(0xf2f3f5)
#define COL_TXT      lv_color_white()
#define COL_ACCENT   lv_color_hex(0xfa4362)

/**********************
 *  SHARED STYLES
 *  (initialized by ui_theme_init, read-only afterwards)
 **********************/
/* screen background */
extern lv_style_t style_screen;

/* inactive lyric line: dim + 60% opacity + big font */
extern lv_style_t style_lyric_dim;
/* active lyric line: white + full opacity + bigger font */
extern lv_style_t style_lyric_act;
/* left info panel: transparent, centered column flex */
extern lv_style_t style_panel;
/* scrollable lyric list: transparent, no scrollbar */
extern lv_style_t style_lyric_list;
/* icon buttons in control row */
extern lv_style_t style_ctrl_button;
/* all buttons need to be placed by row in a left panel's child */
extern lv_style_t style_button_container;
/* progress bar: track & indicator parts */
extern lv_style_t style_bar_track;
extern lv_style_t style_bar_ind;
/* fake album art card */
extern lv_style_t style_album_art;

/* Call once before creating any UI. */
void ui_theme_init(void);

#endif /* UI_THEME_H */
