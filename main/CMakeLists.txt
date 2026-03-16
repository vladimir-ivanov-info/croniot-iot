#idf_component_register(SRCS "main.cpp" INCLUDE_DIRS ".")


# idf_component_register(
#     SRCS
#         "main.cpp"
#     INCLUDE_DIRS
#         "."
#     REQUIRES
#         src
# )

idf_component_register(
    SRCS
        "main.cpp"
    INCLUDE_DIRS
        "."
    REQUIRES
        croniot
)


#Esto le dice a ESP-IDF que tu componente depende de esp_wifi, esp_event, y nvs_flash, y cargará automáticamente esos componentes durante la compilación.
#idf_component_register(
#    SRCS "WifiNetworkConnectionController.cpp"
#    INCLUDE_DIRS "."
#    REQUIRES esp_wifi esp_event nvs_flash
#)


# Agregar la carpeta de componentes personalizados
#set(EXTRA_COMPONENT_DIRS src/network)
