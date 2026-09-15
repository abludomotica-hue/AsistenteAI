idf_component_register(SRCS "main.cpp" "UI_Manager.cpp" "ES8311_Init.c" "OTA_Manager.cpp" "audio_manager.cpp" "network_stream.cpp"
                    INCLUDE_DIRS "."
                    REQUIRES esp_wifi esp_http_client driver esp_lcd nvs_flash cjson esp_https_ota app_update espressif__esp32_p4_function_ev_board bsp_extra espressif__esp_lcd_st7701 esp-sr)

target_compile_options(${COMPONENT_LIB} PRIVATE -Wno-missing-field-initializers -Wno-deprecated-enum-enum-conversion -Wno-unused-variable)
target_link_options(${COMPONENT_LIB} INTERFACE "-Wl,--no-relax")
target_compile_options(${COMPONENT_LIB} INTERFACE "-mno-relax")
