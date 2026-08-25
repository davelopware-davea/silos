cmake_minimum_required(VERSION 3.20)

project(silos LANGUAGES C CXX)

find_package(Python3 REQUIRED COMPONENTS Interpreter)

if(NOT EMSCRIPTEN)
  message(FATAL_ERROR "Configure the Browser target through emcmake")
endif()

set(FREERTOS_KERNEL "${CMAKE_CURRENT_SOURCE_DIR}/third-party/FreeRTOS-Kernel")
set(FREERTOS_EMSCRIPTEN_PORT "${CMAKE_CURRENT_SOURCE_DIR}/SilOS/Platform/Browser/FreeRTOSPort")
set(ULISP_COMPAT "${CMAKE_CURRENT_SOURCE_DIR}/SilOS/Platform/Browser/uLispCompat")
set(ULISP_UPSTREAM "${CMAKE_CURRENT_SOURCE_DIR}/third-party/ulisp-esp/ulisp-esp.ino")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${ULISP_UPSTREAM}")

file(GLOB_RECURSE STORE_INIT_FILES CONFIGURE_DEPENDS
  "${CMAKE_CURRENT_SOURCE_DIR}/store-init/*"
)

configure_file(browser-surface.html.in browser-surface.html @ONLY)

# Arduino ordinarily inserts these declarations while preprocessing a sketch.
# This target compiles the upstream sketch as C++, so reproduce only that
# preprocessing step; SilOS runtime implementation lives in ordinary sources.
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
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/ulisp-generated.inc" "${ULISP_GENERATED_SOURCE}")

add_executable(silos_todo_boot
  SilOS/Platform/Browser/main.cpp
  SilOS/Platform/Browser/BrowserSurface.cpp
  SilOS/Platform/Browser/BrowserStoreInitLoader.cpp
  SilOS/FreeRTOS/QueueRuntime.cpp
  SilOS/Runtime/AppBootstrap.cpp
  SilOS/Runtime/EventPump.cpp
  SilOS/Runtime/State.cpp
  SilOS/Store/InMemoryStoreBackend.cpp
  SilOS/UI/Renderer.cpp
  "${FREERTOS_EMSCRIPTEN_PORT}/port.c"
  "${FREERTOS_KERNEL}/list.c"
  "${FREERTOS_KERNEL}/queue.c"
  "${FREERTOS_KERNEL}/tasks.c"
  "${FREERTOS_KERNEL}/portable/MemMang/heap_4.c"
)
target_compile_features(silos_todo_boot PRIVATE c_std_11 cxx_std_17)
target_compile_options(silos_todo_boot PRIVATE
  -Wall -Wextra -Werror
  -Wno-unused-parameter
  -Wno-unused-but-set-variable
)
set_source_files_properties(SilOS/Platform/Browser/main.cpp PROPERTIES COMPILE_OPTIONS
  "-Wno-sometimes-uninitialized;-Wno-implicit-const-int-float-conversion"
)
target_include_directories(silos_todo_boot PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}"
  "${CMAKE_CURRENT_SOURCE_DIR}/SilOS/Platform/Browser"
  "${CMAKE_CURRENT_BINARY_DIR}"
  "${ULISP_COMPAT}"
  "${FREERTOS_EMSCRIPTEN_PORT}"
  "${FREERTOS_KERNEL}/include"
)
target_link_options(silos_todo_boot PRIVATE
  "-sASYNCIFY=1"
  "-sALLOW_MEMORY_GROWTH=1"
  "-sASSERTIONS=1"
  "--preload-file"
  "${CMAKE_CURRENT_SOURCE_DIR}/store-init@/store-init"
)
set_property(TARGET silos_todo_boot APPEND PROPERTY LINK_DEPENDS "${STORE_INIT_FILES}")
set_target_properties(silos_todo_boot PROPERTIES OUTPUT_NAME "silos-todo-boot" SUFFIX ".js")

enable_testing()
add_test(NAME silos_todo_boot COMMAND silos_todo_boot)
set_tests_properties(silos_todo_boot PROPERTIES PASS_REGULAR_EXPRESSION "SILOS_TODO_BOOT_PASS")
add_test(
  NAME silos_alive_api_stubs
  COMMAND
    "${Python3_EXECUTABLE}"
    "${CMAKE_CURRENT_SOURCE_DIR}/check_alive_api_stubs.py"
)
