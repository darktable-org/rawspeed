if(RAWSPEED_ENABLE_DEBUG_INFO)
  # always debug info
  add_definitions(-g3)
  add_definitions(-ggdb3)
elseif(NOT RAWSPEED_ENABLE_DEBUG_INFO)
  add_definitions(-g0)
else()
  message(SEND_ERROR "RAWSPEED_ENABLE_DEBUG_INFO has unknown value: \"${RAWSPEED_ENABLE_DEBUG_INFO}\"")
endif()
