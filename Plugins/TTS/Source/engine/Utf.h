#pragma once

// Kleine UTF-8 <-> UTF-32 Helfer, damit die Aussprache-Engine ohne JUCE auskommt (testbar unter Linux)

#include <string>

namespace tts
{
inline std::u32string toU32 (const std::string& s)
{
    std::u32string out;
    out.reserve (s.size());
    for (size_t i = 0; i < s.size();)
    {
        const unsigned char c = (unsigned char) s[i];
        char32_t cp = 0;
        int n = 0;
        if (c < 0x80)              { cp = c; n = 1; }
        else if ((c >> 5) == 0x6)  { cp = c & 0x1f; n = 2; }
        else if ((c >> 4) == 0xe)  { cp = c & 0x0f; n = 3; }
        else if ((c >> 3) == 0x1e) { cp = c & 0x07; n = 4; }
        else { ++i; continue; }
        if (i + (size_t) n > s.size()) break;
        for (int k = 1; k < n; ++k)
            cp = (cp << 6) | ((unsigned char) s[i + (size_t) k] & 0x3f);
        out.push_back (cp);
        i += (size_t) n;
    }
    return out;
}

inline std::string toU8 (const std::u32string& s)
{
    std::string out;
    out.reserve (s.size() * 2);
    for (char32_t cp : s)
    {
        if (cp < 0x80) out.push_back ((char) cp);
        else if (cp < 0x800)
        {
            out.push_back ((char) (0xc0 | (cp >> 6)));
            out.push_back ((char) (0x80 | (cp & 0x3f)));
        }
        else if (cp < 0x10000)
        {
            out.push_back ((char) (0xe0 | (cp >> 12)));
            out.push_back ((char) (0x80 | ((cp >> 6) & 0x3f)));
            out.push_back ((char) (0x80 | (cp & 0x3f)));
        }
        else
        {
            out.push_back ((char) (0xf0 | (cp >> 18)));
            out.push_back ((char) (0x80 | ((cp >> 12) & 0x3f)));
            out.push_back ((char) (0x80 | ((cp >> 6) & 0x3f)));
            out.push_back ((char) (0x80 | (cp & 0x3f)));
        }
    }
    return out;
}

inline bool isAsciiAlpha (char32_t c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'); }
inline bool isAsciiDigit (char32_t c) { return c >= '0' && c <= '9'; }
inline bool isAsciiUpper (char32_t c) { return c >= 'A' && c <= 'Z'; }
inline bool isAsciiLower (char32_t c) { return c >= 'a' && c <= 'z'; }

// Buchstaben inkl. gängiger lateinischer Akzente (é, ü ...)
inline bool isLetter (char32_t c)
{
    return isAsciiAlpha (c) || (c >= 0xc0 && c <= 0x24f && c != 0xd7 && c != 0xf7);
}
inline bool isUpperLetter (char32_t c) { return isAsciiUpper (c) || (c >= 0xc0 && c <= 0xde && c != 0xd7); }

inline char32_t lowerChar (char32_t c)
{
    if (isAsciiUpper (c)) return c + 32;
    if (c >= 0xc0 && c <= 0xde && c != 0xd7) return c + 32;
    return c;
}
inline char32_t upperChar (char32_t c)
{
    if (isAsciiLower (c)) return c - 32;
    if (c >= 0xe0 && c <= 0xfe && c != 0xf7) return c - 32;
    return c;
}
inline std::u32string lower (std::u32string s) { for (auto& c : s) c = lowerChar (c); return s; }
inline std::u32string upper (std::u32string s) { for (auto& c : s) c = upperChar (c); return s; }
inline std::u32string capitalize (std::u32string s)
{
    s = lower (s);
    if (! s.empty()) s[0] = upperChar (s[0]);
    return s;
}
} // namespace tts
