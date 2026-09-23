#include <stdio.h>
#include <string.h>

#include "esp_eth.h"
#include "esp_event.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_ota_ops.h"
#include "esp_system.h"
#include "mdns.h"
#include "nvs_flash.h"

#ifndef KIT_HOSTNAME
#define KIT_HOSTNAME "iikit"
#endif

#if !CONFIG_ETH_USE_OPENETH
#error "O ambiente openeth requer CONFIG_ETH_USE_OPENETH=y em sdkconfig.openeth."
#endif

static const char *TAG = "openeth";
static esp_netif_t *s_eth_netif;
static httpd_handle_t s_http_server;

static esp_err_t root_get(httpd_req_t *req)
{
    const char *page =
        "<!doctype html><html lang='pt-br'><meta charset='utf-8'>"
        "<title>IIKit OpenETH</title><body><h1>IIKit OpenETH</h1>"
        "<p>Ethernet e servidor HTTP ativos.</p>"
        "<p><a href='/data'>Estado da rede</a></p>"
        "<p>Para OTA, envie o firmware.bin como corpo bruto em POST /ota.</p>"
        "</body></html>";
    httpd_resp_set_type(req, "text/html; charset=utf-8");
    return httpd_resp_sendstr(req, page);
}

static esp_err_t data_get(httpd_req_t *req)
{
    esp_netif_ip_info_t ip;
    esp_err_t err = esp_netif_get_ip_info(s_eth_netif, &ip);
    if (err != ESP_OK) return err;

    char response[96];
    snprintf(response, sizeof(response),
             "{\"interface\":\"openeth\",\"ip\":\"" IPSTR "\",\"hostname\":\"%s\"}",
             IP2STR(&ip.ip), KIT_HOSTNAME);
    httpd_resp_set_type(req, "application/json");
    return httpd_resp_sendstr(req, response);
}

static esp_err_t ota_post(httpd_req_t *req)
{
    if (req->content_len <= 0) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Envie firmware.bin no corpo da requisicao.");
        return ESP_FAIL;
    }

    const esp_partition_t *partition = esp_ota_get_next_update_partition(NULL);
    esp_ota_handle_t handle;
    esp_err_t err = esp_ota_begin(partition, req->content_len, &handle);
    if (err != ESP_OK) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Nao foi possivel iniciar OTA.");
        return err;
    }

    char buffer[1024];
    int remaining = req->content_len;
    while (remaining > 0) {
        int received = httpd_req_recv(req, buffer, remaining < (int)sizeof(buffer) ? remaining : sizeof(buffer));
        if (received <= 0) {
            esp_ota_abort(handle);
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Falha ao receber firmware.");
            return ESP_FAIL;
        }
        err = esp_ota_write(handle, buffer, received);
        if (err != ESP_OK) {
            esp_ota_abort(handle);
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Falha ao gravar firmware.");
            return err;
        }
        remaining -= received;
    }

    err = esp_ota_end(handle);
    if (err == ESP_OK) err = esp_ota_set_boot_partition(partition);
    if (err != ESP_OK) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Firmware invalido.");
        return err;
    }

    httpd_resp_sendstr(req, "Atualizacao gravada. Reinicie o dispositivo para aplicar.");
    return ESP_OK;
}

static void start_services(void)
{
    if (s_http_server != NULL) return;

    ESP_ERROR_CHECK(mdns_init());
    ESP_ERROR_CHECK(mdns_hostname_set(KIT_HOSTNAME));
    ESP_ERROR_CHECK(mdns_instance_name_set("IIKit OpenETH"));
    ESP_ERROR_CHECK(mdns_service_add(NULL, "_http", "_tcp", 80, NULL, 0));

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    ESP_ERROR_CHECK(httpd_start(&s_http_server, &config));

    const httpd_uri_t root = {.uri = "/", .method = HTTP_GET, .handler = root_get};
    const httpd_uri_t data = {.uri = "/data", .method = HTTP_GET, .handler = data_get};
    const httpd_uri_t ota = {.uri = "/ota", .method = HTTP_POST, .handler = ota_post};
    ESP_ERROR_CHECK(httpd_register_uri_handler(s_http_server, &root));
    ESP_ERROR_CHECK(httpd_register_uri_handler(s_http_server, &data));
    ESP_ERROR_CHECK(httpd_register_uri_handler(s_http_server, &ota));

    ESP_LOGI(TAG, "HTTP: http://%s.local/", KIT_HOSTNAME);
}

static void on_got_ip(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    ip_event_got_ip_t *event = event_data;
    if (event->esp_netif != s_eth_netif) return;

    ESP_LOGI(TAG, "OpenETH recebeu IP: " IPSTR, IP2STR(&event->ip_info.ip));
    start_services();
}

void app_main(void)
{
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    esp_netif_config_t netif_config = ESP_NETIF_DEFAULT_ETH();
    s_eth_netif = esp_netif_new(&netif_config);
    assert(s_eth_netif != NULL);

    eth_mac_config_t mac_config = ETH_MAC_DEFAULT_CONFIG();
    eth_phy_config_t phy_config = ETH_PHY_DEFAULT_CONFIG();
    phy_config.autonego_timeout_ms = 100;

    esp_eth_mac_t *mac = esp_eth_mac_new_openeth(&mac_config);
    esp_eth_phy_t *phy = esp_eth_phy_new_dp83848(&phy_config);
    assert(mac != NULL && phy != NULL);

    esp_eth_handle_t eth_handle = NULL;
    esp_eth_config_t eth_config = ETH_DEFAULT_CONFIG(mac, phy);
    ESP_ERROR_CHECK(esp_eth_driver_install(&eth_config, &eth_handle));

    const uint8_t mac_address[6] = {0x02, 0x00, 0x00, 0x12, 0x34, 0x56};
    ESP_ERROR_CHECK(esp_eth_ioctl(eth_handle, ETH_CMD_S_MAC_ADDR, (void *)mac_address));
    ESP_ERROR_CHECK(esp_netif_attach(s_eth_netif, esp_eth_new_netif_glue(eth_handle)));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_ETH_GOT_IP, on_got_ip, NULL));
    ESP_ERROR_CHECK(esp_eth_start(eth_handle));

    ESP_LOGI(TAG, "OpenETH iniciado; aguardando DHCP.");
}
