set(MTS_PLUGIN_PREFIX "films")

include_directories(
	${OPENEXR_INCLUDE_DIRS}
)

add_plugin(hdrfilm  hdrfilm.cpp)
add_plugin(tiledhdrfilm  tiledhdrfilm.cpp)

target_link_libraries(tiledhdrfilm PRIVATE IlmImf)

# Register the test directory
add_tests(${CMAKE_CURRENT_SOURCE_DIR}/tests)
