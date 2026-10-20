// WIFI functions library
#include "wifi.h"
#include "esp_err.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif_types.h"
#include "esp_wifi_default.h"
#include "esp_wifi_types_generic.h"
#include "sdkconfig.h"
#include <cstdio>
#include <cstring>
#include <esp_wifi.h>
#include <string.h>

bool wifi_check = false; // external check for connection
esp_netif_t *wifi_netif; // empty netif struct
char wifi_ip[16];		// external vars with wifi params
char wifi_nm[16];
char wifi_gw[16];

// Vars for external commands from the bot
const char* ssid = CONFIG_WIFI_SSID;
const char* pass = CONFIG_WIFI_PASS;


// Some macros for init values
// constants for success/fail connection
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT BIT1
#define ESP_MAXIMUM_RETRY 10

/* FreeRTOS event group to signal when we are connected*/
static EventGroupHandle_t s_wifi_event_group;

// Tag for reporting to console
static const char *TAG = "WiFi";

// Counting connection attempts in var
static int s_retry_num = 0;

// Handling the events from wifi connection
static void event_handler(void *arg, esp_event_base_t event_base,
                          int32_t event_id, void *event_data) {
  if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
    esp_wifi_connect();
  } else if (event_base == WIFI_EVENT &&
             event_id == WIFI_EVENT_STA_DISCONNECTED) {
    if (s_retry_num < ESP_MAXIMUM_RETRY) {
	
      esp_wifi_connect();
      s_retry_num++;
      ESP_LOGI(TAG, "retry to connect to the AP");
    } else {
      xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);

	  //restarting the device
	  esp_restart();

    }
    ESP_LOGI(TAG, "connect to the AP fail");

  } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
    ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
    ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));

	// Asigning the ip's to external vars
	snprintf(wifi_ip, sizeof(wifi_ip), IPSTR, IP2STR(&event->ip_info.ip)); 
	snprintf(wifi_nm, sizeof(wifi_nm), IPSTR, IP2STR(&event->ip_info.netmask)); 
	snprintf(wifi_gw, sizeof(wifi_gw), IPSTR, IP2STR(&event->ip_info.gw)); 

    s_retry_num = 0;
    xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
  }
}

// Function to initialize Wi-Fi
void wifi_init_sta() {
  s_wifi_event_group = xEventGroupCreate();

  esp_netif_create_default_wifi_sta();

  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_wifi_init(&cfg));

  esp_event_handler_instance_t instance_any_id;
  esp_event_handler_instance_t instance_got_ip;
  ESP_ERROR_CHECK(esp_event_handler_instance_register(
      WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL, &instance_any_id));
  ESP_ERROR_CHECK(esp_event_handler_instance_register(
      IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, NULL, &instance_got_ip));

  wifi_config_t wifi_config = {
      .sta = {.ssid = {0},
			  .password = {0},
              .scan_method = WIFI_FAST_SCAN,
              .bssid_set = 0,
              .bssid = "",
              .channel = 0,
              .threshold{
                 // .authmode = WIFI_AUTH_WPA2_PSK,
				 .authmode = WIFI_AUTH_OPEN, 
              }},
  };

  //copying credential vars
  strlcpy((char*)wifi_config.sta.ssid, ssid, sizeof(wifi_config.sta.ssid));
  strlcpy((char*)wifi_config.sta.password, pass, sizeof(wifi_config.sta.password));

  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
  ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));

  ESP_ERROR_CHECK(esp_wifi_start());

  ESP_LOGI(TAG, "Wi-Fi initialized, connecting to AP...");
  ESP_LOGI(TAG, "wifi_init_sta finished.");

  /* Waiting until either the connection is established (WIFI_CONNECTED_BIT) or
   * connection failed for the maximum number of re-tries (WIFI_FAIL_BIT). The
   * bits are set by event_handler() (see above) */
  EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
                                         WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
                                         pdFALSE, pdFALSE, portMAX_DELAY);

  /* xEventGroupWaitBits() returns the bits before the call returned, hence we
   * can test which event actually happened. */
  if (bits & WIFI_CONNECTED_BIT) {

	  esp_netif_t *wifi_netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
	  esp_netif_ip_info_t wifi_ip_info;
	  esp_netif_get_ip_info(wifi_netif, &wifi_ip_info);

	  wifi_check = true;
	  ESP_LOGI(TAG, "connected to ap SSID:%s password:%s", ssid, pass);

  } else if (bits & WIFI_FAIL_BIT) {

	  wifi_check = false;
	  ESP_LOGI(TAG, "Failed to connect to SSID:%s, password:%s", ssid, pass);

  } else {

	  wifi_check = false;
	  ESP_LOGE(TAG, "UNEXPECTED EVENT");
  }
}

void wifi_status() {
	// Get WiFi IP info
	ESP_LOGI(TAG, "WiFi IP: %s", wifi_ip);
	ESP_LOGI(TAG, "WiFi Netmask: %s", wifi_nm);
	ESP_LOGI(TAG, "WiFi GW: %s", wifi_gw);
}

void wifi_stop(void)
{
    esp_err_t err = esp_wifi_stop();
    if (err == ESP_ERR_WIFI_NOT_INIT) {
        return;
    }
    ESP_ERROR_CHECK(err);
    ESP_ERROR_CHECK(esp_wifi_deinit());
	esp_netif_t *wifi_netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    ESP_ERROR_CHECK(esp_wifi_clear_default_wifi_driver_and_handlers(wifi_netif));
    esp_netif_destroy(wifi_netif);
    wifi_netif = NULL;
}

