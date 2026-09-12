#pragma once

#include "Theme.h"
#include <juce_gui_basics/juce_gui_basics.h>

namespace onyverb::ui
{

/** A compact dropdown over the dark theme palettes in Theme.h. Doesn't know
    how to actually apply a theme (label refreshes, LookAndFeel refresh,
    persistence) — that's the editor's job, via `onThemeChanged`. */
class ThemeSwitcher final : public juce::Component
{
public:
    ThemeSwitcher()
    {
        int id = 1;
        for (auto& palette : Theme::getThemePalettes())
            combo.addItem (palette.name, id++);

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
