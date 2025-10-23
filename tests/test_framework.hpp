#pragma once

#include <iostream>
#include <string>
#include <vector>
#include <functional>
#include "le_Engine.hpp"

using namespace LogicElements;

/**
 * @brief Enhanced test framework for Logic Elements using Engine factory
 */
class TestFramework
{
public:
    struct TestResult
    {
        std::string testName;
        bool passed;
        std::string message;
    };

    static void RunTest(const std::string& testName, std::function<bool()> testFunc)
    {
        try
        {
            bool result = testFunc();
            results.push_back({testName, result, result ? "PASSED" : "FAILED"});
            
            if (result)
                std::cout << "[PASS] " << testName << std::endl;
            else
                std::cout << "[FAIL] " << testName << std::endl;
        }
        catch (const std::exception& e)
        {
            results.push_back({testName, false, std::string("EXCEPTION: ") + e.what()});
            std::cout << "[EXCEPTION] " << testName << ": " << e.what() << std::endl;
        }
    }

    static void PrintSummary()
    {
        int passed = 0;
        int failed = 0;

        for (const auto& result : results)
        {
            if (result.passed)
                passed++;
            else
                failed++;
        }

        std::cout << "\n========================================" << std::endl;
        std::cout << "Test Summary" << std::endl;
        std::cout << "========================================" << std::endl;
        std::cout << "Total Tests: " << (passed + failed) << std::endl;
        std::cout << "Passed: " << passed << std::endl;
        std::cout << "Failed: " << failed << std::endl;
        std::cout << "Success Rate: " << (100.0 * passed / (passed + failed)) << "%" << std::endl;
        std::cout << "========================================" << std::endl;
    }

    static bool AllPassed()
    {
        for (const auto& result : results)
        {
            if (!result.passed)
                return false;
        }
        return true;
    }

    static void ClearResults()
    {
        results.clear();
    }

    /**
     * @brief Helper to create an element using Engine factory (no args)
     */
    static Element* CreateElement(Engine* engine, const std::string& name, ElementType type)
    {
        Engine::ElementTypeDef comp(name, type);
        return engine->AddElement(&comp);
    }

    /**
     * @brief Helper to create an element using Engine factory (single integer arg)
     */
    static Element* CreateElement(Engine* engine, const std::string& name, ElementType type, 
                                      uint16_t arg0)
    {
        Engine::ElementTypeDef comp(name, type);
        comp.args[0].uArg = arg0;
        return engine->AddElement(&comp);
    }

    /**
     * @brief Helper to create an element using Engine factory (two integer args)
     */
    static Element* CreateElement(Engine* engine, const std::string& name, ElementType type, 
                                      uint16_t arg0, uint16_t arg1)
    {
        Engine::ElementTypeDef comp(name, type);
        comp.args[0].uArg = arg0;
        comp.args[1].uArg = arg1;
        return engine->AddElement(&comp);
    }

    /**
     * @brief Helper to create an element using Engine factory (all float args)
     */
    static Element* CreateElement(Engine* engine, const std::string& name, ElementType type,
                                      float arg0, float arg1, float arg2, float arg3, float arg4)
    {
        Engine::ElementTypeDef comp(name, type);
        comp.args[0].fArg = arg0;
        comp.args[1].fArg = arg1;
        comp.args[2].fArg = arg2;
        comp.args[3].fArg = arg3;
        comp.args[4].fArg = arg4;
        return engine->AddElement(&comp);
    }

    /**
     * @brief Helper to create an element with string argument
     */
    static Element* CreateElementWithString(Engine* engine, const std::string& name, 
                                                ElementType type, const std::string& strArg,
                                                float arg1 = 0.0f, float arg2 = 0.0f, 
                                                float arg3 = 0.0f, float arg4 = 0.0f)
    {
        Engine::ElementTypeDef comp(name, type);
        Engine::CopyAndClampString(strArg, comp.args[0].sArg, LE_ELEMENT_ARGUMENT_LENGTH);
        comp.args[1].fArg = arg1;
        comp.args[2].fArg = arg2;
        comp.args[3].fArg = arg3;
        comp.args[4].fArg = arg4;
        return engine->AddElement(&comp);
    }

    /**
     * @brief Helper to connect elements using port names
     */
    static bool ConnectElements(Engine* engine, const std::string& outputName, const std::string& outputPort,
                                 const std::string& inputName, const std::string& inputPort)
    {
        Engine::ElementNetTypeDef net(outputName, outputPort);
        net.AddInput(inputName, inputPort);
        engine->AddNet(&net);
        return true;
    }

private:
    // Use inline static to avoid multiple definition errors (C++17)
    inline static std::vector<TestResult> results;
};

// Macros for easier test writing
#define ASSERT_TRUE(condition) if (!(condition)) { std::cout << "  ASSERT_TRUE failed at line " << __LINE__ << std::endl; return false; }
#define ASSERT_FALSE(condition) if (condition) { std::cout << "  ASSERT_FALSE failed at line " << __LINE__ << std::endl; return false; }
#define ASSERT_EQUAL(a, b) if ((a) != (b)) { std::cout << "  ASSERT_EQUAL failed at line " << __LINE__ << std::endl; return false; }
#define ASSERT_NEAR(a, b, tolerance) if (std::abs((a) - (b)) > (tolerance)) { std::cout << "  ASSERT_NEAR failed at line " << __LINE__ << ": " << a << " vs " << b << std::endl; return false; }
