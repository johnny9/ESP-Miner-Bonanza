#include "button_input.h"

#include <stddef.h>

void button_input_state_init(button_input_state_t *state,
                             bool initially_pressed, uint32_t now_ms)
{
    if (state == NULL) {
        return;
    }

    *state = (button_input_state_t){
        .raw_pressed = initially_pressed,
        .stable_pressed = initially_pressed,
        .raw_changed_at_ms = now_ms,
        .pressed_at_ms = now_ms,
    };
}

button_input_event_t button_input_state_update(
    button_input_state_t *state, bool raw_pressed, uint32_t now_ms,
    uint32_t debounce_ms, uint32_t long_press_ms)
{
    if (state == NULL) {
        return BUTTON_INPUT_EVENT_NONE;
    }

    if (raw_pressed != state->raw_pressed) {
        state->raw_pressed = raw_pressed;
        state->raw_changed_at_ms = now_ms;
    }

    if (state->raw_pressed != state->stable_pressed &&
        now_ms - state->raw_changed_at_ms >= debounce_ms) {
        state->stable_pressed = state->raw_pressed;

        if (state->stable_pressed) {
            state->pressed_at_ms = now_ms;
            state->long_press_fired = false;
        } else if (!state->long_press_fired) {
            return BUTTON_INPUT_EVENT_SHORT_PRESS;
        }
    }

    if (state->stable_pressed && !state->long_press_fired &&
        now_ms - state->pressed_at_ms >= long_press_ms) {
        state->long_press_fired = true;
        return BUTTON_INPUT_EVENT_LONG_PRESS;
    }

    return BUTTON_INPUT_EVENT_NONE;
}
