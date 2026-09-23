idf_component_register(
    SRCS "openeth_main.c"
    REQUIRES app_update esp_eth esp_event esp_http_server esp_netif mdns nvs_flash
)
