#ifndef BUTTON_INPUT_H_
#define BUTTON_INPUT_H_

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    BUTTON_INPUT_EVENT_NONE = 0,
    BUTTON_INPUT_EVENT_SHORT_PRESS,
    BUTTON_INPUT_EVENT_LONG_PRESS,
} button_input_event_t;

typedef struct {
    bool raw_pressed;
    bool stable_pressed;
    bool long_press_fired;
    uint32_t raw_changed_at_ms;
    uint32_t pressed_at_ms;
} button_input_state_t;

void button_input_state_init(button_input_state_t *state,
                             bool initially_pressed, uint32_t now_ms);

button_input_event_t button_input_state_update(
    button_input_state_t *state, bool raw_pressed, uint32_t now_ms,
    uint32_t debounce_ms, uint32_t long_press_ms);

#endif /* BUTTON_INPUT_H_ */
