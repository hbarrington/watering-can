#pragma once

#include <stdbool.h>
#include <time.h>

typedef struct {
    int on_hour;
    int on_minute;
    int off_hour;
    int off_minute;
} daily_window_t;

// Returns true if `now` falls within the daily [on, off) window described by
// `window`. Correctly handles a window that crosses midnight (e.g. on 22:00,
// off 06:00). An on/off pair that's exactly equal is treated as "always off"
// rather than "always on", to fail safe if the two ever get set the same.
bool light_schedule_is_on(const daily_window_t *window, const struct tm *now);
