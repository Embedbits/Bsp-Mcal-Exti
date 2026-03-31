# Include current directory for provide headers include
set( Exti_IncludeDirs
    ${CMAKE_CURRENT_SOURCE_DIR} 
)


# Set source files for library generation
# All used function files shall be added here.
set( Exti_LibSrcs
    Exti.c )


# Set public header files for the library
# All header files used by other libraries shall be added here.
set( Exti_PublicHeaders 
    Exti_Port.h 
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


# Specify the public headers for the library
set_target_properties( Exti_Lib PROPERTIES PUBLIC_HEADER "${Exti_PublicHeaders}" )


# Include necessary libraries
target_link_libraries( Exti_Lib 
    PUBLIC 
    ${Exti_DependentLibs}
)


target_include_directories( Exti_Lib 
    INTERFACE 
    ${Exti_IncludeDirs}
)


message(STATUS "Exti_Lib library created")

#========================= Documentation configuration ========================#
if(DOXYGEN_AVAILABLE STREQUAL "ON")
    Doxygen_AddPath("${CMAKE_CURRENT_SOURCE_DIR}/")
endif()
