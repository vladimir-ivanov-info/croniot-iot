file(GLOB_RECURSE SRC_FILES "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")

idf_component_register(
  SRCS ${SRC_FILES}
  INCLUDE_DIRS 
    . 
    Messages 
    Parameters 
    Tasks
  REQUIRES 
    nvs_flash
    json
    #esp_littlefs
    #joltwallet__littlefs
)

