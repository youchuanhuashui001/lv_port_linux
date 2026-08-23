#ifndef UI_BRIDGE_H
#define UI_BRIDGE_H

#include "player_logic.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Bridge between the View (SquareLine ui/) and the Model
 * (backend/player_logic). The only place allowed to know both.
 *
 * Call ui_bridge_init() after ui_init(); it registers the status
 * callback with the player and populates the playlist panel once
 * tracks are found.
 */

void ui_bridge_init(const char *music_dir);

/* pl_status_cb_t target - registered by ui_bridge_init(). */
void ui_bridge_on_status(const pl_status_t *st, void *user_data);

#ifdef __cplusplus
}
#endif

#endif /* UI_BRIDGE_H */
