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

#include <cstring>
#include <exception>
#include <utility>

#include "lm/model.hh"

namespace McBopomofo {

static_assert(sizeof(lm::ngram::State) <=
              sizeof(Formosa::Gramambular2::ContextModel::State::bytes));

namespace {

// Decodes the first code point of a UTF-8 string; returns 0 on error.
char32_t FirstCodePoint(const std::string& s) {
  if (s.empty()) {
    return 0;
  }
  auto b = [&](size_t i) { return static_cast<unsigned char>(s[i]); };
  unsigned char c = b(0);
  if (c < 0x80) {
    return c;
  }
  if (c >= 0xF0 && s.size() >= 4) {
    return ((c & 0x07U) << 18) | ((b(1) & 0x3FU) << 12) |
           ((b(2) & 0x3FU) << 6) | (b(3) & 0x3FU);
  }
  if (c >= 0xE0 && s.size() >= 3) {
    return ((c & 0x0FU) << 12) | ((b(1) & 0x3FU) << 6) | (b(2) & 0x3FU);
  }
  if (c >= 0xC0 && s.size() >= 2) {
    return ((c & 0x1FU) << 6) | (b(1) & 0x3FU);
  }
  return 0;
}

// Han characters, including the supplementary planes. Characters the model
// has never seen (most of the supplementary ones) get KenLM's <unk>.
bool IsHan(char32_t cp) {
  return (cp >= 0x3400 && cp <= 0x4DBF) || (cp >= 0x4E00 && cp <= 0x9FFF) ||
         (cp >= 0xF900 && cp <= 0xFAFF) || (cp >= 0x20000 && cp <= 0x3FFFF);
}

// Punctuation marks, which were clause boundaries in the training text.
bool IsPunctuation(char32_t cp) {
  return (cp >= 0x21 && cp <= 0x2F) || (cp >= 0x3A && cp <= 0x40) ||
         (cp >= 0x5B && cp <= 0x60) || (cp >= 0x7B && cp <= 0x7E) ||
         (cp >= 0x2010 && cp <= 0x205E) ||  // General Punctuation
         (cp >= 0x3001 && cp <= 0x3003) ||  // 、。〃
         (cp >= 0x3008 && cp <= 0x3011) ||  // 〈〉《》「」『』【】
         (cp >= 0x3014 && cp <= 0x301F) ||  // 〔〕〖〗〘〙〚〛〜〝〞〟
         (cp >= 0xFE10 && cp <= 0xFE19) ||  // vertical forms
         (cp >= 0xFE30 && cp <= 0xFE4F) ||  // CJK compatibility forms
         (cp >= 0xFE50 && cp <= 0xFE6B) ||  // small form variants
         (cp >= 0xFF01 && cp <= 0xFF0F) || (cp >= 0xFF1A && cp <= 0xFF20) ||
         (cp >= 0xFF3B && cp <= 0xFF40) || (cp >= 0xFF5B && cp <= 0xFF65);
}

// In the training text, runs of Latin letters and digits became "<x>".
bool IsAsciiAlnum(char32_t cp) {
  return (cp >= '0' && cp <= '9') || (cp >= 'A' && cp <= 'Z') ||
         (cp >= 'a' && cp <= 'z');
}

}  // namespace

struct KenLMContextModel::Impl {
  std::unique_ptr<lm::base::Model> model;
  lm::WordIndex other = 0;
  lm::WordIndex endOfSentence = 0;
};

std::unique_ptr<KenLMContextModel> KenLMContextModel::Load(
    const std::string& path) {
  try {
    lm::ngram::Config config;
    config.load_method = util::LAZY;
    auto impl = std::make_unique<Impl>();
    impl->model.reset(lm::ngram::LoadVirtual(path.c_str(), config));
    const auto& vocab = impl->model->BaseVocabulary();
    impl->other = vocab.Index("<x>");
    impl->endOfSentence = vocab.EndSentence();
    return std::unique_ptr<KenLMContextModel>(
        new KenLMContextModel(std::move(impl)));
  } catch (const std::exception&) {
    return nullptr;
  }
}

KenLMContextModel::KenLMContextModel(std::unique_ptr<Impl> impl)
    : impl_(std::move(impl)) {}

KenLMContextModel::~KenLMContextModel() = default;

void KenLMContextModel::beginSentence(State* out) const {
  impl_->model->BeginSentenceWrite(out->bytes.data());
}

double KenLMContextModel::score(const State& in, const std::string& character,
                                State* out) const {
  char32_t cp = FirstCodePoint(character);
  if (IsPunctuation(cp)) {
    double s = endSentence(in);
    beginSentence(out);
    return s;
  }
  // Emoji, symbols, Bopomofo and the like were removed from the training
  // text, so they are scored as unknown words rather than as "<x>".
  lm::WordIndex word = IsHan(cp)          ? impl_->model->BaseVocabulary().Index(character)
                       : IsAsciiAlnum(cp) ? impl_->other
                                          : impl_->model->BaseVocabulary().NotFound();
  return impl_->model->BaseScore(in.bytes.data(), word, out->bytes.data());
}

double KenLMContextModel::endSentence(const State& in) const {
  State unused;
  return impl_->model->BaseScore(in.bytes.data(), impl_->endOfSentence,
                                 unused.bytes.data());
}

bool KenLMContextModel::canRank(const std::string& value) const {
  size_t i = 0;
  while (i < value.size()) {
    auto c = static_cast<unsigned char>(value[i]);
    size_t len = c >= 0xF0 ? 4 : c >= 0xE0 ? 3 : c >= 0xC0 ? 2 : 1;
    if (IsHan(FirstCodePoint(value.substr(i, len)))) {
      return true;
    }
    i += len;
  }
  return false;
}

size_t KenLMContextModel::contextLength() const {
  return impl_->model->Order() - 1;
}

}  // namespace McBopomofo
