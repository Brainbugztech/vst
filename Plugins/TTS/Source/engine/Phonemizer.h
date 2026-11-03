#pragma once

// Englische Aussprache (Text -> Kokoro-Phoneme), nachgebaut nach misaki (hexgrad/misaki, Apache-2.0).
// Nutzt die misaki-Wörterbücher (gold + silver); unbekannte Wörter laufen über Wortzerlegung + Buchstabenregeln.

#include <string>
#include <unordered_map>
#include <optional>
#include <vector>

namespace tts
{
class Phonemizer
{
public:
    // lexiconText: entpackter Inhalt von lexicon_us.txt / lexicon_gb.txt (siehe tools/make_lexicon.py)
    Phonemizer (const std::string& lexiconText, bool british);

    // Text -> Phonem-String (UTF-8), direkt für den Kokoro-Tokenizer
    std::string phonemize (const std::string& text) const;

    bool isBritish() const { return british; }
    size_t size() const { return golds.size() + silvers.size(); }

private:
    struct Entry
    {
        bool tagged = false;
        std::u32string ps;                                         // einfacher Eintrag
        std::vector<std::pair<std::string, std::optional<std::u32string>>> tags; // Eintrag mit Wortart
    };
    struct Ctx
    {
        std::optional<bool> futureVowel;
        bool futureTo = false;
    };
    struct Result
    {
        std::optional<std::u32string> ps;
        int rating = 0;
    };
    using Stress = std::optional<double>;

    void load (const std::string& text);
    static void grow (std::unordered_map<std::u32string, Entry>&);

    Result lookup (std::u32string word, const std::string& tag, Stress stress, const Ctx* ctx) const;
    Result getNNP (const std::u32string& word) const;
    Result specialCase (const std::u32string& word, const std::string& tag, Stress stress, const Ctx& ctx) const;
    bool isKnown (const std::u32string& word) const;
    std::optional<std::u32string> suffixS (const std::u32string& stem) const;
    std::optional<std::u32string> suffixEd (const std::u32string& stem) const;
    std::optional<std::u32string> suffixIng (const std::u32string& stem) const;
    Result stemS (const std::u32string& word, Stress, const Ctx*) const;
    Result stemEd (const std::u32string& word, Stress, const Ctx*) const;
    Result stemIng (const std::u32string& word, Stress, const Ctx*) const;
    Result getWord (std::u32string word, const std::string& tag, Stress stress, const Ctx& ctx) const;
    Result getNumber (std::u32string word, char32_t currency, bool isHead) const;
    Result wordToPhonemes (const std::u32string& rawWord, char32_t currency, bool isHead, const Ctx& ctx) const;
    std::u32string fallback (const std::u32string& word) const;
    std::u32string letterRules (const std::u32string& lowerWord) const;

    static std::u32string applyStress (std::u32string ps, Stress stress);
    static Ctx nextCtx (const Ctx& ctx, const std::u32string& ps, const std::u32string& word);

    bool british;
    std::unordered_map<std::u32string, Entry> golds, silvers;
};
} // namespace tts
