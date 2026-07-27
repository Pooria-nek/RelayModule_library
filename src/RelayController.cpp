#include "RelayModule.h"
#include "Helpers.h"
#include "RelayController.h"

RelayController::RelayController(RelayModule &module)
    : module_(module)
{
}

void RelayController::handleSingleChannelControl(const BusproFrame &frame)
{
    if (frame.payloadLen < 4)
        return;

    const uint8_t lightChannelNo = frame.payload[0];
    const uint8_t Brightness = frame.payload[1];  // 0x00 -> low 0x64 -> high
    const uint8_t highRuntime = frame.payload[2]; // TODO
    const uint8_t lowRuntime = frame.payload[3];  // TODO

    if (lightChannelNo < 1 || lightChannelNo > RELAY_CHANNEL_COUNT)
        return;

    const uint8_t channel = lightChannelNo - 1;

    bool newState;
    if (Brightness == 0x00)
        newState = false;
    else if (Brightness == 0x64)
        newState = true;
    else
        newState = false;

    module_.setRelay(channel, newState);

    uint8_t payload[] = {lightChannelNo, relayToBrightness(module_.getRelay(channel))};
    module_.sendResponse(BusproOp::CONTROL_SINGLE.writeResp(), frame.srcAddress, payload, sizeof(payload));
}

void RelayController::handleReversingControl(const BusproFrame &frame)
{
    if (frame.payloadLen < 4)
        return;

    const uint8_t lightChannelNo = frame.payload[0];
    const uint8_t Brightness = frame.payload[1];  // 0x00 -> low 0x64 -> high
    const uint8_t highRuntime = frame.payload[2]; // TODO
    const uint8_t lowRuntime = frame.payload[3];  // TODO

    if (lightChannelNo < 1 || lightChannelNo > RELAY_CHANNEL_COUNT)
        return;

    const uint8_t channel = lightChannelNo - 1;

    bool newState;
    if (Brightness == 0x00)
        newState = true;
    else if (Brightness == 0x64)
        newState = false;
    else
        newState = false;

    module_.setRelay(channel, newState);

    uint8_t payload[] = {lightChannelNo, relayToBrightness(module_.getRelay(channel))};
    module_.sendResponse(BusproOp::CONTROL_REVERSING.resp(), frame.srcAddress, payload, sizeof(payload));
}

void RelayController::handleReadStatusRequest(const BusproFrame &frame)
{
    if (frame.payloadLen > 0)
        return;

    uint8_t payload[] = {
        relayToBrightness(module_.getRelay(0)),
        relayToBrightness(module_.getRelay(1)),
        relayToBrightness(module_.getRelay(2)),
        relayToBrightness(module_.getRelay(3))};

    module_.sendResponse(BusproOp::CONTROL_SINGLE.readResp(), frame.srcAddress, payload, sizeof(payload));
}