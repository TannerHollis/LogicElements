/**
 * @file le_compile_main.cpp
 * @brief Standalone command-line compiler driver executable for LogicElements.
 */

#include "le_compile_driver.h"

int main(int argc, char* argv[]) {
    return LogicElements::le_compile_driver_main(argc, argv, "le_compile");
}
