find_package(Qt6 REQUIRED COMPONENTS Test Widgets)

# The model is compiled against the real OBS headers but linked against
# fake_obs.cpp instead of libobs, so the tests run without OBS.
add_executable(stv_item_model_test
		stv_item_model_test.cpp
		fake_obs.cpp
		"${PROJECT_SOURCE_DIR}/obs_scene_tree_view/stv_item_model.cpp"
		"${PROJECT_SOURCE_DIR}/obs_scene_tree_view/stv_item_model.h")

target_include_directories(stv_item_model_test PRIVATE
		"${PROJECT_SOURCE_DIR}"
		$<TARGET_PROPERTY:OBS::libobs,INTERFACE_INCLUDE_DIRECTORIES>
		$<TARGET_PROPERTY:OBS::obs-frontend-api,INTERFACE_INCLUDE_DIRECTORIES>)

target_link_libraries(stv_item_model_test PRIVATE Qt6::Test Qt6::Widgets)

add_test(NAME stv_item_model_test COMMAND stv_item_model_test)
set_tests_properties(stv_item_model_test PROPERTIES
		ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
