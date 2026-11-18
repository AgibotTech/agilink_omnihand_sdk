# Copyright (c) 2025, Agibot Co., Ltd.
# AGILINK OmniHand SDK is licensed under Mulan PSL v2

add_executable(demo_omnihand_2025 demo_omnihand_2025.cc)
target_compile_features(demo_omnihand_2025 PRIVATE cxx_std_17)
target_include_directories(demo_omnihand_2025 PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/..")
target_link_libraries(demo_omnihand_2025 PRIVATE ${OMNIHAND_TARGET})
set_target_properties(demo_omnihand_2025 PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${DEMO_OUTPUT_DIR}"
)
