#pragma once

#include <stdbool.h>
#include <stdint.h>

#define RELAY_MAX_CHANNELS 4

// Configures `count` GPIOs (from gpio_nums[0..count-1]) as relay outputs and
// immediately drives them all to de-energized. `active_low` selects the
// polarity: true means driving the pin LOW energizes the relay (confirmed on
// the LAFVIN dual-relay module used so far -- see
// samples/relay-demo/README.md's "Learnings" section for how that was
// diagnosed). `count` is clamped to RELAY_MAX_CHANNELS.
void relay_controller_init(const uint8_t *gpio_nums, uint8_t count, bool active_low);

// Energizes or de-energizes relay `index` (0-based, < count passed to init).
// Out-of-range indices are logged and ignored.
void relay_controller_set(uint8_t index, bool energized);

// Returns whether relay `index` is currently energized, per our own tracked
// state (not an actual GPIO readback). False for an out-of-range index.
bool relay_controller_is_energized(uint8_t index);
