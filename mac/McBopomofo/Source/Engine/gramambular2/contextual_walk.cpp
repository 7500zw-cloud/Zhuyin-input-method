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

#include <algorithm>
#include <chrono>
#include <limits>
#include <string>
#include <unordered_map>
#include <vector>

namespace Formosa::Gramambular2 {

namespace {

uint64_t NowInMicroseconds() {
  auto now = std::chrono::steady_clock::now();
  return std::chrono::duration_cast<std::chrono::microseconds>(
             now.time_since_epoch())
      .count();
}

// Splits a UTF-8 string into code points. Malformed lead bytes are treated as
// single-byte code points so that the walk never stalls on bad data.
std::vector<std::string> SplitCodePoints(const std::string& s) {
  std::vector<std::string> result;
  size_t i = 0;
  while (i < s.size()) {
    auto c = static_cast<unsigned char>(s[i]);
    size_t len = 1;
    if (c >= 0xF0) {
      len = 4;
    } else if (c >= 0xE0) {
      len = 3;
    } else if (c >= 0xC0) {
      len = 2;
    }
    len = std::min(len, s.size() - i);
    result.emplace_back(s.substr(i, len));
    i += len;
  }
  return result;
}

// One step of a path: the node taken and which of its unigrams is used.
struct Step {
  size_t parent;  // index into steps, or kNoParent
  ReadingGrid::NodePtr node;
  size_t unigramIndex;
};

constexpr size_t kNoParent = std::numeric_limits<size_t>::max();

struct Hypothesis {
  double score;
  ContextModel::State state;
  // The last contextLength() characters, used to merge equivalent paths.
  std::vector<std::string> tail;
  size_t step;  // index into steps, or kNoParent for the empty path
};

std::string TailKey(const std::vector<std::string>& tail) {
  std::string key;
  for (const auto& c : tail) {
    key += c;
    key += '\x1f';
  }
  return key;
}

// Keeps only the best hypothesis among those sharing the same tail, then the
// best beamWidth of them. Ties keep the earlier hypothesis, i.e. the one built
// from the better-ranked unigram, so results are deterministic.
void Prune(std::vector<Hypothesis>* hyps, size_t beamWidth) {
  if (hyps->size() <= 1) {
    return;
  }
  std::unordered_map<std::string, size_t> best;
  std::vector<Hypothesis> merged;
  merged.reserve(hyps->size());
  for (auto& h : *hyps) {
    std::string key = TailKey(h.tail);
    auto it = best.find(key);
    if (it == best.end()) {
      best.emplace(std::move(key), merged.size());
      merged.push_back(std::move(h));
    } else if (h.score > merged[it->second].score) {
      merged[it->second] = std::move(h);
    }
  }
  std::stable_sort(merged.begin(), merged.end(),
                   [](const Hypothesis& a, const Hypothesis& b) {
                     return a.score > b.score;
                   });
  if (merged.size() > beamWidth) {
    merged.resize(beamWidth);
  }
  *hyps = std::move(merged);
}

// Whether the grid has a node spanning the two readings at end - 1 and end
// with `value` among its unigrams.
bool IsWordAt(const std::vector<ReadingGrid::Span>& spans, size_t end,
              const std::string& value) {
  if (end == 0 || spans[end - 1].maxLength() < 2) {
    return false;
  }
  const ReadingGrid::NodePtr& node = spans[end - 1].nodeOf(2);
  if (node == nullptr) {
    return false;
  }
  for (const auto& u : node->unigrams()) {
    if (u.value() == value) {
      return true;
    }
  }
  return false;
}

}  // namespace

ReadingGrid::WalkResult ContextualWalk(const ReadingGrid& grid,
                                       const ContextModel& model,
                                       const ContextualWalkOptions& options) {
  ReadingGrid::WalkResult result;
  const auto& spans = grid.spans();
  const size_t readingLen = grid.readings().size();
  if (spans.empty() || readingLen == 0) {
    return result;
  }
  uint64_t start = NowInMicroseconds();
  const size_t contextLength = model.contextLength();
  const size_t beamWidth = std::max<size_t>(options.beamWidth, 1);

  std::vector<Step> steps;
  std::vector<std::vector<Hypothesis>> chart(readingLen + 1);
  Hypothesis initial{0.0, {}, {}, kNoParent};
  model.beginSentence(&initial.state);
  chart[0].push_back(std::move(initial));

  size_t vertices = 0;
  size_t edges = 0;
  for (size_t i = 0; i < readingLen && i < spans.size(); ++i) {
    Prune(&chart[i], beamWidth);
    if (chart[i].empty()) {
      continue;
    }
    ++vertices;
    const ReadingGrid::Span& span = spans[i];
    for (size_t spanLen = 1; spanLen <= span.maxLength(); ++spanLen) {
      const ReadingGrid::NodePtr& node = span.nodeOf(spanLen);
      if (node == nullptr || node->unigrams().empty()) {
        continue;
      }
      ++edges;

      // Which unigrams of this node may be used, and their prior scores.
      std::vector<std::pair<size_t, double>> choices;
      const auto& unigrams = node->unigrams();
      if (node->isOverridden()) {
        size_t current = 0;
        for (size_t k = 0; k < unigrams.size(); ++k) {
          if (unigrams[k].value() == node->value()) {
            current = k;
            break;
          }
        }
        double prior = node->overrideType() ==
                               ReadingGrid::Node::OverrideType::
                                   kOverrideValueWithHighScore
                           ? node->score()
                           : options.unigramWeight * node->score();
        choices.emplace_back(current, prior);
      } else {
        size_t limit = std::min(unigrams.size(), options.maxUnigramsPerNode);
        for (size_t k = 0; k < limit; ++k) {
          if (k > 0 && !model.canRank(unigrams[k].value())) {
            continue;
          }
          choices.emplace_back(k, options.unigramWeight * unigrams[k].score());
        }
      }

      std::vector<Hypothesis>& target = chart[i + spanLen];
      for (const auto& [unigramIndex, prior] : choices) {
        std::vector<std::string> chars =
            SplitCodePoints(unigrams[unigramIndex].value());
        bool doubledIsWord =
            !chars.empty() && IsWordAt(spans, i, chars.front() + chars.front());
        for (const Hypothesis& h : chart[i]) {
          Hypothesis next{h.score + prior + options.nodePenalty, h.state,
                          h.tail, steps.size()};
          if (!chars.empty() && !h.tail.empty() &&
              h.tail.back() == chars.front() && !doubledIsWord &&
              model.canRank(chars.front())) {
            next.score += options.repeatedCharacterPenalty;
          }
          for (const auto& c : chars) {
            ContextModel::State out;
            next.score += model.score(next.state, c, &out);
            next.state = out;
            next.tail.push_back(c);
          }
          if (next.tail.size() > contextLength) {
            next.tail.erase(next.tail.begin(),
                            next.tail.end() -
                                static_cast<std::ptrdiff_t>(contextLength));
          }
          steps.push_back(Step{h.step, node, unigramIndex});
          target.push_back(std::move(next));
        }
      }
    }
  }

  const std::vector<std::string> rightContext =
      SplitCodePoints(options.rightContext);
  const Hypothesis* best = nullptr;
  double bestScore = -std::numeric_limits<double>::infinity();
  for (const Hypothesis& h : chart[readingLen]) {
    double s = h.score;
    ContextModel::State state = h.state;
    for (const auto& c : rightContext) {
      ContextModel::State out;
      s += model.score(state, c, &out);
      state = out;
    }
    s += model.endSentence(state);
    if (s > bestScore) {
      bestScore = s;
      best = &h;
    }
  }
  result.vertices = vertices;
  result.edges = edges;
  if (best == nullptr) {
    result.elapsedMicroseconds = NowInMicroseconds() - start;
    return result;
  }

  size_t totalReadingLen = 0;
  for (size_t s = best->step; s != kNoParent; s = steps[s].parent) {
    const Step& step = steps[s];
    const auto& u = step.node->unigrams()[step.unigramIndex];
    bool isCurrent = u.value() == step.node->value() &&
                     u.score() == step.node->currentUnigram().score();
    result.nodes.push_back(isCurrent
                               ? step.node
                               : step.node->copyWithUnigramAt(step.unigramIndex));
    totalReadingLen += step.node->spanningLength();
  }
  std::reverse(result.nodes.begin(), result.nodes.end());
  result.totalReadings = totalReadingLen;
  result.elapsedMicroseconds = NowInMicroseconds() - start;
  return result;
}

}  // namespace Formosa::Gramambular2
