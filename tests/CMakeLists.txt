# AriaAgent tests — dependency safety and the Mira-backed LLM client.

if(FORGEDEPS_PYTHON_EXECUTABLE)
    set(Python3_EXECUTABLE "${FORGEDEPS_PYTHON_EXECUTABLE}")
endif()
find_package(Python3 3.14 REQUIRED COMPONENTS Interpreter)
add_test(NAME fetch_aria_safety
    COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/scripts/ci/test_fetch_aria.py")
set_tests_properties(fetch_aria_safety PROPERTIES TIMEOUT 120)

add_executable(llm_client_smoke llm_client_smoke.cpp)
target_link_libraries(llm_client_smoke PRIVATE ariaagent_core)
# Windows loads Aria's DLLs from the executable directory, as for aria_agent.
set_target_properties(llm_client_smoke PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
add_test(NAME llm_client_smoke COMMAND llm_client_smoke)
set_tests_properties(llm_client_smoke PROPERTIES TIMEOUT 30)

# Catalog lookup, language switching, and ViewModel/AppText integration use
# generated temporary resources, never a user's settings or session files.
add_executable(localization_smoke localization_smoke.cpp)
target_link_libraries(localization_smoke PRIVATE ariaagent_mod_app)
set_target_properties(localization_smoke PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
add_test(NAME localization_smoke COMMAND localization_smoke "${PROJECT_SOURCE_DIR}/i18n")
set_tests_properties(localization_smoke PROPERTIES TIMEOUT 30)

add_executable(module_smoke module_smoke.cpp)
target_link_libraries(module_smoke PRIVATE ariaagent_modules)
set_target_properties(module_smoke PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
add_test(NAME module_smoke COMMAND module_smoke "${PROJECT_SOURCE_DIR}/i18n")
set_tests_properties(module_smoke PROPERTIES TIMEOUT 30)
