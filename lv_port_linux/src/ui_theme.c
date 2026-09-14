/**
 * @file ui_theme.c
 * @brief Centralized theme: colors & shared lv_style_t objects
 *
 * All artistic values (colors, fonts, paddings) live here so the
 * theme can be tweaked in a single file.
 */
#include "include/ui_theme.h"

/**********************
 *  SHARED STYLE OBJECTS
 **********************/
lv_style_t style_theme_mode;	// dark or light
lv_style_t style_screen;
lv_style_t style_lyric_dim;
lv_style_t style_lyric_act;
lv_style_t style_panel;
lv_style_t style_lyric_list;
lv_style_t style_button_container;
lv_style_t style_ctrl_button;
lv_style_t style_bar_track;
lv_style_t style_bar_ind;
lv_style_t style_album_art;

void ui_theme_init(void)
{
	/* screen */
	lv_style_init(&style_screen);
	lv_style_set_bg_color(&style_screen, COL_BG);
	lv_style_set_bg_opa(&style_screen, LV_OPA_COVER);
	lv_style_set_pad_all(&style_screen, 0);

	/* inactive lyric lines */
	lv_style_init(&style_lyric_dim);
	lv_style_set_text_color(&style_lyric_dim, COL_LINE_DIM);
	lv_style_set_text_opa(&style_lyric_dim, LV_OPA_60);
	lv_style_set_text_font(&style_lyric_dim, &lv_font_montserrat_24);

	/* current lyric line */
	lv_style_init(&style_lyric_act);
	lv_style_set_text_color(&style_lyric_act, COL_LINE_ACT);
	lv_style_set_text_opa(&style_lyric_act, LV_OPA_COVER);
	lv_style_set_text_font(&style_lyric_act, &lv_font_montserrat_28);

	/* left info panel: transparent + centered column flex */
	lv_style_init(&style_panel);
	lv_style_set_bg_opa(&style_panel, LV_OPA_TRANSP);
	lv_style_set_border_width(&style_panel, 0);
	lv_style_set_pad_all(&style_panel, 24);
	lv_style_set_pad_gap(&style_panel, 14);
	lv_style_set_flex_flow(&style_panel, LV_FLEX_FLOW_COLUMN);
	lv_style_set_flex_main_place(&style_panel, LV_FLEX_ALIGN_CENTER);
	lv_style_set_flex_cross_place(&style_panel, LV_FLEX_ALIGN_CENTER);
	lv_style_set_flex_track_place(&style_panel, LV_FLEX_ALIGN_CENTER);
	lv_style_set_layout(&style_panel, LV_LAYOUT_FLEX);

	/* lyric list: transparent scroller, no scrollbar */
	lv_style_init(&style_lyric_list);
	lv_style_set_bg_opa(&style_lyric_list, LV_OPA_TRANSP);
	lv_style_set_border_width(&style_lyric_list, 0);
	lv_style_set_pad_bottom(&style_lyric_list, 40);
	lv_style_set_flex_flow(&style_lyric_list, LV_FLEX_FLOW_COLUMN);
	lv_style_set_pad_gap(&style_lyric_list, 22);
	lv_style_set_layout(&style_lyric_list, LV_LAYOUT_FLEX);

	/* control buttons */
	lv_style_init(&style_ctrl_button);
	lv_style_set_bg_color(&style_ctrl_button, COL_ACCENT);
	lv_style_set_text_color(&style_ctrl_button, COL_TXT);

	/* buttons container */
	lv_style_init(&style_button_container);
	lv_style_set_flex_flow(&style_button_container, LV_FLEX_FLOW_ROW);
	lv_style_set_layout(&style_button_container, LV_LAYOUT_FLEX);

	/* progress bar parts */
	lv_style_init(&style_bar_track);
	lv_style_set_bg_color(&style_bar_track, COL_BG_PANEL);
	lv_style_set_bg_opa(&style_bar_track, LV_OPA_COVER);

	lv_style_init(&style_bar_ind);
	lv_style_set_bg_color(&style_bar_ind, COL_ACCENT);

	/* album art card */
	lv_style_init(&style_album_art);
	lv_style_set_radius(&style_album_art, 16);
	lv_style_set_bg_opa(&style_album_art, LV_OPA_COVER);
	lv_style_set_bg_grad_dir(&style_album_art, LV_GRAD_DIR_LINEAR);
	lv_style_set_bg_color(&style_album_art, lv_color_hex(0x35506b));
	lv_style_set_bg_grad_color(&style_album_art, COL_ACCENT);
	lv_style_set_border_width(&style_album_art, 0);
	lv_style_set_shadow_width(&style_album_art, 60);
	lv_style_set_shadow_opa(&style_album_art, LV_OPA_40);
	lv_style_set_shadow_color(&style_album_art, lv_color_black());
}
