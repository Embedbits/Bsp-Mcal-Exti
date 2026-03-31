# Set source files for library generation
# All used function files shall be added here.
set( Exti_LibSrcs
    Exti.c 
)


# Set public header files for the library
# All header files used by other libraries shall be added here.
set( Exti_PublicHeaders 
    Exti_Port.h 
    Exti_Types.h
)


# Set dependent libraries
# All libraries used by current target shall be added here.
set( Exti_DependentLibs
    Ral_Lib
    Nvic_Lib
    Gpio_Lib
)


# Create library
add_library(Exti_Lib STATIC ${Exti_LibSrcs})

# Include necessary libraries
target_link_libraries( Exti_Lib 
    PRIVATE 
    ${Exti_DependentLibs}
)


# Directory for public headers (build interface)
set(PUBLIC_HEADERS_DIR ${CMAKE_CURRENT_BINARY_DIR}/PublicHeaders)

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

target_include_directories( Exti_Lib 
    PUBLIC
        $<BUILD_INTERFACE:${PUBLIC_HEADERS_DIR}>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${Rcc_IncludeDirs}
)


message(STATUS "Exti_Lib library created")

#========================= Documentation configuration ========================#
if(DOXYGEN_AVAILABLE STREQUAL "ON")
    Doxygen_AddPath("${CMAKE_CURRENT_SOURCE_DIR}/")
endif()
