// Segments space-separated character lines into words using a dictionary of
// (word, log10 score). Viterbi maximizing the sum of word scores; characters
// missing from the dictionary get a fixed penalty. "<x>" stays a token.
#include <cstdio>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>
int main(int argc, char** argv) {
  std::unordered_map<std::string, double> dict;
  size_t maxLen = 1;
  {
    std::ifstream f(argv[1]);
    std::string line, reading, value; double score;
    while (std::getline(f, line)) {
      std::istringstream ls(line);
      if (!(ls >> reading >> value >> score)) continue;
      if (reading[0] == '_' || reading[0] == '#') continue;
      auto it = dict.find(value);
      if (it == dict.end() || score > it->second) dict[value] = score;
    }
  }
  auto ncp = [](const std::string& s){ size_t n=0; for (unsigned char c: s) if ((c&0xC0)!=0x80) ++n; return n; };
  for (auto& [k, v] : dict) maxLen = std::max(maxLen, ncp(k));
  if (maxLen > 8) maxLen = 8;
  const double kUnknown = -9.0;
  std::ios::sync_with_stdio(false);
  std::string line;
  std::vector<std::string> toks; std::vector<double> best; std::vector<int> back;
  while (std::getline(std::cin, line)) {
    toks.clear();
    std::istringstream ss(line); std::string t;
    while (ss >> t) toks.push_back(t);
    size_t n = toks.size();
    best.assign(n + 1, -1e30); back.assign(n + 1, 0); best[0] = 0;
    for (size_t i = 0; i < n; ++i) {
      if (best[i] <= -1e29) continue;
      std::string w;
      for (size_t l = 1; l <= maxLen && i + l <= n; ++l) {
        const std::string& c = toks[i + l - 1];
        if (c == "<x>") { if (l == 1) { if (best[i] > best[i+1]) { best[i+1] = best[i]; back[i+1] = 1; } } break; }
        w += c;
        auto it = dict.find(w);
        double s;
        if (it != dict.end()) s = it->second; else if (l == 1) s = kUnknown; else continue;
        if (best[i] + s > best[i + l]) { best[i + l] = best[i] + s; back[i + l] = (int)l; }
      }
    }
    std::vector<std::string> out;
    for (size_t j = n; j > 0;) { int l = back[j]; std::string w; for (size_t k = j - l; k < j; ++k) w += toks[k]; out.push_back(w); j -= l; }
    for (size_t k = out.size(); k-- > 0;) { std::cout << out[k]; if (k) std::cout << ' '; }
    std::cout << '\n';
  }
}
