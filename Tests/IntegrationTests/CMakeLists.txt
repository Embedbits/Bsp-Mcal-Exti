#===================== Integration tests configuration ========================#
# Integration tests of Exti module on target (Nucleo board). Registered in module
# CMakeLists.txt by IntegrationTesting_AddPath() and created by
# IntegrationTesting_Generate() in "IntegrationTest" build
# (see EmBi_Platform/CMake/IntegrationTesting/README.md).

IntegrationTesting_Add_Test(
    NAME            Exti
    TEST_SOURCE     ItTest_Exti.c
    LINK_LIBS       Exti_Lib            # incl. Gpio_Lib - pin level control
    TIMEOUT         60
)
