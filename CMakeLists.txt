# Template version: 1.0.0
# Sets the version of CMakeLists.txt file.
# Never, under any circumstances, change this line. It is used by
# the module generation script to identify the template version.

#======================== Library configuration ===============================#
# Publicly included directories to provide headers include
set( Exti_PublicIncludeDirs
    
)


# Privately included directories to provide headers include
set( Exti_PrivateIncludeDirs
    ${CMAKE_CURRENT_SOURCE_DIR}
)


# Source files list for current library generation
set( Exti_SourceFiles
    Exti.c
)


# List of public header files provided by the current library
set( Exti_PublicHeaders 
    Exti_Port.h
    Exti_Types.h
)


# List of dependent libraries that are publicly used by the current library
set( Exti_PublicDependentLibs
    Ral_Lib
    Nvic_Lib
    Gpio_Lib
)


# List of dependent libraries that are privately used by the current library
set( Exti_PrivateDependentLibs
    
)


#========================== Library generation ================================#
# Create library
add_library(Exti_Lib STATIC ${Exti_SourceFiles})


# Include necessary libraries to the target
target_link_libraries( Exti_Lib
    PUBLIC 
        ${Exti_PublicDependentLibs} 
    PRIVATE 
        ${Exti_PrivateDependentLibs}
)


# Set public headers directory path in build folder
set(PUBLIC_HEADERS_DIR ${CMAKE_CURRENT_BINARY_DIR}/PublicHeaders)

# Generates the public headers directory in build folder
file(MAKE_DIRECTORY ${PUBLIC_HEADERS_DIR})

# Process all public headers
foreach(PublicHeader IN LISTS Exti_PublicHeaders)
    
    # Copy (or configure) only the public header there
    configure_file(
        ${CMAKE_CURRENT_SOURCE_DIR}/${PublicHeader} 
        ${PUBLIC_HEADERS_DIR}/${PublicHeader}
        COPYONLY
    )
    
endforeach()


# Set include directories for the created target
target_include_directories( Exti_Lib
    PUBLIC
        $<BUILD_INTERFACE:${PUBLIC_HEADERS_DIR}>
        $<INSTALL_INTERFACE:include>
        ${Exti_PublicIncludeDirs}
    PRIVATE
        ${Exti_PrivateIncludeDirs}
)


# Print status message
message(STATUS "Exti_Lib library created")

#========================= Documentation configuration ========================#
if(DOXYGEN_AVAILABLE STREQUAL "ON")
    Doxygen_AddPath("${CMAKE_CURRENT_SOURCE_DIR}/")
endif()

#========================= Unit tests configuration ===========================#
if(UNIT_TESTING_AVAILABLE STREQUAL "ON")
    UnitTesting_AddPath("${CMAKE_CURRENT_SOURCE_DIR}/Tests/UnitTests")
endif()

#===================== Integration tests configuration ========================#
if(INTEGRATION_TESTING_AVAILABLE STREQUAL "ON")
    IntegrationTesting_AddPath("${CMAKE_CURRENT_SOURCE_DIR}/Tests/IntegrationTests")
endif()
