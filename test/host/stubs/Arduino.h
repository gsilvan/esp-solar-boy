#pragma once
#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <map>
#include <mutex>
#include <string>
#include <sys/types.h>
#include <type_traits>

class String : public std::string {
public:
    using std::string::string;
    String() = default;
    String(const std::string &s) : std::string(s) {}
    template<class T, typename std::enable_if<std::is_arithmetic<T>::value, int>::type = 0>
    String(T value) : std::string(std::to_string(value)) {}
    long toInt() const { return std::strtol(c_str(), nullptr, 10); }
    void replace(const String &from, const String &to) {
        size_t at = 0;
        while ((at = find(from, at)) != npos) {
            std::string::replace(at, from.length(), to);
            at += to.length();
        }
    }
};

using boolean = bool;
extern uint32_t fakeMillis;
inline uint32_t millis() { return fakeMillis; }
struct FakeSerial {
    template<class... T> void printf(const char *, T...) {}
    template<class T> void print(const T &) {}
    template<class T> void println(const T &) {}
    void println() {}
};
extern FakeSerial Serial;
enum { OUTPUT, LOW, HIGH };
extern std::map<uint8_t, int> pinValues;
inline void pinMode(uint8_t, int) {}
inline void digitalWrite(uint8_t pin, int value) { pinValues[pin] = value; }

using portMUX_TYPE = std::recursive_mutex;
#define portMUX_INITIALIZER_UNLOCKED {}
#define portENTER_CRITICAL(mux) (mux)->lock()
#define portEXIT_CRITICAL(mux) (mux)->unlock()
