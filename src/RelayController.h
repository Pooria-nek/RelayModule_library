#pragma once

#include "BusproFrame.h"

class RelayModule;

class RelayController
{
public:
    explicit RelayController(RelayModule &module);

    void handleSingleChannelControl(const BusproFrame &frame);
    void handleReversingControl(const BusproFrame &frame);
    void handleReadStatusRequest(const BusproFrame &frame);

private:
    RelayModule &module_;

    uint8_t relayToBrightness(uint8_t brighness);
};