// Copyright (c) 2026 Zhuyin-input-method contributors.
//
// Permission is hereby granted, free of charge, to any person
// obtaining a copy of this software and associated documentation
// files (the "Software"), to deal in the Software without
// restriction, including without limitation the rights to use,
// copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the
// Software is furnished to do so, subject to the following
// conditions:
//
// The above copyright notice and this permission notice shall be
// included in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
// EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
// OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
// NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
// HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
// WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
// OTHER DEALINGS IN THE SOFTWARE.

#include "contextual_walk.h"

#include <cstring>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "gtest/gtest.h"
#include "language_model.h"
#include "reading_grid.h"

namespace Formosa::Gramambular2 {

namespace {

class TableLM : public LanguageModel {
 public:
  explicit TableLM(std::map<std::string, std::vector<Unigram>> table)
      : table_(std::move(table)) {}
  std::vector<Unigram> getUnigrams(const std::string& reading) override {
    auto it = table_.find(reading);
    return it == table_.end() ? std::vector<Unigram>{} : it->second;
  }
  bool hasUnigrams(const std::string& reading) override {
    return table_.count(reading) > 0;
  }

 private:
  std::map<std::string, std::vector<Unigram>> table_;
};

// A bigram character model: log10 P(c | prev) comes from a table, with a flat
// default for unlisted pairs. The previous character is kept in the state.
class BigramModel : public ContextModel {
 public:
  explicit BigramModel(std::map<std::pair<std::string, std::string>, double> t)
      : table_(std::move(t)) {}

  void beginSentence(State* out) const override { write("<s>", out); }

  double score(const State& in, const std::string& c,
               State* out) const override {
    write(c, out);
    auto it = table_.find({read(in), c});
    return it == table_.end() ? kDefault : it->second;
  }

  double endSentence(const State& /*in*/) const override { return 0; }

  // Like a character model, it cannot rank values without Han characters.
  [[nodiscard]] bool canRank(const std::string& value) const override {
    return value.size() >= 3 && static_cast<unsigned char>(value[0]) >= 0xE4 &&
           static_cast<unsigned char>(value[0]) <= 0xE9;
  }

  [[nodiscard]] size_t contextLength() const override { return 1; }

  static constexpr double kDefault = -5;

 private:
  static void write(const std::string& s, State* out) {
    out->bytes.fill(0);
    std::memcpy(out->bytes.data(), s.data(), s.size());
  }
  static std::string read(const State& s) {
    return std::string(reinterpret_cast<const char*>(s.bytes.data()));
  }

  std::map<std::pair<std::string, std::string>, double> table_;
};

std::string Joined(const ReadingGrid::WalkResult& r) {
  std::string s;
  for (const auto& v : r.valuesAsStrings()) {
    s += v;
  }
  return s;
}

std::shared_ptr<TableLM> SampleLM() {
  using U = LanguageModel::Unigram;
  // 意識 has a better unigram score than 定義 and 識 than 是, so the
  // unigram-only walk prefers 訂意識.
  return std::make_shared<TableLM>(std::map<std::string, std::vector<U>>{
      {"ㄉㄧㄥˋ", {U("訂", -5), U("定", -5.2)}},
      {"ㄧˋ", {U("意", -5), U("義", -5.5)}},
      {"ㄕˋ", {U("識", -5.5), U("是", -6)}},
      {"ㄒㄧㄥˊ", {U("形", -5), U("型", -5.5)}},
      {"_punctuation_<", {U("，", 0), U("︽", 0), U("〈", 0)}},
      {"ㄉㄧㄥˋ-ㄧˋ", {U("定義", -4.3)}},
      {"ㄧˋ-ㄕˋ", {U("意識", -3.5)}},
  });
}

BigramModel SampleContext() {
  return BigramModel({
      {{"定", "義"}, -0.5},
      {{"義", "是"}, -0.5},
      {{"意", "識"}, -1.0},
      {{"定", "型"}, -0.5},
      // A model may happen to prefer a symbol it cannot really judge.
      {{"訂", "︽"}, -0.1},
      // 定 is more likely than 訂 before a Latin letter.
      {{"定", "K"}, -0.5},
  });
}

}  // namespace

TEST(ContextualWalkTest, EmptyGrid) {
  ReadingGrid grid(SampleLM());
  auto result = ContextualWalk(grid, SampleContext());
  EXPECT_TRUE(result.nodes.empty());
  EXPECT_EQ(result.totalReadings, 0);
}

TEST(ContextualWalkTest, ContextBeatsUnigramScores) {
  ReadingGrid grid(SampleLM());
  for (const char* r : {"ㄉㄧㄥˋ", "ㄧˋ", "ㄕˋ"}) {
    ASSERT_TRUE(grid.insertReading(r));
  }
  EXPECT_EQ(Joined(grid.walk()), "訂意識");

  auto result = ContextualWalk(grid, SampleContext());
  EXPECT_EQ(Joined(result), "定義是");
  EXPECT_EQ(result.totalReadings, 3);
  EXPECT_EQ(result.readingsAsStrings(),
            (std::vector<std::string>{"ㄉㄧㄥˋ-ㄧˋ", "ㄕˋ"}));
}

TEST(ContextualWalkTest, ChoosesNonTopUnigramWithoutModifyingGrid) {
  ReadingGrid grid(SampleLM());
  ASSERT_TRUE(grid.insertReading("ㄉㄧㄥˋ"));
  ASSERT_TRUE(grid.insertReading("ㄒㄧㄥˊ"));
  EXPECT_EQ(Joined(grid.walk()), "訂形");

  auto result = ContextualWalk(grid, SampleContext());
  EXPECT_EQ(Joined(result), "定型");
  ASSERT_EQ(result.nodes.size(), 2);
  EXPECT_FALSE(result.nodes[0]->isOverridden());

  // The grid's own nodes still have their top unigrams.
  EXPECT_EQ(grid.spans()[0].nodeOf(1)->value(), "訂");
  EXPECT_EQ(grid.spans()[1].nodeOf(1)->value(), "形");
}

TEST(ContextualWalkTest, RespectsHardOverride) {
  ReadingGrid grid(SampleLM());
  for (const char* r : {"ㄉㄧㄥˋ", "ㄧˋ", "ㄕˋ"}) {
    ASSERT_TRUE(grid.insertReading(r));
  }
  ASSERT_TRUE(grid.overrideCandidate(2, "識"));
  auto result = ContextualWalk(grid, SampleContext());
  std::string text = Joined(result);
  EXPECT_EQ(text.substr(text.size() - std::string("識").size()), "識");
  EXPECT_TRUE(result.nodes.back()->isOverridden());
}

TEST(ContextualWalkTest, KeepsTopUnigramForUnrankableValues) {
  // Punctuation alternatives all score the same under a character model; the
  // walk must keep the dictionary's first choice instead of an arbitrary one.
  ReadingGrid grid(SampleLM());
  ASSERT_TRUE(grid.insertReading("ㄉㄧㄥˋ"));
  ASSERT_TRUE(grid.insertReading("_punctuation_<"));
  ASSERT_TRUE(grid.insertReading("ㄒㄧㄥˊ"));
  for (int i = 0; i < 20; ++i) {
    auto result = ContextualWalk(grid, SampleContext());
    ASSERT_EQ(result.nodes.size(), 3);
    EXPECT_EQ(result.nodes[1]->value(), "，");
  }
}

TEST(ContextualWalkTest, OverriddenUnrankableValueIsKept) {
  ReadingGrid grid(SampleLM());
  ASSERT_TRUE(grid.insertReading("ㄉㄧㄥˋ"));
  ASSERT_TRUE(grid.insertReading("_punctuation_<"));
  ASSERT_TRUE(grid.overrideCandidate(1, "︽"));
  auto result = ContextualWalk(grid, SampleContext());
  ASSERT_EQ(result.nodes.size(), 2);
  EXPECT_EQ(result.nodes[1]->value(), "︽");
}

TEST(ContextualWalkTest, RightContextIsScored) {
  // With nothing after it, 訂 keeps its better unigram score; a letter known to
  // follow the composition lets the model pick 定 instead.
  ReadingGrid grid(SampleLM());
  ASSERT_TRUE(grid.insertReading("ㄉㄧㄥˋ"));
  EXPECT_EQ(Joined(ContextualWalk(grid, SampleContext())), "訂");

  ContextualWalkOptions options;
  options.rightContext = "K";
  auto result = ContextualWalk(grid, SampleContext(), options);
  EXPECT_EQ(Joined(result), "定");
  EXPECT_EQ(result.totalReadings, 1);
}

TEST(ContextualWalkTest, PenalizesDoubledCharacterThatIsNotAWord) {
  using U = LanguageModel::Unigram;
  auto lm = std::make_shared<TableLM>(std::map<std::string, std::vector<U>>{
      {"ㄋㄧˇ", {U("你", -3)}},
      {"ㄕˋ", {U("是", -2), U("試", -4)}},
      {"ㄕˋ-ㄕˋ", {U("試試", -4.5)}},
      {"ㄋㄧˇ-ㄕˋ", {U("你是", -4)}},
  });
  // The model likes 是 after 是 more than 試試 as a whole.
  BigramModel context(
      {{{"是", "是"}, -1.0}, {{"你", "是"}, -0.5}, {{"試", "試"}, -0.5}});
  ReadingGrid grid(lm);
  for (const char* r : {"ㄋㄧˇ", "ㄕˋ", "ㄕˋ"}) {
    ASSERT_TRUE(grid.insertReading(r));
  }
  ContextualWalkOptions options;
  options.repeatedCharacterPenalty = 0;
  EXPECT_EQ(Joined(ContextualWalk(grid, context, options)), "你是是");
  options.repeatedCharacterPenalty = -5;
  EXPECT_EQ(Joined(ContextualWalk(grid, context, options)), "你試試");
}

TEST(ContextualWalkTest, DoubledCharacterThatIsAWordIsNotPenalized) {
  using U = LanguageModel::Unigram;
  auto lm = std::make_shared<TableLM>(std::map<std::string, std::vector<U>>{
      {"ㄊㄧㄢ", {U("天", -3)}},
      {"ㄊㄧㄢ-ㄊㄧㄢ", {U("天天", -4)}},
  });
  BigramModel context({{{"天", "天"}, -0.5}});
  ReadingGrid grid(lm);
  ASSERT_TRUE(grid.insertReading("ㄊㄧㄢ"));
  ASSERT_TRUE(grid.insertReading("ㄊㄧㄢ"));
  ContextualWalkOptions options;
  options.repeatedCharacterPenalty = -100;
  // 天 + 天 as two nodes is still allowed, since 天天 is a word.
  auto result = ContextualWalk(grid, context, options);
  EXPECT_EQ(Joined(result), "天天");
  EXPECT_GT(result.nodes.size(), 0);
}

TEST(ContextualWalkTest, RecurringWordFollowsTheMajority) {
  using U = LanguageModel::Unigram;
  auto lm = std::make_shared<TableLM>(std::map<std::string, std::vector<U>>{
      {"ㄋㄚˊ", {U("拿", -3)}},
      {"ㄅㄟ", {U("背", -3)}},
      {"ㄑㄧˊ", {U("旗", -5), U("棋", -5)}},
      {"ㄗ˙", {U("子", -5)}},
      {"ㄑㄧˊ-ㄗ˙", {U("旗子", -5), U("棋子", -5.1)}},
  });
  // 旗 follows 拿, but 棋 follows 背.
  BigramModel context({{{"拿", "旗"}, -0.5}, {{"背", "棋"}, -0.5}});
  ReadingGrid grid(lm);
  for (const char* r : {"ㄋㄚˊ", "ㄑㄧˊ", "ㄗ˙", "ㄋㄚˊ", "ㄑㄧˊ", "ㄗ˙", "ㄅㄟ",
                        "ㄑㄧˊ", "ㄗ˙"}) {
    ASSERT_TRUE(grid.insertReading(r));
  }
  ContextualWalkOptions options;
  options.consistencyBonus = 0;
  EXPECT_EQ(Joined(ContextualWalk(grid, context, options)), "拿旗子拿旗子背棋子");
  options.consistencyBonus = 5;
  EXPECT_EQ(Joined(ContextualWalk(grid, context, options)), "拿旗子拿旗子背旗子");
}

TEST(ContextualWalkTest, LeftContextIsScored) {
  // 型 is more likely than 形 after a committed Latin letter.
  ReadingGrid grid(SampleLM());
  ASSERT_TRUE(grid.insertReading("ㄒㄧㄥˊ"));
  BigramModel context({{{"K", "型"}, -0.5}});
  EXPECT_EQ(Joined(ContextualWalk(grid, context)), "形");
  ContextualWalkOptions options;
  options.leftContext = "K";
  EXPECT_EQ(Joined(ContextualWalk(grid, context, options)), "型");
}

TEST(ContextualWalkTest, LongRangeCueAtClauseEnd) {
  using U = LanguageModel::Unigram;
  auto lm = std::make_shared<TableLM>(std::map<std::string, std::vector<U>>{
      {"ㄒㄧㄤˋ", {U("向", -3), U("像", -3)}},
      {"ㄊㄚ", {U("他", -3)}},
      {"ㄧ", {U("一", -3)}},
      {"ㄧㄤˋ", {U("樣", -3)}},
      {"ㄉㄨㄥ", {U("東", -3)}},
      {"ㄧ-ㄧㄤˋ", {U("一樣", -3)}},
  });
  BigramModel context({{{"<s>", "向"}, -0.5}});
  ContextualWalkOptions options;
  options.longRangeCues = {{"ㄒㄧㄤˋ", "像", {"ㄧ-ㄧㄤˋ$"}, 10}};

  ReadingGrid grid(lm);
  for (const char* r : {"ㄒㄧㄤˋ", "ㄊㄚ", "ㄧ", "ㄧㄤˋ"}) {
    ASSERT_TRUE(grid.insertReading(r));
  }
  EXPECT_EQ(Joined(ContextualWalk(grid, context, options)), "像他一樣");

  // 一樣 followed by more text in the clause is not a cue.
  ASSERT_TRUE(grid.insertReading("ㄉㄨㄥ"));
  EXPECT_EQ(Joined(ContextualWalk(grid, context, options)), "向他一樣東");
}

TEST(ContextualWalkTest, NodeCountsAreReported) {
  ReadingGrid grid(SampleLM());
  for (const char* r : {"ㄉㄧㄥˋ", "ㄧˋ", "ㄕˋ"}) {
    ASSERT_TRUE(grid.insertReading(r));
  }
  auto result = ContextualWalk(grid, SampleContext());
  EXPECT_EQ(result.vertices, 3);
  EXPECT_EQ(result.edges, 5);
}

}  // namespace Formosa::Gramambular2
