# Collect all example source files
file(GLOB EXAMPLE_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/*.cc")

# Add an executable for each example
foreach(EXAMPLE_FILE ${EXAMPLE_SOURCES})
  get_filename_component(EXAMPLE_NAME ${EXAMPLE_FILE} NAME_WE)
  add_executable(${EXAMPLE_NAME} ${EXAMPLE_FILE})
  target_link_libraries(${EXAMPLE_NAME} PRIVATE MaMONAta)

  # Ensure examples always see the flag (keeps TIMING_ENABLED as a configurable option
  # at the top level but forces the compile define for example targets)
  target_compile_definitions(${EXAMPLE_NAME} PRIVATE TIMING_ENABLED=1)
endforeach()

# list all example targets created above, e.g. my_example1 my_example2
set(EXAMPLE_TARGETS
  ${EXAMPLES}
)

add_custom_target(examples DEPENDS ${EXAMPLE_TARGETS})
