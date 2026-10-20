#include "tbot.hpp"
#include "cJSON.h"
#include "esp_crt_bundle.h"
#include "esp_http_client.h"
#include "esp_log.h"
//#include "esp_tls.h"
//#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <cstring>
#include <iomanip>
#include <sstream>
#include <string>

static const char *TAG = "TelegramBot";

// Data structure to hold the response
struct HttpData {
    std::string data;  // Stores accumulated HTTP response data
};

// Helper function to URL encode strings
std::string TelegramBot::url_encode(const std::string &value) {
  std::ostringstream escaped;
  escaped.fill('0');
  escaped << std::hex;

  for (char c : value) {
    if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
      escaped << c;
      continue;
    }
    escaped << '%' << std::setw(2) << int((unsigned char)c);
  }

  return escaped.str();
}

TelegramBot::TelegramBot(const std::string &bot_token,
                         const std::string &chat_id)
    : bot_token(bot_token), chat_id(chat_id), task_handle(nullptr),
      running(false) {}

void TelegramBot::start(std::function<void(const std::string &)> callback) {
  if (running)
    return;

  command_callback = callback;
  running = true;
  xTaskCreate(&TelegramBot::bot_task_wrapper, "telegram_bot_task", 4 * 8192,
              this, 5, &task_handle);
}

void TelegramBot::stop() {
  running = false;
  if (task_handle) {
    vTaskDelete(task_handle);
    task_handle = nullptr;
  }
}

// Task wrapper for the bot starting
void TelegramBot::bot_task_wrapper(void *arg) {
  TelegramBot *bot = static_cast<TelegramBot *>(arg);
  bot->bot_task();
}

// The actual bot task to execute
void TelegramBot::bot_task() {
  // Counting errors befor task restart
  int error_count = 0;
  const int max_errors = 5;

  int last_update_id = 0;
  char *json_buffer = NULL; // Moving to heap since we have a stack overflow
  cJSON *root = NULL;
  cJSON *result = NULL;

  while (running && error_count < max_errors) {
    try {
      std::string updates = get_updates(last_update_id + 1);
      ESP_LOGD(TAG, "Raw updates: %s", updates.c_str());
      if (!updates.empty()) {
        // Alloc the buffer on heap
        json_buffer = (char *)malloc(updates.size() + 1);
        if (json_buffer) {
          strcpy(json_buffer, updates.c_str());
		  root = (cJSON *)malloc(sizeof(json_buffer) + 1);
   		  root = cJSON_Parse(json_buffer); 
		  // Parsing the received JSON object
          if (root && cJSON_IsTrue(cJSON_GetObjectItem(root, "ok"))) {
            result = cJSON_GetObjectItem(root, "result");
			// Debug root cJSON object
			ESP_LOGD(TAG, "result: %s", cJSON_Print(root));
            if (result && cJSON_IsArray(result)) {
              int array_size = cJSON_GetArraySize(result);
              for (int i = 0; i < array_size; i++) {
                cJSON *update = cJSON_GetArrayItem(result, i);
                cJSON *update_id_obj = cJSON_GetObjectItem(update, "update_id");
                cJSON *message_obj = cJSON_GetObjectItem(update, "message");

                if (update_id_obj && message_obj) {
                  last_update_id = update_id_obj->valueint;

                  cJSON *chat_obj = cJSON_GetObjectItem(message_obj, "chat");
                  cJSON *text_obj = cJSON_GetObjectItem(message_obj, "text");

                  if (chat_obj && text_obj) {
                    cJSON *chat_id_obj = cJSON_GetObjectItem(chat_obj, "id");
                    if (chat_id_obj &&
                        std::to_string(chat_id_obj->valueint) == chat_id) {
                      std::string command = text_obj->valuestring;
                      ESP_LOGI(TAG, "Received command: %s", command.c_str());
                      command_callback(command);
                    }
                  }
                }
              }
            }
          }
		  // CLean the memory
          if (root) {
            cJSON_Delete(root);
            root = NULL;
          }
          free(json_buffer);
          json_buffer = NULL;
        }
      } else {
        ESP_LOGD(TAG, "No updates received");
        vTaskDelay(pdMS_TO_TICKS(5000));
        continue; // Skip processing if no updates
      }
      error_count = 0;
    } catch (...) {
      error_count++;
      ESP_LOGE(TAG, "Error in bot task (%d/%d)", error_count, max_errors);

      vTaskDelay(pdMS_TO_TICKS(5000));
    }
  }

  // If we have too many errors on the above...
  if (error_count >= max_errors) {
    ESP_LOGE(TAG, "Too many errors, restarting task...");
    vTaskDelay(pdMS_TO_TICKS(10000));
    start(command_callback); // Restart the task
  }
}

// URL constructor
std::string TelegramBot::get_updates(int offset) {
  std::string url =
      "https://api.telegram.org/bot" + bot_token + "/getUpdates?timeout=10";
  if (offset > 0) {
    url += "&offset=" + std::to_string(offset);
  }
  ESP_LOGD(TAG, "Sending GET to URL: %s", url.c_str());
  return http_get_request(url);
}

// Constructing a URL for sending message and calling the request
bool TelegramBot::send_message(const std::string &message) {
  std::string encoded_msg = url_encode(message);
  std::string url = "https://api.telegram.org/bot" + bot_token +
                    "/sendMessage?chat_id=" + chat_id + "&text=" + encoded_msg;

  ESP_LOGD(TAG, "Sending message to URL: %s", url.c_str());
  std::string response = http_get_request(url);
  return !response.empty();
}

// Performing the actual request to the server
std::string TelegramBot::http_get_request(const std::string &url) {
	HttpData http_data;  // Local struct to accumulate data
  // Allocate config on heap to prevent stack overflow
  esp_http_client_config_t *config =
      (esp_http_client_config_t *)malloc(sizeof(esp_http_client_config_t));
  memset(config, 0, sizeof(esp_http_client_config_t));

  config->url = url.c_str();
  config->timeout_ms = 15000;
  config->event_handler = http_event_handler;
  config->user_data = &http_data;  // Pass data accumulator to event handler
  config->buffer_size = 2048;
  config->buffer_size_tx = 1024;
  config->skip_cert_common_name_check = false; // Verify certificate CN
  config->crt_bundle_attach =
      esp_crt_bundle_attach; // Enable certificate verification

  ESP_LOGD(TAG, "Starting an HTTPS query");
  esp_http_client_handle_t client = esp_http_client_init(config);
  free(config);

  if (!client) {
    ESP_LOGE(TAG, "Failed to initialize HTTP client");
    return "";
  }

  // Set user agent (some APIs require this)
  ESP_LOGD(TAG, "Setting HTTP User-Agent header");
  esp_http_client_set_header(client, "User-Agent", "ESP32 TelegramBot");

  ESP_LOGD(TAG, "Performing the HTTPS request");
  esp_err_t err = esp_http_client_perform(client);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "HTTP GET request failed: %s", esp_err_to_name(err));
    esp_http_client_cleanup(client);
    return "";

  } else {
    ESP_LOGD(TAG, "HTTPS Status = %d, content_length = %" PRId64,
             esp_http_client_get_status_code(client),
             esp_http_client_get_content_length(client));
  }

  esp_http_client_cleanup(client);

    return http_data.data;
}

// Handle received data on events
esp_err_t TelegramBot::http_event_handler(esp_http_client_event_t *evt) {
	// The actual structure to hold the data
	HttpData *http_data = static_cast<HttpData*>(evt->user_data);
  switch (evt->event_id) {
  case HTTP_EVENT_ERROR:
    ESP_LOGD(TAG, "HTTP_EVENT_ERROR");
    break;
  case HTTP_EVENT_ON_CONNECTED:
    ESP_LOGD(TAG, "HTTP_EVENT_ON_CONNECTED");
    break;
  case HTTP_EVENT_HEADER_SENT:
    ESP_LOGD(TAG, "HTTP_EVENT_HEADER_SENT");
    break;
  case HTTP_EVENT_ON_HEADER:
    ESP_LOGD(TAG, "HTTP_EVENT_ON_HEADER, key=%s, value=%s", evt->header_key,
             evt->header_value);
    break;
  case HTTP_EVENT_ON_DATA:
	// In case we have incoming data...
	if (evt->data_len > 0 && http_data) {
                // Append incoming data to the accumulator
                http_data->data.append(static_cast<const char*>(evt->data), evt->data_len);
            }
    ESP_LOGD(TAG, "HTTP_EVENT_ON_DATA, len=%d", evt->data_len);
    break;
  case HTTP_EVENT_ON_FINISH:
    ESP_LOGD(TAG, "HTTP_EVENT_ON_FINISH");
    break;
  case HTTP_EVENT_DISCONNECTED:
    ESP_LOGD(TAG, "HTTP_EVENT_DISCONNECTED");
    break;
  case HTTP_EVENT_REDIRECT:
    ESP_LOGD(TAG, "HTTP_EVENT_REDIRECT");
    break;
	// needed for esp-idf v6
//  case HTTP_EVENT_ON_HEADERS_COMPLETE:
//    ESP_LOGD(TAG, "HTTP_EVENT_ON_HEADERS_COMPLETE");
//    break;
//  case HTTP_EVENT_ON_STATUS_CODE:
//    ESP_LOGD(TAG, "HTTP_EVENT_ON_STATUS_CODE");
//    break;
  }
  return ESP_OK;
}
