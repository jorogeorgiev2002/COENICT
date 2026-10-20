idf_component_register(SRCS 
						"natproxy_main.cpp"
                       PRIV_REQUIRES "wifi" 
					   				 "tbot"
									 "wg"
									 "esp_wireguard"
									 "spi_flash"
									 "esp_wifi"
									 "esp-tls"
									 "esp_http_client"
									 "nvs_flash"
									 "esp_timer"
                       INCLUDE_DIRS ".")

