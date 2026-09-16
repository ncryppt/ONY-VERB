#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "Theme.h"
#include "../Presets/FactoryPresets.h"
#include <functional>

namespace onyverb::ui
{

/** Top-center preset browser: dropdown + prev/next arrows over the 12
    factory presets (two per mode, applied directly to the parameter tree),
    plus real save/load of user presets as XML files under the user's
    application-data directory. */
class PresetBar final : public juce::Component
{
public:
    explicit PresetBar (juce::AudioProcessorValueTreeState& state) : apvts (state)
    {
        presetsDir = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                         .getChildFile ("ONYVA").getChildFile ("ONY Verb").getChildFile ("Presets");
        presetsDir.createDirectory();

        prevButton.setButtonText ("<");
        nextButton.setButtonText (">");
        saveButton.setButtonText ("Save As...");
        loadButton.setButtonText ("Import...");

        for (auto* b : { &prevButton, &nextButton, &saveButton, &loadButton })
        {
            b->getProperties().set ("pill", true);
            addAndMakeVisible (b);
        }

        prevButton.onClick = [this] { stepPreset (-1); };
        nextButton.onClick = [this] { stepPreset (1); };
        saveButton.onClick = [this] { saveAsNewPreset(); };
        loadButton.onClick = [this] { importPresetFromFile(); };

        combo.setJustificationType (juce::Justification::centred);
        combo.onChange = [this] { onPresetSelected(); };
        addAndMakeVisible (combo);

        refreshPresetList();
    }

    void resized() override
    {
        auto b = getLocalBounds();
        prevButton.setBounds (b.removeFromLeft (28).reduced (2));
        nextButton.setBounds (b.removeFromRight (28).reduced (2));
        loadButton.setBounds (b.removeFromRight (86).reduced (2));
        saveButton.setBounds (b.removeFromRight (96).reduced (2));
        combo.setBounds (b.reduced (4, 2));
    }

    /** Lets the editor wire shared click-feedback (particle bursts) onto
        every button here without this class needing to know anything about
        that — the theme switcher's combo box isn't included since it isn't
        a Button. */
    void forEachButton (const std::function<void (juce::Button&)>& fn)
    {
        for (auto* b : { &prevButton, &nextButton, &saveButton, &loadButton })
            fn (*b);
    }

private:
    void refreshPresetList()
    {
        combo.clear (juce::dontSendNotification);

        // Presets are grouped into a heading per pack (an artist series, or
        // just "Factory") — assumes presets sharing a pack already sit
        // consecutively in getFactoryPresets(), so a heading only needs to
        // start whenever the pack name actually changes.
        int id = 1;
        const char* currentPack = nullptr;
        for (auto& preset : getFactoryPresets())
        {
            if (currentPack == nullptr || juce::String (currentPack) != juce::String (preset.pack))
            {
                if (currentPack != nullptr)
                    combo.addSeparator();
                combo.addSectionHeading (preset.pack);
                currentPack = preset.pack;
            }
            combo.addItem (preset.name, id++);
        }
        factoryCount = id - 1;

        presetFiles = presetsDir.findChildFiles (juce::File::findFiles, false, "*.onyverbpreset");
        presetFiles.sort();

        if (! presetFiles.isEmpty())
        {
            if (factoryCount > 0)
                combo.addSeparator();
            combo.addSectionHeading ("User");
            for (auto& f : presetFiles)
                combo.addItem (f.getFileNameWithoutExtension(), id++);
        }
    }

    void stepPreset (int direction)
    {
        auto numItems = combo.getNumItems();
        if (numItems <= 1) return;
        auto newIndex = (combo.getSelectedItemIndex() + direction + numItems) % numItems;
        combo.setSelectedItemIndex (newIndex);
    }

    void onPresetSelected()
    {
        auto index = combo.getSelectedItemIndex();

        if (juce::isPositiveAndBelow (index, factoryCount))
        {
            applyFactoryPreset (apvts, getFactoryPresets()[(size_t) index]);
            return;
        }

        auto fileIndex = index - factoryCount;
        if (juce::isPositiveAndBelow (fileIndex, presetFiles.size()))
            if (auto xml = juce::XmlDocument::parse (presetFiles.getReference (fileIndex)))
                apvts.replaceState (juce::ValueTree::fromXml (*xml));
    }

    void saveAsNewPreset()
    {
        auto* alert = new juce::AlertWindow ("Save Preset", "Preset name:", juce::MessageBoxIconType::NoIcon);
        alert->addTextEditor ("name", "My Preset");
        alert->addButton ("Save", 1, juce::KeyPress (juce::KeyPress::returnKey));
        alert->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));

        alert->enterModalState (true, juce::ModalCallbackFunction::create ([this, alert] (int result)
        {
            if (result == 1)
            {
                auto name = alert->getTextEditorContents ("name").trim();
                if (name.isNotEmpty())
                {
                    auto file = presetsDir.getChildFile (name + ".onyverbpreset");
                    if (auto state = apvts.copyState(); state.isValid())
                        if (auto xml = state.createXml())
                            xml->writeTo (file);
                    refreshPresetList();
                    combo.setText (name, juce::dontSendNotification);
                }
            }
            delete alert;
        }), false);
    }

    void importPresetFromFile()
    {
        fileChooser = std::make_unique<juce::FileChooser> ("Import Preset", presetsDir, "*.onyverbpreset");
        fileChooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
            [this] (const juce::FileChooser& fc)
            {
                auto file = fc.getResult();
                if (file.existsAsFile())
                    if (auto xml = juce::XmlDocument::parse (file))
                        apvts.replaceState (juce::ValueTree::fromXml (*xml));
            });
    }

    juce::AudioProcessorValueTreeState& apvts;
    juce::File presetsDir;
    juce::Array<juce::File> presetFiles;
    int factoryCount = 0;
    juce::ComboBox combo;
    juce::TextButton prevButton, nextButton, saveButton, loadButton;
    std::unique_ptr<juce::FileChooser> fileChooser;
};

} // namespace onyverb::ui
