cmake_minimum_required(VERSION 3.20)

project(freewisp_wasm_smoke_test LANGUAGES C)

add_executable(freewisp_wasm_smoke main.c)

set_target_properties(freewisp_wasm_smoke PROPERTIES
  OUTPUT_NAME "freewisp-wasm-smoke"
  SUFFIX ".js"
)
