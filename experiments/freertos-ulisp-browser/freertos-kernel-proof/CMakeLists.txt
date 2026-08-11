cmake_minimum_required(VERSION 3.20)

project(freewisp_freertos_kernel_proof LANGUAGES C)

if(NOT EMSCRIPTEN)
  message(FATAL_ERROR "Configure this target through emcmake")
endif()

set(FREERTOS_KERNEL "${CMAKE_CURRENT_SOURCE_DIR}/../third-party/FreeRTOS-Kernel")
set(FREERTOS_EMSCRIPTEN_PORT "${CMAKE_CURRENT_SOURCE_DIR}/../emscripten-freertos-port")

add_executable(freewisp_freertos_kernel_proof
  main.c
  "${FREERTOS_EMSCRIPTEN_PORT}/port.c"
  "${FREERTOS_KERNEL}/list.c"
  "${FREERTOS_KERNEL}/queue.c"
  "${FREERTOS_KERNEL}/tasks.c"
  "${FREERTOS_KERNEL}/timers.c"
  "${FREERTOS_KERNEL}/portable/MemMang/heap_4.c"
)

target_compile_features(freewisp_freertos_kernel_proof PRIVATE c_std_11)
target_compile_options(freewisp_freertos_kernel_proof PRIVATE
  -Wall
  -Wextra
  -Werror
  -Wno-unused-parameter
  -Wno-unused-but-set-variable
)
target_include_directories(freewisp_freertos_kernel_proof PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}"
  "${FREERTOS_EMSCRIPTEN_PORT}"
  "${FREERTOS_KERNEL}/include"
)
target_link_options(freewisp_freertos_kernel_proof PRIVATE
  "-sASYNCIFY=1"
  "-sALLOW_MEMORY_GROWTH=1"
  "-sASSERTIONS=1"
)

set_target_properties(freewisp_freertos_kernel_proof PROPERTIES
  OUTPUT_NAME "freewisp-freertos-kernel-proof"
  SUFFIX ".js"
)

configure_file(index.html index.html COPYONLY)

enable_testing()
add_test(NAME freertos_kernel_proof COMMAND freewisp_freertos_kernel_proof)
set_tests_properties(freertos_kernel_proof PROPERTIES
  PASS_REGULAR_EXPRESSION "FREEWISP_KERNEL_PROOF_PASS"
)
