#======================== Unit tests configuration ============================#
# Unit tests of Exti module. Registered in module CMakeLists.txt by
# UnitTesting_AddPath() and created by UnitTesting_Generate() in "UnitTest"
# build (see EmBi_Platform/CMake/UnitTesting/README.md).

get_filename_component(EXTI_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../.." ABSOLUTE)
get_filename_component(MCAL_DIR "${EXTI_DIR}/.." ABSOLUTE)

UnitTesting_Add_Test(
    NAME            Exti
    TEST_SOURCE     Test_Exti.c
    UUT_SOURCES     ${EXTI_DIR}/Exti.c
    MOCK_HEADERS    ${MCAL_DIR}/Gpio/Gpio_Port.h
                    ${MCAL_DIR}/Nvic/Nvic_Port.h
    INCLUDE_DIRS    ${EXTI_DIR}
                    ${MCAL_DIR}/Gpio
                    ${MCAL_DIR}/Nvic
    LINK_LIBS       Ral_Lib
)
