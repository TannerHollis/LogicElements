# Logic Elements Configuration Guide

How to configure Logic Elements features at build time.

---

## 📚 Table of Contents

- [CMake Options](#cmake-options)
- [Available Options](#available-options)
- [Build Examples](#build-examples)

[⬆️ Back to Main README](README.md)

---

## CMake Options

### Basic Usage

```bash
# Enable all features (default)
cmake -S . -B build

# Disable specific features
cmake -DLE_ELEMENTS_MATH=OFF -DLE_DNP3=OFF -S . -B build

# Minimal build (digital only)
cmake -DLE_ELEMENTS_ANALOG=OFF -DLE_DNP3=OFF -S . -B build

# Build
cmake --build build
```

### How It Works

1. **CMake options** defined in `src/LogicElements/CMakeLists.txt`
2. **Template file** `le_Config.hpp.in` has `#cmakedefine` directives
3. **CMake generates** `le_Config.hpp` in build directory
4. **No source editing** required!

### Advantages

✅ **No source file editing**  
✅ **Build-time configuration**  
✅ **Easy build variants**  
✅ **Version control friendly**  
✅ **IDE integration**  
✅ **Validation possible**  

[⬆️ Back to Top](#logic-elements-configuration-guide) | [⬆️ Main README](README.md)

---


## Available Options

### Feature Flags

| Option | Default | Description | Dependencies |
|--------|---------|-------------|--------------|
| `LE_ELEMENT_TEST_MODE` | ON | Allow direct element instantiation | - |
| `LE_ENGINE_EXECUTION_DIAG` | ON | Enable execution diagnostics | - |
| `LE_ELEMENTS_ANALOG` | ON | Enable analog elements | - |
| `LE_ELEMENTS_ANALOG_COMPLEX` | ON | Enable complex number support | ANALOG |
| `LE_ELEMENTS_PID` | ON | Enable PID controller | ANALOG |
| `LE_ELEMENTS_MATH` | ON | Enable Math expression evaluator | ANALOG |
| `LE_DNP3` | ON | Enable DNP3 protocol support | - |

### CMake Configuration

```bash
# Full-featured build (default)
cmake -S . -B build

# Minimal digital-only build
cmake -DLE_ELEMENTS_ANALOG=OFF -DLE_DNP3=OFF -S . -B build

# Analog without complex
cmake -DLE_ELEMENTS_ANALOG_COMPLEX=OFF -S . -B build

# Disable specific features
cmake -DLE_ELEMENTS_MATH=OFF -DLE_ELEMENTS_PID=OFF -S . -B build
```

[⬆️ Back to Top](#logic-elements-configuration-guide) | [⬆️ Main README](README.md)

---

## Build Examples

### Example 1: Minimal Digital Build

```bash
cmake \
  -DLE_ELEMENTS_ANALOG=OFF \
  -DLE_DNP3=OFF \
  -S . -B build/minimal

cmake --build build/minimal
```

**Includes only:**
- AND, OR, NOT, RTrig, FTrig
- Timer, Counter, SER, Mux
- Node_Digital

### Example 2: Full Analog + Control

```bash
cmake \
  -DLE_ELEMENTS_ANALOG=ON \
  -DLE_ELEMENTS_MATH=ON \
  -DLE_ELEMENTS_PID=ON \
  -S . -B build/control

cmake --build build/control
```

**Includes everything** for control systems.

### Example 3: Testing Different Configs

```bash
# Config 1: Digital only
cmake -DLE_ELEMENTS_ANALOG=OFF -S . -B build/digital
cmake --build build/digital

# Config 2: Full features
cmake -DLE_ELEMENTS_ANALOG=ON -S . -B build/full
cmake --build build/full

# Both builds coexist!
```

[⬆️ Back to Top](#logic-elements-configuration-guide) | [⬆️ Main README](README.md)

---

## 🎯 **Recommendation**

### **Use CMake-Generated Config** ✅

**Benefits for your project:**

1. **Multiple Build Configurations:** Easy to test digital-only, analog-only, etc.
2. **CI/CD Friendly:** Automated builds can configure via command line
3. **No Source Editing:** Keep `le_Config.hpp.in` in version control
4. **Build Cache:** CMake knows when to rebuild
5. **Professional:** Industry standard approach

### **Migration Steps**

1. ✅ **Template created:** `le_Config.hpp.in` (done above)
2. ✅ **CMake updated:** Options and configure_file (done above)
3. ⚠️ **Keep current le_Config.hpp** as fallback for now
4. ✅ **Test generated config** works correctly
5. ⚠️ **Then remove** manual `le_Config.hpp` (optional)

### **Your Current Approach**

**Verdict:** 👍 **Acceptable for now, but consider upgrading**

- ✅ Works correctly
- ✅ Simple for single developer
- ⚠️ Not ideal for team/CI
- ⚠️ Harder to maintain build variants

**Recommendation:** Consider migrating to CMake options when convenient. I've created the template files above to make migration easy!

[⬆️ Back to Top](#logic-elements-configuration-guide) | [⬆️ Main README](README.md)

---

## Summary

| Approach | Current | Recommended |
|----------|---------|-------------|
| **Method** | Manual #define | CMake options |
| **Ease of Use** | Simple | Very easy |
| **Team Friendly** | ⚠️ No | ✅ Yes |
| **CI/CD** | ⚠️ No | ✅ Yes |
| **Build Variants** | ⚠️ Hard | ✅ Easy |
| **Industry Standard** | ⚠️ No | ✅ Yes |

**Your current approach works, but CMake options would be better for scalability.**

[⬆️ Back to Main README](README.md)

