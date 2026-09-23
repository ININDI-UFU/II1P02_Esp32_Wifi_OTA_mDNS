#pragma once

#include <Arduino.h>

#if defined(LASECSIMUL_OPENETH)

extern "C" {
#include "esp_eth.h"
#include "esp_eth_mac.h"
#include "esp_eth_netif_glue.h"
#include "esp_eth_phy.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "lwip/ip4_addr.h"
}

using wl_status_t = uint8_t;

#ifndef WL_CONNECTED
static constexpr wl_status_t WL_CONNECTED = 3;
static constexpr wl_status_t WL_CONNECT_FAILED = 4;
static constexpr wl_status_t WL_DISCONNECTED = 6;
#endif

class WiFiCompatClass {
public:
    wl_status_t begin(const char *ssid, const char *password = nullptr);
    wl_status_t status() const;
    IPAddress localIP() const;

private:
    bool startOpenEth();
    bool hasIp() const;

    esp_eth_handle_t ethHandle_ = nullptr;
    esp_netif_t *netif_ = nullptr;
    EventGroupHandle_t events_ = nullptr;
};

extern WiFiCompatClass WiFi;

static constexpr EventBits_t GOT_IP = BIT0;

static void onGotIp(
    void *arg,
    esp_event_base_t,
    int32_t,
    void *)
{
    auto *self = static_cast<WiFiCompatClass *>(arg);
    xEventGroupSetBits(self->events_, GOT_IP);
}

WiFiCompatClass WiFi;

bool WiFiCompatClass::startOpenEth()
{
    if (ethHandle_ != nullptr)
        return true;

    events_ = xEventGroupCreate();
    if (events_ == nullptr)
        return false;

    esp_err_t result = esp_netif_init();
    if (result != ESP_OK && result != ESP_ERR_INVALID_STATE)
        return false;

    result = esp_event_loop_create_default();
    if (result != ESP_OK && result != ESP_ERR_INVALID_STATE)
        return false;

    esp_event_handler_register(
        IP_EVENT,
        IP_EVENT_ETH_GOT_IP,
        [](void *arg, esp_event_base_t base, int32_t id, void *data) {
            auto *self = static_cast<WiFiCompatClass *>(arg);
            xEventGroupSetBits(self->events_, GOT_IP);
        },
        this
    );

    esp_netif_config_t netifConfig = ESP_NETIF_DEFAULT_ETH();
    netif_ = esp_netif_new(&netifConfig);
    if (netif_ == nullptr)
        return false;

    eth_mac_config_t macConfig = ETH_MAC_DEFAULT_CONFIG();
    eth_phy_config_t phyConfig = ETH_PHY_DEFAULT_CONFIG();

    phyConfig.phy_addr = 1;
    phyConfig.reset_gpio_num = -1;
    phyConfig.autonego_timeout_ms = 100;

    auto *mac = esp_eth_mac_new_openeth(&macConfig);
    auto *phy = esp_eth_phy_new_dp83848(&phyConfig);

    if (mac == nullptr || phy == nullptr)
        return false;

    esp_eth_config_t ethConfig = ETH_DEFAULT_CONFIG(mac, phy);

    if (esp_eth_driver_install(&ethConfig, &ethHandle_) != ESP_OK)
        return false;

    uint8_t macAddress[6] = {
        0x02, 0x4c, 0x53, 0x00, 0x00, 0x01
    };

    esp_eth_ioctl(
        ethHandle_,
        ETH_CMD_S_MAC_ADDR,
        macAddress
    );

    auto glue = esp_eth_new_netif_glue(ethHandle_);

    if (esp_netif_attach(netif_, glue) != ESP_OK)
        return false;

    return esp_eth_start(ethHandle_) == ESP_OK;
}

wl_status_t WiFiCompatClass::begin(
    const char *,
    const char *)
{
    // SSID e senha são aceitos para manter compatibilidade,
    // mas não são usados pelo OpenETH.
    if (!startOpenEth())
        return WL_CONNECT_FAILED;

    EventBits_t bits = xEventGroupWaitBits(
        events_,
        GOT_IP,
        pdFALSE,
        pdTRUE,
        pdMS_TO_TICKS(15000)
    );

    return (bits & GOT_IP) ? WL_CONNECTED : WL_CONNECT_FAILED;
}

bool WiFiCompatClass::hasIp() const
{
    if (netif_ == nullptr)
        return false;

    esp_netif_ip_info_t info{};
    if (esp_netif_get_ip_info(netif_, &info) != ESP_OK)
        return false;

    return info.ip.addr != 0;
}

wl_status_t WiFiCompatClass::status() const
{
    return hasIp() ? WL_CONNECTED : WL_DISCONNECTED;
}

IPAddress WiFiCompatClass::localIP() const
{
    if (netif_ == nullptr)
        return IPAddress(0, 0, 0, 0);

    esp_netif_ip_info_t info{};
    if (esp_netif_get_ip_info(netif_, &info) != ESP_OK)
        return IPAddress(0, 0, 0, 0);

    return IPAddress(
        ip4_addr1(&info.ip),
        ip4_addr2(&info.ip),
        ip4_addr3(&info.ip),
        ip4_addr4(&info.ip)
    );
}

#else

#include <WiFi.h>

#endif

