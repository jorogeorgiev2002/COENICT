#pragma once

#include <string>
#include <functional>
#include "esp_http_client.h"
#include "wifi.h"

class TelegramBot {
public:
    /**
     * @brief Initialize the Telegram bot
     * @param bot_token Your Telegram bot token
     * @param chat_id Your Telegram chat ID
     */
    TelegramBot(const std::string& bot_token, const std::string& chat_id);

    /**
     * @brief Start the Telegram bot task
     * @param command_callback Function to call when a command is received
     */
    void start(std::function<void(const std::string&)> command_callback);

    /**
     * @brief Stop the Telegram bot task
     */
    void stop();

    /**
     * @brief Send a message to Telegram
     * @param message The message to send
     * @return true if successful, false otherwise
     */
    bool send_message(const std::string& message);

private:
    std::string bot_token;
    std::string chat_id;
    TaskHandle_t task_handle;
    bool running;
    std::function<void(const std::string&)> command_callback;

    void bot_task();
    static void bot_task_wrapper(void* arg);
    std::string get_updates(int offset);
    std::string http_get_request(const std::string& url);
    static esp_err_t http_event_handler(esp_http_client_event_t *evt);
	std::string url_encode(const std::string &value);
};
