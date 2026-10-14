#pragma once

#include <stdbool.h>
#include "esp_err.h"
#include "light_schedule.h"

// Thread-safe shared state for the grow light schedule + manual override +
// last-applied actual state. Exists so the HTTP API (running in the httpd
// server's task) and the main scheduling loop (running in app_main's task)
// can safely read/write the same state without main.c depending on the web
// server, or the web server depending on relay_controller directly.

// Loads the schedule window from NVS if light_state_set_window() has saved
// one there before; otherwise seeds NVS with `default_window` (the
// Kconfig-configured values) and uses that. Call once at boot, before
// anything else in this module is used.
esp_err_t light_state_init(const daily_window_t *default_window);

// Current schedule window (thread-safe copy).
daily_window_t light_state_get_window(void);

// Updates the schedule window and persists it to NVS so it survives a
// reboot. Does not touch any currently-active manual override.
esp_err_t light_state_set_window(const daily_window_t *window);

// Engages a manual override: the resolved on/off state (see
// light_state_resolve) is forced to `desired_on` regardless of what the
// schedule says, until the schedule's own on/off decision next changes from
// `current_schedule_on` (the value it was in effect at the moment this was
// called) -- at which point the override auto-clears and the real schedule
// takes back over. Never leaves lights stuck in a manual state forever.
void light_state_set_override(bool desired_on, bool current_schedule_on);

// Explicitly clears an active manual override early ("resume schedule now").
// Harmless no-op if no override is active.
void light_state_clear_override(void);

// Called once per poll by the main loop with the schedule's current
// should-be-on value (from light_schedule_is_on()). Returns the value that
// should actually be applied to the relays this poll -- the override's
// desired state if one is active and the schedule hasn't transitioned yet,
// or the schedule's own value otherwise. Auto-clears the override as a side
// effect when a transition is detected.
bool light_state_resolve(bool schedule_on);

// Records the state that was actually just applied to the relays, so
// status queries can report it without depending on relay_controller.
void light_state_set_actual(bool energized);

typedef struct {
    daily_window_t window;
    bool schedule_says_on;
    bool override_active;
    bool actual_on;
} light_status_t;

// Thread-safe snapshot of everything /status needs. `schedule_on` is passed
// in since the caller already has to compute it (against the current time)
// to drive light_state_resolve -- this just merges it with stored state.
light_status_t light_state_get_status(bool schedule_on);
