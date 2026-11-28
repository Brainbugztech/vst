#include "bb_center.h"

// Monocypher (Ed25519, RFC 8032) – BSD-2/CC0, als C++ mitkompiliert
#if defined (_MSC_VER)
 #pragma warning (push)
 #pragma warning (disable: 4244 4146 4267 4456 4457 4701 4702 4706)
#elif defined (__clang__)
 #pragma clang diagnostic push
 #pragma clang diagnostic ignored "-Weverything"
#elif defined (__GNUC__)
 #pragma GCC diagnostic push
 #pragma GCC diagnostic ignored "-Wall"
 #pragma GCC diagnostic ignored "-Wextra"
 #pragma GCC diagnostic ignored "-Wconversion"
 #pragma GCC diagnostic ignored "-Wsign-conversion"
 #pragma GCC diagnostic ignored "-Wshadow"
#endif
#include "third_party/monocypher/monocypher.c"
#undef FOR
#include "third_party/monocypher/monocypher-ed25519.c"
#if defined (_MSC_VER)
 #pragma warning (pop)
#elif defined (__clang__)
 #pragma clang diagnostic pop
#elif defined (__GNUC__)
 #pragma GCC diagnostic pop
#endif

namespace bbcenter
{
using namespace juce;

//==============================================================================
namespace
{
    File brainBugzDataDir()
    {
        auto dir = File::getSpecialLocation (File::userApplicationDataDirectory);
       #if JUCE_MAC
        dir = dir.getChildFile ("Application Support");
       #endif
        return dir.getChildFile ("Brain Bugz");
    }

    bool hexToBytes (const String& hex, uint8_t* out, size_t len)
    {
        auto h = hex.trim();
        if ((size_t) h.length() != len * 2)
            return false;

        for (size_t i = 0; i < len; ++i)
        {
            auto hi = CharacterFunctions::getHexDigitValue (h[(int) (i * 2)]);
            auto lo = CharacterFunctions::getHexDigitValue (h[(int) (i * 2 + 1)]);
            if (hi < 0 || lo < 0)
                return false;
            out[i] = (uint8_t) ((hi << 4) | lo);
        }
        return true;
    }

    String restAuthHeaders (const String& key)
    {
        String h;
        h << "apikey: " << key << "\r\n";
        if (key.startsWith ("eyJ"))   // alter anon-JWT
            h << "Authorization: Bearer " << key << "\r\n";
        return h;
    }

    /** Führt fn auf dem Message-Thread aus, aber nur wenn das Objekt noch lebt. */
    void onMessageThread (std::shared_ptr<std::atomic<bool>> alive, std::function<void()> fn)
    {
        MessageManager::callAsync ([alive, fn = std::move (fn)]
        {
            if (alive != nullptr && alive->load())
                fn();
        });
    }

    MessageBoxOptions okCancel (MessageBoxIconType icon, const String& title, const String& message,
                                const String& okText, const String& cancelText, Component* parent)
    {
       #if JUCE_MAJOR_VERSION >= 9
        return MessageBoxOptions().withIconType (icon).withTitle (title).withMessage (message)
                                  .withButton (okText, 1).withButton (cancelText, 0)
                                  .withAssociatedComponent (parent);
       #else
        return MessageBoxOptions::makeOptionsOkCancel (icon, title, message, okText, cancelText, parent);
       #endif
    }

    var readJson (const File& f)
    {
        if (! f.existsAsFile())
            return {};
        return JSON::parse (f.loadFileAsString());
    }
}

//==============================================================================
// Crypto
//==============================================================================
bool Crypto::verifySignature (const String& publicKeyHex, const String& message, const String& signatureBase64)
{
    uint8_t pub[32];
    if (! hexToBytes (publicKeyHex, pub, sizeof (pub)))
        return false;

    MemoryOutputStream sigStream;
    if (! Base64::convertFromBase64 (sigStream, signatureBase64.trim()) || sigStream.getDataSize() != 64)
        return false;

    auto utf8 = message.toRawUTF8();
    auto len  = std::strlen (utf8);

    return crypto_ed25519_check (static_cast<const uint8_t*> (sigStream.getData()), pub,
                                 reinterpret_cast<const uint8_t*> (utf8), len) == 0;
}

String Crypto::releaseMessage (const String& pluginId, const String& version, const String& platform, const String& sha256Hex)
{
    return "BBREL1|" + pluginId + "|" + version + "|" + platform + "|" + sha256Hex.toLowerCase();
}

String Crypto::licenseMessage (const String& key, const String& pluginId, const String& email)
{
    return "BBLIC1|" + key + "|" + pluginId + "|" + email;
}

int Crypto::compareVersions (const String& a, const String& b)
{
    auto pa = StringArray::fromTokens (a.trim(), ".", {});
    auto pb = StringArray::fromTokens (b.trim(), ".", {});

    for (int i = 0; i < jmax (pa.size(), pb.size()); ++i)
    {
        auto x = i < pa.size() ? pa[i].getIntValue() : 0;
        auto y = i < pb.size() ? pb[i].getIntValue() : 0;
        if (x != y)
            return x < y ? -1 : 1;
    }
    return 0;
}

//==============================================================================
// Updater
//==============================================================================
Updater::Updater (Config cfg)
    : config (std::move (cfg)), alive (std::make_shared<std::atomic<bool>> (true))
{
    cleanupOldFiles();
}

Updater::~Updater()
{
    alive->store (false);
}

String Updater::restHeaders() const
{
    return restAuthHeaders (config.publishableKey);
}

//==============================================================================
// Settings
//==============================================================================
static File settingsFile() { return brainBugzDataDir().getChildFile ("settings.xml"); }

File Settings::getVst3Folder()
{
    if (auto xml = XmlDocument::parse (settingsFile()))
    {
        auto path = xml->getStringAttribute ("vst3Folder");
        if (path.isNotEmpty() && File::isAbsolutePath (path))
            return File (path);
    }
    return {};
}

void Settings::setVst3Folder (const File& folder)
{
    std::unique_ptr<XmlElement> xml (XmlDocument::parse (settingsFile()));
    if (xml == nullptr) xml = std::make_unique<XmlElement> ("BrainBugzSettings");
    xml->setAttribute ("vst3Folder", folder.getFullPathName());
    settingsFile().getParentDirectory().createDirectory();
    xml->writeTo (settingsFile());
}

File Settings::getRecommendedVst3Folder()
{
   #if JUCE_WINDOWS
    auto local = SystemStats::getEnvironmentVariable ("LOCALAPPDATA", {});
    if (local.isNotEmpty())
        return File (local).getChildFile ("Programs").getChildFile ("Common").getChildFile ("VST3");
    return File::getSpecialLocation (File::userApplicationDataDirectory).getChildFile ("VST3");
   #else
    return File::getSpecialLocation (File::userHomeDirectory).getChildFile ("Library/Audio/Plug-Ins/VST3");
   #endif
}

File Updater::cacheFile() const
{
    return brainBugzDataDir().getChildFile ("Updates").getChildFile (config.pluginId + "-" + config.platform + ".json");
}

File Updater::getOwnBundle()
{
    auto module = File::getSpecialLocation (File::currentExecutableFile);

    // Bundle-Ordner suchen: …/Name.vst3/Contents/x86_64-win/Name.vst3
    for (auto dir = module.getParentDirectory(); dir.exists() && ! dir.isRoot(); dir = dir.getParentDirectory())
    {
        if (dir.isDirectory() && dir.getFileName().endsWithIgnoreCase (".vst3"))
            return dir;
        if (dir.getFileName().endsWithIgnoreCase ("VST3") && ! dir.getFileName().containsChar ('.'))
            break;   // im VST3-Hauptordner angekommen
    }
    return module;   // altes Einzeldatei-Format
}

void Updater::cleanupOldFiles()
{
    auto bundle = getOwnBundle();
    Array<File> old;

    if (bundle.isDirectory())
        old = bundle.findChildFiles (File::findFiles, true, "*.bbold");
    else
        old = bundle.getParentDirectory().findChildFiles (File::findFiles, false, "*.bbold");

    for (auto& f : old)
        f.deleteFile();   // klappt nicht, solange sie noch geladen ist – dann beim nächsten Mal
}

void Updater::handleRelease (const ReleaseInfo& r)
{
    if (r.isValid() && Crypto::compareVersions (r.version, config.currentVersion) > 0)
    {
        available = r;
        if (onUpdateAvailable) onUpdateAvailable (available);
    }
    else
    {
        available = {};
        if (onNoUpdate) onNoUpdate();
    }
}

void Updater::checkForUpdates (bool force)
{
    if (config.supabaseUrl.isEmpty() || config.pluginId.isEmpty())
        return;

    // Zwischenspeicher (max. alle 12 h online fragen)
    if (! force)
    {
        auto cached = readJson (cacheFile());
        auto checked = (int64) cached.getProperty ("checked", 0);
        if (checked > 0 && Time::currentTimeMillis() - checked < (int64) 12 * 3600 * 1000)
        {
            ReleaseInfo r;
            r.version   = cached.getProperty ("version", {}).toString();
            r.changelog = cached.getProperty ("changelog", {}).toString();
            r.filePath  = cached.getProperty ("file_path", {}).toString();
            r.sha256    = cached.getProperty ("sha256", {}).toString();
            r.signature = cached.getProperty ("signature", {}).toString();
            r.fileSize  = (int64) cached.getProperty ("file_size", 0);

            if (r.isValid() && ! Crypto::verifySignature (config.releasePublicKeyHex,
                    Crypto::releaseMessage (config.pluginId, r.version, config.platform, r.sha256), r.signature))
                r = {};

            onMessageThread (alive, [this, r] { handleRelease (r); });
            return;
        }
    }

    if (busy.exchange (true))
        return;

    auto cfg = config;
    auto headers = restHeaders();
    auto cache = cacheFile();
    auto aliveFlag = alive;

    Thread::launch ([this, cfg, headers, cache, aliveFlag]
    {
        String q;
        q << cfg.supabaseUrl << "/rest/v1/bb_releases"
          << "?select=version,changelog,file_path,sha256,signature,file_size"
          << "&plugin_id=eq." << URL::addEscapeChars (cfg.pluginId, true)
          << "&platform=eq."  << URL::addEscapeChars (cfg.platform, true)
          << "&published=eq.true&order=created_at.desc&limit=20";

        int status = 0;
        auto stream = URL (q).createInputStream (URL::InputStreamOptions (URL::ParameterHandling::inAddress)
                                                    .withExtraHeaders (headers)
                                                    .withConnectionTimeoutMs (8000)
                                                    .withStatusCode (&status));
        ReleaseInfo best;
        bool ok = false;

        if (stream != nullptr && status == 200)
        {
            ok = true;
            auto json = JSON::parse (stream->readEntireStreamAsString());

            if (auto* arr = json.getArray())
            {
                for (auto& row : *arr)
                {
                    ReleaseInfo r;
                    r.version   = row.getProperty ("version", {}).toString();
                    r.changelog = row.getProperty ("changelog", {}).toString();
                    r.filePath  = row.getProperty ("file_path", {}).toString();
                    r.sha256    = row.getProperty ("sha256", {}).toString();
                    r.signature = row.getProperty ("signature", {}).toString();
                    r.fileSize  = (int64) row.getProperty ("file_size", 0);

                    // Nur korrekt signierte Releases zählen
                    if (! Crypto::verifySignature (cfg.releasePublicKeyHex,
                            Crypto::releaseMessage (cfg.pluginId, r.version, cfg.platform, r.sha256), r.signature))
                        continue;

                    if (! best.isValid() || Crypto::compareVersions (r.version, best.version) > 0)
                        best = r;
                }
            }
        }

        if (ok)
        {
            auto* obj = new DynamicObject();
            obj->setProperty ("checked",   Time::currentTimeMillis());
            obj->setProperty ("version",   best.version);
            obj->setProperty ("changelog", best.changelog);
            obj->setProperty ("file_path", best.filePath);
            obj->setProperty ("sha256",    best.sha256);
            obj->setProperty ("signature", best.signature);
            obj->setProperty ("file_size", best.fileSize);
            cache.getParentDirectory().createDirectory();
            cache.replaceWithText (JSON::toString (var (obj)));
        }

        onMessageThread (aliveFlag, [this, best] { busy = false; handleRelease (best); });
    });
}

Result Updater::swapBundle (const File& newBundle, const File& target)
{
    // Liste (Quelle → Ziel) aufbauen
    Array<std::pair<File, File>> jobs;

    if (target.isDirectory() || ! target.exists())
    {
        for (auto& f : newBundle.findChildFiles (File::findFiles, true))
            jobs.add ({ f, target.getChildFile (f.getRelativePathFrom (newBundle)) });
    }
    else
    {
        // Altes Einzeldatei-Plugin: die DLL aus dem neuen Bundle nehmen
        auto dlls = newBundle.findChildFiles (File::findFiles, true, "*.vst3");
        if (dlls.isEmpty())
            return Result::fail ("Im Update wurde keine Plugin-Datei gefunden.");
        jobs.add ({ dlls.getFirst(), target });
    }

    Array<std::pair<File, File>> moved;   // für Rückgängig

    for (auto& [src, dest] : jobs)
    {
        dest.getParentDirectory().createDirectory();

        if (dest.existsAsFile())
        {
            auto old = dest.getSiblingFile (dest.getFileName() + ".bbold");
            for (int i = 2; old.exists() && ! old.deleteFile(); ++i)
                old = dest.getSiblingFile (dest.getFileName() + "." + String (i) + ".bbold");

            // Laufende DLL kann unter Windows umbenannt (aber nicht überschrieben) werden
            if (! dest.moveFileTo (old))
            {
                for (auto& [o, d] : moved) o.moveFileTo (d);
                return Result::fail ("Konnte " + dest.getFileName() + " nicht ersetzen (Schreibrechte?).");
            }
            moved.add ({ old, dest });
        }

        if (! src.copyFileTo (dest))
        {
            dest.deleteFile();
            for (auto& [o, d] : moved) { d.deleteFile(); o.moveFileTo (d); }
            return Result::fail ("Konnte " + dest.getFileName() + " nicht schreiben.");
        }
    }
    return Result::ok();
}

void Updater::installUpdate()
{
    if (! available.isValid() || busy.exchange (true))
        return;

    auto cfg = config;
    auto rel = available;
    auto headers = restHeaders();
    auto aliveFlag = alive;

    Thread::launch ([this, cfg, rel, headers, aliveFlag]
    {
        auto finish = [this, aliveFlag] (bool ok, String msg, File zip)
        {
            onMessageThread (aliveFlag, [this, ok, msg, zip]
            {
                busy = false;
                if (onInstallFinished) onInstallFinished (ok, msg, zip);
            });
        };
        auto progress = [this, aliveFlag] (float p)
        {
            onMessageThread (aliveFlag, [this, p] { if (onProgress) onProgress (p); });
        };

        // 1) Signatur nochmal prüfen
        if (! Crypto::verifySignature (cfg.releasePublicKeyHex,
                Crypto::releaseMessage (cfg.pluginId, rel.version, cfg.platform, rel.sha256), rel.signature))
            return finish (false, String::fromUTF8 ("Update-Signatur ung" "\xc3\xbc" "ltig " "\xe2\x80\x93" " Installation abgebrochen."), {});

        // 2) Herunterladen
        auto work = File::getSpecialLocation (File::tempDirectory).getChildFile ("BrainBugzUpdate");
        work.createDirectory();
        auto zip = brainBugzDataDir().getChildFile ("Downloads")
                       .getChildFile (cfg.pluginId + "-" + rel.version + "-" + cfg.platform + ".zip");
        zip.getParentDirectory().createDirectory();
        zip.deleteFile();

        String url;
        url << cfg.supabaseUrl << "/storage/v1/object/public/bb-releases/" << rel.filePath;

        int status = 0;
        auto in = URL (url).createInputStream (URL::InputStreamOptions (URL::ParameterHandling::inAddress)
                                                  .withConnectionTimeoutMs (15000)
                                                  .withStatusCode (&status));
        if (in == nullptr || status != 200)
            return finish (false, "Download fehlgeschlagen (keine Verbindung?).", {});

        {
            FileOutputStream out (zip);
            if (! out.openedOk())
                return finish (false, "Konnte die Datei nicht speichern.", {});

            HeapBlock<char> buffer (65536);
            int64 total = 0;
            for (;;)
            {
                if (! aliveFlag->load())
                    return;
                auto n = in->read (buffer, 65536);
                if (n <= 0) break;
                out.write (buffer, (size_t) n);
                total += n;
                if (rel.fileSize > 0)
                    progress ((float) jlimit (0.0, 0.95, (double) total / (double) rel.fileSize));
            }
            out.flush();
        }

        // 3) Prüfsumme
        if (SHA256 (zip).toHexString().toLowerCase() != rel.sha256.toLowerCase())
        {
            zip.deleteFile();
            return finish (false, String::fromUTF8 ("Datei besch" "\xc3\xa4" "digt (Pr" "\xc3\xbc" "fsumme falsch). Bitte sp" "\xc3\xa4" "ter nochmal versuchen."), {});
        }

        // 4) Entpacken
        auto extract = work.getChildFile ("extract-" + cfg.pluginId);
        extract.deleteRecursively();
        extract.createDirectory();
        {
            ZipFile zf (zip);
            auto res = zf.uncompressTo (extract, true);
            if (res.failed())
                return finish (false, "Entpacken fehlgeschlagen: " + res.getErrorMessage(), zip);
        }

        File newBundle;
        for (auto& d : extract.findChildFiles (File::findDirectories, true, "*.vst3"))
            if (d.getChildFile ("Contents").isDirectory()) { newBundle = d; break; }

        if (! newBundle.isDirectory())
            return finish (false, "Im Update wurde kein .vst3-Bundle gefunden.", zip);

        // 5) Austauschen (nur Windows; macOS → manuelle Installation wegen Code-Signatur)
       #if JUCE_WINDOWS
        auto target = getOwnBundle();
        {
            // Eingestellter VST3-Ordner hat Vorrang (z. B. wenn das Plugin unter Programme liegt)
            auto custom = Settings::getVst3Folder();
            if (custom.isDirectory() && ! target.isAChildOf (custom))
                target = custom.getChildFile (target.isDirectory() ? target.getFileName() : newBundle.getFileName());
        }
        target.getParentDirectory().createDirectory();
        auto testFile = (target.isDirectory() ? target : target.getParentDirectory()).getChildFile (".bbwrite");

        if (! testFile.replaceWithText ("x"))
            return finish (false, String::fromUTF8 ("Kein Schreibzugriff auf den Plugin-Ordner.\n"
                                  "Tipp: Plugins in einen Benutzerordner installieren "
                                  "(z. B. %LOCALAPPDATA%\\Programs\\Common\\VST3) " "\xe2\x80\x93" " oder das Update "
                                  "aus dem ge" "\xc3\xb6" "ffneten Ordner manuell kopieren."), zip);
        testFile.deleteFile();

        auto res = swapBundle (newBundle, target);
        extract.deleteRecursively();
        if (res.failed())
            return finish (false, res.getErrorMessage(), zip);

        // Update-Cache leeren, damit nach dem Neustart neu geprüft wird
        brainBugzDataDir().getChildFile ("Updates").getChildFile (cfg.pluginId + "-" + cfg.platform + ".json").deleteFile();

        progress (1.0f);
        finish (true, "Version " + rel.version + " ist installiert.\nBitte die DAW neu starten.", {});
       #else
        progress (1.0f);
        finish (false, String::fromUTF8 ("Update geladen. Bitte die Datei im ge" "\xc3\xb6" "ffneten Ordner installieren."), zip);
       #endif
    });
}

//==============================================================================
// License
//==============================================================================
License::License (Config cfg)
    : config (std::move (cfg)), alive (std::make_shared<std::atomic<bool>> (true))
{
    loadFromDisk();
}

License::~License()
{
    alive->store (false);
}

String License::getMachineId()
{
    auto id = SystemStats::getUniqueDeviceID();
    if (id.isEmpty())
        id = SystemStats::getComputerName() + "|" + SystemStats::getLogonName();
    return SHA256 ((id + "|brainbugz").toUTF8()).toHexString().toLowerCase();
}

File License::licenseFile() const
{
    return brainBugzDataDir().getChildFile ("Licenses").getChildFile (config.pluginId + ".lic");
}

bool License::verifyStored (const var& v) const
{
    auto key    = v.getProperty ("key", {}).toString();
    auto plugin = v.getProperty ("plugin_id", {}).toString();
    auto email  = v.getProperty ("email", {}).toString();
    auto sig    = v.getProperty ("signature", {}).toString();
    auto mach   = v.getProperty ("machine_id", {}).toString();

    if (key.isEmpty() || sig.isEmpty())
        return false;
    if (plugin != "*" && plugin != config.pluginId)
        return false;
    if (mach != getMachineId())
        return false;

    return Crypto::verifySignature (config.licensePublicKeyHex, Crypto::licenseMessage (key, plugin, email), sig);
}

void License::loadFromDisk()
{
    auto v = readJson (licenseFile());
    if (verifyStored (v))
    {
        stored = v;
        licensed = true;
    }
    else
    {
        stored = var();
        licensed = false;
    }
}

void License::store (const var& v)
{
    auto f = licenseFile();
    f.getParentDirectory().createDirectory();
    f.replaceWithText (JSON::toString (v));
    stored = v;
    licensed = verifyStored (v);
}

String License::getLicensedTo() const
{
    auto name = stored.getProperty ("name", {}).toString();
    return name.isNotEmpty() ? name : stored.getProperty ("email", {}).toString();
}

String License::getLicenseKey() const
{
    return stored.getProperty ("key", {}).toString();
}

String License::errorText (const String& code)
{
    if (code == "invalid_key")          return String::fromUTF8 ("Ung" "\xc3\xbc" "ltiges Schl" "\xc3\xbc" "ssel-Format (BB-XXXXX-XXXXX-XXXXX-XXXXX).");
    if (code == "unknown_key")          return String::fromUTF8 ("Diesen Schl" "\xc3\xbc" "ssel gibt es nicht.");
    if (code == "revoked")              return "Diese Lizenz wurde gesperrt.";
    if (code == "wrong_plugin")         return String::fromUTF8 ("Dieser Schl" "\xc3\xbc" "ssel gilt f" "\xc3\xbc" "r ein anderes Plugin.");
    if (code == "too_many_activations") return "Maximale Anzahl Rechner erreicht. Bitte auf einem alten Rechner deaktivieren.";
    if (code == "not_activated")        return "Auf diesem Rechner nicht aktiviert.";
    if (code == "network")              return "Keine Verbindung zum Server.";
    if (code == "bad_signature")        return "Antwort vom Server ist nicht korrekt signiert.";
    return "Fehler: " + code;
}

void License::post (const String& action, const String& key,
                    std::function<void (const var&, const String&)> done)
{
    auto cfg = config;
    auto aliveFlag = alive;
    auto machine = getMachineId();
    auto machineName = SystemStats::getComputerName();

    Thread::launch ([cfg, aliveFlag, action, key, machine, machineName, done]
    {
        auto* obj = new DynamicObject();
        obj->setProperty ("action", action);
        obj->setProperty ("key", key.trim().toUpperCase());
        obj->setProperty ("plugin_id", cfg.pluginId);
        obj->setProperty ("machine_id", machine);
        obj->setProperty ("machine_name", machineName);

        String headers = restAuthHeaders (cfg.publishableKey);
        headers << "Content-Type: application/json\r\n";

        int status = 0;
        auto url = URL (cfg.supabaseUrl + "/functions/v1/bb-license").withPOSTData (JSON::toString (var (obj), true));
        auto in = url.createInputStream (URL::InputStreamOptions (URL::ParameterHandling::inPostData)
                                            .withExtraHeaders (headers)
                                            .withConnectionTimeoutMs (10000)
                                            .withStatusCode (&status));
        var response;
        String error;

        if (in == nullptr || status == 0)
            error = "network";
        else
        {
            response = JSON::parse (in->readEntireStreamAsString());
            if (! (bool) response.getProperty ("ok", false))
                error = response.getProperty ("error", status >= 500 ? "server" : "unknown").toString();
        }

        onMessageThread (aliveFlag, [done, response, error] { done (response, error); });
    });
}

void License::activate (const String& key, std::function<void (bool, const String&)> done)
{
    post ("activate", key, [this, done] (const var& r, const String& error)
    {
        if (error.isNotEmpty())
            return done (false, errorText (error));

        auto lic = r.getProperty ("license", {});
        auto* obj = new DynamicObject();
        obj->setProperty ("key",        lic.getProperty ("key", {}));
        obj->setProperty ("plugin_id",  lic.getProperty ("plugin_id", {}));
        obj->setProperty ("email",      lic.getProperty ("email", {}));
        obj->setProperty ("name",       lic.getProperty ("name", {}));
        obj->setProperty ("signature",  lic.getProperty ("signature", {}));
        obj->setProperty ("machine_id", getMachineId());
        obj->setProperty ("last_check", Time::currentTimeMillis());
        var v (obj);

        if (! verifyStored (v))
            return done (false, errorText ("bad_signature"));

        store (v);
        if (onStateChanged) onStateChanged();
        done (true, String::fromUTF8 ("Lizenz aktiviert " "\xe2\x80\x93" " danke f" "\xc3\xbc" "r den Support!"));
    });
}

void License::deactivate (std::function<void (bool, const String&)> done)
{
    auto key = getLicenseKey();
    if (key.isEmpty())
        return done (true, {});

    post ("deactivate", key, [this, done] (const var&, const String& error)
    {
        if (error.isNotEmpty() && error != "not_activated")
            return done (false, errorText (error));

        licenseFile().deleteFile();
        stored = var();
        licensed = false;
        if (onStateChanged) onStateChanged();
        done (true, "Rechner freigegeben.");
    });
}

void License::recheckIfDue()
{
    if (! licensed.load())
        return;

    auto last = (int64) stored.getProperty ("last_check", 0);
    if (Time::currentTimeMillis() - last < (int64) config.licenseRecheckDays * 24 * 3600 * 1000)
        return;

    post ("check", getLicenseKey(), [this] (const var&, const String& error)
    {
        if (error.isEmpty())
        {
            if (auto* o = stored.getDynamicObject())
                o->setProperty ("last_check", Time::currentTimeMillis());
            store (stored);
            return;
        }

        // Nur bei eindeutiger Antwort deaktivieren – ohne Internet bleibt alles wie es ist
        if (error == "revoked" || error == "unknown_key" || error == "not_activated" || error == "wrong_plugin")
        {
            licenseFile().deleteFile();
            stored = var();
            licensed = false;
            if (onStateChanged) onStateChanged();
        }
    });
}

//==============================================================================
// CenterBar
//==============================================================================
CenterBar::CenterBar (Kit& kit) : CenterBar (kit.updater, kit.license) {}

CenterBar::CenterBar (Updater& u, License& l) : updater (u), license (l)
{
   #if JUCE_MAJOR_VERSION >= 8
    status.setFont (FontOptions (13.0f));
   #else
    status.setFont (Font (13.0f));
   #endif
    status.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (status);
    addAndMakeVisible (licenseButton);
    addAndMakeVisible (folderButton);
    addChildComponent (updateButton);

    folderButton.onClick = [this] { showVst3FolderDialog(); };
    licenseButton.onClick = [this] { showLicenseDialog(); };
    wireUp();
}

void CenterBar::showLicenseDialog()
{
    {
        if (! license.isLicensed())
            return showActivateDialog();

        SafePointer<CenterBar> safe (this);
        AlertWindow::showAsync (okCancel (MessageBoxIconType::QuestionIcon, "Lizenz",
                                    String::fromUTF8 ("Lizenziert f" "\xc3\xbc" "r: ") + license.getLicensedTo()
                                                  + String::fromUTF8 ("\nSchl" "\xc3\xbc" "ssel: ") + license.getLicenseKey()
                                                  + "\n\nDiesen Rechner freigeben (z. B. bei PC-Wechsel)?",
                                    "Freigeben", String::fromUTF8 ("Schlie" "\xc3\x9f" "en"), this),
            [safe] (int r)
            {
                if (r == 1 && safe != nullptr)
                    safe->license.deactivate ([safe] (bool, const String& msg)
                    {
                        if (safe != nullptr && msg.isNotEmpty())
                            AlertWindow::showMessageBoxAsync (MessageBoxIconType::InfoIcon, "Lizenz", msg, {}, safe.getComponent());
                    });
            });
    }
}

void CenterBar::wireUp()
{
    updateButton.onClick = [this]
    {
        auto rel = updater.getAvailableRelease();
        if (! rel.isValid() || updater.isBusy())
            return;

        SafePointer<CenterBar> safe (this);
        AlertWindow::showAsync (okCancel (MessageBoxIconType::InfoIcon, "Update auf v" + rel.version,
                                    (rel.changelog.isNotEmpty() ? rel.changelog + "\n\n" : String())
                                                  + "Jetzt installieren? Danach die DAW neu starten.",
                                    "Installieren", String::fromUTF8 ("Sp" "\xc3\xa4" "ter"), this),
            [safe] (int r)
            {
                if (r == 1 && safe != nullptr)
                {
                    safe->updateButton.setEnabled (false);
                    safe->updater.installUpdate();
                }
            });
    };

    updater.onUpdateAvailable = [this] (const ReleaseInfo& r)
    {
        updateText = "Update v" + r.version + String::fromUTF8 (" verf" "\xc3\xbc" "gbar");
        refresh();
    };
    updater.onNoUpdate = [this] { updateText = {}; refresh(); };
    updater.onProgress = [this] (float p)
    {
        updateText = String::fromUTF8 ("Update wird geladen " "\xe2\x80\xa6" " ") + String (roundToInt (p * 100.0f)) + " %";
        refresh();
    };
    updater.onInstallFinished = [this] (bool ok, const String& msg, const File& zip)
    {
        updateText = ok ? String::fromUTF8 ("Update installiert " "\xe2\x80\x93" " DAW neu starten") : "Update fehlgeschlagen";
        updateButton.setEnabled (! ok);
        refresh();
        AlertWindow::showMessageBoxAsync (ok ? MessageBoxIconType::InfoIcon : MessageBoxIconType::WarningIcon,
                                          "Update", msg, {}, this);
        if (zip.existsAsFile())
            zip.revealToUser();
    };
    license.onStateChanged = [this] { refresh(); };

    updater.checkForUpdates();
    license.recheckIfDue();
    refresh();
}

void CenterBar::showVst3FolderDialog()
{
    auto current = Settings::getVst3Folder();
    auto own = Updater::getOwnBundle();
    String msg;
    msg << "Plugin liegt gerade in:\n" << own.getParentDirectory().getFullPathName() << "\n\n"
        << String::fromUTF8 ("VST3-Ordner f" "\xc3\xbc" "r Updates:\n")
        << (current.isDirectory() ? current.getFullPathName() : String::fromUTF8 ("(nicht gesetzt \xe2\x80\x93 Updates landen neben dem Plugin)")) << "\n\n"
        << "Empfohlen (ohne Admin-Rechte):\n" << Settings::getRecommendedVst3Folder().getFullPathName() << "\n"
        << String::fromUTF8 ("Denselben Ordner in Ableton unter Einstellungen \xe2\x86\x92 Plug-Ins als VST3-Ordner eintragen.");

    SafePointer<CenterBar> safe (this);
    auto aw = std::make_shared<AlertWindow> ("VST3-Ordner", msg, MessageBoxIconType::NoIcon, this);
    aw->addButton (String::fromUTF8 ("Ordner w" "\xc3\xa4" "hlen"), 1);
    aw->addButton ("Empfohlenen nehmen", 2);
    aw->addButton (String::fromUTF8 ("Schlie" "\xc3\x9f" "en"), 0, KeyPress (KeyPress::escapeKey));
    aw->enterModalState (true, ModalCallbackFunction::create ([aw, safe] (int r)
    {
        if (safe == nullptr) return;
        if (r == 2)
        {
            auto rec = Settings::getRecommendedVst3Folder();
            rec.createDirectory();
            Settings::setVst3Folder (rec);
            AlertWindow::showMessageBoxAsync (MessageBoxIconType::InfoIcon, "VST3-Ordner", "Gespeichert:\n" + rec.getFullPathName(), {}, safe.getComponent());
        }
        else if (r == 1)
        {
            auto start = Settings::getVst3Folder();
            if (! start.isDirectory()) start = Settings::getRecommendedVst3Folder();
            safe->chooser = std::make_unique<FileChooser> (String::fromUTF8 ("VST3-Ordner w" "\xc3\xa4" "hlen"), start);
            safe->chooser->launchAsync (FileBrowserComponent::openMode | FileBrowserComponent::canSelectDirectories,
                [safe] (const FileChooser& fc)
                {
                    auto f = fc.getResult();
                    if (f.isDirectory())
                    {
                        Settings::setVst3Folder (f);
                        if (safe != nullptr)
                            AlertWindow::showMessageBoxAsync (MessageBoxIconType::InfoIcon, "VST3-Ordner", "Gespeichert:\n" + f.getFullPathName(), {}, safe.getComponent());
                    }
                });
        }
    }), false);
}

CenterBar::~CenterBar()
{
    onChanged = nullptr;
    updater.onUpdateAvailable = nullptr;
    updater.onNoUpdate = nullptr;
    updater.onProgress = nullptr;
    updater.onInstallFinished = nullptr;
    license.onStateChanged = nullptr;
}

void CenterBar::setColours (Colour background, Colour accentColour, Colour text)
{
    bg = background; accent = accentColour; textCol = text;
    refresh();
}

void CenterBar::refresh()
{
    String s = license.isLicensed() ? ("Lizenziert: " + license.getLicensedTo()) : String ("Nicht aktiviert (Demo)");
    if (updateText.isNotEmpty())
        s << "   |   " << updateText;

    status.setText (s, dontSendNotification);
    status.setColour (Label::textColourId, textCol);

    licenseButton.setButtonText (license.isLicensed() ? "Lizenz" : "Aktivieren");
    updateButton.setVisible (updater.getAvailableRelease().isValid());

    for (auto* b : { &licenseButton, &updateButton, &folderButton })
    {
        b->setColour (TextButton::buttonColourId, bg.brighter (0.15f));
        b->setColour (TextButton::textColourOffId, accent);
        b->setColour (ComboBox::outlineColourId, accent.withAlpha (0.5f));
    }
    resized();
    repaint();
    if (onChanged)
        onChanged();
}

void CenterBar::paint (Graphics& g)
{
    g.fillAll (bg);
    g.setColour (accent.withAlpha (0.35f));
    g.fillRect (getLocalBounds().removeFromTop (1));
}

void CenterBar::resized()
{
    auto r = getLocalBounds().reduced (6, 3);
    licenseButton.setBounds (r.removeFromRight (90));
    r.removeFromRight (6);
    folderButton.setBounds (r.removeFromRight (90));
    if (updateButton.isVisible())
    {
        r.removeFromRight (6);
        updateButton.setBounds (r.removeFromRight (80));
    }
    status.setBounds (r);
}

void CenterBar::showActivateDialog()
{
    auto aw = std::make_shared<AlertWindow> ("Plugin aktivieren",
                                             String::fromUTF8 ("Gib deinen Lizenzschl" "\xc3\xbc" "ssel ein (BB-XXXXX-XXXXX-XXXXX-XXXXX)."),
                                             MessageBoxIconType::NoIcon, this);
    aw->addTextEditor ("key", {}, String::fromUTF8 ("Lizenzschl" "\xc3\xbc" "ssel"));
    aw->addButton ("Aktivieren", 1, KeyPress (KeyPress::returnKey));
    aw->addButton ("Abbrechen", 0, KeyPress (KeyPress::escapeKey));

    SafePointer<CenterBar> safe (this);
    aw->enterModalState (true, ModalCallbackFunction::create ([aw, safe] (int r)
    {
        if (r != 1 || safe == nullptr)
            return;

        auto key = aw->getTextEditorContents ("key");
        safe->status.setText (String::fromUTF8 ("Aktiviere \xe2\x80\xa6"), dontSendNotification);
        safe->license.activate (key, [safe] (bool ok, const String& msg)
        {
            if (safe == nullptr) return;
            safe->refresh();
            AlertWindow::showMessageBoxAsync (ok ? MessageBoxIconType::InfoIcon : MessageBoxIconType::WarningIcon,
                                              "Lizenz", msg, {}, safe.getComponent());
        });
    }), false);
}


//==============================================================================
// Kit
//==============================================================================
Kit::Kit (const String& pluginId, const String& version, Options opts)
    : updater (Config::forPlugin (pluginId, version)),
      license (Config::forPlugin (pluginId, version)),
      options (opts)
{
}

bool Kit::isUnlocked() const noexcept
{
    return options.demoMode == DemoMode::none || license.isLicensed();
}

void Kit::prepare (double sampleRate)
{
    sr = sampleRate > 0 ? sampleRate : 44100.0;
    samplePos = 0;
}

template <typename T>
void Kit::processDemo (AudioBuffer<T>& buffer)
{
    const auto unlocked = isUnlocked();
    const int n = buffer.getNumSamples();
    const float fadeStep = (float) (1.0 / (0.01 * sr));   // 10 ms Fade

    if (unlocked && gain >= 1.0f)
    {
        samplePos += n;
        return;
    }

    const auto period = (int64) (options.demoEverySecs * sr);
    const auto muteLen = (int64) (options.demoMuteSecs * sr);

    for (int i = 0; i < n; ++i)
    {
        bool mute = false;
        if (! unlocked)
        {
            if (options.demoMode == DemoMode::silent)
                mute = true;
            else if (options.demoMode == DemoMode::muteBursts && period > 0)
                mute = (samplePos % period) >= (period - muteLen);
        }

        gain = mute ? jmax (0.0f, gain - fadeStep) : jmin (1.0f, gain + fadeStep);

        if (gain < 1.0f)
            for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
                buffer.getWritePointer (ch)[i] *= (T) gain;

        ++samplePos;
    }
}

void Kit::process (AudioBuffer<float>& buffer)  { processDemo (buffer); }
void Kit::process (AudioBuffer<double>& buffer) { processDemo (buffer); }

} // namespace bbcenter
