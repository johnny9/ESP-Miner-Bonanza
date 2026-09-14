#ifndef SELF_TEST_API_H
#define SELF_TEST_API_H
#include "esp_http_server.h"
typedef struct GlobalState GlobalState;
esp_err_t self_test_api_register(httpd_handle_t server, GlobalState *state);
#endif
