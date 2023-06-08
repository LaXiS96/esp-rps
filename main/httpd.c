#include "httpd.h"

#include "util.h"

httpd_handle_t httpd = NULL;

size_t httpd_get_form_value(const char *body, const char *key, char *dest, size_t dest_len)
{
    char *key_ptr;
    if ((key_ptr = strstr(body, key)) != NULL)
    {
        char *start_ptr = key_ptr + strlen(key);

        // Either up to next '&' or end of string
        char *end_ptr = strchr(start_ptr, '&');
        if (end_ptr == NULL)
            end_ptr = strchr(start_ptr, '\0');

        size_t val_len = min(end_ptr - start_ptr, dest_len);
        strncpy(dest, start_ptr, val_len);
        dest[val_len < dest_len ? val_len : dest_len - 1] = '\0';
        return strlen(dest);
    }

    return 0;
}

esp_err_t httpd_send_badrequest(httpd_req_t *req, const char *msg)
{
    httpd_resp_set_status(req, "400 Bad Request");
    httpd_resp_send(req, msg != NULL ? msg : "Bad Request", -1);
    return ESP_OK;
}

esp_err_t httpd_send_conflict(httpd_req_t *req, const char *msg)
{
    httpd_resp_set_status(req, "409 Conflict");
    httpd_resp_send(req, msg != NULL ? msg : "Conflict", -1);
    return ESP_OK;
}

esp_err_t httpd_send_forbidden(httpd_req_t *req, const char *msg)
{
    httpd_resp_set_status(req, "403 Forbidden");
    httpd_resp_send(req, msg != NULL ? msg : "Forbidden", -1);
    return ESP_OK;
}

void httpd_init()
{
    // if (httpd != NULL)
    // TODO throw

    httpd_config_t httpd_config = HTTPD_DEFAULT_CONFIG();
    ESP_ERROR_CHECK(httpd_start(&httpd, &httpd_config));
}
