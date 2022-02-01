#include <thread>

#include "gtest/gtest.h"

#include "twine/twine.h"

using namespace twine;

namespace twine {

}

bool flag = false;

void test_function(RtConditionVariable* cond_var)
{
    flag = cond_var->wait();
}

class RtConditionVariableTest : public ::testing::Test
{
protected:
    RtConditionVariableTest() {}

    void SetUp()
    {
        _module_under_test = RtConditionVariable::create_rt_condition_variable();
        ASSERT_NE(nullptr, _module_under_test);
    }

    std::unique_ptr<RtConditionVariable> _module_under_test;
};

TEST_F(RtConditionVariableTest, FunctionalityTest)
{
    flag = false;
    std::thread thread(test_function, _module_under_test.get());

    ASSERT_FALSE(flag);
    std::this_thread::sleep_for(std::chrono::microseconds(500));

    ASSERT_FALSE(flag);
    _module_under_test->notify();
    std::this_thread::sleep_for(std::chrono::milliseconds(5));

    ASSERT_TRUE(flag);
    thread.join();
}

#ifdef TWINE_BUILD_XENOMAI_TESTS

#endif