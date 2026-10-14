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

#ifndef SRC_ENGINE_GRAMAMBULAR2_CONTEXT_MODEL_H_
#define SRC_ENGINE_GRAMAMBULAR2_CONTEXT_MODEL_H_

#include <array>
#include <cstddef>
#include <string>

namespace Formosa::Gramambular2 {

// A character-level language model that scores a sentence one character at a
// time. Unlike LanguageModel, whose unigram scores ignore the surrounding
// words, a ContextModel scores each character given the characters before it,
// which is what lets a walk prefer 定義是 over 訂意識 in 好用的定義是什麼.
class ContextModel {
 public:
  // Opaque, fixed-size storage for the model's state. Implementations must
  // fit their state in it (checked with static_assert).
  struct State {
    alignas(8) std::array<unsigned char, 64> bytes{};
  };

  virtual ~ContextModel() = default;

  // Writes the state at the beginning of a sentence.
  virtual void beginSentence(State* out) const = 0;

  // Returns log10 P(character | state) and writes the next state. The
  // character is a single Unicode code point encoded in UTF-8.
  virtual double score(const State& in, const std::string& character,
                       State* out) const = 0;

  // Returns log10 P(end of sentence | state).
  virtual double endSentence(const State& in) const = 0;

  // Number of preceding characters the model looks at (order - 1). Paths
  // whose last contextLength() characters agree are interchangeable from then
  // on, so a walk only needs to keep the best of them.
  [[nodiscard]] virtual size_t contextLength() const = 0;
};

}  // namespace Formosa::Gramambular2

#endif  // SRC_ENGINE_GRAMAMBULAR2_CONTEXT_MODEL_H_
