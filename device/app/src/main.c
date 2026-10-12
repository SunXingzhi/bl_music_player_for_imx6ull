#include <unistd.h>
#include <pthread.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <lvgl/lvgl.h>
#include <lv_demos.h>

#include "lib/driver_backends.h"
#include "lib/simulator_settings.h"
#include "lib/simulator_util.h"

/* Global simulator settings, defined in driver_backends.c */
extern simulator_settings_t settings;

lv_style_t test_screen_style;

void lyrics_demo_create(void);

/**
 * @brief entry point
 * @description start a demo
 * @param argc the count of arguments in argv
 * @param argv The arguments
 */
int main()
{

        /* Initialize LVGL. */
        lv_init();

        /* Register all available backends (must be called first). */
        driver_backends_register();

        /* Default window size used by e.g. the SDL backend. */
        settings.window_width  = 800;
        settings.window_height = 480;

        /* Init default display backend (host = SDL, arm = FBDEV, per .config). */
        if(driver_backends_init_backend(NULL) == -1) {
                die("Failed to init display backend");
        }
        /* Init input backend; silently skipped if EVDEV not compiled in (host). */
        /* NOTE: don't pre-check with driver_backends_is_supported() - it mutates
           its argument (toupper) and crashes on string literals. */
        if(driver_backends_init_backend("EVDEV") == -1) {
                die("Failed to initialize evdev");
        }

        lyrics_demo_create();

        while(1) {
                uint32_t ms = lv_timer_handler();
                if(ms == LV_NO_TIMER_READY) {
                        ms = LV_DEF_REFR_PERIOD;
                }
                usleep(ms * 1000);
        }

        return 0;
}
