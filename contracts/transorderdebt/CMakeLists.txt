add_contract(transorderdebt transorderdebt ${CMAKE_CURRENT_SOURCE_DIR}/src/transorderdebt.cpp)

target_include_directories(transorderdebt
   PUBLIC
   ${CMAKE_CURRENT_SOURCE_DIR}/include)

set_target_properties(transorderdebt
   PROPERTIES
   RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")


target_compile_options( transorderdebt PUBLIC)