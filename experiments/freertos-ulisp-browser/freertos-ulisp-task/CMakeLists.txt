cmake_minimum_required(VERSION 3.20)

project(freewisp_freertos_ulisp_task LANGUAGES C CXX)

if(NOT EMSCRIPTEN)
  message(FATAL_ERROR "Configure this target through emcmake")
endif()

set(FREERTOS_KERNEL "${CMAKE_CURRENT_SOURCE_DIR}/../third-party/FreeRTOS-Kernel")
set(FREERTOS_EMSCRIPTEN_PORT "${CMAKE_CURRENT_SOURCE_DIR}/../emscripten-freertos-port")
set(ULISP_UPSTREAM "${CMAKE_CURRENT_SOURCE_DIR}/../third-party/ulisp-esp/ulisp-esp.ino")

# Arduino inserts forward declarations while preprocessing a sketch. Generate
# those declarations, then add the FreeWisp safe-point call without changing the
# pinned upstream source.
file(READ "${ULISP_UPSTREAM}" ULISP_SOURCE)
string(REGEX MATCHALL
  "\n[A-Za-z_][A-Za-z0-9_ *]*[(][^\n;]*[)] *[{]"
  ULISP_FUNCTIONS
  "${ULISP_SOURCE}"
)
set(ULISP_PROTOTYPES "")
foreach(SIGNATURE IN LISTS ULISP_FUNCTIONS)
  string(REGEX REPLACE "^\n" "" PROTOTYPE "${SIGNATURE}")
  string(REGEX REPLACE "[{]$" ";" PROTOTYPE "${PROTOTYPE}")
  string(APPEND ULISP_PROTOTYPES "${PROTOTYPE}\n")
endforeach()

string(REPLACE
  "// Forward references"
  "// Forward references\n${ULISP_PROTOTYPES}"
  ULISP_GENERATED_SOURCE
  "${ULISP_SOURCE}"
)
string(REPLACE
  "void testescape () {"
  "void testescape () {\n  freewisp_ulisp_safe_point();"
  ULISP_GENERATED_SOURCE
  "${ULISP_GENERATED_SOURCE}"
)
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/ulisp-generated.inc" "${ULISP_GENERATED_SOURCE}")

add_executable(freewisp_freertos_ulisp_task
  main.cpp
  "${FREERTOS_EMSCRIPTEN_PORT}/port.c"
  "${FREERTOS_KERNEL}/list.c"
  "${FREERTOS_KERNEL}/queue.c"
  "${FREERTOS_KERNEL}/tasks.c"
  "${FREERTOS_KERNEL}/portable/MemMang/heap_4.c"
)

target_compile_features(freewisp_freertos_ulisp_task PRIVATE c_std_11 cxx_std_17)
target_compile_options(freewisp_freertos_ulisp_task PRIVATE
  -Wall
  -Wextra
  -Werror
  -Wno-unused-parameter
  -Wno-unused-but-set-variable
)
set_source_files_properties(main.cpp PROPERTIES COMPILE_OPTIONS
  "-Wno-sometimes-uninitialized;-Wno-implicit-const-int-float-conversion"
)
target_include_directories(freewisp_freertos_ulisp_task PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}"
  "${CMAKE_CURRENT_BINARY_DIR}"
  "${CMAKE_CURRENT_SOURCE_DIR}/../standalone-ulisp/compat"
  "${FREERTOS_EMSCRIPTEN_PORT}"
  "${FREERTOS_KERNEL}/include"
)
target_link_options(freewisp_freertos_ulisp_task PRIVATE
  "-sASYNCIFY=1"
  "-sALLOW_MEMORY_GROWTH=1"
  "-sASSERTIONS=1"
)

set_target_properties(freewisp_freertos_ulisp_task PROPERTIES
  OUTPUT_NAME "freewisp-freertos-ulisp-task"
  SUFFIX ".js"
)

configure_file(index.html index.html COPYONLY)

enable_testing()
add_test(NAME freertos_ulisp_task COMMAND freewisp_freertos_ulisp_task)
set_tests_properties(freertos_ulisp_task PROPERTIES
  PASS_REGULAR_EXPRESSION "FREEWISP_ULISP_TASK_PASS"
)
