# Zwei Varianten aus demselben Code:
#  - BugWobble     = Instrument (nur Ausgang, MIDI rein)
#  - BugWobble FX  = Effekt (Audio rein/raus)
# Ein Instrument mit Audio-Eingang bringt Ableton Live beim Laden zum Absturz.
function(bugwobble_variant target product code is_synth categories)
    if(is_synth)
        set(midi_in TRUE)
    else()
        set(midi_in FALSE)
    endif()

    juce_add_plugin(${target}
        COMPANY_NAME "Brain Bugz"
        BUNDLE_ID com.brainbugz.${target}
        PLUGIN_MANUFACTURER_CODE Bbgz
        PLUGIN_CODE ${code}
        FORMATS VST3
        PRODUCT_NAME "${product}"
        VERSION 0.1.0
        IS_SYNTH ${is_synth}
        NEEDS_MIDI_INPUT ${midi_in}
        NEEDS_MIDI_OUTPUT FALSE
        IS_MIDI_EFFECT FALSE
        EDITOR_WANTS_KEYBOARD_FOCUS FALSE
        VST3_CATEGORIES ${categories}
        COPY_PLUGIN_AFTER_BUILD FALSE)

    target_sources(${target} PRIVATE
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp)

    if(is_synth)
        set(synth_flag 1)
    else()
        set(synth_flag 0)
    endif()

    target_compile_definitions(${target} PUBLIC
        BUGWOBBLE_IS_SYNTH=${synth_flag}
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0)

    target_link_libraries(${target}
        PRIVATE
            juce::juce_audio_processors
        PUBLIC
            juce::juce_recommended_config_flags
            juce::juce_recommended_lto_flags
            juce::juce_recommended_warning_flags)
endfunction()

bugwobble_variant(BrainbugzBugWobble   "Brainbugz BugWobble"    Bwob TRUE  "Instrument Synth")
bugwobble_variant(BrainbugzBugWobbleFX "Brainbugz BugWobble FX" Bwof FALSE "Fx Filter")
