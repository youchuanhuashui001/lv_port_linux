#include "page_manager.h"
#include "pages/page_main.h"
#include "pages/page_player.h"

// 页面注册表
static const page_lifecycle_t *page_lifecycles[PAGE_ID_MAX];

// 存储每个页面主容器的数组
static lv_obj_t *page_screens[PAGE_ID_MAX];

// 当前页面的ID
static page_id_t current_page_id;

/**
 * @brief 注册所有页面及其生命周期函数
 */
static void register_all_pages(void)
{
	page_lifecycles[PAGE_ID_MAIN] = get_page_main_lifecycle();
	page_lifecycles[PAGE_ID_PLAYER] = get_page_player_lifecycle();
	// 在此注册更多页面
}

void page_manager_init(void)
{
	// 初始化所有页面容器指针为NULL
	for (int i = 0; i < PAGE_ID_MAX; i++) {
		page_screens[i] = NULL;
	}

	register_all_pages();

	// 设置初始页面ID，但此时不创建UI
	current_page_id = PAGE_ID_MAIN;
}

void switch_to_page(page_id_t id)
{
	if (id >= PAGE_ID_MAX) {
		return; // 无效ID
	}

	const page_lifecycle_t *current_page = page_lifecycles[current_page_id];
	const page_lifecycle_t *next_page = page_lifecycles[id];

	// 隐藏当前页面
	if (page_screens[current_page_id] != NULL && current_page->hide) {
		current_page->hide();
	}

	current_page_id = id;

	// 如果目标页面UI还未创建，则创建它
	if (page_screens[current_page_id] == NULL) {
		if (next_page->create) {
			// 我们需要一个父对象来创建页面，这里使用LVGL的活动屏幕
			lv_obj_t *parent = lv_scr_act();
			next_page->create(parent);
		}
	}

	// 显示新页面
	if (page_screens[current_page_id] != NULL && next_page->show) {
		next_page->show();
	}
}

void page_manager_register_screen(page_id_t id, lv_obj_t *screen)
{
    if (id < PAGE_ID_MAX) {
        page_screens[id] = screen;
    }
}

lv_obj_t *page_manager_get_screen(page_id_t id)
{
	if (id < PAGE_ID_MAX) {
		return page_screens[id];
	}
	return NULL;
}
