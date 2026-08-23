#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>

#include <lvgl.h>
#include <lvgl/driver_backends.h>
#include <lvgl/simulator_settings.h>

#include "ui/ui.h"
#include "backend/player_logic.h"

extern simulator_settings_t settings;

int main(int argc, char **argv)
{
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

	/* 初始化 SquareLine Studio 导出的 UI */
	ui_init();

	/* Enter the run loop - does not return */
	driver_backends_run_loop();

	return 0;
}
