#ifndef ESP_SOLAR_BOY_TEMPLATE_H
#define ESP_SOLAR_BOY_TEMPLATE_H

#include <Arduino.h>
#include <map>

inline String processTemplate(const String &templateContent, const std::map<String, String> &variables) {
    // Estimate final size to reduce reallocations
    size_t estimatedSize = templateContent.length();
    for (const auto &pair: variables) {
        estimatedSize += pair.second.length() - (pair.first.length() + 4); // Account for {{key}} replacement
    }

    String result;
    result.reserve(estimatedSize); // Preallocate memory
    result = templateContent;

    for (const auto &pair: variables) {
        result.replace("{{" + pair.first + "}}", pair.second);
    }

    return result;
}


#endif //ESP_SOLAR_BOY_TEMPLATE_H
