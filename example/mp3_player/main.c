#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>

#include <lvgl.h>
#include <lvgl/driver_backends.h>
#include <lvgl/simulator_settings.h>

#include "ui/ui.h"
#include "backend/ui_bridge.h"

extern simulator_settings_t settings;

/* Directory scanned for *.mp3 files; override with LV_MUSIC_DIR. */
#define MUSIC_DIR_DEFAULT "/home/tanxzh/Music"

int main(int argc, char **argv)
{
	const char *music_dir;

	(void)argc;
	(void)argv;

	/* Initialize LVGL. */
	lv_init();

	settings.window_width = 1024;
	settings.window_height = 600;

	/* Initialize the configured default backend */
	driver_backends_register();
	if (driver_backends_init_backend(NULL) == -1) {
		fprintf(stderr, "Failed to initialize a backend\n");
		exit(EXIT_FAILURE);
	}

	printf("LVGL backend initialized.\n");

	/* Initialize SquareLine Studio exported UI */
	ui_init();

	/* Wire UI to the playback engine and start scanning the library. */
	music_dir = getenv("LV_MUSIC_DIR");
	if (music_dir == NULL) music_dir = MUSIC_DIR_DEFAULT;
	ui_bridge_init(music_dir);

	/* Enter the run loop - does not return */
	driver_backends_run_loop();

	return 0;
}
