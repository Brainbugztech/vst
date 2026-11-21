#pragma once

// BrainBugz Touch Me!: Oberfläche exakt nach Benetts freigegebenem GUI-Kit (1536 x 1024 Design-Pixel).
// Alles wird in Design-Koordinaten gezeichnet und proportional skaliert.

#include "PluginProcessor.h"
#include "TouchMeAssets.h"

class TouchMeLook : public juce::LookAndFeel_V4
{
public:
    TouchMeLook();
    juce::Font getPopupMenuFont() override;
    void drawCornerResizer (juce::Graphics& g, int w, int h, bool isMouseOver, bool isMouseDragging) override;
};

class TouchMeEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit TouchMeEditor (TouchMeProcessor&);
    ~TouchMeEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

    static constexpr int designW = 1536, designH = 1024;

private:
    struct UserPreset { juce::String name; int program = 0; float x = 0.5f, y = 0.5f, mix = 1.0f; bool hold = false; };
    struct Entry { bool user = false; int index = 0; };
    enum class Hit { none, pad, fx, hold, prev, next, save, topPrev, topNext, category, list, scrollUp, scrollDown, scrollTrack, slider };

    void timerCallback() override;
    float scale() const { return (float) getWidth() / (float) designW; }
    juce::Point<float> toDesign (juce::Point<float> p) const { return p / scale(); }
    Hit hitTest (juce::Point<float> d, int& index) const;

    void rebuildLayer();
    void paintStatic (juce::Graphics& g);
    void paintSemiStatic (juce::Graphics& g);
    void paintDynamic (juce::Graphics& g);
    void drawWave (juce::Graphics& g);

    // Presets / Liste
    void rebuildList();
    void selectEntry (const Entry& e);
    void stepPreset (int dir);
    void ensureVisible();
    int selectedRow() const;
    juce::String entryNumber (const Entry& e) const;
    juce::String entryName (const Entry& e) const;
    void showCategoryMenu();
    void savePreset();
    juce::File userPresetFile() const;
    void loadUserPresets();
    void storeUserPresets();

    void setParam (const char* id, float plain);
    float param (const char* id) const { return proc.apvts.getRawParameterValue (id)->load(); }
    void setPadFromMouse (juce::Point<float> d);
    void setMixFromMouse (juce::Point<float> d);

    TouchMeProcessor& proc;
    TouchMeLook look;
    juce::Typeface::Ptr mono;
    juce::Font font (float em, float kernPx = 0.0f, float hScale = 1.0f) const;

    juce::Image imgBg, imgPad, imgLineV, imgLineH, imgCursor, imgBtnN, imgBtnA, imgPanel, imgList, imgThumbScroll,
                imgCategory, imgPct, imgTrackFull, imgTrackEmpty, imgSliderThumb, imgTop, imgXLcd, imgYLcd, imgWave;
    juce::Image staticLayer;      // Hintergrund + leere Displays in Design-Größe
    juce::Image cachedLayer;      // skaliert, inkl. Tasten/Texte, wird nur bei Änderung neu gebaut
    bool layerDirty = true;

    std::vector<UserPreset> userPresets;
    std::vector<Entry> list;
    int catFilter = -1;           // -1 alle, 0..11 Kategorie, 12 User
    int scrollRow = 0;
    int selectedUser = -1;        // ausgewähltes User-Preset (oder -1)
    int shownProgram = -1, shownFx = -1, shownUser = -1;
    float shownMix = -1.0f; bool shownHold = false;

    Hit hover = Hit::none; int hoverIndex = -1;
    Hit pressed = Hit::none; int pressedIndex = -1;
    bool padDown = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TouchMeEditor)
};
