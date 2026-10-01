#include "WebResourceTests.h"
#include "WebResourceManager.h"
#include <cassert>
#include <iostream>

namespace ppf42 {

bool WebResourceTests::runTests()
{
#if JUCE_WEB_BROWSER
    WebResourceManager manager;

    // Test 1: Root URL sanitization resolves to index.html
    auto res1 = manager.getResource("https://juce.backend/");
    if (!res1.has_value() || res1->data.empty())
    {
        std::cerr << "[WebResourceTests] Failed: https://juce.backend/ did not return a valid resource." << std::endl;
        return false;
    }
    if (!res1->mimeType.contains("text/html"))
    {
        std::cerr << "[WebResourceTests] Failed: Unexpected mime type for index: " << res1->mimeType << std::endl;
        return false;
    }

    // Test 2: Virtual host with query and hash
    auto res2 = manager.getResource("https://juce.backend/index.html?v=1.0#section");
    if (!res2.has_value() || res2->data.empty())
    {
        std::cerr << "[WebResourceTests] Failed: URL with query/hash did not return resource." << std::endl;
        return false;
    }

    // Test 3: Fallback minimal HTML
    auto res3 = manager.getResource("nonexistent_file_404.xyz");
    // Should be nullopt since nonexistent_file_404 is not an asset
    if (res3.has_value())
    {
        std::cerr << "[WebResourceTests] Failed: Nonexistent resource should return nullopt." << std::endl;
        return false;
    }

    std::cout << "[WebResourceTests] All WebResourceManager tests passed successfully." << std::endl;
    return true;
#else
    return true;
#endif
}

} // namespace ppf42
