# Copyright (c) 2025, Agibot Co., Ltd.
# AGILINK OmniHand SDK is licensed under Mulan PSL v2

add_executable(demo_omnihand_3_ultra_m demo_omnihand_3_ultra_m.cc)
target_compile_features(demo_omnihand_3_ultra_m PRIVATE cxx_std_17)
target_include_directories(demo_omnihand_3_ultra_m PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/..")
target_link_libraries(demo_omnihand_3_ultra_m PRIVATE ${OMNIHAND_TARGET})
set_target_properties(demo_omnihand_3_ultra_m PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${DEMO_OUTPUT_DIR}"
)
if(OMNIHAND_WITH_XENSE)
  target_compile_definitions(demo_omnihand_3_ultra_m PRIVATE OMNIHAND_WITH_XENSE=1)
endif()
