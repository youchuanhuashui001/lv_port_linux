#include "page_main.h"

static void player_button_event_handler(lv_event_t *e)
{
	lv_event_code_t code = lv_event_get_code(e);

	if (code == LV_EVENT_CLICKED) {
		// 当按钮被点击时，切换到播放器页面
		switch_to_page(PAGE_ID_PLAYER);
	}
}

// 创建UI
static void create(lv_obj_t *parent)
{
	lv_obj_t *screen = lv_obj_create(parent);
	lv_obj_set_size(screen, LV_PCT(100), LV_PCT(100));
	lv_obj_set_scrollable(screen, false);

	// --- 标题 ---
	lv_obj_t *title_label = lv_label_create(screen);
	lv_label_set_text(title_label, "音乐库");
	lv_obj_set_style_text_font(title_label, &lv_font_montserrat_24, 0);
	lv_obj_align(title_label, LV_ALIGN_TOP_MID, 0, 10);

	// --- 歌曲列表 ---
	lv_obj_t *list = lv_list_create(screen);
	lv_obj_set_size(list, 300, 200);
	lv_obj_align(list, LV_ALIGN_CENTER, 0, -20);

	const char *tracks[] = {"Bohemian Rhapsody", "Stairway to Heaven", "Hotel California", "Sweet Child O' Mine", NULL};
	for (int i = 0; tracks[i] != NULL; i++) {
		lv_obj_t *list_btn = lv_list_add_button(list, LV_SYMBOL_AUDIO, tracks[i]);
		// 在未来，可以为这些按钮添加事件回调
	}

	// --- 导航按钮 ---
	lv_obj_t *player_btn = lv_btn_create(screen);
	lv_obj_add_event_cb(player_btn, player_button_event_handler, LV_EVENT_ALL, NULL);
	lv_obj_align(player_btn, LV_ALIGN_BOTTOM_MID, 0, -20);

	lv_obj_t *btn_label = lv_label_create(player_btn);
	lv_label_set_text(btn_label, "Go to Player");
	lv_obj_center(btn_label);

	// 注册 screen
	page_manager_register_screen(PAGE_ID_MAIN, screen);
}

// 销毁UI
static void destroy(void)
{
	lv_obj_t *screen = page_manager_get_screen(PAGE_ID_MAIN);
	if (screen) {
		lv_obj_del(screen);
		page_manager_register_screen(PAGE_ID_MAIN, NULL);
	}
}

// 显示页面
static void show(void)
{
	lv_obj_t *screen = page_manager_get_screen(PAGE_ID_MAIN);
	if (screen) {
		lv_obj_set_hidden(screen, false);
	}
}

// 隐藏页面
static void hide(void)
{
	lv_obj_t *screen = page_manager_get_screen(PAGE_ID_MAIN);
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
const page_lifecycle_t *get_page_main_lifecycle(void)
{
	return &lifecycle;
}
