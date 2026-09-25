# AriaAgent tests — currently the Continuo-backed LLM client smoke test.

add_executable(llm_client_smoke llm_client_smoke.cpp)
target_link_libraries(llm_client_smoke PRIVATE ariaagent_core)
add_test(NAME llm_client_smoke COMMAND llm_client_smoke)
