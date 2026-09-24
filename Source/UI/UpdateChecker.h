#pragma once

#include <juce_core/juce_core.h>
#include <juce_events/juce_events.h>
#include <array>
#include <functional>

namespace onyverb::ui
{

/** Asks GitHub for the latest published release on a background thread
    and reports back on the message thread — never blocks the UI or the
    audio thread. Only one request runs at a time. */
class UpdateChecker final : private juce::Thread
{
public:
    struct Result
    {
        enum class Status { upToDate, updateAvailable, failed };
        Status status = Status::failed;
        juce::String latestTag;
        juce::String releaseUrl;
    };

    UpdateChecker() : juce::Thread ("ONYVerb update check") {}
    ~UpdateChecker() override { stopThread (8000); }

    /** Called on the message thread once a check finishes. */
    std::function<void (const Result&)> onResult;

    void check (juce::String currentVersion, juce::String apiUrl)
    {
        if (isThreadRunning())
            return;

        installedVersion = std::move (currentVersion);
        endpoint = std::move (apiUrl);
        startThread();
    }

    /** "v0.1.8" -> {0,1,8}; anything unparseable reads as 0. */
    static std::array<int, 3> parseVersion (const juce::String& text)
    {
        std::array<int, 3> parts { 0, 0, 0 };
        juce::StringArray tokens;
        tokens.addTokens (text.trim().trimCharactersAtStart ("vV"), ".", "");

        for (int i = 0; i < 3 && i < tokens.size(); ++i)
            parts[(size_t) i] = tokens[i].getIntValue();

        return parts;
    }

    static bool isNewer (const juce::String& candidate, const juce::String& installed)
    {
        return parseVersion (candidate) > parseVersion (installed);
    }

private:
    void run() override
    {
        Result result;
        int statusCode = 0;

        auto options = juce::URL::InputStreamOptions (juce::URL::ParameterHandling::inAddress)
                           .withConnectionTimeoutMs (5000)
                           .withStatusCode (&statusCode)
                           .withExtraHeaders ("User-Agent: ONY-Verb-Update-Check\r\nAccept: application/vnd.github+json\r\n");

        if (auto stream = juce::URL (endpoint).createInputStream (options))
        {
            if (statusCode == 200)
            {
                auto json = juce::JSON::parse (stream->readEntireStreamAsString());
                auto tag = json["tag_name"].toString();

                if (tag.isNotEmpty())
                {
                    result.latestTag = tag;
                    result.releaseUrl = json["html_url"].toString();
                    result.status = isNewer (tag, installedVersion) ? Result::Status::updateAvailable
                                                                    : Result::Status::upToDate;
                }
            }
        }

        juce::MessageManager::callAsync ([weak = juce::WeakReference<UpdateChecker> (this), result]
        {
            if (auto* self = weak.get())
                if (self->onResult)
                    self->onResult (result);
        });
    }

    juce::String installedVersion, endpoint;

    JUCE_DECLARE_WEAK_REFERENCEABLE (UpdateChecker)
};

} // namespace onyverb::ui
