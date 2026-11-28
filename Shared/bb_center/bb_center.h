/*******************************************************************************
 BEGIN_JUCE_MODULE_DECLARATION

  ID:                 bb_center
  vendor:             brainbugz
  version:            1.3.0
  name:               Brain Bugz Center (Updates + Lizenzen)
  description:        Update-Check, Selbst-Update und Lizenzaktivierung über das Brain Bugz Center (Supabase)
  license:            proprietary
  minimumCppStandard: 17
  dependencies:       juce_core, juce_events, juce_cryptography, juce_gui_basics, juce_audio_basics

 END_JUCE_MODULE_DECLARATION
*******************************************************************************/

#pragma once
#define BB_CENTER_H_INCLUDED

#include <juce_core/juce_core.h>
#include <juce_events/juce_events.h>
#include <juce_cryptography/juce_cryptography.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_basics/juce_audio_basics.h>

// Gemeinsame Werte für ALLE Brain-Bugz-Plugins (vom Center erzeugt)
#include "bb_center_keys.h"
#ifndef BB_KEYS_CONFIGURED
 #error "bb_center_keys.h ist noch leer: Im Brain Bugz Center unter Plugins -> 'Schluessel-Datei fuers Modul' kopieren und einfuegen."
#endif

#include <atomic>
#include <functional>
#include <memory>

namespace bbcenter
{
class Kit;

//==============================================================================
/** Werte kommen aus dem Center (Seite "Plugins" → "Code für dein Plugin"). */
struct Config
{
    juce::String pluginId;          // z. B. "snarehead"  (gleiche ID wie im Center)
    juce::String currentVersion;    // z. B. "1.0.0"      (bei jedem Release hochzählen!)
    juce::String supabaseUrl;       // https://xxxx.supabase.co
    juce::String publishableKey;    // sb_publishable_…   (NIE den Secret Key!)
    juce::String releasePublicKeyHex;  // 64 Hex-Zeichen aus dem Center (prüft Updates)
    juce::String licensePublicKeyHex;  // 64 Hex-Zeichen aus dem Center (prüft Lizenzen)

   #if JUCE_MAC
    juce::String platform = "mac";
   #else
    juce::String platform = "win64";
   #endif

    int licenseRecheckDays = 7;     // wie oft die Lizenz online nachgeprüft wird

    /** Füllt Server + Schlüssel aus bb_center_keys.h – pro Plugin nur ID und Version nötig. */
    static Config forPlugin (const juce::String& id, const juce::String& version)
    {
        Config c;
        c.pluginId            = id;
        c.currentVersion      = version;
        c.supabaseUrl         = BB_SUPABASE_URL;
        c.publishableKey      = BB_PUBLISHABLE_KEY;
        c.releasePublicKeyHex = BB_RELEASE_PUBLIC_KEY;
        c.licensePublicKeyHex = BB_LICENSE_PUBLIC_KEY;
        return c;
    }
};

//==============================================================================
/** Was passiert ohne Lizenz? */
enum class DemoMode
{
    none,        // läuft frei (z. B. Gratis-Plugin) – nur Updates, keine Lizenz nötig
    muteBursts,  // alle 60 s kurz stumm (Standard)
    silent       // ohne Lizenz komplett stumm
};

//==============================================================================
/** Gemeinsame Einstellungen fuer ALLE Brain-Bugz-Plugins
    (gespeichert in %APPDATA%/Brain Bugz/settings.xml). */
struct Settings
{
    /** Ordner, in den Updates installiert werden. Leer = dort, wo das Plugin gerade liegt. */
    static juce::File getVst3Folder();
    static void setVst3Folder (const juce::File& folder);

    /** Empfohlener Ordner ohne Admin-Rechte: %LOCALAPPDATA%\Programs\Common\VST3 */
    static juce::File getRecommendedVst3Folder();
};

//==============================================================================
/** Ed25519 + Hilfsfunktionen. */
struct Crypto
{
    static bool verifySignature (const juce::String& publicKeyHex,
                                 const juce::String& message,
                                 const juce::String& signatureBase64);

    static juce::String releaseMessage (const juce::String& pluginId, const juce::String& version,
                                        const juce::String& platform, const juce::String& sha256Hex);

    static juce::String licenseMessage (const juce::String& key, const juce::String& pluginId,
                                        const juce::String& email);

    /** -1 wenn a < b, 0 gleich, 1 wenn a > b  (Format 1.2.3) */
    static int compareVersions (const juce::String& a, const juce::String& b);
};

//==============================================================================
struct ReleaseInfo
{
    juce::String version, changelog, filePath, sha256, signature;
    juce::int64 fileSize = 0;
    bool isValid() const { return version.isNotEmpty(); }
};

//==============================================================================
/** Sucht nach Updates und installiert sie.

    Ablauf beim Kunden:
      1. checkForUpdates() → onUpdateAvailable wird gerufen, wenn es eine neuere, korrekt
         signierte Version gibt.
      2. installUpdate() → lädt, prüft SHA-256, entpackt und tauscht das Plugin aus.
         Die laufende DLL wird umbenannt (.bbold), die neue an ihren Platz gelegt.
      3. Nach dem Neustart von Ableton ist die neue Version aktiv, Reste werden aufgeräumt.

    Alle Callbacks kommen auf dem Message-Thread.
*/
class Updater
{
public:
    explicit Updater (Config cfg);
    ~Updater();

    /** Fragt höchstens alle 12 h online nach (Ergebnis wird zwischengespeichert,
        damit 20 Instanzen im Projekt nicht 20 Anfragen machen). force = sofort prüfen. */
    void checkForUpdates (bool force = false);
    void installUpdate();

    const ReleaseInfo& getAvailableRelease() const noexcept { return available; }
    bool isBusy() const noexcept { return busy.load(); }

    /** Pfad des laufenden Plugin-Bundles (…/Name.vst3) */
    static juce::File getOwnBundle();

    /** Räumt .bbold-Dateien vom letzten Update auf (wird im Konstruktor gerufen). */
    static void cleanupOldFiles();

    std::function<void (const ReleaseInfo&)>   onUpdateAvailable;
    std::function<void()>                       onNoUpdate;
    std::function<void (float progress)>        onProgress;   // 0..1
    /** success: Update liegt bereit, Neustart der DAW nötig.
        Bei Fehler steht in message, was los ist; manualZip ist dann evtl. die geladene Datei. */
    std::function<void (bool success, const juce::String& message, const juce::File& manualZip)> onInstallFinished;

private:
    Config config;
    ReleaseInfo available;
    std::atomic<bool> busy { false };
    std::shared_ptr<std::atomic<bool>> alive;

    juce::String restHeaders() const;
    juce::File cacheFile() const;
    void handleRelease (const ReleaseInfo& r);
    static juce::Result swapBundle (const juce::File& newBundle, const juce::File& target);

    JUCE_DECLARE_NON_COPYABLE (Updater)
};

//==============================================================================
/** Lizenzaktivierung. Nach einmaliger Online-Aktivierung läuft das Plugin auch offline.
    Gespeichert wird in  %APPDATA%/Brain Bugz/Licenses/<pluginId>.lic  (macOS: ~/Library/Application Support/…)
*/
class License
{
public:
    explicit License (Config cfg);
    ~License();

    /** Gespeicherte Lizenz gültig? (Signatur + Rechner wurden beim Laden geprüft.)
        Thread-sicher und schnell – darf auch in processBlock abgefragt werden. */
    bool isLicensed() const noexcept { return licensed.load(); }

    juce::String getLicensedTo() const;   // Name oder E-Mail
    juce::String getLicenseKey() const;

    /** Online-Aktivierung mit Schlüssel (BB-XXXXX-…). Callback auf dem Message-Thread. */
    void activate (const juce::String& key, std::function<void (bool ok, const juce::String& message)> done);

    /** Rechner wieder freigeben (z. B. bei PC-Wechsel). */
    void deactivate (std::function<void (bool ok, const juce::String& message)> done);

    /** Prüft alle paar Tage online, ob die Lizenz gesperrt wurde. Ohne Internet bleibt sie gültig. */
    void recheckIfDue();

    std::function<void()> onStateChanged;

    static juce::String getMachineId();

private:
    Config config;
    juce::var stored;
    std::atomic<bool> licensed { false };
    std::shared_ptr<std::atomic<bool>> alive;

    juce::File licenseFile() const;
    void store (const juce::var& v);
    void loadFromDisk();
    bool verifyStored (const juce::var& v) const;
    void post (const juce::String& action, const juce::String& key,
               std::function<void (const juce::var& response, const juce::String& error)> done);
    static juce::String errorText (const juce::String& code);

    JUCE_DECLARE_NON_COPYABLE (License)
};

//==============================================================================
/** Fertige, schlichte Leiste für den Plugin-Editor (z. B. ganz unten, 28 px hoch).
    Zeigt Lizenzstatus + Update-Hinweis. Farben passend zu Brain Bugz, per setColours änderbar.

    Im Editor:
        addAndMakeVisible (centerBar);           // centerBar ist ein bbcenter::CenterBar
        centerBar.setBounds (getLocalBounds().removeFromBottom (28));
*/
class CenterBar : public juce::Component
{
public:
    CenterBar (Updater& updater, License& license);
    explicit CenterBar (Kit& kit);
    ~CenterBar() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    void setColours (juce::Colour background, juce::Colour accent, juce::Colour text);

    /** Kann auch von eigenen Buttons im Plugin aufgerufen werden. */
    void showActivateDialog();
    void showLicenseDialog();      // aktiviert ODER zeigt Lizenz-Infos
    void showVst3FolderDialog();

    // Fuer Plugins mit eigener Anzeige (die Leiste selbst kann dann unsichtbar bleiben)
    std::function<void()> onChanged;                       // nach jeder Aenderung (Message-Thread)
    juce::String getUpdateText() const { return updateText; }
    bool isUpdateAvailable() const { return updater.getAvailableRelease().isValid(); }
    bool isLicensed() const { return license.isLicensed(); }
    juce::String getLicensedTo() const { return license.getLicensedTo(); }
    void startUpdate()  { if (updateButton.onClick) updateButton.onClick(); }
    void checkNow()     { updater.checkForUpdates (true); }

private:
    Updater& updater;
    License& license;
    juce::Label status;
    juce::TextButton updateButton { "Update" }, licenseButton { "Aktivieren" }, folderButton { "VST-Ordner" };
    std::unique_ptr<juce::FileChooser> chooser;
    juce::Colour bg { 0xff0f0d1a }, accent { 0xff39ff88 }, textCol { 0xffe9e6ff };
    juce::String updateText;

    void refresh();
    void wireUp();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CenterBar)
};


//==============================================================================
/** Das Grundgerüst für jedes Brain-Bugz-Plugin.

    PluginProcessor.h:
        bbcenter::Kit bb { "touchme" };          // Version kommt automatisch aus dem Projekt

    prepareToPlay():   bb.prepare (sampleRate);
    processBlock():    bb.process (buffer);         // ganz am Ende
    PluginEditor:      bbcenter::CenterBar bar { p.bb };
*/
class Kit
{
public:
    struct Options
    {
        DemoMode demoMode       = DemoMode::muteBursts;
        double   demoEverySecs  = 60.0;   // alle 60 s …
        double   demoMuteSecs   = 2.0;    // … 2 s stumm
    };

    // inline, damit die Version aus DEINEM Plugin-Projekt (JucePlugin_VersionString) genommen wird
    explicit Kit (const juce::String& pluginId) : Kit (pluginId, defaultVersion(), Options()) {}
    Kit (const juce::String& pluginId, DemoMode mode) : Kit (pluginId, defaultVersion(), withMode (mode)) {}
    Kit (const juce::String& pluginId, const juce::String& version, Options opts);

    Updater updater;
    License license;

    /** true, wenn lizenziert ODER der Demo-Modus "none" ist (Gratis-Plugin). */
    bool isUnlocked() const noexcept;
    DemoMode getDemoMode() const noexcept { return options.demoMode; }

    void prepare (double sampleRate);

    /** Am Ende von processBlock aufrufen. Ohne Lizenz greift die Demo (mit kurzen Fades, kein Knacken). */
    void process (juce::AudioBuffer<float>& buffer);
    void process (juce::AudioBuffer<double>& buffer);

    static juce::String defaultVersion()
    {
       #ifdef JucePlugin_VersionString
        return JucePlugin_VersionString;
       #else
        return "1.0.0";
       #endif
    }

private:
    static Options withMode (DemoMode m) { Options o; o.demoMode = m; return o; }

    Options options;
    double sr = 44100.0;
    juce::int64 samplePos = 0;
    float gain = 1.0f;

    template <typename T> void processDemo (juce::AudioBuffer<T>&);

    JUCE_DECLARE_NON_COPYABLE (Kit)
};

} // namespace bbcenter
