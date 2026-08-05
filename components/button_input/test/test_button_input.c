#include <stdint.h>

#include "button_input.h"
#include "unity.h"

#define DEBOUNCE_MS 30U
#define LONG_PRESS_MS 2000U

TEST_CASE("button input debounces a short press", "[button_input]")
{
    button_input_state_t state;
    button_input_state_init(&state, false, 0);

    TEST_ASSERT_EQUAL(BUTTON_INPUT_EVENT_NONE,
                      button_input_state_update(
                          &state, true, 10, DEBOUNCE_MS, LONG_PRESS_MS));
    TEST_ASSERT_EQUAL(BUTTON_INPUT_EVENT_NONE,
                      button_input_state_update(
                          &state, false, 20, DEBOUNCE_MS, LONG_PRESS_MS));
    TEST_ASSERT_EQUAL(BUTTON_INPUT_EVENT_NONE,
                      button_input_state_update(
                          &state, true, 30, DEBOUNCE_MS, LONG_PRESS_MS));
    TEST_ASSERT_EQUAL(BUTTON_INPUT_EVENT_NONE,
                      button_input_state_update(
                          &state, true, 60, DEBOUNCE_MS, LONG_PRESS_MS));
    TEST_ASSERT_EQUAL(BUTTON_INPUT_EVENT_NONE,
                      button_input_state_update(
                          &state, false, 100, DEBOUNCE_MS, LONG_PRESS_MS));
    TEST_ASSERT_EQUAL(BUTTON_INPUT_EVENT_SHORT_PRESS,
                      button_input_state_update(
                          &state, false, 130, DEBOUNCE_MS, LONG_PRESS_MS));
    TEST_ASSERT_EQUAL(BUTTON_INPUT_EVENT_NONE,
                      button_input_state_update(
                          &state, false, 140, DEBOUNCE_MS, LONG_PRESS_MS));
}

TEST_CASE("button input emits one long press and no release click",
          "[button_input]")
{
    button_input_state_t state;
    button_input_state_init(&state, false, 0);

    TEST_ASSERT_EQUAL(BUTTON_INPUT_EVENT_NONE,
                      button_input_state_update(
                          &state, true, 10, DEBOUNCE_MS, LONG_PRESS_MS));
    TEST_ASSERT_EQUAL(BUTTON_INPUT_EVENT_NONE,
                      button_input_state_update(
                          &state, true, 40, DEBOUNCE_MS, LONG_PRESS_MS));
    TEST_ASSERT_EQUAL(BUTTON_INPUT_EVENT_NONE,
                      button_input_state_update(
                          &state, true, 2039, DEBOUNCE_MS, LONG_PRESS_MS));
    TEST_ASSERT_EQUAL(BUTTON_INPUT_EVENT_LONG_PRESS,
                      button_input_state_update(
                          &state, true, 2040, DEBOUNCE_MS, LONG_PRESS_MS));
    TEST_ASSERT_EQUAL(BUTTON_INPUT_EVENT_NONE,
                      button_input_state_update(
                          &state, true, 3000, DEBOUNCE_MS, LONG_PRESS_MS));
    TEST_ASSERT_EQUAL(BUTTON_INPUT_EVENT_NONE,
                      button_input_state_update(
                          &state, false, 3010, DEBOUNCE_MS, LONG_PRESS_MS));
    TEST_ASSERT_EQUAL(BUTTON_INPUT_EVENT_NONE,
                      button_input_state_update(
                          &state, false, 3040, DEBOUNCE_MS, LONG_PRESS_MS));
}

TEST_CASE("button held during initialization can emit a long press",
          "[button_input]")
{
    button_input_state_t state;
    button_input_state_init(&state, true, 100);

    TEST_ASSERT_EQUAL(BUTTON_INPUT_EVENT_NONE,
                      button_input_state_update(
                          &state, true, 2099, DEBOUNCE_MS, LONG_PRESS_MS));
    TEST_ASSERT_EQUAL(BUTTON_INPUT_EVENT_LONG_PRESS,
                      button_input_state_update(
                          &state, true, 2100, DEBOUNCE_MS, LONG_PRESS_MS));
}

TEST_CASE("button timing handles the millisecond counter wrapping",
          "[button_input]")
{
    button_input_state_t state;
    uint32_t start = UINT32_MAX - 20U;
    button_input_state_init(&state, false, start);

    TEST_ASSERT_EQUAL(BUTTON_INPUT_EVENT_NONE,
                      button_input_state_update(
                          &state, true, start + 1U,
                          DEBOUNCE_MS, LONG_PRESS_MS));
    TEST_ASSERT_EQUAL(BUTTON_INPUT_EVENT_NONE,
                      button_input_state_update(
                          &state, true, 10U,
                          DEBOUNCE_MS, LONG_PRESS_MS));
    TEST_ASSERT_EQUAL(BUTTON_INPUT_EVENT_NONE,
                      button_input_state_update(
                          &state, true, 11U,
                          DEBOUNCE_MS, LONG_PRESS_MS));
    TEST_ASSERT_TRUE(state.stable_pressed);
}

TEST_CASE("button input ignores a null state", "[button_input]")
{
    button_input_state_init(NULL, false, 0);
    TEST_ASSERT_EQUAL(BUTTON_INPUT_EVENT_NONE,
                      button_input_state_update(
                          NULL, true, 0, DEBOUNCE_MS, LONG_PRESS_MS));
}
