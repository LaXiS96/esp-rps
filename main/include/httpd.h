#pragma once

#include "esp_http_server.h"

extern httpd_handle_t httpd;

void httpd_init();

// All char* parameters are zero-terminated strings
size_t httpd_get_form_value(const char *body, const char *key, char *dest, size_t dest_len);

esp_err_t httpd_send_badrequest(httpd_req_t *req, const char *msg);
esp_err_t httpd_send_conflict(httpd_req_t *req, const char *msg);
esp_err_t httpd_send_forbidden(httpd_req_t *req, const char *msg);
