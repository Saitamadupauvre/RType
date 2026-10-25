#include <gtest/gtest.h>

#include <string>
#include <utility>

#include "engine/core/DynamicLibrary.hpp"

using engine::core::DynamicLibrary;

namespace {

using AnswerFunction = int (*)();

constexpr const char* missing_path = "no-such-directory/no-such-library";

} // namespace

TEST(DynamicLibrary, OpensLibraryAndFindsSymbol) {
    const DynamicLibrary library(SAMPLE_LIBRARY_PATH);

    ASSERT_TRUE(library) << library.error();
    EXPECT_TRUE(library.error().empty());
    const auto answer = library.symbol<AnswerFunction>("sample_answer");
    ASSERT_NE(answer, nullptr);
    EXPECT_EQ(answer(), 42);
}

TEST(DynamicLibrary, ReportsMissingFileWithItsPath) {
    const DynamicLibrary library(missing_path);

    EXPECT_FALSE(library);
    EXPECT_NE(library.error().find(missing_path), std::string::npos) << library.error();
}

TEST(DynamicLibrary, MissingSymbolIsNull) {
    const DynamicLibrary library(SAMPLE_LIBRARY_PATH);

    ASSERT_TRUE(library) << library.error();
    EXPECT_EQ(library.symbol<AnswerFunction>("no_such_symbol"), nullptr);
}

TEST(DynamicLibrary, ClosedLibraryHasNoSymbols) {
    const DynamicLibrary library(missing_path);

    EXPECT_EQ(library.symbol<AnswerFunction>("sample_answer"), nullptr);
}

TEST(DynamicLibrary, MoveConstructionKeepsLibraryOpen) {
    DynamicLibrary source(SAMPLE_LIBRARY_PATH);
    const DynamicLibrary moved(std::move(source));

    ASSERT_TRUE(moved);
    const auto answer = moved.symbol<AnswerFunction>("sample_answer");
    ASSERT_NE(answer, nullptr);
    EXPECT_EQ(answer(), 42);
}

TEST(DynamicLibrary, MoveAssignmentReplacesFailedLibrary) {
    DynamicLibrary target(missing_path);
    DynamicLibrary source(SAMPLE_LIBRARY_PATH);

    target = std::move(source);

    ASSERT_TRUE(target);
    EXPECT_TRUE(target.error().empty());
    EXPECT_NE(target.symbol<AnswerFunction>("sample_answer"), nullptr);
}
