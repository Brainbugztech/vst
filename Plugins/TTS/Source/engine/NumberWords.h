#pragma once

// Zahlen als englische Wörter (wie num2words, aber ohne "and"): Grundzahl, Ordnungszahl, Jahreszahl

#include <string>
#include <vector>
#include <cstdint>

namespace tts::numwords
{
inline const char* ones[] = { "zero", "one", "two", "three", "four", "five", "six", "seven", "eight", "nine",
                              "ten", "eleven", "twelve", "thirteen", "fourteen", "fifteen", "sixteen",
                              "seventeen", "eighteen", "nineteen" };
inline const char* tens[] = { "", "", "twenty", "thirty", "forty", "fifty", "sixty", "seventy", "eighty", "ninety" };

inline void below1000 (int n, std::vector<std::string>& out)
{
    if (n >= 100)
    {
        out.push_back (ones[n / 100]);
        out.push_back ("hundred");
        n %= 100;
        if (n == 0) return;
    }
    if (n < 20) { out.push_back (ones[n]); return; }
    out.push_back (tens[n / 10]);
    if (n % 10) out.push_back (ones[n % 10]);
}

inline std::vector<std::string> cardinal (uint64_t n)
{
    std::vector<std::string> out;
    if (n == 0) { out.push_back ("zero"); return out; }
    static const char* scales[] = { "", "thousand", "million", "billion", "trillion", "quadrillion" };
    std::vector<int> groups;
    while (n > 0) { groups.push_back ((int) (n % 1000)); n /= 1000; }
    for (int i = (int) groups.size() - 1; i >= 0; --i)
    {
        if (groups[(size_t) i] == 0) continue;
        below1000 (groups[(size_t) i], out);
        if (i > 0 && i < 6) out.push_back (scales[i]);
    }
    return out;
}

inline std::string ordinalOf (const std::string& w)
{
    static const std::pair<const char*, const char*> irregular[] = {
        { "one", "first" }, { "two", "second" }, { "three", "third" }, { "five", "fifth" },
        { "eight", "eighth" }, { "nine", "ninth" }, { "twelve", "twelfth" } };
    for (auto& p : irregular)
        if (w == p.first) return p.second;
    if (! w.empty() && w.back() == 'y') return w.substr (0, w.size() - 1) + "ieth";
    return w + "th";
}

inline std::vector<std::string> ordinal (uint64_t n)
{
    auto w = cardinal (n);
    w.back() = ordinalOf (w.back());
    return w;
}

inline std::vector<std::string> year (int y)
{
    if (y < 1000 || y > 9999) return cardinal ((uint64_t) y);
    if (y % 1000 < 10 || (y % 100 == 0 && y % 1000 == 0)) return cardinal ((uint64_t) y);
    std::vector<std::string> out;
    const int hi = y / 100, lo = y % 100;
    below1000 (hi, out);
    if (lo == 0) { out.push_back ("hundred"); return out; }
    if (lo < 10) { out.push_back ("O"); out.push_back (ones[lo]); return out; }
    below1000 (lo, out);
    return out;
}
} // namespace tts::numwords
