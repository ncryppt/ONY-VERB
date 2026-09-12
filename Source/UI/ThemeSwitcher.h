#pragma once

#include "Theme.h"
#include <juce_gui_basics/juce_gui_basics.h>

namespace onyverb::ui
{

/** A compact dropdown over the theme palettes in Theme.h, grouped into Dark
    and Light sections. Doesn't know how to actually apply a theme (label
    refreshes, LookAndFeel refresh, persistence) — that's the editor's job,
    via `onThemeChanged`. */
class ThemeSwitcher final : public juce::Component
{
public:
    ThemeSwitcher()
    {
        auto& palettes = Theme::getThemePalettes();
        int id = 1;
        bool lightHeadingAdded = false;

        combo.addSectionHeading ("Dark");
        for (auto& palette : palettes)
        {
            if (palette.isLight && ! lightHeadingAdded)
            {
                combo.addSeparator();
                combo.addSectionHeading ("Light");
                lightHeadingAdded = true;
            }
            combo.addItem (palette.name, id++);
        }

        combo.setJustificationType (juce::Justification::centred);
        combo.onChange = [this]
        {
            if (onThemeChanged)
                onThemeChanged (combo.getSelectedItemIndex());
        };
        addAndMakeVisible (combo);
    }

    void setSelectedIndex (int index) { combo.setSelectedItemIndex (index, juce::dontSendNotification); }

    void resized() override { combo.setBounds (getLocalBounds()); }

    std::function<void (int)> onThemeChanged;

private:
    juce::ComboBox combo;
};

} // namespace onyverb::ui
