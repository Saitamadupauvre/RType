#include <gtest/gtest.h>

#include "platform/Executable.hpp"

TEST(Executable, AppendsThePlatformSuffix) {
#if defined(_WIN32)
    EXPECT_EQ(editor::executable_name("engine-runtime"), "engine-runtime.exe");
#else
    EXPECT_EQ(editor::executable_name("engine-runtime"), "engine-runtime");
#endif
}

TEST(Executable, KeepsAnEmptyBaseName) {
#if defined(_WIN32)
    EXPECT_EQ(editor::executable_name(""), ".exe");
#else
    EXPECT_EQ(editor::executable_name(""), "");
#endif
}
