#include "wg.h"
#include "esp_netif.h"
#include "esp_netif_ip_addr.h"
#include "esp_wireguard.h"
#include "lwip/ip4_addr.h"
#include "lwip/netif.h"
#include "lwip/lwip_napt.h"
#include "sdkconfig.h"
#include <cstdlib>
#include <esp_log.h>
#include <esp_err.h>
#include <cstring>

const char* TAG = "WireGuardTunnel";
bool fl_tunnel = false;
// defining buffer to store the endpoint
std::string endpoint_str = CONFIG_WG_PEER_ADDRESS;
// pointer to the endpoint buffer since config takes pointer, not value
char* endpoint = &endpoint_str[0];
int end_port = CONFIG_WG_PEER_PORT;
static wireguard_config_t wg_config = ESP_WIREGUARD_CONFIG_DEFAULT();
static wireguard_ctx_t ctx = {NULL,NULL,NULL};

void set_wg_config() {
	wg_config.private_key = (char* )CONFIG_WG_PRIVATE_KEY;
	wg_config.listen_port = CONFIG_WG_LOCAL_PORT;
	wg_config.public_key = (char* )CONFIG_WG_PEER_PUBLIC_KEY;
	wg_config.allowed_ip = (char* )CONFIG_WG_LOCAL_IP_ADDRESS;
	wg_config.allowed_ip_mask = (char* )CONFIG_WG_LOCAL_IP_NETMASK;
	wg_config.endpoint = endpoint;
	wg_config.port = end_port;
	wg_config.persistent_keepalive = CONFIG_WG_PERSISTENT_KEEP_ALIVE;
	wg_config.preshared_key = (char* )CONFIG_WG_PRESHARED_KEY;
	ESP_LOGI(TAG,"Tunnel params configured");
	ESP_LOGI(TAG,"Endpoint: %s", wg_config.endpoint);
	ESP_LOGI(TAG,"End port: %d", wg_config.port);
}

bool wg_validate_config() {
	if (!wg_config.private_key || !wg_config.public_key || !wg_config.endpoint || !wg_config.preshared_key) {
		ESP_LOGE(TAG, "Missing required WireGuard configuration");
		return false;
	}
	ESP_LOGI(TAG, "WireGuard config looks OK");
	return true;
}

esp_err_t wg_init() {

	if (!fl_tunnel) {
		wg_validate_config();
		ESP_LOGI(TAG, "Initialzing the tunnel...");
		esp_err_t err = esp_wireguard_init(&wg_config, &ctx);

		// Actual connection
		err = esp_wireguard_connect(&ctx);

		ESP_LOGI(TAG,"Wait 5 seconds for the tunnel to establish");
		vTaskDelay(pdMS_TO_TICKS(5000));

		// Check peer
		if (wg_check() == ESP_OK) {

			ESP_LOGI(TAG,"WG ifname: %s", ctx.netif->name);
			ESP_LOGI(TAG,"WG ifkey: %d", ctx.netif->num);
			ESP_LOGI(TAG,"WG IP: " IPSTR, IP2STR(&ctx.netif->ip_addr));
			ESP_LOGI(TAG,"WG netmask: " IPSTR, IP2STR(&ctx.netif->netmask));
			ESP_LOGI(TAG,"WG gw: " IPSTR, IP2STR(&ctx.netif->gw));
		}

		// Enable NAPT trough LwIP
		ip_napt_enable(ip4_addr_get_u32(&ctx.netif->ip_addr), 1);

		fl_tunnel = true;
		return err;
	} else {
		ESP_LOGE(TAG, "Tunnel already up!");
		return ESP_OK;
	}
}

esp_err_t wg_check() {
	// Checks if the peer on the other side is up
	ESP_LOGI(TAG, "Checking if peer is UP");
	esp_err_t err = esp_wireguardif_peer_is_up(&ctx);
	if (err == ESP_OK) {
		// the link is up 
		ESP_LOGI(TAG,"Link is up");
	} else {
		// the link is not up 
		ESP_LOGE(TAG,"Link FAIL");
	}
	return err;
}

esp_err_t wg_disconnect() {
	if (fl_tunnel) {
		ESP_LOGI(TAG, "DIsconnecting the tunnel...");
		esp_err_t err = esp_wireguard_disconnect(&ctx);
		if (err == ESP_OK) {
			ESP_LOGI(TAG, "Tunnel disconnected");
			fl_tunnel = false;
			return err;
		}
		ESP_LOGE(TAG, "Error disconnecting the tunnel");
		return err;
	} else {
		ESP_LOGI(TAG, "Tunnel not connected");
		return ESP_OK;
	}
}
