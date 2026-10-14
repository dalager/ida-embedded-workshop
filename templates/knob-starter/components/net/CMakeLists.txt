idf_component_register(
    SRCS "net.c" "net_http.c" "upload_plan.c"
    INCLUDE_DIRS "include"
    REQUIRES esp_wifi esp_netif esp_event nvs_flash esp_http_client esp_timer mbedtls
)
