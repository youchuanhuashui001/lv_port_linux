#ifndef PLAYER_LOGIC_H
#define PLAYER_LOGIC_H

#ifdef __cplusplus
extern "C" {
#endif

/*
 * 初始化播放器逻辑后端
 */
void player_logic_init(void);

/*
 * 切换播放/暂停状态
 */
void player_logic_toggle_play(void);

/*
 * 切换到下一首歌
 */
void player_logic_next_track(void);

/*
 * 切换到上一首歌
 */
void player_logic_prev_track(void);

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif /* PLAYER_LOGIC_H */
