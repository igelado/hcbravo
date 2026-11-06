# SPDX-License-Identifier: LGPL-2.1-only
add_executable(hcbravo-validate conf-validator.cpp
    ${PROJECT_SOURCE_DIR}/src/common/console-logger.cpp)
target_link_libraries(hcbravo-validate PRIVATE hcbravo-conf)
set_target_properties(hcbravo-validate PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}")
