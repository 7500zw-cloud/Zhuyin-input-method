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

// Converts the readings of a test set and prints one line per sentence:
// "id<TAB>output". Used to compare ReadingGrid::walk() with ContextualWalk()
// on the same dictionary.
//
// Usage:
//   ContextualWalkEval data.txt [model.bin] [beam] [unigramWeight] [nodePenalty] [repeatPenalty] [consistencyBonus]
//     [wordModel.bin] [wordWeight]
//     < testset.tsv > output.tsv
// Without model.bin, the original unigram walk is used. The test set is a TSV
// with a header; column 1 is the id and column 4 the space-separated readings.
// An optional column named "left" gives the text before the composition.

#include <algorithm>
#include <chrono>
#include <iostream>
#include <limits>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "KenLMContextModel.h"
#include "McBopomofoLM.h"
#include "gramambular2/contextual_walk.h"
#include "gramambular2/reading_grid.h"

using Formosa::Gramambular2::ContextualWalk;
using Formosa::Gramambular2::ContextualWalkOptions;
using Formosa::Gramambular2::ReadingGrid;

int main(int argc, char* argv[]) {
  if (argc < 2) {
    std::cerr << "usage: " << argv[0]
              << " data.txt [model.bin] [beam] [unigramWeight] [nodePenalty]"
                 " [repeatPenalty] [consistencyBonus] [wordModel.bin]"
                 " [wordWeight]\n";
    return 1;
  }
  auto lm = std::make_shared<McBopomofo::McBopomofoLM>();
  lm->loadLanguageModel(argv[1]);
  if (!lm->isDataModelLoaded()) {
    std::cerr << "cannot load " << argv[1] << "\n";
    return 1;
  }
  std::unique_ptr<McBopomofo::KenLMContextModel> context;
  if (argc >= 3) {
    context = McBopomofo::KenLMContextModel::Load(argv[2]);
    if (context == nullptr) {
      std::cerr << "cannot load " << argv[2] << "\n";
      return 1;
    }
  }
  ContextualWalkOptions options;
  if (argc >= 4) options.beamWidth = std::stoul(argv[3]);
  if (argc >= 5) options.unigramWeight = std::stod(argv[4]);
  if (argc >= 6) options.nodePenalty = std::stod(argv[5]);
  if (argc >= 7) options.repeatedCharacterPenalty = std::stod(argv[6]);
  if (argc >= 8) options.consistencyBonus = std::stod(argv[7]);
  std::unique_ptr<McBopomofo::KenLMContextModel> wordModel;
  if (argc >= 9) {
    wordModel = McBopomofo::KenLMContextModel::Load(argv[8]);
    if (wordModel == nullptr) {
      std::cerr << "cannot load " << argv[8] << "\n";
      return 1;
    }
    options.wordModel = wordModel.get();
  }
  if (argc >= 10) options.wordWeight = std::stod(argv[9]);

  ReadingGrid grid(lm);
  std::string line;
  std::getline(std::cin, line);  // header
  size_t leftColumn = std::numeric_limits<size_t>::max();
  {
    std::stringstream header(line);
    std::string name;
    for (size_t k = 0; std::getline(header, name, '\t'); ++k) {
      if (name == "left") leftColumn = k;
    }
  }
  size_t sentences = 0;
  size_t missing = 0;
  uint64_t totalMicros = 0;
  uint64_t worstMicros = 0;
  while (std::getline(std::cin, line)) {
    std::vector<std::string> cols;
    std::stringstream ss(line);
    std::string col;
    while (std::getline(ss, col, '\t')) cols.push_back(col);
    if (cols.size() < 4) continue;

    grid.clear();
    bool ok = true;
    options.leftContext = leftColumn < cols.size() ? cols[leftColumn] : "";
    std::stringstream readings(cols[3]);
    std::string reading;
    while (readings >> reading) {
      ok = grid.insertReading(reading) && ok;
    }
    if (!ok) ++missing;

    auto start = std::chrono::steady_clock::now();
    ReadingGrid::WalkResult result =
        context ? ContextualWalk(grid, *context, options) : grid.walk();
    auto micros = std::chrono::duration_cast<std::chrono::microseconds>(
                      std::chrono::steady_clock::now() - start)
                      .count();
    totalMicros += micros;
    worstMicros = std::max<uint64_t>(worstMicros, micros);
    ++sentences;

    std::string out;
    for (const auto& v : result.valuesAsStrings()) out += v;
    std::cout << cols[0] << "\t" << out << "\n";
  }
  std::cerr << sentences << " sentences, " << missing
            << " with unknown readings, average "
            << (sentences ? totalMicros / sentences : 0) << " us, worst "
            << worstMicros << " us\n";
  return 0;
}
