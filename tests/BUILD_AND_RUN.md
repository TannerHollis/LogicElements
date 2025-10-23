# Building and Running Logic Elements Tests

Quick guide to building and running the test suite.

---

## 📚 Table of Contents

- [Build Instructions](#build-instructions)
- [Running Tests](#running-tests)
- [Troubleshooting](#troubleshooting)
- [Test Configuration](#test-configuration)

[⬆️ Back to Tests README](README.md) | [⬆️ Main README](../README.md)

---

## Build Instructions

### Method 1: Command Line (Recommended)

```bash
# From project root
cmake -DBUILD_TESTS=ON -S . -B build
cmake --build build
```

### Method 2: Reconfigure Existing Build

```bash
cmake -DBUILD_TESTS=ON build
cmake --build build
```

### Method 3: IDE (Visual Studio / CLion)

1. Open CMakeLists.txt in IDE
2. Set CMake option: `BUILD_TESTS=ON`
3. Reconfigure project
4. Build solution

[⬆️ Back to Top](#building-and-running-logic-elements-tests) | [⬆️ Main README](../README.md)

---

## Running Tests

### Direct Execution

```bash
# Windows
.\build\tests\Logic_Elements_Tests.exe

# Linux/Mac
./build/tests/Logic_Elements_Tests
```

### Using CTest

```bash
cd build
ctest

# Verbose output
ctest --verbose

# Output on failure only
ctest --output-on-failure
```

[⬆️ Back to Top](#building-and-running-logic-elements-tests) | [⬆️ Main README](../README.md)

---

## Troubleshooting

### Tests Not Building

**Problem:** CMake doesn't find test files
```bash
# Solution: Ensure BUILD_TESTS is ON
cmake -DBUILD_TESTS=ON -S . -B build
```

**Problem:** Link errors
```bash
# Solution: Build Logic_Elements library first
cmake --build build --target Logic_Elements
cmake --build build --target Logic_Elements_Tests
```

### Test Failures

**Problem:** Timer test fails
```
Reason: Actual time delays required
Solution: Tests use std::this_thread::sleep_for()
```

### Compilation Issues

**Problem:** Missing le_Engine symbols
```bash
# Solution: Verify Logic_Elements library is linked
# Check tests/CMakeLists.txt:
target_link_libraries(Logic_Elements_Tests PRIVATE Logic_Elements)
```

[⬆️ Back to Top](#building-and-running-logic-elements-tests) | [⬆️ Main README](../README.md)

---

## Test Configuration

### Conditional Tests

Tests automatically adapt to configuration:

```cpp
#ifdef LE_ELEMENTS_ANALOG
    // Analog element tests enabled
#endif

#ifdef LE_ELEMENTS_ANALOG_COMPLEX
    // Complex element tests enabled
#endif

#ifdef LE_ELEMENTS_MATH
    // Math element tests enabled
#endif
```

### CMake Options

```bash
# Build with tests (default: OFF)
cmake -DBUILD_TESTS=ON -B build

# Build without tests
cmake -B build
```

[⬆️ Back to Top](#building-and-running-logic-elements-tests) | [⬆️ Main README](../README.md)

---

## Expected Output

```
========================================
Logic Elements Test Suite
Port-Based Architecture - Factory Tests
ALL Elements Tested Individually
========================================

>>> SIMPLE DIGITAL ELEMENTS <<<
=== le_AND Tests ===
[PASS] le_AND - 2 inputs logic
[PASS] le_AND - 4 inputs logic
[PASS] le_AND - Port names

... (50+ more tests) ...

========================================
Test Summary
========================================
Total Tests: 50+
Passed: 50+
Failed: 0
Success Rate: 100%
========================================
```

[⬆️ Back to Top](#building-and-running-logic-elements-tests) | [⬆️ Tests README](README.md) | [⬆️ Main README](../README.md)

---

**Quick command:** `cmake -DBUILD_TESTS=ON -B build && cmake --build build && ./build/tests/Logic_Elements_Tests`
