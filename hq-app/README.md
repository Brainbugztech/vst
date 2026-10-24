# Bugz HQ (Windows-App)

Ein Fenster mit dem Brain-Bugz-Projektboard links und einem Discord-Overlay rechts:
wer ist online, wer sitzt in welchem Voice-Channel, Chat lesen und Updates posten.

## Exe holen
GitHub → Actions → "Build Bugz HQ" → neuester Lauf → Artifact **BugzHQ** herunterladen,
entpacken, `BugzHQ.exe` auf den Desktop legen. Keine Installation nötig.

## Beim ersten Start
1. **Einstellungen** → Link zum Board eintragen (`https://claude.ai/artifact/…`), speichern.
   Im Board einmal bei claude.ai anmelden, das bleibt gespeichert.
2. **Discord-Bot-Token** eintragen. Der Token wird mit Windows verschlüsselt nur auf diesem PC gespeichert.
3. Server auswählen, speichern.

## Discord-Bot einrichten
1. https://discord.com/developers/applications → New Application → Bot.
2. Unter *Privileged Gateway Intents* alle drei einschalten: Presence, Server Members, Message Content.
3. OAuth2 → URL Generator → Scope `bot`, Rechte *View Channels*, *Send Messages*, *Read Message History* → Link öffnen und den Bot auf euren Server einladen.
4. Token unter *Bot → Reset Token* kopieren. Den Token nie in Chats, Repos oder Screenshots zeigen.

Jeder von euch (Benett, Dexter) trägt den Token bei sich lokal ein. Ihr könnt denselben Bot benutzen.
