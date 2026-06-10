idf_component_register(SRCS "main.c"
                        "gpio_handler.c"
                        "led_handler.c"
                        "pwm_handler.c"
                        "adc_handler.c"
                        "uart_handler.c"
                        "wifi_handler.c"
                        "web_server.c"
                        "ota_handler.c"
                    INCLUDE_DIRS "."
                    EMBED_TXTFILES dashboard/index.html dashboard/style.css dashboard/app.js
                    REQUIRES nvs_flash esp_wifi esp_event esp_netif esp_http_server esp_https_ota esp_adc esp_driver_ledc esp_driver_uart app_update json)
