idf_component_register(SRCS "led.c"
                    PRIV_REQUIRES driver
                    INCLUDE_DIRS "include" "../../managed_components/espressif__led_strip/include")
