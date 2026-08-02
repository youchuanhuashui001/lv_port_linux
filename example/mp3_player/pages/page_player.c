#include "page_player.h"

static void back_button_event_handler(lv_event_t *e)
{
	lv_event_code_t code = lv_event_get_code(e);

	if (code == LV_EVENT_CLICKED) {
		// 切换回主页面
		switch_to_page(PAGE_ID_MAIN);
	}
}

// 创建UI
static void create(lv_obj_t *parent)
{
	lv_obj_t *screen = lv_obj_create(parent);
	lv_obj_set_size(screen, LV_PCT(100), LV_PCT(100));
	lv_obj_set_scrollable(screen, false);
	lv_obj_set_style_bg_color(screen, lv_color_hex(0x202040), 0);
	lv_obj_set_style_text_color(screen, lv_color_white(), 0);

	// --- 返回按钮 ---
	lv_obj_t *back_btn = lv_btn_create(screen);
	lv_obj_set_size(back_btn, 60, 30);
	lv_obj_align(back_btn, LV_ALIGN_TOP_LEFT, 10, 10);
	lv_obj_add_event_cb(back_btn, back_button_event_handler, LV_EVENT_ALL, NULL);
	lv_obj_t *back_label = lv_label_create(back_btn);
	lv_label_set_text(back_label, LV_SYMBOL_LEFT " Back");
	lv_obj_center(back_label);

	// --- 专辑封面占位符 ---
	lv_obj_t *album_art = lv_obj_create(screen);
	lv_obj_set_size(album_art, 200, 200);
	lv_obj_align(album_art, LV_ALIGN_TOP_MID, 0, 50);
	lv_obj_set_style_bg_color(album_art, lv_color_hex(0x404060), 0);
	lv_obj_set_style_border_width(album_art, 0, 0);

	// --- 歌曲信息 ---
	lv_obj_t *title_label = lv_label_create(screen);
	lv_label_set_text(title_label, "Track Title");
	lv_obj_set_style_text_font(title_label, &lv_font_montserrat_20, 0);
	lv_obj_align_to(title_label, album_art, LV_ALIGN_OUT_BOTTOM_MID, 0, 15);

	lv_obj_t *artist_label = lv_label_create(screen);
	lv_label_set_text(artist_label, "Artist Name");
	lv_obj_set_style_text_color(artist_label, lv_color_hex(0xb0b0b0), 0);
	lv_obj_align_to(artist_label, title_label, LV_ALIGN_OUT_BOTTOM_MID, 0, 5);

	// --- 进度条 ---
	lv_obj_t *slider = lv_slider_create(screen);
	lv_obj_set_width(slider, 220);
	lv_obj_align_to(slider, artist_label, LV_ALIGN_OUT_BOTTOM_MID, 0, 20);
	lv_slider_set_value(slider, 40, LV_ANIM_OFF);

	// --- 控制按钮 ---
	lv_obj_t *cont_row = lv_obj_create(screen);
	lv_obj_set_size(cont_row, 250, 60);
	lv_obj_align_to(cont_row, slider, LV_ALIGN_OUT_BOTTOM_MID, 0, 15);
	lv_obj_set_style_bg_opa(cont_row, LV_OPA_TRANSP, 0);
	lv_obj_set_style_border_width(cont_row, 0, 0);
	lv_obj_set_flex_flow(cont_row, LV_FLEX_FLOW_ROW);
	lv_obj_set_flex_align(cont_row, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

	lv_obj_t *prev_btn = lv_btn_create(cont_row);
	lv_label_set_text(lv_label_create(prev_btn), LV_SYMBOL_PREV);

	lv_obj_t *play_btn = lv_btn_create(cont_row);
	lv_obj_set_size(play_btn, 50, 50);
	lv_label_set_text(lv_label_create(play_btn), LV_SYMBOL_PLAY);

	lv_obj_t *next_btn = lv_btn_create(cont_row);
	lv_label_set_text(lv_label_create(next_btn), LV_SYMBOL_NEXT);

	// 注册 screen
	page_manager_register_screen(PAGE_ID_PLAYER, screen);
}

// 销毁UI
static void destroy(void)
{
	lv_obj_t *screen = page_manager_get_screen(PAGE_ID_PLAYER);
	if (screen) {
		lv_obj_del(screen);
		page_manager_register_screen(PAGE_ID_PLAYER, NULL);
	}
}

// 显示页面
static void show(void)
{
	lv_obj_t *screen = page_manager_get_screen(PAGE_ID_PLAYER);
	if (screen) {
		lv_obj_set_hidden(screen, false);
	}
}

// 隐藏页面
static void hide(void)
{
	lv_obj_t *screen = page_manager_get_screen(PAGE_ID_PLAYER);
	if (screen) {
		lv_obj_set_hidden(screen, true);
	}
}

// 定义该页面的生命周期函数
static const page_lifecycle_t lifecycle = {
	.create = create,
	.destroy = destroy,
	.show = show,
	.hide = hide,
};

// 提供给外部的接口
const page_lifecycle_t *get_page_player_lifecycle(void)
{
	return &lifecycle;
}
