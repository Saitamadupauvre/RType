#include <gtest/gtest.h>

#include <string>
#include <string_view>

#include "text/TextEdit.hpp"

using editor::Motion;
using editor::TextEdit;

namespace {

constexpr std::string_view e_acute = "\xC3\xA9";
constexpr std::string_view euro = "\xE2\x82\xAC";

float ten_per_byte(std::string_view prefix) { return static_cast<float>(prefix.size()) * 10.F; }

} // namespace

TEST(TextEdit, StartsWithTheCaretAtTheEnd) {
    const TextEdit edit("abc");
    EXPECT_EQ(edit.caret(), 3U);
    EXPECT_FALSE(edit.has_selection());
}

TEST(TextEdit, InsertsAtTheCaret) {
    TextEdit edit("ac");
    edit.move(Motion::left, false);
    edit.insert("b");
    EXPECT_EQ(edit.text(), "abc");
    EXPECT_EQ(edit.caret(), 2U);
}

TEST(TextEdit, InsertReplacesTheSelection) {
    TextEdit edit("hello world");
    edit.move(Motion::word_left, true);
    edit.insert("there");
    EXPECT_EQ(edit.text(), "hello there");
    EXPECT_FALSE(edit.has_selection());
}

TEST(TextEdit, PasteOverSelectAllReplacesEverything) {
    TextEdit edit("old");
    edit.select_all();
    edit.insert("new text");
    EXPECT_EQ(edit.text(), "new text");
    EXPECT_EQ(edit.caret(), 8U);
}

TEST(TextEdit, EraseBackwardRemovesTheCharacterBeforeTheCaret) {
    TextEdit edit("abc");
    edit.erase_backward();
    EXPECT_EQ(edit.text(), "ab");
}

TEST(TextEdit, EraseBackwardAtTheStartDoesNothing) {
    TextEdit edit("abc");
    edit.move(Motion::home, false);
    edit.erase_backward();
    EXPECT_EQ(edit.text(), "abc");
    EXPECT_EQ(edit.caret(), 0U);
}

TEST(TextEdit, EraseForwardRemovesTheCharacterAfterTheCaret) {
    TextEdit edit("abc");
    edit.move(Motion::home, false);
    edit.erase_forward();
    EXPECT_EQ(edit.text(), "bc");
}

TEST(TextEdit, EraseForwardAtTheEndDoesNothing) {
    TextEdit edit("abc");
    edit.erase_forward();
    EXPECT_EQ(edit.text(), "abc");
}

TEST(TextEdit, EraseRemovesTheSelectionOnly) {
    TextEdit edit("abcd");
    edit.move(Motion::left, false);
    edit.move(Motion::left, true);
    edit.erase_forward();
    EXPECT_EQ(edit.text(), "abd");
    EXPECT_EQ(edit.caret(), 2U);
}

TEST(TextEdit, EraseBackwardRemovesAWholeMultiByteCharacter) {
    TextEdit edit(std::string("a") + std::string(euro));
    edit.erase_backward();
    EXPECT_EQ(edit.text(), "a");
}

TEST(TextEdit, EraseForwardRemovesAWholeMultiByteCharacter) {
    TextEdit edit(std::string(e_acute) + "b");
    edit.move(Motion::home, false);
    edit.erase_forward();
    EXPECT_EQ(edit.text(), "b");
}

TEST(TextEdit, LeftAndRightStepOverMultiByteCharacters) {
    TextEdit edit(std::string("a") + std::string(euro) + "b");
    edit.move(Motion::left, false);
    EXPECT_EQ(edit.caret(), 4U);
    edit.move(Motion::left, false);
    EXPECT_EQ(edit.caret(), 1U);
    edit.move(Motion::right, false);
    EXPECT_EQ(edit.caret(), 4U);
}

TEST(TextEdit, LeftStopsAtTheStartAndRightAtTheEnd) {
    TextEdit edit("a");
    edit.move(Motion::right, false);
    EXPECT_EQ(edit.caret(), 1U);
    edit.move(Motion::home, false);
    edit.move(Motion::left, false);
    EXPECT_EQ(edit.caret(), 0U);
}

TEST(TextEdit, HomeAndEndJumpToTheEdges) {
    TextEdit edit("abc");
    edit.move(Motion::home, false);
    EXPECT_EQ(edit.caret(), 0U);
    edit.move(Motion::end, false);
    EXPECT_EQ(edit.caret(), 3U);
}

TEST(TextEdit, WordLeftSkipsSeparatorsThenTheWord) {
    TextEdit edit("one two  ");
    edit.move(Motion::word_left, false);
    EXPECT_EQ(edit.caret(), 4U);
    edit.move(Motion::word_left, false);
    EXPECT_EQ(edit.caret(), 0U);
}

TEST(TextEdit, WordRightSkipsSeparatorsThenTheWord) {
    TextEdit edit("  one_1 two");
    edit.move(Motion::home, false);
    edit.move(Motion::word_right, false);
    EXPECT_EQ(edit.caret(), 7U);
    edit.move(Motion::word_right, false);
    EXPECT_EQ(edit.caret(), 11U);
}

TEST(TextEdit, WordMotionsTreatMultiByteLettersAsWordCharacters) {
    TextEdit edit(std::string("caf") + std::string(e_acute) + " x");
    edit.move(Motion::home, false);
    edit.move(Motion::word_right, false);
    EXPECT_EQ(edit.caret(), 5U);
}

TEST(TextEdit, ShiftExtendsTheSelectionFromTheAnchor) {
    TextEdit edit("abc");
    edit.move(Motion::left, true);
    edit.move(Motion::left, true);
    EXPECT_EQ(edit.anchor(), 3U);
    EXPECT_EQ(edit.selection(), "bc");
}

TEST(TextEdit, LeftCollapsesASelectionToItsStart) {
    TextEdit edit("abcd");
    edit.move(Motion::left, true);
    edit.move(Motion::left, true);
    edit.move(Motion::left, false);
    EXPECT_EQ(edit.caret(), 2U);
    EXPECT_FALSE(edit.has_selection());
}

TEST(TextEdit, RightCollapsesASelectionToItsEnd) {
    TextEdit edit("abcd");
    edit.move(Motion::home, false);
    edit.move(Motion::right, true);
    edit.move(Motion::right, true);
    edit.move(Motion::right, false);
    EXPECT_EQ(edit.caret(), 2U);
    EXPECT_FALSE(edit.has_selection());
}

TEST(TextEdit, SelectAllSelectsTheWholeText) {
    TextEdit edit("abc");
    edit.select_all();
    EXPECT_EQ(edit.selection(), "abc");
}

TEST(TextEdit, PlaceCaretSnapsInsideAMultiByteCharacterToItsStart) {
    TextEdit edit(std::string("a") + std::string(euro));
    edit.place_caret(2, false);
    EXPECT_EQ(edit.caret(), 1U);
}

TEST(TextEdit, PlaceCaretClampsPastTheEnd) {
    TextEdit edit("ab");
    edit.place_caret(99, false);
    EXPECT_EQ(edit.caret(), 2U);
}

TEST(TextEdit, PlaceCaretWithExtendKeepsTheAnchor) {
    TextEdit edit("abcd");
    edit.place_caret(1, false);
    edit.place_caret(3, true);
    EXPECT_EQ(edit.selection(), "bc");
}

TEST(TextEdit, EraseWordBackwardRemovesTheWordBeforeTheCaret) {
    TextEdit edit("one two");
    edit.erase_word_backward();
    EXPECT_EQ(edit.text(), "one ");
}

TEST(TextEdit, EraseWordBackwardAlsoRemovesTheSeparatorsBeforeIt) {
    TextEdit edit("one two  ");
    edit.erase_word_backward();
    EXPECT_EQ(edit.text(), "one ");
}

TEST(TextEdit, EraseWordForwardRemovesTheWordAfterTheCaret) {
    TextEdit edit("one two");
    edit.move(Motion::home, false);
    edit.erase_word_forward();
    EXPECT_EQ(edit.text(), " two");
}

TEST(TextEdit, EraseWordRemovesTheSelectionOnly) {
    TextEdit edit("one two");
    edit.move(Motion::left, true);
    edit.erase_word_backward();
    EXPECT_EQ(edit.text(), "one tw");
}

TEST(TextEdit, SelectWordSelectsTheWordAroundTheCaret) {
    TextEdit edit("one two three");
    edit.place_caret(5, false);
    edit.select_word();
    EXPECT_EQ(edit.selection(), "two");
}

TEST(TextEdit, SelectWordAtAWordEndSelectsThatWord) {
    TextEdit edit("one two");
    edit.place_caret(3, false);
    edit.select_word();
    EXPECT_EQ(edit.selection(), "one");
}

TEST(TextEdit, SelectWordBetweenSeparatorsSelectsOneCharacter) {
    TextEdit edit("a  b");
    edit.place_caret(2, false);
    edit.select_word();
    EXPECT_EQ(edit.selection(), " ");
}

TEST(TextEdit, SelectWordKeepsMultiByteLettersInTheWord) {
    TextEdit edit(std::string("caf") + std::string(e_acute) + " x");
    edit.place_caret(1, false);
    edit.select_word();
    EXPECT_EQ(edit.selection(), std::string("caf") + std::string(e_acute));
}

TEST(CaretAt, PicksTheNearestBoundary) {
    EXPECT_EQ(editor::caret_at("abcd", 14.F, ten_per_byte), 1U);
    EXPECT_EQ(editor::caret_at("abcd", 16.F, ten_per_byte), 2U);
}

TEST(CaretAt, ClampsLeftOfTheTextToTheStart) {
    EXPECT_EQ(editor::caret_at("abcd", -50.F, ten_per_byte), 0U);
}

TEST(CaretAt, ClampsRightOfTheTextToTheEnd) {
    EXPECT_EQ(editor::caret_at("abcd", 500.F, ten_per_byte), 4U);
}

TEST(CaretAt, ReturnsZeroForAnEmptyText) {
    EXPECT_EQ(editor::caret_at("", 30.F, ten_per_byte), 0U);
}

TEST(CaretAt, NeverLandsInsideAMultiByteCharacter) {
    const std::string text = std::string("a") + std::string(euro);
    EXPECT_EQ(editor::caret_at(text, 22.F, ten_per_byte), 1U);
    EXPECT_EQ(editor::caret_at(text, 35.F, ten_per_byte), 4U);
}
