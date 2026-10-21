#include "WiFiPortal.h"
#include <WiFiManager.h>

// Keep WebServer's HTTP method definitions out of the async server translation unit.
void provision() {
    WiFiManager wifiManager;
    wifiManager.autoConnect("esp-solar-boy");
}
