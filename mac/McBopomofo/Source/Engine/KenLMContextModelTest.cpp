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

#include "KenLMContextModel.h"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>

#include "gtest/gtest.h"

namespace McBopomofo {

namespace {

// A bigram model where 義 strongly follows 定.
constexpr char kArpa[] = R"(\data\
ngram 1=7
ngram 2=2

\1-grams:
-1.0	<unk>	0
-99	<s>	-0.3
-1.0	</s>	0
-0.7	定	-0.3
-0.7	義	-0.3
-0.9	是	-0.3
-0.5	<x>	-0.3

\2-grams:
-0.1	定 義
-0.2	義 </s>

\end\
)";

class KenLMContextModelTest : public ::testing::Test {
 protected:
  void SetUp() override {
    path_ = std::filesystem::temp_directory_path() / "kenlm_context_test.arpa";
    std::ofstream(path_) << kArpa;
    model_ = KenLMContextModel::Load(path_.string());
    ASSERT_NE(model_, nullptr);
  }
  void TearDown() override { std::filesystem::remove(path_); }

  double Score(const std::string& c) {
    Formosa::Gramambular2::ContextModel::State begin;
    Formosa::Gramambular2::ContextModel::State out;
    model_->beginSentence(&begin);
    return model_->score(begin, c, &out);
  }

  std::filesystem::path path_;
  std::unique_ptr<KenLMContextModel> model_;
};

}  // namespace

TEST_F(KenLMContextModelTest, MissingFile) {
  EXPECT_EQ(KenLMContextModel::Load("/nonexistent/model.bin"), nullptr);
}

TEST_F(KenLMContextModelTest, ContextLength) {
  EXPECT_EQ(model_->contextLength(), 1);
}

TEST_F(KenLMContextModelTest, BigramIsUsed) {
  Formosa::Gramambular2::ContextModel::State s0;
  Formosa::Gramambular2::ContextModel::State s1;
  Formosa::Gramambular2::ContextModel::State s2;
  model_->beginSentence(&s0);
  model_->score(s0, "定", &s1);
  EXPECT_NEAR(model_->score(s1, "義", &s2), -0.1, 1e-4);
  EXPECT_NEAR(model_->endSentence(s2), -0.2, 1e-4);
  // Without the bigram, 是 after 定 backs off: -0.3 + -0.9.
  EXPECT_NEAR(model_->score(s1, "是", &s2), -1.2, 1e-4);
}

TEST_F(KenLMContextModelTest, AsciiIsOtherAndSymbolsAreUnknown) {
  // Every unigram after <s> also pays the <s> backoff of -0.3.
  EXPECT_NEAR(Score("A"), -0.5 - 0.3, 1e-4);                  // <x>
  EXPECT_NEAR(Score("7"), -0.5 - 0.3, 1e-4);                  // <x>
  EXPECT_NEAR(Score("\xF0\x9F\x90\x98"), -1.0 - 0.3, 1e-4);  // emoji: <unk>
  EXPECT_NEAR(Score("ㄛ"), -1.0 - 0.3, 1e-4);                 // Bopomofo: <unk>
  EXPECT_NEAR(Score("\xF0\xA3\x87\xAB"), -1.0 - 0.3, 1e-4);  // unseen Han: <unk>
}

TEST_F(KenLMContextModelTest, PunctuationEndsClause) {
  Formosa::Gramambular2::ContextModel::State s0;
  Formosa::Gramambular2::ContextModel::State s1;
  Formosa::Gramambular2::ContextModel::State s2;
  Formosa::Gramambular2::ContextModel::State s3;
  model_->beginSentence(&s0);
  model_->score(s0, "義", &s1);
  // "，" is scored as </s> after 義 (the bigram 義 </s> = -0.2) ...
  EXPECT_NEAR(model_->score(s1, "，", &s2), -0.2, 1e-4);
  EXPECT_NEAR(model_->score(s1, "？", &s2), -0.2, 1e-4);
  EXPECT_NEAR(model_->score(s1, "︽", &s2), -0.2, 1e-4);
  // ... and what follows starts a new clause.
  EXPECT_NEAR(model_->score(s2, "定", &s3), Score("定"), 1e-6);
}

TEST_F(KenLMContextModelTest, CanRank) {
  EXPECT_TRUE(model_->canRank("定義"));
  EXPECT_TRUE(model_->canRank("A定"));
  EXPECT_FALSE(model_->canRank("，"));
  EXPECT_FALSE(model_->canRank("︽"));
  EXPECT_FALSE(model_->canRank("\xF0\x9F\x90\x98"));
  EXPECT_FALSE(model_->canRank("abc"));
  EXPECT_FALSE(model_->canRank(""));
}

}  // namespace McBopomofo
