#include "RelayModule.h"

RelayModule::RelayModule(
    BusproTransport &bus,
    MemoryCore &flash,
    uint32_t sectorAddress,
    const uint8_t relayPins[RELAY_CHANNEL_COUNT],
    bool activeHigh
    // ISceneStore *sceneStore
    )
    : bus_(bus),
      flash_(flash),
      memoryaddress_(sectorAddress),
      activeHigh_(activeHigh),
      controllerFunction(*this)
{
    copyMcuUID(uid_);
    for (uint8_t i = 0; i < RELAY_CHANNEL_COUNT; i++)
        relayPins_[i] = relayPins[i];
}

bool RelayModule::begin()
{
    for (uint8_t i = 0; i < RELAY_CHANNEL_COUNT; ++i)
    {
        pinMode(relayPins_[i], OUTPUT);
        applyRelayHardware(i); // ensure relays start OFF and consistent with relayState_
    }

    // flash_.eraseSector(memoryaddress_);

    if (firstime())
    {

        init();
    }

    return syncValues();
}

bool RelayModule::firstime()
{
    uint8_t fuid_[12];
    flash_.read(flash_.findAdrress(memoryaddress_, MemoryAdress::DEVICE_MAC_ADDRESS), fuid_, 12);

    // bool sameUid = (memcmp(fuid_, getMcuUID(), 12) == 0);

    uint16_t fdevType;
    flash_.readObject(flash_.findAdrress(memoryaddress_, MemoryAdress::DEVICE_TYPE), fdevType);

    // bool sameType = (devType_ == fdevType);

    if ((!bufferEquals(fuid_, uid_, 12)) || (devType_ != fdevType))
    {
        return true;
    }
    else
    {
        return false;
    }
}

bool RelayModule::init()
{
    flash_.writeObject(flash_.findAdrress(memoryaddress_, MemoryAdress::DEVICE_ADDRESS), deviceAddress_);
    flash_.writeObject(flash_.findAdrress(memoryaddress_, MemoryAdress::DEVICE_TYPE), devType_);

    flash_.writeObject(flash_.findAdrress(memoryaddress_, MemoryAdress::DEVICE_MAC_ADDRESS), uid_);

    flash_.writeObject(flash_.findAdrress(memoryaddress_, MemoryAdress::DEVICE_REMARK), "4Relay");
    flash_.writeObject(flash_.findAdrress(memoryaddress_, MemoryAdress::DEVICE_HARDWARE_VER), "hardware");
    flash_.writeObject(flash_.findAdrress(memoryaddress_, MemoryAdress::DEVICE_SOFTWARE_VER), "software");

    char remark[20];
    for (size_t i = 1; i <= RELAY_CHANNEL_COUNT; i++)
    {
        // uint8_t channel = i + 1;
        snprintf(remark, sizeof(remark), "Relay %u", static_cast<unsigned>(i));
        flash_.writeObject(flash_.findAdrress(memoryaddress_, MemoryAdress::channelRemark(i)), remark);

        flash_.writeObject(flash_.findAdrress(memoryaddress_, MemoryAdress::channelAddress(MemoryAdress::RELAY_CHANNEL_ENABLE, i)), uint8_t{0x01});
        flash_.writeObject(flash_.findAdrress(memoryaddress_, MemoryAdress::channelAddress(MemoryAdress::RELAY_CHANNEL_ONDELAY, i)), uint8_t{0x00});
        flash_.writeObject(flash_.findAdrress(memoryaddress_, MemoryAdress::channelAddress(MemoryAdress::RELAY_CHANNEL_ONPROTECT, i)), uint8_t{0x00});
    }

    for (size_t z = 0; z < RELAY_CHANNEL_COUNT; z++)
    {
        snprintf(remark, sizeof(remark), "Zone %u", static_cast<unsigned>(z));
        flash_.writeObject(flash_.findAdrress(memoryaddress_, MemoryAdress::zoneRemark(memoryaddress_, z)), remark);

        for (size_t s = 0; s < (RELAY_CHANNEL_COUNT * 2); s++)
        {
            /* code */
        }
    }

    return true;
}

bool RelayModule::syncValues()
{
    flash_.readObject(flash_.findAdrress(memoryaddress_, MemoryAdress::DEVICE_ADDRESS), deviceAddress_);
    flash_.readObject(flash_.findAdrress(memoryaddress_, MemoryAdress::DEVICE_MAC_ADDRESS), uid_);
    flash_.readObject(flash_.findAdrress(memoryaddress_, MemoryAdress::RELAY_CHANNEL_ENABLE), relayEnable_);
    flash_.readObject(flash_.findAdrress(memoryaddress_, MemoryAdress::RELAY_CHANNEL_ONDELAY), relayDelay_);
    flash_.readObject(flash_.findAdrress(memoryaddress_, MemoryAdress::RELAY_CHANNEL_ONPROTECT), relayProtect_);

    maxZone();

    return true;
}

uint8_t RelayModule::maxZone()
{
    uint8_t payload[RELAY_CHANNEL_COUNT];

    flash_.read(flash_.findAdrress(memoryaddress_, MemoryAdress::RELAY_CHANNEL_ZONE), payload, RELAY_CHANNEL_COUNT);

    for (uint8_t i = 0; i < RELAY_CHANNEL_COUNT; ++i)
    {
        if (payload[i] > sceneCount)
        {
            sceneCount = payload[i];
        }
    }

    return sceneCount;
}

void RelayModule::process(const BusproFrame &frame)
{
    // if (frame.devType != devType_)
    //     return;

    if (frame.dstAddress == 0xFFFF) // universal comands
    {
        switch (frame.opCode)
        {
        case BusproOp::DEVICE_SEARCH_HDL.req():
            handleSearchDevice(frame);
            break;

        case BusproOp::DEVICE_REMARK.readReq():
            handleReadDeviceRemark(frame);
            break;
        }
    }
    else if (frame.dstAddress == deviceAddress_) // my comands
    {
        switch (frame.opCode)
        {
            /////////////////////////////// UNIVERSAL REQUEST ///////////////////////////////

        case BusproOp::DEVICE_FIRMWARE.req():
            handleReadFirmware(frame);
            break;

        case BusproOp::DEVICE_HARDWARE.req():
            handleReadHardware(frame);
            break;

        case BusproOp::DEVICE_FINDIT.req():
            handleFindDevice(frame);
            break;

        case BusproOp::DEVICE_SEARCH_HDL.req():
            handleSearchDevice(frame);
            break;

        case BusproOp::DEVICE_MAC_ADDRESS.readReq():
            handleReadMacaddress(frame);
            break;

        case BusproOp::DEVICE_MAC_ADDRESS.writeReq():
            handleModifyMacaddress(frame);
            break;

        case BusproOp::DEVICE_REMARK.readReq():
            handleReadDeviceRemark(frame);
            break;

        case BusproOp::DEVICE_REMARK.writeReq():
            handleModifyDeviceRemark(frame);
            break;

            /////////////////////////////// DEVICE REQUEST ///////////////////////////////

            /////////////////////////////// RELAY BASIC INFORMATION ///////////////////////////////

        case BusproOp::CHANNEL_REMARK.readReq():
            handleReadChannelRemark(frame);
            break;

        case BusproOp::CHANNEL_REMARK.writeReq():
            handleModifyChannelRemark(frame);
            break;

        case BusproOp::CHANNEL_ONDELAY.readReq():
            handleReadChannelOndelay(frame);
            break;

        case BusproOp::CHANNEL_ONDELAY.writeReq():
            handleModifyChannelOndelay(frame);
            break;

        case BusproOp::CHANNEL_ONPROTECT.readReq():
            handleReadChannelOnprotect(frame);
            break;

        case BusproOp::CHANNEL_ONPROTECT.writeReq():
            handleModifyChannelOnprotect(frame);
            break;

        case BusproOp::RELAY_CHANNEL_ENABLE.readReq():
            handleReadChannelEnable(frame);
            break;

        case BusproOp::RELAY_CHANNEL_ENABLE.writeReq():
            handleModifyChannelEnable(frame);
            break;

            /////////////////////////////// RELAY ZONE SETTING ///////////////////////////////

        case BusproOp::ZONE_MEMBERS.readReq():
            handleReadZone(frame);
            break;

        case BusproOp::ZONE_MEMBERS.writeReq():
            handleModifyZone(frame);
            break;

        case BusproOp::ZONE_REMARK.readReq():
            handleReadZoneRemark(frame);
            break;

        case BusproOp::ZONE_REMARK.writeReq():
            handleModifyZoneRemark(frame);
            break;

            /////////////////////////////// RELAY SCENE SETTING ///////////////////////////////

        case BusproOp::SCENE_READ.req():
            handleSceneRead(frame);
            break;

        case BusproOp::SCENE_MODIFY.req():
            handleSceneModify(frame);
            break;

        case BusproOp::SCENE_REMARK.readReq():
            handleReadSceneRemark(frame);
            break;

        case BusproOp::SCENE_REMARK.writeReq():
            handleModifySceneRemark(frame);
            break;

        case BusproOp::SCENE_POWERON_EN.readReq():
            handleSceneResumeENRead(frame);
            break;

        case BusproOp::SCENE_POWERON_EN.writeReq():
            handleSceneResumeENModify(frame);
            break;

        case BusproOp::SCENE_POWERON_NUM.readReq():
            handleSceneResumeNumRead(frame);
            break;

        case BusproOp::SCENE_POWERON_NUM.writeReq():
            handleSceneResumeNumModify(frame);
            break;

            /////////////////////////////// RELAY CURTAIN ///////////////////////////////

        case BusproOp::CURTAIN_CONFIG.readReq():
            handleCurtainRead(frame);
            break;

        case BusproOp::CURTAIN_CONFIG.writeReq():
            handleCurtainModify(frame);
            break;

            /////////////////////////////// RELAY CONTROLL ///////////////////////////////

        case BusproOp::CONTROL_SINGLE.readReq():
            controllerFunction.handleReadStatusRequest(frame);
            break;

        case BusproOp::CONTROL_SINGLE.writeReq():
            controllerFunction.handleSingleChannelControl(frame);
            break;

        case BusproOp::CONTROL_REVERSING.req():
            controllerFunction.handleReversingControl(frame);
            break;
        }
    }
}

void RelayModule::sendResponse(uint16_t opcode, uint16_t dst, const uint8_t *payload, uint8_t payloadLen)
{
    bus_.send(deviceAddress_, devType_, opcode, dst, payload, payloadLen);
}

void RelayModule::applyRelayHardware(uint8_t channel)
{
    const bool on = relayState_[channel];
    const bool pinLevel = activeHigh_ ? on : !on;
    if (relayEnable_[channel] == true)
    {
        digitalWrite(relayPins_[channel], pinLevel ? HIGH : LOW);
    }
}

bool RelayModule::setRelay(uint8_t channel, bool on)
{
    if (channel >= RELAY_CHANNEL_COUNT)
        return false;
    relayState_[channel] = on;
    applyRelayHardware(channel);
    return true;
}

bool RelayModule::getRelay(uint8_t channel) const
{
    if (channel >= RELAY_CHANNEL_COUNT)
        return false;
    return relayState_[channel];
}

void RelayModule::setAllRelays(const bool states[RELAY_CHANNEL_COUNT])
{
    for (uint8_t i = 0; i < RELAY_CHANNEL_COUNT; ++i)
    {
        relayState_[i] = states[i];
        applyRelayHardware(i);
    }
}

// void RelayModule::readMcuUID()
// {
//     memcpy(uid_, reinterpret_cast<const void *>(0x1FFFF7E8U), sizeof(uid_));
// }

////////////////////////////////////////////////////////////////////////////////
/////////////////////////////// DEVICE HANDLERS ////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

/////////////////////////////// RELAY BASIC INFORMATION ///////////////////////////////

void RelayModule::handleModifyChannelRemark(const BusproFrame &frame)
{
    if (frame.payloadLen != 21)
        return;

    flash_.update(flash_.findAdrress(memoryaddress_, MemoryAdress::channelRemark(frame.payload[0])), frame.payload + 1, frame.payloadLen - 1);

    uint8_t payload[1] = {0xF8};
    sendResponse(BusproOp::CHANNEL_REMARK.writeResp(), frame.srcAddress, payload, sizeof(payload));
}

void RelayModule::handleReadChannelRemark(const BusproFrame &frame)
{
    if (frame.payloadLen != 1)
        return;

    uint8_t payload[21];

    payload[0] = frame.payload[0];

    flash_.read(flash_.findAdrress(memoryaddress_, MemoryAdress::channelRemark(frame.payload[0])), payload + 1, 20);
    sendResponse(BusproOp::CHANNEL_REMARK.readResp(), frame.srcAddress, payload, sizeof(payload));
}

void RelayModule::handleModifyChannelOndelay(const BusproFrame &frame)
{
    if (frame.payloadLen != RELAY_CHANNEL_COUNT)
        return;

    flash_.update(flash_.findAdrress(memoryaddress_, MemoryAdress::RELAY_CHANNEL_ONDELAY), frame.payload, frame.payloadLen);

    uint8_t payload[1] = {0xF8};
    sendResponse(BusproOp::CHANNEL_ONDELAY.writeResp(), frame.srcAddress, payload, sizeof(payload));
}

void RelayModule::handleReadChannelOndelay(const BusproFrame &frame)
{
    if (frame.payloadLen != 0)
        return;

    uint8_t payload[4];

    flash_.read(flash_.findAdrress(memoryaddress_, MemoryAdress::RELAY_CHANNEL_ONDELAY), payload, RELAY_CHANNEL_COUNT);
    sendResponse(BusproOp::CHANNEL_ONDELAY.readResp(), frame.srcAddress, payload, sizeof(payload));
}

void RelayModule::handleModifyChannelOnprotect(const BusproFrame &frame)
{
    if (frame.payloadLen != RELAY_CHANNEL_COUNT)
        return;

    flash_.update(flash_.findAdrress(memoryaddress_, MemoryAdress::RELAY_CHANNEL_ONPROTECT), frame.payload, frame.payloadLen);

    uint8_t payload[1] = {0xF8};
    sendResponse(BusproOp::CHANNEL_ONPROTECT.writeResp(), frame.srcAddress, payload, sizeof(payload));
}

void RelayModule::handleReadChannelOnprotect(const BusproFrame &frame)
{
    if (frame.payloadLen != 0)
        return;

    uint8_t payload[4];

    flash_.read(flash_.findAdrress(memoryaddress_, MemoryAdress::RELAY_CHANNEL_ONPROTECT), payload, RELAY_CHANNEL_COUNT);
    sendResponse(BusproOp::CHANNEL_ONPROTECT.readResp(), frame.srcAddress, payload, sizeof(payload));
}

void RelayModule::handleModifyChannelEnable(const BusproFrame &frame)
{
    if (frame.payloadLen != (RELAY_CHANNEL_COUNT + 1) &&
        frame.payload[0] == RELAY_CHANNEL_COUNT)
        return;

    flash_.update(flash_.findAdrress(memoryaddress_, MemoryAdress::RELAY_CHANNEL_ENABLE), frame.payload + 1, RELAY_CHANNEL_COUNT);

    uint8_t payload[1] = {0xF8};
    sendResponse(BusproOp::RELAY_CHANNEL_ENABLE.writeResp(), frame.srcAddress, payload, sizeof(payload));
}

void RelayModule::handleReadChannelEnable(const BusproFrame &frame)
{
    if (frame.payloadLen != 0)
        return;

    uint8_t payload[5];
    payload[0] = RELAY_CHANNEL_COUNT;

    flash_.read(flash_.findAdrress(memoryaddress_, MemoryAdress::RELAY_CHANNEL_ENABLE), payload + 1, RELAY_CHANNEL_COUNT);
    sendResponse(BusproOp::RELAY_CHANNEL_ENABLE.readResp(), frame.srcAddress, payload, sizeof(payload));
}

/////////////////////////////// RELAY ZONE SETTING ///////////////////////////////

void RelayModule::handleReadZone(const BusproFrame &frame)
{
    if (frame.payloadLen != 1)
        return;

    uint8_t payload[5 + RELAY_CHANNEL_COUNT];

    payload[0] = 0x01; // UNKNOWN
    payload[1] = 0xD0; // UNKNOWN
    payload[2] = static_cast<uint8_t>(deviceAddress_ >> 8);
    payload[3] = static_cast<uint8_t>(deviceAddress_ & 0xFF);

    flash_.read(flash_.findAdrress(memoryaddress_, MemoryAdress::RELAY_CHANNEL_ZONE), payload + 5, RELAY_CHANNEL_COUNT);

    // Calculate the highest assigned zone number.
    uint8_t maxZone = 0;
    for (uint8_t i = 0; i < RELAY_CHANNEL_COUNT; ++i)
    {
        if (payload[5 + i] > maxZone)
        {
            maxZone = payload[5 + i];
            sceneCount = maxZone;
        }
    }

    payload[4] = maxZone;

    sendResponse(BusproOp::ZONE_MEMBERS.readResp(), frame.srcAddress, payload, sizeof(payload));
}

void RelayModule::handleModifyZone(const BusproFrame &frame)
{
    if (frame.payloadLen != (3 + RELAY_CHANNEL_COUNT))
        return;

    flash_.update(flash_.findAdrress(memoryaddress_, MemoryAdress::RELAY_CHANNEL_ZONE), frame.payload + 3, RELAY_CHANNEL_COUNT);

    uint8_t payload[1] = {0xF8};
    sendResponse(BusproOp::ZONE_MEMBERS.writeResp(), frame.srcAddress, payload, sizeof(payload));
}

void RelayModule::handleReadZoneRemark(const BusproFrame &frame)
{
    if (frame.payloadLen != 1)
        return;

    uint8_t payload[21];

    payload[0] = frame.payload[0];

    flash_.read(flash_.findAdrress(memoryaddress_, MemoryAdress::zoneRemark(memoryaddress_, frame.payload[0])), payload + 1, 20);

    sendResponse(BusproOp::ZONE_REMARK.readResp(), frame.srcAddress, payload, sizeof(payload));
}

void RelayModule::handleModifyZoneRemark(const BusproFrame &frame)
{
    if (frame.payloadLen != (1 + 20))
        return;

    flash_.update(flash_.findAdrress(memoryaddress_, MemoryAdress::zoneRemark(memoryaddress_, frame.payload[0])), frame.payload + 1, 20);

    uint8_t payload[1] = {frame.payload[0]};
    sendResponse(BusproOp::ZONE_REMARK.writeResp(), frame.srcAddress, payload, sizeof(payload));
}

/////////////////////////////// RELAY SCENE SETTING ///////////////////////////////

void RelayModule::handleSceneRead(const BusproFrame &frame)
{
    if (frame.payloadLen != 2)
        return;

    uint8_t zone = frame.payload[0];
    uint8_t scene = frame.payload[1];

    uint8_t payload[4 + RELAY_CHANNEL_COUNT];

    flash_.read(flash_.findAdrress(memoryaddress_, MemoryAdress::sceneRuntime(memoryaddress_, zone, scene)), payload + 2, 2);
    flash_.read(flash_.findAdrress(memoryaddress_, MemoryAdress::sceneChannel(memoryaddress_, zone, scene)), payload + 4, RELAY_CHANNEL_COUNT);

    sendResponse(BusproOp::SCENE_READ.resp(), frame.srcAddress, payload, sizeof(payload));
}

void RelayModule::handleSceneModify(const BusproFrame &frame)
{
}

void RelayModule::handleReadSceneRemark(const BusproFrame &frame)
{
    if (frame.payloadLen != 2)
        return;

    uint8_t payload[22];

    payload[0] = frame.payload[0];
    payload[1] = frame.payload[1];

    // flash_.read(flash_.findAdrress(memoryaddress_,MemoryAdress::sceneRemark(frame.payload[0])), payload + 1, 20);

    sendResponse(BusproOp::SCENE_REMARK.readResp(), frame.srcAddress, payload, sizeof(payload));
}

void RelayModule::handleModifySceneRemark(const BusproFrame &frame)
{
    if (frame.payloadLen != (1 + 20))
        return;

    // flash_.update(flash_.findAdrress(memoryaddress_,MemoryAdress::sceneRemark(frame.payload[0])), frame.payload + 1, 20);

    uint8_t payload[1] = {frame.payload[0]};
    sendResponse(BusproOp::SCENE_REMARK.writeResp(), frame.srcAddress, payload, sizeof(payload));
}

void RelayModule::handleSceneResumeENRead(const BusproFrame &frame)
{
    if (frame.payloadLen != 0)
        return;

    uint8_t payload[sceneCount];

    flash_.read(flash_.findAdrress(memoryaddress_, MemoryAdress::sceneResumeEN(memoryaddress_, frame.payload[0])), payload, sceneCount);

    sendResponse(BusproOp::SCENE_POWERON_EN.readResp(), frame.srcAddress, payload, sizeof(payload));
}

void RelayModule::handleSceneResumeENModify(const BusproFrame &frame)
{
    // if (frame.payloadLen != 0)
    //     return;

    uint8_t payload[1] = {0xF8};

    sendResponse(BusproOp::SCENE_POWERON_EN.writeResp(), frame.srcAddress, payload, sizeof(payload));
}

void RelayModule::handleSceneResumeNumRead(const BusproFrame &frame)
{
    if (frame.payloadLen != 0)
        return;

    uint8_t payload[sceneCount];

    flash_.read(flash_.findAdrress(memoryaddress_, MemoryAdress::sceneResumeEN(memoryaddress_, frame.payload[0])), payload, sceneCount);

    sendResponse(BusproOp::SCENE_POWERON_NUM.readResp(), frame.srcAddress, payload, sizeof(payload));
}

void RelayModule::handleSceneResumeNumModify(const BusproFrame &frame)
{
    // if (frame.payloadLen != 0)
    //     return;

    uint8_t payload[1] = {0xF8};

    sendResponse(BusproOp::SCENE_POWERON_NUM.writeResp(), frame.srcAddress, payload, sizeof(payload));
}

/////////////////////////////// RELAY CURTAIN ///////////////////////////////

void RelayModule::handleCurtainRead(const BusproFrame &frame)
{
    if (frame.payloadLen != 1)
        return;

    uint8_t payload[1 + CURTAIN_CHANNEL_COUNT];

    sendResponse(BusproOp::CURTAIN_CONFIG.readResp(), frame.srcAddress, payload, sizeof(payload));
}

void RelayModule::handleCurtainModify(const BusproFrame &frame)
{
    uint8_t payload[1] = {0xF8};

    sendResponse(BusproOp::CURTAIN_CONFIG.writeResp(), frame.srcAddress, payload, sizeof(payload));
}

/////////////////////////////////////////////////////////////////////////////////
/////////////////////////////// UNIVERSAL REQUEST ///////////////////////////////
/////////////////////////////////////////////////////////////////////////////////

void RelayModule::handleReadFirmware(const BusproFrame &frame)
{
    if (frame.payloadLen != 0)
        return;

    uint8_t payload[32] = {0x00};

    flash_.readObject(flash_.findAdrress(memoryaddress_, MemoryAdress::DEVICE_SOFTWARE_VER), payload);

    sendResponse(BusproOp::DEVICE_FIRMWARE.resp(), frame.srcAddress, payload, sizeof(payload));
}

void RelayModule::handleReadHardware(const BusproFrame &frame)
{
    if (frame.payloadLen != 0)
        return;

    uint8_t payload[32] = {0x00};

    flash_.readObject(flash_.findAdrress(memoryaddress_, MemoryAdress::DEVICE_HARDWARE_VER), payload);

    sendResponse(BusproOp::DEVICE_HARDWARE.resp(), frame.srcAddress, payload, sizeof(payload));
}

void RelayModule::handleFindDevice(const BusproFrame &frame)
{
    if (frame.payloadLen != 0)
        return;

    uint8_t payload[8] = {0x00};

    sendResponse(BusproOp::DEVICE_FINDIT.resp(), frame.srcAddress, payload, sizeof(payload));
}

void RelayModule::handleSearchDevice(const BusproFrame &frame)
{
    // if (frame.payloadLen != 4)
    //     return;

    // if (frame.payload)
    // {
    //     /* code */
    // }

    uint8_t payload[32] = {0x00};

    payload[0] = frame.payload[0];
    payload[1] = frame.payload[1];

    sendResponse(BusproOp::DEVICE_SEARCH_HDL.resp(), frame.srcAddress, payload, sizeof(payload));
}

void RelayModule::handleReadMacaddress(const BusproFrame &frame)
{
    if (frame.payloadLen != 0)
        return;

    uint8_t payload[8] = {0x00};

    copyArray(getMcuUID(), payload);

    sendResponse(BusproOp::DEVICE_MAC_ADDRESS.readResp(), frame.srcAddress, payload, sizeof(payload));
}

void RelayModule::handleModifyMacaddress(const BusproFrame &frame)
{
    if (frame.payloadLen != 10)
        return;

    if (!bufferEquals(frame.payload, getMcuUID()))
        return;

    uint16_t address = (static_cast<uint16_t>(frame.payload[8]) << 8) | frame.payload[9];

    flash_.updateObject(flash_.findAdrress(memoryaddress_, MemoryAdress::DEVICE_ADDRESS), address);
    syncValues();

    uint8_t payload[1] = {0xF8};
    sendResponse(BusproOp::DEVICE_MAC_ADDRESS.writeResp(), frame.srcAddress, payload, sizeof(payload));
}

void RelayModule::handleReadDeviceRemark(const BusproFrame &frame)
{
    if (frame.payloadLen != 0)
        return;

    uint8_t payload[20];

    flash_.readObject(flash_.findAdrress(memoryaddress_, MemoryAdress::DEVICE_REMARK), payload);

    sendResponse(BusproOp::DEVICE_REMARK.readResp(), 0xFFFF, payload, sizeof(payload));
}

void RelayModule::handleModifyDeviceRemark(const BusproFrame &frame)
{
    if (frame.payloadLen != 20)
        return;

    flash_.update(flash_.findAdrress(memoryaddress_, MemoryAdress::DEVICE_REMARK), frame.payload, frame.payloadLen);

    uint8_t payload[1] = {0xF8};
    sendResponse(BusproOp::DEVICE_REMARK.writeResp(), frame.srcAddress, payload, sizeof(payload));
}

/////////////////////////////////////////////////////////////////////////////////
/////////////////////////////// UNIVERSAL REQUEST ///////////////////////////////
/////////////////////////////////////////////////////////////////////////////////