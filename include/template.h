#ifndef ESP_SOLAR_BOY_TEMPLATE_H
#define ESP_SOLAR_BOY_TEMPLATE_H
#include <Arduino.h>
#include <map>

inline String processTemplate(const String &templateContent, const std::map<String, String> &variables) {
    String result = templateContent;

    for (const auto &pair: variables) {
        result.replace("{{" + pair.first + "}}", pair.second);
    }

    return result;
}


#endif //ESP_SOLAR_BOY_TEMPLATE_H
