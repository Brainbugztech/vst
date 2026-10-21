#pragma once

#include <juce_core/juce_core.h>
#include <vector>
#include <utility>

// Etappe 8: Werkspresets.
// Ein Werkspreset speichert nur die Werte, die vom Standard abweichen, plus Seed und
// einen Step-Seed. Die 4 Bleeps und das Step-Pattern entstehen beim Laden daraus neu,
// immer gleich, weil der Zufall am Seed haengt. Deshalb sind die Presets winzig.

namespace bleep
{

struct FactoryPreset
{
    const char* name;
    std::vector<std::pair<const char*, float>> values;   // Parameter-ID, Klartext-Wert
    int seed;
    int stepSeed;
};

inline const std::vector<FactoryPreset>& factoryPresets()
{
    static const std::vector<FactoryPreset> presets = {
        { "Init", {}, 4271, 0 },

        { "Arcade Coin",
          { { "sq_oct", 0 }, { "mod_pd", 0 }, { "penv_amt", 12 }, { "penv_decay", 25 }, { "amp_d", 120 },
            { "arp_scale", 2 }, { "arp_range", 2 }, { "variation", 30 }, { "delay_mix", 20 }, { "delay_fb", 30 },
            { "filter_cut", 6000 }, { "filter_env", 20 } },
          1987, 11 },

        { "Metal Mosquito",
          { { "sq_oct", 1 }, { "mod_pd", 60 }, { "lfo_rate", 180 }, { "lfo_pitch", 0.5f }, { "arp_rate", 3 },
            { "arp_scale", 3 }, { "comb_on", 1 }, { "comb_freq", 1200 }, { "comb_fb", -85 }, { "comb_mix", 60 },
            { "filter_type", 1 }, { "filter_cut", 3000 }, { "filter_res", 60 }, { "delay_mix", 15 } },
          3141, 23 },

        { "Phrygian Patrol",
          { { "arp_scale", 3 }, { "mod_pd", 40 }, { "comb_on", 1 }, { "comb_fb", -70 }, { "comb_mix", 35 },
            { "delay_fb", 50 }, { "arp_swing", 12 }, { "variation", 45 } },
          666, 7 },

        { "Broken Modem",
          { { "crush_on", 1 }, { "crush_bits", 4 }, { "crush_rate", 8 }, { "crush_mix", 70 }, { "lfo_rate", 320 },
            { "lfo_pitch", 3 }, { "mod_pd", 80 }, { "pitch_link", 0 }, { "arp_rate", 3 }, { "roll_auto", 2 },
            { "variation", 70 }, { "delay_mix", 10 } },
          5600, 42 },

        { "Laser Rain",
          { { "penv_amt", 36 }, { "penv_decay", 90 }, { "sq_oct", 1 }, { "delay_fb", 70 }, { "delay_mix", 45 },
            { "filter_env", 60 }, { "arp_rate", 5 }, { "roll_density", 90 }, { "arp_range", 3 }, { "arp_scale", 1 } },
          2049, 99 },

        { "Sub Blip",
          { { "sq_oct", -2 }, { "mod_pd", 20 }, { "penv_amt", 12 }, { "penv_decay", 40 }, { "amp_d", 220 },
            { "filter_cut", 900 }, { "filter_res", 20 }, { "filter_env", 30 }, { "arp_rate", 1 }, { "arp_scale", 1 },
            { "delay_on", 0 }, { "variation", 25 } },
          808, 3 },

        { "R2 Chatter",
          { { "lfo_rate", 14 }, { "lfo_pitch", 9 }, { "mod_pd", 50 }, { "penv_amt", -18 }, { "variation", 85 },
            { "slot_mode", 0 }, { "roll_auto", 1 }, { "roll_density", 80 }, { "arp_range", 2 }, { "delay_mix", 18 } },
          2187, 77 },

        { "Glass Droid",
          { { "sq_oct", 0 }, { "sq_pw", 25 }, { "comb_on", 1 }, { "comb_freq", 600 }, { "comb_fb", 80 },
            { "comb_damp", 10 }, { "comb_count", 3 }, { "comb_mix", 50 }, { "filter_type", 2 }, { "filter_cut", 300 },
            { "filter_env", 0 }, { "arp_scale", 2 }, { "delay_fb", 55 } },
          1701, 15 },

        { "Robo Bass Walk",
          { { "sq_oct", -2 }, { "mod_oct", -2 }, { "mod_pd", 55 }, { "arp_scale", 1 }, { "crush_on", 1 },
            { "crush_bits", 10 }, { "crush_mix", 30 }, { "filter_cut", 1500 }, { "filter_res", 45 },
            { "filter_env", 50 }, { "delay_on", 0 }, { "arp_gate", 60 } },
          404, 31 },

        { "Bit Fireflies",
          { { "sq_oct", 2 }, { "penv_amt", 7 }, { "penv_decay", 15 }, { "amp_d", 60 }, { "arp_rate", 3 },
            { "roll_density", 60 }, { "arp_range", 3 }, { "arp_scale", 2 }, { "crush_on", 1 }, { "crush_bits", 6 },
            { "crush_rate", 2 }, { "crush_mix", 40 }, { "delay_fb", 65 }, { "delay_mix", 40 }, { "delay_time", 5 } },
          1337, 58 },
    };
    return presets;
}

} // namespace bleep
