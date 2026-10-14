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

#ifndef SRC_ENGINE_KENLMCONTEXTMODEL_H_
#define SRC_ENGINE_KENLMCONTEXTMODEL_H_

#include <memory>
#include <string>

#include "gramambular2/context_model.h"

namespace McBopomofo {

// A ContextModel backed by a KenLM character n-gram model (binary format).
//
// The model is expected to be trained on text where each Han character is a
// token and every run of other characters (Latin letters, digits, symbols) is
// the single token "<x>"; see lm/extract_wiki40b.py in the top-level project.
// Punctuation split the training text into clauses, so a punctuation mark is
// scored as the end of a clause and the next character starts a new one.
class KenLMContextModel : public Formosa::Gramambular2::ContextModel {
 public:
  // Returns nullptr if the file cannot be loaded.
  static std::unique_ptr<KenLMContextModel> Load(const std::string& path);

  ~KenLMContextModel() override;

  void beginSentence(State* out) const override;
  double score(const State& in, const std::string& character,
               State* out) const override;
  double endSentence(const State& in) const override;
  // True if the value contains at least one Han character.
  [[nodiscard]] bool canRank(const std::string& value) const override;
  [[nodiscard]] size_t contextLength() const override;

 private:
  struct Impl;
  explicit KenLMContextModel(std::unique_ptr<Impl> impl);
  std::unique_ptr<Impl> impl_;
};

}  // namespace McBopomofo

#endif  // SRC_ENGINE_KENLMCONTEXTMODEL_H_
