#include "RelayModule.h"
#include <type_traits>

RelayModule::RelayModule(
    BusproTransport &bus,
    MemoryCore &flash,
    const uint8_t relayPins[RELAY_CHANNEL_COUNT],
    bool activeHigh)
    : bus_(bus),
      flash_(flash),
      activeHigh_(activeHigh),
      controllerFunction(*this)
{
    static_assert(std::is_reference<decltype(bus_)>::value, "bus_ must be a reference");
    // copyMcuUID(uid_);
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

    if (firstime())
    {
        init();
    }
    reMatch();

    return syncValues();
}

void RelayModule::update()
{
}

bool RelayModule::firstime()
{
    uint8_t payload[16];
    flash_.read(InfoStorageLayout::deviceAddress(), payload, sizeof(payload));

    for (uint8_t i = 0; i < sizeof(payload); i++)
    {
        if (payload[i] != 0xFF)
            return false;
    }

    return true;
}

bool RelayModule::reMatch()
{
    char storedVersion[20] = {};

    auto address = InfoStorageLayout::softwareVersionAddress();

    flash_.read(address, storedVersion, sizeof(storedVersion));

    if (strncmp(storedVersion, SOFTWARE_VERSION, sizeof(storedVersion)) == 0)
        return false;

    char newVersion[20] = {};
    strncpy(newVersion, SOFTWARE_VERSION, sizeof(newVersion) - 1);

    flash_.update(address, SOFTWARE_VERSION, sizeof(storedVersion));

    return true;
}

bool RelayModule::init()
{
    uint8_t payload[2] = {static_cast<uint8_t>(deviceAddress_ >> 8), static_cast<uint8_t>(deviceAddress_ & 0xFF)};
    flash_.write(InfoStorageLayout::deviceAddress(), payload, sizeof(payload));

    uint8_t payload1[2] = {static_cast<uint8_t>(devType_ >> 8), static_cast<uint8_t>(devType_ & 0xFF)};
    flash_.write(InfoStorageLayout::deviceTypeAddress(), payload1, sizeof(payload1));

    // // MCU UID
    // const uint8_t *uid = mcu::getMcuUID();
    // flash_.write(InfoStorageLayout::deviceMacAddress(), uid, sizeof(uid));

    flash_.write(InfoStorageLayout::deviceRemarkAddress(), FACTORY_NAME, 20);
    flash_.write(InfoStorageLayout::hardwareVersionAddress(), HARDWARE_VERSION, 30);
    flash_.write(InfoStorageLayout::softwareVersionAddress(), SOFTWARE_VERSION, 20);

    char remark[20];
    for (size_t i = 1; i <= RELAY_CHANNEL_COUNT; i++)
    {
        snprintf(remark, sizeof(remark), "Relay %d", static_cast<unsigned>(i));
        flash_.write(InfoStorageLayout::channelRemarkAddress(i), remark, sizeof(remark));

        uint8_t enable = 0x01;

        flash_.write(InfoStorageLayout::channelEnableAddress(i), &enable, 1);
        flash_.write(InfoStorageLayout::channelOnDelayAddress(i), 0, 1);
        flash_.write(InfoStorageLayout::channelOnProtectAddress(i), 0, 1);

        uint8_t defzone = 0x01;

        flash_.write(InfoStorageLayout::channelZoneAddress(i), &defzone, 1);
    }

    for (uint8_t z = 1; z <= ZoneStorageLayout::ZONE_COUNT; ++z)
    {
        flash_.write(ZoneStorageLayout::scenePoweronEnAddress(z), 0, 1);

        flash_.write(ZoneStorageLayout::scenePoweronNumAddress(z), 0, 1);

        // Initialize zone remark
        snprintf(remark, sizeof(remark), "Zone %u", static_cast<unsigned>(z));
        uint32_t zoneAddress = ZoneStorageLayout::zoneInfoAddress(z);

        if (zoneAddress == UINT32_MAX)
            return false;

        flash_.write(zoneAddress + offsetof(ZoneInfo, remark), reinterpret_cast<const uint8_t *>(remark), sizeof(remark));

        // Initialize scene remarks
        for (uint8_t s = 1; s <= ZoneStorageLayout::SCENE_COUNT; ++s)
        {
            snprintf(remark, sizeof(remark), "Scene %u", static_cast<unsigned>(s));
            uint32_t sceneAddress = ZoneStorageLayout::sceneRemarkAddress(z, s);

            if (sceneAddress == UINT32_MAX)
                return false;

            flash_.write(sceneAddress, reinterpret_cast<const uint8_t *>(remark), sizeof(remark));
        }
    }

    return true;
}

bool RelayModule::syncValues()
{
    uint8_t payload[2] = {};
    flash_.read(InfoStorageLayout::deviceAddress(), payload, sizeof(payload));
    deviceAddress_ = (static_cast<uint16_t>(payload[0]) << 8) | payload[1];

    // flash_.read(InfoStorageLayout::deviceMacAddress(), uid_);
    flash_.read(InfoStorageLayout::channelEnableAddress(), relayEnable_, sizeof(relayEnable_));
    flash_.read(InfoStorageLayout::channelOnDelayAddress(), relayDelay_, sizeof(relayEnable_));
    flash_.read(InfoStorageLayout::channelOnProtectAddress(), relayProtect_, sizeof(relayEnable_));

    // maxZone();

    return true;
}

// uint8_t RelayModule::maxZone()
// {
//     uint8_t payload[RELAY_CHANNEL_COUNT];

//     flash_.read(InfoStorageLayout::channelZoneAddress(), payload, RELAY_CHANNEL_COUNT);

//     for (uint8_t i = 0; i < RELAY_CHANNEL_COUNT; ++i)
//     {
//         if (payload[i] > sceneCount)
//         {
//             sceneCount = payload[i];
//         }
//     }

//     return sceneCount;
// }

void RelayModule::process(const BusproFrame &frame)
{
    // if (frame.devType != devType_)
    //     return;

    if (frame.srcAddress == deviceAddress_)
    {
        return;
    }

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

        case BusproOp::Relay::CHANNEL_REMARK.readReq():
            handleReadChannelRemark(frame);
            break;

        case BusproOp::Relay::CHANNEL_REMARK.writeReq():
            handleModifyChannelRemark(frame);
            break;

        case BusproOp::Relay::CHANNEL_ONDELAY.readReq():
            handleReadChannelOndelay(frame);
            break;

        case BusproOp::Relay::CHANNEL_ONDELAY.writeReq():
            handleModifyChannelOndelay(frame);
            break;

        case BusproOp::Relay::CHANNEL_ONPROTECT.readReq():
            handleReadChannelOnprotect(frame);
            break;

        case BusproOp::Relay::CHANNEL_ONPROTECT.writeReq():
            handleModifyChannelOnprotect(frame);
            break;

        case BusproOp::Relay::CHANNEL_ENABLE.readReq():
            handleReadChannelEnable(frame);
            break;

        case BusproOp::Relay::CHANNEL_ENABLE.writeReq():
            handleModifyChannelEnable(frame);
            break;

            /////////////////////////////// RELAY ZONE SETTING ///////////////////////////////

        case BusproOp::Relay::ZONE_MEMBERS.readReq():
            handleReadZone(frame);
            break;

        case BusproOp::Relay::ZONE_MEMBERS.writeReq():
            handleModifyZone(frame);
            break;

        case BusproOp::Relay::ZONE_REMARK.readReq():
            handleReadZoneRemark(frame);
            break;

        case BusproOp::Relay::ZONE_REMARK.writeReq():
            handleModifyZoneRemark(frame);
            break;

            /////////////////////////////// RELAY SCENE SETTING ///////////////////////////////

        case BusproOp::Relay::SCENE_READ.req():
            handleSceneRead(frame);
            break;

        case BusproOp::Relay::SCENE_MODIFY.req():
            handleSceneModify(frame);
            break;

        case BusproOp::Relay::SCENE_REMARK.readReq():
            handleReadSceneRemark(frame);
            break;

        case BusproOp::Relay::SCENE_REMARK.writeReq():
            handleModifySceneRemark(frame);
            break;

        case BusproOp::Relay::SCENE_POWERON_EN.readReq():
            handleSceneResumeENRead(frame);
            break;

        case BusproOp::Relay::SCENE_POWERON_EN.writeReq():
            handleSceneResumeENModify(frame);
            break;

        case BusproOp::Relay::SCENE_POWERON_NUM.readReq():
            handleSceneResumeNumRead(frame);
            break;

        case BusproOp::Relay::SCENE_POWERON_NUM.writeReq():
            handleSceneResumeNumModify(frame);
            break;

            /////////////////////////////// RELAY CURTAIN ///////////////////////////////

        case BusproOp::Relay::CURTAIN_CONFIG.readReq():
            handleCurtainRead(frame);
            break;

        case BusproOp::Relay::CURTAIN_CONFIG.writeReq():
            handleCurtainModify(frame);
            break;

            /////////////////////////////// RELAY CONTROLL ///////////////////////////////

        case BusproOp::READ_STATE.req():
            // controllerFunction.handleReadStatusRequest(frame);
            break;

        case BusproOp::CONTROL_SINGLE.req():
            controllerFunction.handleSingleChannelControl(frame);
            break;

        case BusproOp::CONTROL_REVERSING.req():
            controllerFunction.handleReversingControl(frame);
            break;

        default:
            break;
        }

        return;
    }
}

// for sending 1 byte success
void RelayModule::sendSuccess(uint16_t opcode, uint16_t dst)
{
    uint8_t payload[1] = {BusproOp::SUCCESS};
    sendResponse(opcode, dst, payload, sizeof(payload));
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

    flash_.update(InfoStorageLayout::channelRemarkAddress(frame.payload[0]), frame.payload + 1, frame.payloadLen - 1);

    sendSuccess(BusproOp::Relay::CHANNEL_REMARK.writeResp(), frame.srcAddress);
}
void RelayModule::handleReadChannelRemark(const BusproFrame &frame)
{
    if (frame.payloadLen != 1)
        return;

    uint8_t payload[21];

    payload[0] = frame.payload[0];

    flash_.read(InfoStorageLayout::channelRemarkAddress(frame.payload[0]), payload + 1, 20);
    sendResponse(BusproOp::Relay::CHANNEL_REMARK.readResp(), frame.srcAddress, payload, sizeof(payload));
}

void RelayModule::handleModifyChannelOndelay(const BusproFrame &frame)
{
    if (frame.payloadLen != RELAY_CHANNEL_COUNT)
        return;

    flash_.update(InfoStorageLayout::channelOnDelayAddress(), frame.payload, frame.payloadLen);

    sendSuccess(BusproOp::Relay::CHANNEL_ONDELAY.writeResp(), frame.srcAddress);
}
void RelayModule::handleReadChannelOndelay(const BusproFrame &frame)
{
    if (frame.payloadLen != 0)
        return;

    uint8_t payload[RELAY_CHANNEL_COUNT];

    flash_.read(InfoStorageLayout::channelOnDelayAddress(), payload, RELAY_CHANNEL_COUNT);
    sendResponse(BusproOp::Relay::CHANNEL_ONDELAY.readResp(), frame.srcAddress, payload, sizeof(payload));
}

void RelayModule::handleModifyChannelOnprotect(const BusproFrame &frame)
{
    if (frame.payloadLen != RELAY_CHANNEL_COUNT)
        return;

    flash_.update(InfoStorageLayout::channelOnProtectAddress(), frame.payload, frame.payloadLen);

    sendSuccess(BusproOp::Relay::CHANNEL_ONPROTECT.writeResp(), frame.srcAddress);
}
void RelayModule::handleReadChannelOnprotect(const BusproFrame &frame)
{
    if (frame.payloadLen != 0)
        return;

    uint8_t payload[RELAY_CHANNEL_COUNT];

    flash_.read(InfoStorageLayout::channelOnProtectAddress(), payload, RELAY_CHANNEL_COUNT);
    sendResponse(BusproOp::Relay::CHANNEL_ONPROTECT.readResp(), frame.srcAddress, payload, sizeof(payload));
}

void RelayModule::handleModifyChannelEnable(const BusproFrame &frame)
{
    if (frame.payloadLen != (RELAY_CHANNEL_COUNT + 1) &&
        frame.payload[0] == RELAY_CHANNEL_COUNT)
        return;

    flash_.update(InfoStorageLayout::channelEnableAddress(), frame.payload + 1, RELAY_CHANNEL_COUNT);

    syncValues();

    sendSuccess(BusproOp::Relay::CHANNEL_ENABLE.writeResp(), frame.srcAddress);
}
void RelayModule::handleReadChannelEnable(const BusproFrame &frame)
{
    if (frame.payloadLen != 0)
        return;

    uint8_t payload[5];
    payload[0] = RELAY_CHANNEL_COUNT;

    // use memory copy for optimization

    flash_.read(InfoStorageLayout::channelEnableAddress(), payload + 1, RELAY_CHANNEL_COUNT);
    sendResponse(BusproOp::Relay::CHANNEL_ENABLE.readResp(), frame.srcAddress, payload, sizeof(payload));
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

    flash_.read(InfoStorageLayout::channelZoneAddress(), payload + 5, RELAY_CHANNEL_COUNT);

    // Calculate the highest assigned zone number.
    uint8_t maxZone = 1;
    // for (uint8_t i = 0; i < RELAY_CHANNEL_COUNT; ++i)
    // {
    //     if (payload[5 + i] > maxZone)
    //     {
    //         maxZone = payload[5 + i];
    //         sceneCount = maxZone;
    //     }
    // }

    payload[4] = maxZone;

    sendResponse(BusproOp::Relay::ZONE_MEMBERS.readResp(), frame.srcAddress, payload, sizeof(payload));
}
void RelayModule::handleModifyZone(const BusproFrame &frame)
{
    if (frame.payloadLen != (3 + RELAY_CHANNEL_COUNT))
        return;

    // flash_.update(InfoStorageLayout::channelZoneAddress(), frame.payload + 3, RELAY_CHANNEL_COUNT);

    sendSuccess(BusproOp::Relay::ZONE_MEMBERS.writeResp(), frame.srcAddress);
}

void RelayModule::handleReadZoneRemark(const BusproFrame &frame)
{
    if (frame.payloadLen != 1)
        return;

    uint8_t payload[21];

    payload[0] = frame.payload[0];

    flash_.read(ZoneStorageLayout::zoneRemarkAddress(frame.payload[0]), payload + 1, 20);

    sendResponse(BusproOp::Relay::ZONE_REMARK.readResp(), frame.srcAddress, payload, sizeof(payload));
}
void RelayModule::handleModifyZoneRemark(const BusproFrame &frame)
{
    if (frame.payloadLen != (1 + 20))
        return;

    flash_.update(ZoneStorageLayout::zoneRemarkAddress(frame.payload[0]), frame.payload + 1, 20);

    uint8_t payload[1] = {frame.payload[0]};
    sendResponse(BusproOp::Relay::ZONE_REMARK.writeResp(), frame.srcAddress, payload, sizeof(payload));
}

/////////////////////////////// RELAY SCENE SETTING ///////////////////////////////

void RelayModule::handleSceneRead(const BusproFrame &frame)
{
    if (frame.payloadLen != 2)
        return;

    uint8_t zone = frame.payload[0];
    uint8_t scene = frame.payload[1];
    uint8_t minute = frame.payload[2];
    uint8_t second = frame.payload[3];

    uint8_t payload[4 + RELAY_CHANNEL_COUNT];

    flash_.read(ZoneStorageLayout::channelStatesAddress(zone, scene), payload + 2, RELAY_CHANNEL_COUNT);

    sendResponse(BusproOp::Relay::SCENE_READ.resp(), frame.srcAddress, payload, sizeof(payload));
}
void RelayModule::handleSceneModify(const BusproFrame &frame)
{
    if (frame.payloadLen != 4 + RELAY_CHANNEL_COUNT)
        return;

    uint8_t zone = frame.payload[0];
    uint8_t scene = frame.payload[1];
    uint8_t minute = frame.payload[2];
    uint8_t second = frame.payload[3];

    flash_.update(ZoneStorageLayout::channelStatesAddress(zone, scene), frame.payload + 2, RELAY_CHANNEL_COUNT);

    sendSuccess(BusproOp::Relay::SCENE_MODIFY.resp(), frame.srcAddress);
}

void RelayModule::handleReadSceneRemark(const BusproFrame &frame)
{
    if (frame.payloadLen != (2))
        return;

    uint8_t payload[22];

    payload[0] = frame.payload[0];
    payload[1] = frame.payload[1];

    flash_.read(ZoneStorageLayout::sceneRemarkAddress(frame.payload[0], frame.payload[1]), payload + 2, 20);

    sendResponse(BusproOp::Relay::SCENE_REMARK.readResp(), frame.srcAddress, payload, sizeof(payload));
}
void RelayModule::handleModifySceneRemark(const BusproFrame &frame)
{
    if (frame.payloadLen != (2 + 20))
        return;

    uint8_t payload[22];

    flash_.update(ZoneStorageLayout::sceneRemarkAddress(frame.payload[0], frame.payload[1]), frame.payload + 2, 20);

    payload[0] = {frame.payload[0]};
    payload[1] = {frame.payload[1]};

    flash_.read(ZoneStorageLayout::sceneRemarkAddress(frame.payload[0], frame.payload[1]), payload + 2, 20);

    sendResponse(BusproOp::Relay::SCENE_REMARK.writeResp(), frame.srcAddress, payload, sizeof(payload));
}

void RelayModule::handleSceneResumeENRead(const BusproFrame &frame)
{
    if (frame.payloadLen != 0)
        return;

    uint8_t payload[sceneCount] = {0};

    // DOTO

    sendResponse(BusproOp::Relay::SCENE_POWERON_EN.readResp(), frame.srcAddress, payload, sizeof(payload));
}
void RelayModule::handleSceneResumeENModify(const BusproFrame &frame)
{
    if (frame.payloadLen != maxZone)
        return;

    // DOTO

    sendSuccess(BusproOp::Relay::SCENE_POWERON_EN.writeResp(), frame.srcAddress);
}

void RelayModule::handleSceneResumeNumRead(const BusproFrame &frame)
{
    if (frame.payloadLen != 0)
        return;

    uint8_t payload[sceneCount] = {0};

    // DOTO

    sendResponse(BusproOp::Relay::SCENE_POWERON_NUM.readResp(), frame.srcAddress, payload, sizeof(payload));
}
void RelayModule::handleSceneResumeNumModify(const BusproFrame &frame)
{
    if (frame.payloadLen != maxZone)
        return;

    // DOTO

    sendSuccess(BusproOp::Relay::SCENE_POWERON_NUM.writeResp(), frame.srcAddress);
}

/////////////////////////////// RELAY CURTAIN ///////////////////////////////

void RelayModule::handleCurtainRead(const BusproFrame &frame)
{
    if (frame.payloadLen != 1)
        return;

    // DOTO

    uint8_t payload[5];

    sendResponse(BusproOp::Relay::CURTAIN_CONFIG.readResp(), frame.srcAddress, payload, sizeof(payload));
}

void RelayModule::handleCurtainModify(const BusproFrame &frame)
{
    if (frame.payloadLen != 5)
        return;

    // DOTO

    sendSuccess(BusproOp::Relay::CURTAIN_CONFIG.writeResp(), frame.srcAddress);
}

/////////////////////////////////////////////////////////////////////////////////
/////////////////////////////// UNIVERSAL REQUEST ///////////////////////////////
/////////////////////////////////////////////////////////////////////////////////

void RelayModule::handleReadFirmware(const BusproFrame &frame)
{
    if (frame.payloadLen != 0)
        return;

    uint8_t payload[20] = {0x00};

    flash_.read(InfoStorageLayout::softwareVersionAddress(), payload, sizeof(payload));

    sendResponse(BusproOp::DEVICE_FIRMWARE.resp(), frame.srcAddress, payload, sizeof(payload));
}

void RelayModule::handleReadHardware(const BusproFrame &frame)
{
    if (frame.payloadLen != 0)
        return;

    uint8_t payload[30] = {};

    flash_.read(InfoStorageLayout::hardwareVersionAddress(), payload, sizeof(payload));

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
    if (frame.payloadLen != 4 || frame.payloadLen != 2)
        return;

    uint8_t payload[30 + frame.payloadLen] = {0x00};

    payload[0] = frame.payload[0];
    payload[1] = frame.payload[1];

    if (frame.payloadLen == 4)
    {
        payload[2] = frame.payload[2];
        payload[3] = frame.payload[3];
    }

    flash_.read(InfoStorageLayout::deviceRemarkAddress(), payload + frame.payloadLen, 20);

    sendResponse(BusproOp::DEVICE_SEARCH_HDL.resp(), frame.srcAddress, payload, sizeof(payload));
}

void RelayModule::handleReadMacaddress(const BusproFrame &frame)
{
    if (frame.payloadLen != 0)
        return;

    uint8_t payload[12] = {0x00};

    // mcu::copyArray(mcu::getMcuUID(), payload, 8);

    sendResponse(BusproOp::DEVICE_MAC_ADDRESS.readResp(), frame.srcAddress, payload, sizeof(payload));
}
void RelayModule::handleModifyMacaddress(const BusproFrame &frame)
{
    if (frame.payloadLen != 10)
        return;

    // if (!mcu::bufferEquals(frame.payload, mcu::getMcuUID()))
    //     return;

    uint8_t address[2] = {frame.payload[8], frame.payload[9]};

    flash_.update(InfoStorageLayout::deviceAddress(), address, 2);
    syncValues();

    sendSuccess(BusproOp::DEVICE_MAC_ADDRESS.writeResp(), frame.srcAddress);
}

void RelayModule::handleReadDeviceRemark(const BusproFrame &frame)
{
    if (frame.payloadLen != 0)
        return;

    uint8_t payload[20];

    flash_.read(InfoStorageLayout::deviceRemarkAddress(), payload, sizeof(payload));

    sendResponse(BusproOp::DEVICE_REMARK.readResp(), 0xFFFF, payload, sizeof(payload));
}

void RelayModule::handleModifyDeviceRemark(const BusproFrame &frame)
{
    if (frame.payloadLen != 20)
        return;

    flash_.update(InfoStorageLayout::deviceRemarkAddress(), frame.payload, frame.payloadLen);

    sendSuccess(BusproOp::DEVICE_REMARK.writeResp(), frame.srcAddress);
}

/////////////////////////////////////////////////////////////////////////////////
/////////////////////////////// UNIVERSAL REQUEST ///////////////////////////////
/////////////////////////////////////////////////////////////////////////////////