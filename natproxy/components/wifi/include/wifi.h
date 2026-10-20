// Header file for wifi.cpp
#ifndef WIFI_H
#define WIFI_H

#include <esp_wifi.h>
#include <string>

#ifdef __cplusplus
	extern "C" {
#endif

	extern bool wifi_check;
	extern char wifi_ip[16];
	extern char wifi_nm[16];
	extern char wifi_gw[16];
	extern const char* ssid;
	extern const char* pass;
	void wifi_init_sta();
	void wifi_status();
	void wifi_stop();
#ifdef __cplusplus
	}
#endif

#endif // WIFI_H
