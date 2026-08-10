cmake_minimum_required(VERSION 3.20)

project(freewisp_standalone_ulisp LANGUAGES CXX)

if(NOT EMSCRIPTEN)
  message(FATAL_ERROR "Configure this target through emcmake")
endif()

# Arduino normally inserts forward declarations while preprocessing an .ino
# sketch. Generate the equivalent declarations without changing the pinned
# upstream snapshot, then inject them at uLisp's forward-reference marker.
set(ULISP_UPSTREAM "${CMAKE_CURRENT_SOURCE_DIR}/../third-party/ulisp-esp/ulisp-esp.ino")
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

add_executable(freewisp_ulisp main.cpp)
target_compile_features(freewisp_ulisp PRIVATE cxx_std_17)
target_include_directories(freewisp_ulisp PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/compat
  ${CMAKE_CURRENT_BINARY_DIR}
)
target_link_options(freewisp_ulisp PRIVATE
  "-sALLOW_MEMORY_GROWTH=1"
  "-sEXPORTED_RUNTIME_METHODS=['ccall']"
)

configure_file(index.html index.html COPYONLY)

set_target_properties(freewisp_ulisp PROPERTIES
  OUTPUT_NAME "freewisp-ulisp"
  SUFFIX ".js"
)

enable_testing()
add_test(NAME standalone_ulisp_arithmetic COMMAND freewisp_ulisp "(+ 20 22)")
set_tests_properties(standalone_ulisp_arithmetic PROPERTIES
  PASS_REGULAR_EXPRESSION "uLisp 4.9a.*42"
)
add_test(NAME standalone_ulisp_lists COMMAND freewisp_ulisp "(mapcar (lambda (x) (* x x)) '(1 2 3 4))")
set_tests_properties(standalone_ulisp_lists PROPERTIES
  PASS_REGULAR_EXPRESSION "\\(1 4 9 16\\)"
)
