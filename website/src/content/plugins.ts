import type { Plugin } from "../types";
import data from "virtual:brainbugz-plugins";

// Die Plugin-Daten stehen in src/content/plugins.json, die Cover in src/content/plugin-media/.
// Beim Build kommen NUR Plugins mit "published": true auf die Website (siehe vite.config.ts).
// Anleitung zum Hinzufügen und Veröffentlichen: website/README.md
export const plugins = data as Plugin[];

/** Standard-Installationsanleitung für VST3 unter Windows. */
export const defaultInstallSteps = [
  "Download and unzip the file.",
  "Copy the .vst3 file into C:\\Program Files\\Common Files\\VST3.",
  "In Ableton Live: Options → Preferences → Plug-Ins, turn on 'Use VST3 Plug-in System Folders' and click Rescan.",
  "Other DAWs: rescan your plug-ins and look for the Brain Bugz folder.",
];
