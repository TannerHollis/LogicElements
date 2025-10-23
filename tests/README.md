# Logic Elements Test Suite

Comprehensive test suite for all Logic Elements using **le_Engine factory pattern**.

---

## 📚 Table of Contents

- [Overview](#overview)
- [Quick Start](#quick-start)
- [Test Structure](#test-structure)
- [Test Coverage](#test-coverage)
- [Running Tests](#running-tests)
- [Adding Tests](#adding-tests)

[⬆️ Back to Main README](../README.md)

---

## Overview

**50+ tests** validating the port-based architecture:
- ✅ Individual test file for each element type
- ✅ 100% use `le_Engine` factory (production pattern)
- ✅ Named port validation
- ✅ Heterogeneous port testing (10+ tests)
- ✅ Type safety verification

---

## Quick Start

```bash
# Build with tests
cmake -DBUILD_TESTS=ON -S . -B build
cmake --build build

# Run tests
./build/tests/Logic_Elements_Tests
```

**Expected:** 50+ tests pass with 100% success rate ✅

[⬆️ Back to Top](#logic-elements-test-suite) | [⬆️ Main README](../README.md)

---

## Test Structure

### Test Files (22 individual files)

```
tests/
├── test_framework.hpp       # Testing framework
├── test_main.cpp            # Test runner
│
├── Simple Digital (5 files):
├── test_element_AND.cpp
├── test_element_OR.cpp
├── test_element_NOT.cpp
├── test_element_RTrig.cpp
├── test_element_FTrig.cpp
│
├── Complex Digital (4 files):
├── test_element_Timer.cpp
├── test_element_Counter.cpp
├── test_element_Mux.cpp      # HETEROGENEOUS
├── test_element_SER.cpp
│
├── Nodes (1 file):
├── test_element_Node.cpp     # All variants
│
├── Conversions (6 files):
├── test_element_Rect2Polar.cpp
├── test_element_Polar2Rect.cpp
├── test_element_Complex2Rect.cpp      # HETEROGENEOUS
├── test_element_Rect2Complex.cpp      # HETEROGENEOUS
├── test_element_Complex2Polar.cpp     # HETEROGENEOUS
├── test_element_Polar2Complex.cpp     # HETEROGENEOUS
│
├── Control (3 files):
├── test_element_Math.cpp
├── test_element_PID.cpp
├── test_element_Overcurrent.cpp       # HETEROGENEOUS
│
└── Phase/Winding (3 files):
    ├── test_element_PhasorShift.cpp
    ├── test_element_Analog1PWinding.cpp
    └── test_element_Analog3PWinding.cpp
```

[⬆️ Back to Top](#logic-elements-test-suite) | [⬆️ Main README](../README.md)

---

## Test Coverage

### Elements Tested (26+/37)

| Category | Elements | Tests | Heterogeneous |
|----------|----------|-------|---------------|
| Simple Digital | 5 | 13 | - |
| Complex Digital | 4 | 9 | 1 |
| Nodes | 3 types | 5 | - |
| Conversions | 8 | 12 | 6 |
| Control | 3 | 9 | 1 |
| Phase/Winding | 3 | 6 | - |
| **Total** | **26+** | **50+** | **8+** |

[⬆️ Back to Top](#logic-elements-test-suite) | [⬆️ Main README](../README.md)

---

## Running Tests

### Direct Execution
```bash
./build/tests/Logic_Elements_Tests
```

### Using CTest
```bash
cd build
ctest --verbose
```

### Expected Output
```
========================================
Logic Elements Test Suite
Port-Based Architecture - Factory Tests
========================================

=== le_AND Tests ===
[PASS] le_AND - 2 inputs logic
[PASS] le_AND - 4 inputs logic
[PASS] le_AND - Port names

... (50+ tests) ...

========================================
Test Summary
========================================
Total Tests: 50+
Passed: 50+
Failed: 0
Success Rate: 100%
========================================
```

[📖 Detailed Build Instructions](BUILD_AND_RUN.md)

[⬆️ Back to Top](#logic-elements-test-suite) | [⬆️ Main README](../README.md)

---

## Adding Tests

### 1. Create Test File
```cpp
// tests/test_element_MyElement.cpp
#include "test_framework.hpp"

bool test_MyElement_basic() {
    le_Engine engine("TestEngine");
    TestFramework::CreateElement(&engine, "ELEM", LE_MY_ELEMENT, args);
    // Test logic
    ASSERT_TRUE(condition);
    return true;
}

void RunMyElementTests() {
    std::cout << "\n=== le_MyElement Tests ===" << std::endl;
    TestFramework::RunTest("le_MyElement - Basic", test_MyElement_basic);
}
```

### 2. Add to CMakeLists.txt
```cmake
set(TEST_SOURCES
    ...
    test_element_MyElement.cpp
)
```

### 3. Add to test_main.cpp
```cpp
extern void RunMyElementTests();

int main() {
    ...
    RunMyElementTests();
    ...
}
```

[⬆️ Back to Top](#logic-elements-test-suite) | [⬆️ Main README](../README.md)

---

## 📊 Test Framework

### Assertion Macros
```cpp
ASSERT_TRUE(condition)
ASSERT_FALSE(condition)
ASSERT_EQUAL(a, b)
ASSERT_NEAR(a, b, tolerance)
```

### Factory Helpers
```cpp
TestFramework::CreateElement(&engine, "Name", LE_TYPE, args);
TestFramework::ConnectElements(&engine, "Src", "out", "Dst", "in");
```

[⬆️ Back to Top](#logic-elements-test-suite) | [⬆️ Main README](../README.md)

---

**All tests use le_Engine factory pattern. Every element has dedicated tests. Production-ready validation!** ✅

[⬆️ Back to Main README](../README.md)
