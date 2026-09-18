#include "light_schedule.h"

static int minute_of_day(int hour, int minute)
{
    return hour * 60 + minute;
}

bool light_schedule_is_on(const daily_window_t *window, const struct tm *now)
{
    int now_min = minute_of_day(now->tm_hour, now->tm_min);
    int on_min = minute_of_day(window->on_hour, window->on_minute);
    int off_min = minute_of_day(window->off_hour, window->off_minute);

    if (on_min == off_min) {
        return false;
    }
    if (on_min < off_min) {
        return now_min >= on_min && now_min < off_min;
    }
    // Window crosses midnight.
    return now_min >= on_min || now_min < off_min;
}
