#ifndef WG_H
#define WG_H

#include "esp_wireguard.h"
#include <stdbool.h>
#include <string>

#ifdef __cplusplus
extern "C" {
#endif

/** A flag variable to indicate if the tunnel is up
*/
extern bool fl_tunnel;
extern std::string endpoint_str;
extern char* endpoint;
extern int end_port;

/**
 * @brief Configures the WireGuard tunnel with parameters from Kconfig
 */
void set_wg_config();

/**
 * @brief Validates the WireGuard configuration
 * @return true if configuration is valid, false otherwise
 */
bool wg_validate_config();

/**
 * @brief Initializes and starts the WireGuard tunnel
*/
esp_err_t wg_init();

/**
 * @brief Checks if the WireGuard tunnel is up and running
 * @return true if tunnel is up, false otherwise
 */
esp_err_t wg_check();

/**
 * @brief Disconnects the WireGuard tunnel
 */
esp_err_t wg_disconnect();

#ifdef __cplusplus
}
#endif

#endif // WG_H
