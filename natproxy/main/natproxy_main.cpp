/*   Main file of the project
*/
#include "esp_err.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "nvs_flash.h"
#include "sdkconfig.h"
#include "tbot.hpp"
#include "wifi.h"
#include <esp_log.h>
#include <iomanip>
#include <memory>
#include <sstream>
#include <stdio.h>
#include <string.h>
#include "wg.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <string>
#include <esp_netif_sntp.h>
#include <esp_timer.h>

static const char *TAG_MAIN = "Main";
// Variable to store when the timer was started
static int64_t timer_start_time = 0;

extern "C" {
	void app_main(void);
}

/**
 * @brief Setting a timer callback function for restart
 */
static void timer_callback(void* arg) {
    esp_restart(); // This function does not return
}

int64_t get_remaining_time_us(void) {
    int64_t current_time = esp_timer_get_time();
    int64_t elapsed_time = current_time - timer_start_time;
    int64_t remaining_time = (21600LL * 1000000LL) - elapsed_time;

    // Prevent negative values if the timer has already expired
    if (remaining_time < 0) {
        remaining_time = 0;
    }
    return remaining_time / 1000000LL / 60;
}

/**
 * @brief Sync local time with SNTP server
 *
 * @note Needed for the WireGuard tunnel
 */
void get_time() {
	time_t now;
	char strftime_buf[64];
	struct tm timeinfo;

	time(&now);
	// Set timezone to China Standard Time
	setenv("TZ", "CST-8", 1);
	tzset();

	localtime_r(&now, &timeinfo);
	strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);
	ESP_LOGI(TAG_MAIN, "The current date/time in Shanghai is: %s", strftime_buf);
}

/**
 * @brief Printing memory info for debugging
 * */
void print_memory_info() {
	UBaseType_t stack_remaining = uxTaskGetStackHighWaterMark(NULL);
	ESP_LOGI(TAG_MAIN, "Stack remaining: %u", stack_remaining);

	ESP_LOGI("MEMORY", "Free heap: %d bytes", esp_get_free_heap_size());
	ESP_LOGI("MEMORY", "Minimum free heap: %d bytes",
			esp_get_minimum_free_heap_size());

	multi_heap_info_t info;
	heap_caps_get_info(&info, MALLOC_CAP_INTERNAL);
	ESP_LOGI("MEMORY", "Total free: %d bytes", info.total_free_bytes);
	ESP_LOGI("MEMORY", "Largest free block: %d bytes", info.largest_free_block);
}

/**
 * @brief Iterates over all LwIP interfaces
 *
 * @return Unique pointer to a string. The ownership of the pointer is passed 
 * to the caller. 
 *
 * @note Logs interface data to console
 */
std::unique_ptr<std::string> get_status() {
    struct netif *netif;

    ESP_LOGI(TAG_MAIN, "Listing all LwIP network interfaces:");

	std::stringstream ss;
	ss << "LwIP interfaces" << "\n----------------\n";

    // Iterate through the global netif_list
    for (netif = netif_list; netif != NULL; netif = netif->next) {
        ESP_LOGI(TAG_MAIN, "Name: %c%c%d", netif->name[0], netif->name[1], netif->num);
		ss << "Name: " << netif->name[0] << netif->name[1] << (int)netif->num << "\n";

        ESP_LOGI(TAG_MAIN, "  MAC address: %02x:%02x:%02x:%02x:%02x:%02x",
                 netif->hwaddr[0], netif->hwaddr[1], netif->hwaddr[2],
                 netif->hwaddr[3], netif->hwaddr[4], netif->hwaddr[5]);
		 ss << "MAC address: ";
		 ss << std::hex << std::setw(2) << std::setfill('0') << (int)netif->hwaddr[0] << ":"
			 << std::hex << std::setw(2) << std::setfill('0') << (int)netif->hwaddr[1] << ":"
			 << std::hex << std::setw(2) << std::setfill('0') << (int)netif->hwaddr[2] << ":"
			 << std::hex << std::setw(2) << std::setfill('0') << (int)netif->hwaddr[3] << ":"
			 << std::hex << std::setw(2) << std::setfill('0') << (int)netif->hwaddr[4] << ":"
			 << std::hex << std::setw(2) << std::setfill('0') << (int)netif->hwaddr[5] << "\n";

        ESP_LOGI(TAG_MAIN, "  IP address: %s", ipaddr_ntoa(&netif->ip_addr));
		ss << "IP address: " << ipaddr_ntoa(&netif->ip_addr) << "\n";

        ESP_LOGI(TAG_MAIN, "  Netmask:    %s", ipaddr_ntoa(&netif->netmask));
		ss << "Netmask: " <<  ipaddr_ntoa(&netif->netmask) << "\n";

        ESP_LOGI(TAG_MAIN, "  Gateway:    %s", ipaddr_ntoa(&netif->gw));
		ss << "Gateway: " <<  ipaddr_ntoa(&netif->gw) << "\n";

        ESP_LOGI(TAG_MAIN, "  Is up: %s", netif_is_up(netif) ? "yes" : "no");
		ss << "Is up: " <<  (int)netif_is_up(netif) << "\n" << "\n";

    }

	ss << "\n----------------------\n";
	ss << "Endpoint: " << endpoint << ":" << std::to_string(end_port) << "\n";

	// create message
	auto msg = std::make_unique<std::string>(ss.str());
	return msg;
}

void app_main(void) {
	// Dinamycaly define log levels
	esp_log_level_set("esp_wireguard", ESP_LOG_DEBUG);
	esp_log_level_set("wireguardif", ESP_LOG_DEBUG);
	esp_log_level_set("wireguard", ESP_LOG_DEBUG);

	// Initialize NVS
	esp_err_t ret = nvs_flash_init();
	if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
			ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
		ESP_ERROR_CHECK(nvs_flash_erase());
		ret = nvs_flash_init();
	}
	ESP_ERROR_CHECK(ret);

	// Start TCP/IP stack
	ESP_ERROR_CHECK(esp_netif_init());

	// Start event loop to monitor wifi connection, etc...
	ESP_ERROR_CHECK(esp_event_loop_create_default());

	// Add stack overflow hook to monitor stack
	esp_log_level_set("TASK", ESP_LOG_INFO);
	vTaskSetApplicationTaskTag(NULL, (TaskHookFunction_t)app_main);

	// Configure the restart timer
    const esp_timer_create_args_t timer_args = {
        .callback = &timer_callback,
        .arg = NULL,
        .dispatch_method = ESP_TIMER_TASK, // Callback is run from a high-priority task
        .name = "restart_timer"
    };

    esp_timer_handle_t timer_handle;
    ESP_ERROR_CHECK(esp_timer_create(&timer_args, &timer_handle));

	// Recording timer start time
	timer_start_time = esp_timer_get_time();
    // Start the timer (6 hours one-shot timer)
    ESP_ERROR_CHECK(esp_timer_start_once(timer_handle, 21600LL * 1000000)); // Time is in microseconds

	// Checking stack and memory
	// print_memory_info();

	// Initialize Wi-Fi
	wifi_init_sta();

	// Wait for WiFi connection
	while (wifi_check == false) {
		vTaskDelay(pdMS_TO_TICKS(5000));
	}
	// Sync time
	get_time();
	// Load WireGUard tunnel config
	set_wg_config();
	wg_validate_config();

	// Create Telegram bot instance with CONFIG_ values
	TelegramBot bot(CONFIG_BOT_TOKEN, CONFIG_CHAT_ID);

	// Start the bot with a command handler
	bot.start([&bot](const std::string &command) {
			ESP_LOGD(TAG_MAIN, "Processing command: %s", command.c_str());

			// Command processing logic
			if (command == "/start") {

				ESP_LOGD(TAG_MAIN, "Received start command");
				bot.send_message("Received /start");

			} else if (command == "/status") {
			// Calling the status function

				ESP_LOGD(TAG_MAIN, "Received status command");
				bot.send_message("Received /status");
				// Calls function to list LwIP netif's
				bot.send_message(get_status()->c_str());

			} else if (command == "/t") {
			// Calling the status function

				ESP_LOGD(TAG_MAIN, "Checking timer");
				bot.send_message("Timer: " + std::to_string(get_remaining_time_us()) + " min");

			} else if (command == "/rst") {
			// Calling the status function

				ESP_LOGD(TAG_MAIN, "Received reset command");
				bot.send_message("Received /rst");
				esp_restart();

			} else if (command == "/wg") {
			// Command for establishing the WireGuard tunnel

				ESP_LOGD(TAG_MAIN, "Received /wg command");
				bot.send_message("Received /wg");
				// CHeck if tunnel is up
				if (fl_tunnel) {
					bot.send_message("Tunnel is already up");
				}
				// Starting the tunnel
				ESP_ERROR_CHECK(wg_init());
// 				if (wifi_netif) {
// 					esp_netif_set_default_netif(wifi_netif);
// 
// 				} 

			} else if (command == "/d") {
				// Command for disconnecting the tunnel

				ESP_LOGD(TAG_MAIN, "Received disconnect command");
				bot.send_message("Received /d");
				if (!fl_tunnel) {
					bot.send_message("Tunnel already down!");
				} else {
					ESP_ERROR_CHECK(wg_disconnect());
					bot.send_message("Shutting down tunnel");
				}

			} else if (command.substr(0,11) == "/newserver ") {
				// Command for changing the server
				
				std::istringstream iss(command);
				std::string cmd, ip, portStr;
				iss >> cmd >> ip >> portStr;  // Extract the three parts
				
				// Update the string and pointer
				endpoint_str = ip;  // Simple assignment
				endpoint = &endpoint_str[0];  // Update pointer to new data

				end_port = std::stoi(portStr);    // Convert port to integer

				ESP_LOGI(TAG_MAIN, "New IP: %s, new port %d", endpoint, end_port);
				wg_disconnect();
				set_wg_config();
				wg_init();

			} else if (command.substr(0,9) == "/newssid ") {
				// Command for changing WIFI
				
				wifi_stop();

				std::istringstream iss(command);
				std::string cmd, ssid_cmd, pass_cmd;
				iss >> cmd >> ssid_cmd >> pass_cmd;  // Extract the three parts
				
				// Update the string and pointer
				ssid = ssid_cmd.c_str();  // assign new ssid
				pass = pass_cmd.c_str();    // assign new pass

				ESP_LOGI(TAG_MAIN, "New SSID: %s, with pass: %s", ssid, pass);

				wifi_init_sta();

			}
			// Add more commands as needed
	});

	// Periodicaly check memory and keep tasks running
	while (true) {
		// Keep this in comments unless for debug
		//    UBaseType_t stack_remaining = uxTaskGetStackHighWaterMark(NULL);
		//    ESP_LOGI("MAIN", "Stack remaining: %u", stack_remaining);
		//    print_memory_info();

		//  Keep the task alive
		vTaskDelay(pdMS_TO_TICKS(5000));
	}

}
