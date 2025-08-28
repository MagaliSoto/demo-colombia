find_path(MOSQUITTO_INCLUDE_DIR
    NAMES mosquitto.h
    PATHS /usr/local/include
)

find_library(MOSQUITTO_LIBRARY
    NAMES mosquitto
    PATHS /usr/local/lib
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Mosquitto DEFAULT_MSG MOSQUITTO_LIBRARY MOSQUITTO_INCLUDE_DIR)

if(MOSQUITTO_FOUND)
    set(MOSQUITTO_LIBRARIES ${MOSQUITTO_LIBRARY})
    set(MOSQUITTO_INCLUDE_DIRS ${MOSQUITTO_INCLUDE_DIR})
else()
    set(MOSQUITTO_LIBRARIES)
    set(MOSQUITTO_INCLUDE_DIRS)
endif()

mark_as_advanced(MOSQUITTO_INCLUDE_DIRS MOSQUITTO_LIBRARIES)
