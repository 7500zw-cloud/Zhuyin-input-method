// Copyright (c) 2022 and onwards The McBopomofo Authors.
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

#include "McBopomofoLM.h"

#include <algorithm>
#include <cstring>
#include <limits>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "gramambular2/reading_grid.h"

namespace McBopomofo {

static constexpr std::string_view kMacroPrefix = "MACRO@";
static constexpr double kMacroScore = -8.0;

void McBopomofoLM::loadLanguageModel(const char* languageModelDataPath) {
  if (languageModelDataPath) {
    languageModel_.close();
    languageModel_.open(languageModelDataPath);
  }
}

bool McBopomofoLM::isDataModelLoaded() const {
  return languageModel_.isLoaded();
}

void McBopomofoLM::loadAssociatedPhrasesV2(const char* associatedPhrasesPath) {
  if (associatedPhrasesPath) {
    associatedPhrasesV2_.close();
    associatedPhrasesV2_.open(associatedPhrasesPath);
  }
}

void McBopomofoLM::loadUserPhrases(const char* userPhrasesDataPath,
                                   const char* excludedPhrasesDataPath) {
  userPhrases_.close();
  excludedPhrases_.close();

  if (userPhrasesDataPath) {
    userPhrasesDataPath_ = userPhrasesDataPath;
    userPhrases_.open(userPhrasesDataPath);
  } else {
    userPhrasesDataPath_.reset();
  }

  if (excludedPhrasesDataPath) {
    excludedPhrasesDataPath_ = excludedPhrasesDataPath;
    excludedPhrases_.open(excludedPhrasesDataPath);
  } else {
    excludedPhrasesDataPath_.reset();
  }
}

bool McBopomofoLM::isAssociatedPhrasesV2Loaded() const {
  return associatedPhrasesV2_.isLoaded();
}

void McBopomofoLM::loadPhraseReplacementMap(const char* phraseReplacementPath) {
  phraseReplacement_.close();

  if (phraseReplacementPath) {
    phraseReplacementPath_ = phraseReplacementPath;
    phraseReplacement_.open(phraseReplacementPath);
  } else {
    phraseReplacementPath_.reset();
  }
}

static McBopomofoLM::IssueType TranslateIssue(
    ByteBlockBackedDictionary::Issue::Type t) {
  switch (t) {
    case ByteBlockBackedDictionary::Issue::Type::MISSING_SECOND_COLUMN:
      return McBopomofoLM::IssueType::MISSING_SECOND_COLUMN;
    case ByteBlockBackedDictionary::Issue::Type::NULL_CHARACTER_IN_TEXT:
      return McBopomofoLM::IssueType::NULL_CHARACTER_IN_TEXT;
    default:
      // should not happen
      return McBopomofoLM::IssueType::NO_ISSUE;
  }
}

std::vector<McBopomofoLM::UserFileIssue> McBopomofoLM::getUserFileIssues()
    const {
  std::vector<McBopomofoLM::UserFileIssue> issues;

  if (userPhrasesDataPath_.has_value()) {
    for (const auto& issue : userPhrases_.getParsingIssues()) {
      issues.emplace_back(McBopomofoLM::UserFileType::USER_PHRASES,
                          userPhrasesDataPath_.value(),
                          TranslateIssue(issue.type), issue.lineNumber);
    }
  }

  if (excludedPhrasesDataPath_.has_value()) {
    for (const auto& issue : excludedPhrases_.getParsingIssues()) {
      issues.emplace_back(McBopomofoLM::UserFileType::EXCLUDED_PHRASES,
                          excludedPhrasesDataPath_.value(),
                          TranslateIssue(issue.type), issue.lineNumber);
    }
  }

  if (phraseReplacementPath_.has_value()) {
    for (const auto& issue : phraseReplacement_.getParsingIssues()) {
      issues.emplace_back(McBopomofoLM::UserFileType::PHRASE_REPLACEMENT_MAP,
                          phraseReplacementPath_.value(),
                          TranslateIssue(issue.type), issue.lineNumber);
    }
  }

  return issues;
}

namespace {

// The readings obtained by swapping ㄣ and ㄥ at the end of any of the
// syllables (before the tone mark) of a "-"-separated reading, excluding the
// reading itself.
std::vector<std::string> NasalFinalVariants(const std::string& key) {
  static const std::string kEn = "ㄣ";
  static const std::string kEng = "ㄥ";
  // Byte offsets of the ㄣ or ㄥ that end a syllable's rhyme.
  std::vector<size_t> positions;
  size_t start = 0;
  while (start <= key.size()) {
    size_t end = key.find('-', start);
    if (end == std::string::npos) {
      end = key.size();
    }
    std::string syllable = key.substr(start, end - start);
    // Tone marks ˊˇˋ˙ are two bytes each in UTF-8; the first tone is unmarked.
    size_t rhymeEnd = syllable.size();
    for (const char* tone : {"ˊ", "ˇ", "ˋ", "˙"}) {
      size_t len = std::strlen(tone);
      if (syllable.size() >= len &&
          syllable.compare(syllable.size() - len, len, tone) == 0) {
        rhymeEnd = syllable.size() - len;
        break;
      }
    }
    if (rhymeEnd >= kEn.size()) {
      std::string last = syllable.substr(rhymeEnd - kEn.size(), kEn.size());
      if (last == kEn || last == kEng) {
        positions.push_back(start + rhymeEnd - kEn.size());
      }
    }
    start = end + 1;
  }
  std::vector<std::string> variants;
  if (positions.empty() || positions.size() > 4) {
    return variants;
  }
  for (size_t mask = 1; mask < (1U << positions.size()); ++mask) {
    std::string variant = key;
    for (size_t i = 0; i < positions.size(); ++i) {
      if (mask & (1U << i)) {
        std::string current = variant.substr(positions[i], kEn.size());
        variant.replace(positions[i], kEn.size(), current == kEn ? kEng : kEn);
      }
    }
    variants.push_back(std::move(variant));
  }
  return variants;
}

}  // namespace

void McBopomofoLM::setNasalFinalTolerance(bool enabled, double penalty) {
  nasalFinalTolerance_ = enabled;
  nasalFinalPenalty_ = penalty;
}

bool McBopomofoLM::nasalFinalTolerance() const { return nasalFinalTolerance_; }

std::vector<Formosa::Gramambular2::LanguageModel::Unigram>
McBopomofoLM::getUnigrams(const std::string& key) {
  auto unigrams = getExactUnigrams(key);
  if (!nasalFinalTolerance_ || !unigrams.empty() || key.empty() ||
      key[0] == '_' ||
      key.find(Formosa::Gramambular2::ReadingGrid::kDefaultSeparator) ==
          std::string::npos) {
    return unigrams;
  }
  // The typed reading has no phrase; offer the phrases of its ㄣ/ㄥ variants.
  std::unordered_map<std::string, double> best;
  for (const auto& variant : NasalFinalVariants(key)) {
    for (const auto& u : getExactUnigrams(variant)) {
      double score = u.score() + nasalFinalPenalty_;
      auto it = best.find(u.value());
      if (it == best.end() || score > it->second) {
        best[u.value()] = score;
      }
    }
  }
  for (const auto& [value, score] : best) {
    unigrams.emplace_back(value, score);
  }
  std::stable_sort(unigrams.begin(), unigrams.end(),
                   [](const auto& a, const auto& b) {
                     return a.score() > b.score() ||
                            (a.score() == b.score() && a.value() < b.value());
                   });
  return unigrams;
}

std::vector<Formosa::Gramambular2::LanguageModel::Unigram>
McBopomofoLM::getExactUnigrams(const std::string& key) {
  if (key == " ") {
    std::vector<Formosa::Gramambular2::LanguageModel::Unigram> spaceUnigrams;
    spaceUnigrams.emplace_back(" ", 0);
    return spaceUnigrams;
  }

  std::vector<Formosa::Gramambular2::LanguageModel::Unigram> allUnigrams;
  std::vector<Formosa::Gramambular2::LanguageModel::Unigram> userUnigrams;

  std::unordered_set<std::string> excludedValues;
  std::unordered_set<std::string> insertedValues;

  if (excludedPhrases_.hasUnigrams(key)) {
    std::vector<Formosa::Gramambular2::LanguageModel::Unigram>
        excludedUnigrams = excludedPhrases_.getUnigrams(key);
    std::transform(excludedUnigrams.begin(), excludedUnigrams.end(),
                   std::inserter(excludedValues, excludedValues.end()),
                   [](const Formosa::Gramambular2::LanguageModel::Unigram& u) {
                     return u.value();
                   });
  }

  if (userPhrases_.hasUnigrams(key)) {
    std::vector<Formosa::Gramambular2::LanguageModel::Unigram> rawUserUnigrams =
        userPhrases_.getUnigrams(key);
    userUnigrams = filterAndTransformUnigrams(rawUserUnigrams, excludedValues,
                                              insertedValues);
  }

  if (languageModel_.hasUnigrams(key)) {
    std::vector<Formosa::Gramambular2::LanguageModel::Unigram>
        rawGlobalUnigrams = languageModel_.getUnigrams(key);
    allUnigrams = filterAndTransformUnigrams(rawGlobalUnigrams, excludedValues,
                                             insertedValues);
  }

  // This relies on the fact that we always use the default separator.
  bool isKeyMultiSyllable =
      key.find(Formosa::Gramambular2::ReadingGrid::kDefaultSeparator) !=
      std::string::npos;

  // If key is multi-syllabic (for example, ㄉㄨㄥˋ-ㄈㄢˋ), we just
  // insert all collected userUnigrams on top of the unigrams fetched from
  // the database. If key is mono-syllabic (for example, ㄉㄨㄥˋ), then
  // we'll have to rewrite the collected userUnigrams.
  //
  // This is because, by default, user unigrams have a score of 0, which
  // guarantees that grid walks will choose them. This is problematic,
  // however, when a single-syllabic user phrase is competing with other
  // multisyllabic phrases that start with the same syllable. For example,
  // if a user has 丼 for ㄉㄨㄥˋ, and because that unigram has a score
  // of 0, no other phrases in the database that start with ㄉㄨㄥˋ would
  // be able to compete with it. Without the rewrite, ㄉㄨㄥˋ-ㄗㄨㄛˋ
  // would always result in "丼" + "作" instead of "動作" because the
  // node for "丼" would dominate the walk.
  if (isKeyMultiSyllable || allUnigrams.empty()) {
    allUnigrams.insert(allUnigrams.begin(), userUnigrams.begin(),
                       userUnigrams.end());
  } else if (!userUnigrams.empty()) {
    // Find the highest score from the existing allUnigrams.
    double topScore = std::numeric_limits<double>::lowest();
    for (const auto& unigram : allUnigrams) {
      topScore = std::max(topScore, unigram.score());
    }

    // Boost by a very small number. This is the score for user phrases.
    constexpr double epsilon = 0.000000001;
    double boostedScore = topScore + epsilon;

    std::vector<Formosa::Gramambular2::LanguageModel::Unigram>
        rewrittenUserUnigrams;
    for (const auto& unigram : userUnigrams) {
      rewrittenUserUnigrams.emplace_back(unigram.value(), boostedScore);
    }
    allUnigrams.insert(allUnigrams.begin(), rewrittenUserUnigrams.begin(),
                       rewrittenUserUnigrams.end());
  }

  return allUnigrams;
}

bool McBopomofoLM::hasUnigrams(const std::string& key) {
  if (key == " ") {
    return true;
  }

  if (!excludedPhrases_.hasUnigrams(key)) {
    return userPhrases_.hasUnigrams(key) || languageModel_.hasUnigrams(key);
  }

  return !getUnigrams(key).empty();
}

std::string McBopomofoLM::getReading(const std::string& value) const {
  std::vector<ParselessLM::FoundReading> foundReadings =
      languageModel_.getReadings(value);
  double topScore = std::numeric_limits<double>::lowest();
  std::string topValue;
  for (const auto& foundReading : foundReadings) {
    if (foundReading.score > topScore) {
      topValue = foundReading.reading;
      topScore = foundReading.score;
    }
  }
  return topValue;
}

std::vector<AssociatedPhrasesV2::Phrase> McBopomofoLM::findAssociatedPhrasesV2(
    const std::string& prefixValue,
    const std::vector<std::string>& prefixReadings) const {
  return associatedPhrasesV2_.findPhrases(prefixValue, prefixReadings);
}

void McBopomofoLM::setPhraseReplacementEnabled(bool enabled) {
  phraseReplacementEnabled_ = enabled;
}

bool McBopomofoLM::phraseReplacementEnabled() const {
  return phraseReplacementEnabled_;
}

void McBopomofoLM::setExternalConverterEnabled(bool enabled) {
  externalConverterEnabled_ = enabled;
}

bool McBopomofoLM::externalConverterEnabled() const {
  return externalConverterEnabled_;
}

void McBopomofoLM::setExternalConverter(
    std::function<std::string(const std::string&)> externalConverter) {
  externalConverter_ = std::move(externalConverter);
}

void McBopomofoLM::setMacroConverter(
    std::function<std::string(const std::string&)> macroConverter) {
  macroConverter_ = std::move(macroConverter);
}

std::string McBopomofoLM::convertMacro(const std::string& input) const {
  if (macroConverter_ != nullptr) {
    return macroConverter_(input);
  }
  return input;
}

std::vector<Formosa::Gramambular2::LanguageModel::Unigram>
McBopomofoLM::filterAndTransformUnigrams(
    const std::vector<Formosa::Gramambular2::LanguageModel::Unigram>& unigrams,
    const std::unordered_set<std::string>& excludedValues,
    std::unordered_set<std::string>& insertedValues) const {
  std::vector<Formosa::Gramambular2::LanguageModel::Unigram> results;

  for (auto&& unigram : unigrams) {
    // excludedValues filters out the unigrams with the original value.
    // insertedValues filters out the ones with the converted value
    const std::string& rawValue = unigram.value();
    if (excludedValues.find(rawValue) != excludedValues.end()) {
      continue;
    }

    std::string value = rawValue;
    if (phraseReplacementEnabled_) {
      std::string replacement = phraseReplacement_.valueForKey(value);
      if (!replacement.empty()) {
        if (value != replacement) {
          value = replacement;
        }
      }
    }
    if (macroConverter_ != nullptr) {
      std::string replacement = macroConverter_(value);
      if (value != replacement) {
        value = replacement;
      }
    }

    // Check if the string is an unsupported macro
    if (unigram.score() == kMacroScore && value.size() > kMacroPrefix.size() &&
        value.compare(0, kMacroPrefix.size(), kMacroPrefix) == 0) {
      continue;
    }

    if (externalConverterEnabled_ && externalConverter_ != nullptr) {
      std::string replacement = externalConverter_(value);
      if (value != replacement) {
        value = replacement;
      }
    }
    if (insertedValues.find(value) == insertedValues.end()) {
      results.emplace_back(value, unigram.score(), rawValue);
      insertedValues.insert(value);
    }
  }
  return results;
}

void McBopomofoLM::loadLanguageModel(std::unique_ptr<ParselessPhraseDB> db) {
  languageModel_.close();
  languageModel_.open(std::move(db));
}

void McBopomofoLM::loadAssociatedPhrasesV2(
    std::unique_ptr<ParselessPhraseDB> db) {
  associatedPhrasesV2_.close();
  associatedPhrasesV2_.open(std::move(db));
}

void McBopomofoLM::loadUserPhrases(const char* data, size_t length) {
  userPhrases_.close();
  userPhrases_.load(data, length);
}

void McBopomofoLM::loadExcludedPhrases(const char* data, size_t length) {
  excludedPhrases_.close();
  excludedPhrases_.load(data, length);
}

void McBopomofoLM::loadPhraseReplacementMap(const char* data, size_t length) {
  phraseReplacement_.close();
  phraseReplacement_.load(data, length);
}

}  // namespace McBopomofo
