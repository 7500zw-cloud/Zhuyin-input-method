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

#ifndef SRC_ENGINE_GRAMAMBULAR2_CONTEXTUAL_WALK_H_
#define SRC_ENGINE_GRAMAMBULAR2_CONTEXTUAL_WALK_H_

#include <cstddef>
#include <string>
#include <vector>

#include "context_model.h"
#include "reading_grid.h"

namespace Formosa::Gramambular2 {

struct ContextualWalkOptions {
  // Number of partial paths kept at each reading position.
  size_t beamWidth = 30;
  // Only the top-scoring unigrams of each node are considered.
  size_t maxUnigramsPerNode = 30;
  // Weight of the node's unigram score (a log10 probability).
  double unigramWeight = 0.4;
  // Added once per node on the path. Negative values favor longer phrases.
  double nodePenalty = 0.0;
  // Text known to come right before the grid, such as the end of what was
  // already committed in the document ("這12" before ㄓ ㄧㄥˇ ㄆㄧㄢˋ). It
  // conditions the first characters of every path.
  std::string leftContext;
  // Text known to follow the grid, such as a Latin letter that is about to be
  // typed right after the composition. It is scored after every complete path
  // so that, e.g., 跟 can win over 根 before an English word.
  std::string rightContext;
  // Added when a node starts with the same character the path ends with and
  // the doubled character is not itself a word in the grid, as in 你是是這個
  // for 你試試這個. A character model has seen such typos often enough not to
  // rule them out, while real reduplications (試試, 看看) are dictionary words.
  double repeatedCharacterPenalty = -1.5;
  // Added to a word for a reading that occurs several times in the
  // composition, when that word was chosen for most of the occurrences, so a
  // recurring word is converted the same way each time.
  double consistencyBonus = 2.0;
  // A word the model cannot tell apart from its homophones because the cue
  // that decides it comes later in the clause than the model can see, as 像
  // in 要像人口那種動畫一樣 (向 otherwise). A single-syllable node with
  // `reading` gets `bonus` for `value` when one of `cues` (readings joined by
  // "-") follows it before the next punctuation mark. A cue ending in "$" must
  // also end the clause, so 一樣 in 向老師借一樣東西 is not a cue.
  struct LongRangeCue {
    std::string reading;
    std::string value;
    std::vector<std::string> cues;
    double bonus;
  };
  std::vector<LongRangeCue> longRangeCues = {
      {"ㄒㄧㄤˋ", "像", {"ㄧ-ㄧㄤˋ$", "ㄧ-ㄧㄤˋ-ㄉㄜ˙", "ㄙˋ-ㄉㄜ˙"}, 1.5},
      // 穿八號衣服, not 川八號衣服.
      {"ㄔㄨㄢ", "穿",
       {"ㄧ-ㄈㄨˊ", "ㄎㄨˋ-ㄗ˙", "ㄒㄧㄝˊ-ㄗ˙", "ㄑㄩㄣˊ-ㄗ˙", "ㄨㄞˋ-ㄊㄠˋ",
        "ㄨㄚˋ-ㄗ˙"},
       1.5},
  };
};

// Finds the most likely path through the grid by scoring whole sentences with
// a ContextModel, choosing both the segmentation and, unlike
// ReadingGrid::walk(), which unigram of each node to use.
//
// Overridden nodes are respected: a node overridden with
// kOverrideValueWithHighScore always keeps its value and receives its
// overriding score, so the walk passes through it just as ReadingGrid::walk()
// does; a soft override (kOverrideValueWithScoreFromTopUnigram) keeps its
// value but competes on normal scores.
//
// Returned nodes are the grid's own nodes when their current unigram was
// chosen, or copies (Node::copyWithUnigramAt) when another unigram was, so the
// grid itself is never modified.
ReadingGrid::WalkResult ContextualWalk(const ReadingGrid& grid,
                                       const ContextModel& model,
                                       const ContextualWalkOptions& options =
                                           ContextualWalkOptions());

}  // namespace Formosa::Gramambular2

#endif  // SRC_ENGINE_GRAMAMBULAR2_CONTEXTUAL_WALK_H_
