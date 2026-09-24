#pragma once

namespace onyverb
{

/** Matches the current GitHub release tag — bump this by hand alongside
    each release until this is wired up to the actual build/CI version.
    The update check compares it against the latest published release. */
constexpr const char* pluginVersion = "v0.1.9";

/** Where the update check looks for the newest release. Swap this for the
    store's own version feed once the product is sold through Shopify. */
constexpr const char* latestReleaseApiUrl = "https://api.github.com/repos/ncryppt/ONY-VERB/releases/latest";

} // namespace onyverb
