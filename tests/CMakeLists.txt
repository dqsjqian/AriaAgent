# AriaAgent tests — dependency safety and the Mira-backed LLM client.

find_package(Python3 REQUIRED COMPONENTS Interpreter)
add_test(NAME fetch_aria_safety
    COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/tools/ci/test_fetch_aria.py")
set_tests_properties(fetch_aria_safety PROPERTIES TIMEOUT 120)

add_executable(llm_client_smoke llm_client_smoke.cpp)
target_link_libraries(llm_client_smoke PRIVATE ariaagent_core)
# Windows loads Aria's DLLs from the executable directory, as for aria_agent.
set_target_properties(llm_client_smoke PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
add_test(NAME llm_client_smoke COMMAND llm_client_smoke)
set_tests_properties(llm_client_smoke PROPERTIES TIMEOUT 30)
