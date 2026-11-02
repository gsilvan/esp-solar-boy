#pragma once
#include "IPAddress.h"
#include <functional>
#include <vector>

struct Modbus {
    enum ResultCode { EX_SUCCESS, EX_ILLEGAL_ADDRESS = 2, EX_TIMEOUT = 0xe4,
                      EX_GENERAL_FAILURE = 0xe1, EX_CANCEL = 0xe6 };
};

class ModbusIP {
public:
    using Callback = std::function<bool(Modbus::ResultCode, uint16_t, void *)>;
    struct Request {
        uint16_t address;
        uint16_t *destination;
        uint16_t count;
        uint8_t unit;
        Callback callback;
        Modbus::ResultCode result = Modbus::EX_SUCCESS;
        bool ready = false;
        uint16_t words[2] = {};
    };
    static ModbusIP *latest;
    std::vector<Request> requests;
    bool connected = false;
    bool rejectRequests = false;
    ModbusIP() { latest = this; }
    void client() {}
    bool isConnected(IPAddress) const { return connected; }
    void connect(IPAddress, uint16_t) { connected = true; }
    void disconnect(IPAddress) { connected = false; }
    void dropTransactions() {
        for (auto &r : requests) { r.ready = true; r.result = Modbus::EX_CANCEL; }
    }
    uint16_t readHreg(IPAddress, uint16_t address, uint16_t *destination,
                      uint16_t count, Callback callback, uint8_t unit) {
        if (rejectRequests) return 0;
        Request r;
        r.address = address; r.destination = destination; r.count = count;
        r.unit = unit; r.callback = callback;
        requests.push_back(r);
        return requests.size();
    }
    void reply(size_t index, int32_t value, Modbus::ResultCode result = Modbus::EX_SUCCESS) {
        auto &r = requests.at(index);
        r.words[0] = r.count == 1 ? uint16_t(value) : uint16_t(uint32_t(value) >> 16);
        r.words[1] = uint16_t(value);
        r.result = result;
        r.ready = true;
    }
    void task() {
        for (size_t i = 0; i < requests.size();) {
            if (!requests[i].ready) { ++i; continue; }
            const auto r = requests[i];
            requests.erase(requests.begin() + i);
            if (r.result == Modbus::EX_SUCCESS) {
                for (size_t j = 0; j < r.count; ++j) r.destination[j] = r.words[j];
            }
            r.callback(r.result, 1, nullptr);
        }
    }
};
