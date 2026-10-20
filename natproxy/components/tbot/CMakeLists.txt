idf_component_register(SRCS "tbot.cpp"
                       INCLUDE_DIRS "include"
					   REQUIRES esp_http_client esp_https_ota wifi json)
