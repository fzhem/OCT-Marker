cmake_minimum_required(VERSION 3.1.0 FATAL_ERROR)
project(octmarker DESCRIPTION "A platform independent open source tool for open OCT images and create different labels on it")
list(APPEND CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake_module/)

set( CMAKE_CXX_STANDARD 17 )


find_package(Boost 1.40 COMPONENTS serialization system iostreams REQUIRED)
find_package(OpenCV REQUIRED)
find_package(LibOctData 1 CONFIG REQUIRED)
find_package(OctCppFramework REQUIRED)


option(BUILD_WITH_SEGMENTATION_ML    "build with support for NN"     OFF)
option(BUILD_MATLAB_MEX_FUNCTIONS    "build matlab mex functions"    OFF)
option(BUILD_OCTAVE_MEX_FUNCTIONS    "build octave mex functions"    OFF)
option(BUILD_QT_PROGRAMM             "build main programm"           ON )
option(BUILD_MEX_WITH_STATIC_CPP_LIB "build mex with static c++ lib" OFF)
option(CREATE_DOCUMENTATION          "create documentation with doxygen" OFF)


IF(CREATE_DOCUMENTATION)
	find_package(Doxygen REQUIRED dot OPTIONAL_COMPONENTS mscgen dia)
	IF(CMAKE_VERSION VERSION_GREATER 3.9)
		set(DOXYGEN_GENERATE_HTML  YES)
		set(DOXYGEN_GENERATE_MAN   NO )
		set(DOXYGEN_GENERATE_LATEX YES)

		set(DOXYGEN_CALL_GRAPH YES)
		set(DOXYGEN_CALLER_GRAPH YES)
		set(DOXYGEN_EXTRACT_ALL YES)
		set(DOXYGEN_EXCLUDE_PATTERNS moc_* ui_* */build/* */release/* *.m)

		set(DOXYGEN_DOT_IMAGE_FORMAT svg)
		set(DOXYGEN_INTERACTIVE_SVG  YES)

		set(DOXYGEN_PROJECT_NAME "OCT-Marker")

		doxygen_add_docs(
		    doxygen
		    ${PROJECT_SOURCE_DIR}/src/
		    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/src/
		    COMMENT "Generate man pages"
		)
	ELSE()
		message(WARNING "CMAKE >= 3.9 is need for automatic doxygen generation")
	ENDIF()
ENDIF()


if(BUILD_QT_PROGRAMM)

	find_package(Qt5 COMPONENTS Core Widgets LinguistTools Svg REQUIRED)

	include(GetGitRevisionDescription)
	get_git_head_revision(GIT_REFSPEC GIT_SHA1)
	git_author_date(GIT_DATE)

	configure_file("${CMAKE_CURRENT_SOURCE_DIR}/src/buildconstants.cpp.in" "${CMAKE_CURRENT_BINARY_DIR}/buildconstants.cpp" @ONLY)
	list(APPEND octmarker_SRCS "${CMAKE_CURRENT_BINARY_DIR}/buildconstants.cpp")


	set(CMAKE_AUTOMOC ON)
	set(CMAKE_AUTOUIC ON)


	include_directories(${CMAKE_SOURCE_DIR}/src/)

	if(CMAKE_COMPILER_IS_GNUCXX)
		string(APPEND CMAKE_CXX_FLAGS " -Wall -Wextra -Wunreachable-code -Wconversion -Wold-style-cast")        ## Optimize -O3 -Weverything -Weffc++
		if(WIN32)
			set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS} -mwindows")
		endif()
	endif()


	if(BUILD_WITH_SEGMENTATION_ML)
		find_package(FANN REQUIRED)
		add_definitions(-DML_SUPPORT)
		include_directories(SYSTEM ${FANN_INCLUDE_DIR})
	endif()


	set(srcs_directories data_structure algos widgets windows manager markermodules model markerobjects helper imagefilter qt)

	foreach(loop_var ${srcs_directories})
		file(GLOB_RECURSE sources_base "${CMAKE_CURRENT_SOURCE_DIR}/src/${loop_var}/*.cpp")
		list(APPEND octmarker_SRCS ${sources} ${sources_base})
	endforeach(loop_var)

	file(GLOB ui_forms ${CMAKE_CURRENT_SOURCE_DIR}/ui/*.ui)
	list(APPEND octmarker_SRCS ${ui_forms})


	file(GLOB LANG ${CMAKE_CURRENT_SOURCE_DIR}/language_files/*.ts)

	set(octmarker_RESOURCES application.qrc)


	QT5_ADD_RESOURCES(octmarker_RESOURCES_RCC ${octmarker_RESOURCES})
	QT5_ADD_TRANSLATION(TRANSLATIONS ${LANG})


	add_executable(octmarker WIN32 src/main.cpp src/prepareprogrammoptions.cpp ${octmarker_SRCS}
		${TRANSLATIONS}
		${octmarker_RESOURCES_RCC} octmarker.rc)


	if(CMAKE_BUILD_TYPE STREQUAL "Release")
		set_property(TARGET octmarker PROPERTY WIN32_EXECUTABLE true)
	endif()

	target_link_libraries(octmarker Qt5::Core Qt5::Widgets ${CMAKE_THREAD_LIBS_INIT})
	target_link_libraries(octmarker opencv_core)
	target_link_libraries(octmarker Boost::serialization Boost::system Boost::iostreams)
	target_link_libraries(octmarker LibOctData::octdata)
	target_link_libraries(octmarker OctCppFramework::oct_cpp_framework)
	target_link_libraries(octmarker ${FANN_LIBRARY})

	install(TARGETS octmarker RUNTIME DESTINATION bin)

endif()


if(BUILD_MATLAB_MEX_FUNCTIONS)
	find_package(Matlab COMPONENTS MX_LIBRARY REQUIRED)

	matlab_add_mex(NAME read_seg SRC src_matlab/read_seg.cpp src/manager/octmarkerio.cpp src/data_structure/simplematcompress.cpp LINK_TO ${Boost_LIBRARIES})
	set_target_properties(read_seg PROPERTIES COMPILE_DEFINITIONS "MEX_COMPILE")
	if(BUILD_MEX_WITH_STATIC_CPP_LIB)
		set_target_properties(read_seg PROPERTIES LINK_FLAGS "-static-libstdc++")
	endif()

	target_include_directories(read_seg SYSTEM PRIVATE ${Matlab_INCLUDE_DIRS} PUBLIC ${CMAKE_SOURCE_DIR}/src/)
	target_link_libraries(read_seg OctCppFramework::oct_cpp_framework)
endif()


if(BUILD_OCTAVE_MEX_FUNCTIONS)
	find_package(Octave COMPONENTS MX_LIBRARY REQUIRED)

	octave_add_oct(oct_read_seg SOURCES src_matlab/read_seg.cpp src/manager/octmarkerio.cpp src/data_structure/simplematcompress.cpp LINK_LIBRARIES ${Boost_LIBRARIES} EXTENSION mex)
	set_target_properties(oct_read_seg PROPERTIES COMPILE_DEFINITIONS "MEX_COMPILE")
# 	if(BUILD_MEX_WITH_STATIC_CPP_LIB)
# 		set_target_properties(oct_read_seg PROPERTIES LINK_FLAGS "-static-libstdc++")
# 	endif()
	target_include_directories(oct_read_seg SYSTEM PRIVATE ${OCTAVE_INCLUDE_DIRS} PUBLIC ${CMAKE_SOURCE_DIR}/src/)
	target_link_libraries(oct_read_seg OctCppFramework::oct_cpp_framework)

endif()

