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
        rebuildItems();

        combo.setJustificationType (juce::Justification::centred);
        combo.onChange = [this]
        {
            if (onThemeChanged)
                onThemeChanged (combo.getSelectedId() - 1);
        };
        addAndMakeVisible (combo);
    }

    /** Item IDs are palette index + 1, so hiding some entries never shifts
        which palette a menu item maps to. */
    void setSelectedIndex (int index) { combo.setSelectedId (index + 1, juce::dontSendNotification); }

    int getSelectedIndex() const { return combo.getSelectedId() - 1; }

    void setNsfwHidden (bool shouldHide)
    {
        if (nsfwHidden == shouldHide)
            return;

        nsfwHidden = shouldHide;
        auto selectedId = combo.getSelectedId();
        rebuildItems();
        combo.setSelectedId (selectedId, juce::dontSendNotification);
    }

    void resized() override { combo.setBounds (getLocalBounds()); }

    std::function<void (int)> onThemeChanged;

private:
    void rebuildItems()
    {
        combo.clear (juce::dontSendNotification);

        auto& palettes = Theme::getThemePalettes();
        bool lightHeadingAdded = false;

        combo.addSectionHeading ("Dark");
        for (size_t i = 0; i < palettes.size(); ++i)
        {
            auto& palette = palettes[i];

            if (nsfwHidden && Theme::isNsfwTheme (palette))
                continue;

            if (palette.isLight && ! lightHeadingAdded)
            {
                combo.addSeparator();
                combo.addSectionHeading ("Light");
                lightHeadingAdded = true;
            }
            combo.addItem (palette.name, (int) i + 1);
        }
    }

    juce::ComboBox combo;
    bool nsfwHidden = false;
};

} // namespace onyverb::ui
