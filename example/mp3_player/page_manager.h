#ifndef PAGE_MANAGER_H
#define PAGE_MANAGER_H

#include "lvgl.h"

// 使用枚举定义所有页面ID
typedef enum {
	PAGE_ID_MAIN,
	PAGE_ID_PLAYER,
	// 在此添加更多页面ID
	PAGE_ID_MAX,
} page_id_t;

// 页面生命周期函数指针结构体
typedef struct {
	// 创建页面的UI，parent是父对象，通常是 lv_scr_act()
	void (*create)(lv_obj_t *parent);
	// 销毁页面的UI和资源
	void (*destroy)(void);
	// 显示页面（例如，播放进入动画）
	void (*show)(void);
	// 隐藏页面（例如，播放退出动画）
	void (*hide)(void);
} page_lifecycle_t;

/**
 * @brief 初始化页面管理器
 */
void page_manager_init(void);

/**
 * @brief 切换到指定ID的页面
 * @param id 要切换到的页面的ID
 */
void switch_to_page(page_id_t id);

/**
 * @brief 供页面模块调用，以注册它们创建的屏幕容器
 * @param id 页面ID
 * @param screen 页面创建的 lv_obj_t* 容器
 */
void page_manager_register_screen(page_id_t id, lv_obj_t *screen);

/**
 * @brief 获取指定ID页面的主容器对象
 * @param id 页面ID
 * @return lv_obj_t* 页面的主容器
 */
lv_obj_t *page_manager_get_screen(page_id_t id);


#endif // PAGE_MANAGER_H
