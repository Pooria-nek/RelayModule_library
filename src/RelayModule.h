#pragma once

#include <Arduino.h>
#include "BusproFrame.h"
#include "BusproTransport.h"
#include "BusproDevice.h"
#include "MemoryCore.h"

#include "RelayController.h"

#define RELAY4

// under progress
#ifdef RELAY1
constexpr uint8_t RELAY_CHANNEL_COUNT = 1;
constexpr uint16_t RELAY_TYPE = 5500; //
#elifdef RELAY2
constexpr uint8_t RELAY_CHANNEL_COUNT = 2;
constexpr uint16_t RELAY_TYPE = 5501; //
#elifdef RELAY3
constexpr uint8_t RELAY_CHANNEL_COUNT = 3;
constexpr uint16_t RELAY_TYPE = 467; // on protect | stair light - AC setting
#elifdef RELAY4
constexpr uint8_t RELAY_CHANNEL_COUNT = 4;
// constexpr uint16_t RELAY_TYPE = 423; // sequence - stair light
// constexpr uint16_t RELAY_TYPE = 424; // sequence
// constexpr uint16_t RELAY_TYPE = 433; // sequence
// constexpr uint16_t RELAY_TYPE = 434; // sequence
// constexpr uint16_t RELAY_TYPE = 435; // sequence
// constexpr uint16_t RELAY_TYPE = 437; // sequence
// constexpr uint16_t RELAY_TYPE = 438; // sequence
// constexpr uint16_t RELAY_TYPE = 441; // sequence - stair light
// constexpr uint16_t RELAY_TYPE = 444; // on protect | sequence - stair light
// constexpr uint16_t RELAY_TYPE = 447; // on protect | sequence - stair light
constexpr uint16_t RELAY_TYPE = 462; // curtain
#elifdef RELAY6
constexpr uint8_t RELAY_CHANNEL_COUNT = 6;
// constexpr uint16_t RELAY_TYPE = 425; // sequence
constexpr uint16_t RELAY_TYPE = 426; // sequence
#elifdef RELAY8
constexpr uint8_t RELAY_CHANNEL_COUNT = 8;
// constexpr uint16_t RELAY_TYPE = 427; // sequence
// constexpr uint16_t RELAY_TYPE = 428; // sequence
// constexpr uint16_t RELAY_TYPE = 436; // sequence
// constexpr uint16_t RELAY_TYPE = 439; // sequence
// constexpr uint16_t RELAY_TYPE = 442; // sequence - stair light
// constexpr uint16_t RELAY_TYPE = 445; // sequence - stair light
// constexpr uint16_t RELAY_TYPE = 448; // on protect | sequence - stair light
constexpr uint16_t RELAY_TYPE = 463; // curtain
#elifdef RELAY12
constexpr uint8_t RELAY_CHANNEL_COUNT = 12;
// constexpr uint16_t RELAY_TYPE = 429; // sequence
// constexpr uint16_t RELAY_TYPE = 430; // sequence
// constexpr uint16_t RELAY_TYPE = 431; // sequence
// constexpr uint16_t RELAY_TYPE = 440; // sequence
// constexpr uint16_t RELAY_TYPE = 443; // sequence - stair light
// constexpr uint16_t RELAY_TYPE = 446; // on protect | sequence - stair light
// constexpr uint16_t RELAY_TYPE = 449; // on protect | sequence - stair light
constexpr uint16_t RELAY_TYPE = 464; // curtain
#elifdef RELAY16
constexpr uint8_t RELAY_CHANNEL_COUNT = 16;
// constexpr uint16_t RELAY_TYPE = 450; // sequence - stair light
// constexpr uint16_t RELAY_TYPE = 451; // on protect | sequence - stair light
// constexpr uint16_t RELAY_TYPE = 465; // sequence - curtain
constexpr uint16_t RELAY_TYPE = 466; // sequence - curtain
#elifdef RELAY24
constexpr uint8_t RELAY_CHANNEL_COUNT = 24;
constexpr uint16_t RELAY_TYPE = 432; // sequence
#elifdef WIRELESS
constexpr uint8_t RELAY_CHANNEL_COUNT = 24;
constexpr uint16_t RELAY_TYPE = 6100;
#endif

constexpr uint8_t CURTAIN_CHANNEL_COUNT = (RELAY_CHANNEL_COUNT / 2);
constexpr uint8_t MAX_SCENE_ENTRIES = RELAY_CHANNEL_COUNT * 2; // tune to taste / available RAM

class RelayModule
{
public:
    RelayModule(
        BusproTransport &bus,
        MemoryCore &flash,
        const uint8_t relayPins[RELAY_CHANNEL_COUNT],
        bool activeHigh = true);

    bool begin();
    void update();
    bool firstime();

    bool init();

    uint8_t maxZone();
    bool syncValues();

    void process(const BusproFrame &frame);

    // // Call frequently from loop(); non-blocking.
    // void poll();

    // --- Direct relay control (also usable outside of bus commands) ---
    bool setRelay(uint8_t channel /*0-3*/, bool on);
    bool getRelay(uint8_t channel) const;
    void setAllRelays(const bool states[RELAY_CHANNEL_COUNT]);

    // // --- Scene table management (normally driven by a config tool, but
    // //     exposed directly too in case you want to seed scenes in code) ---
    // bool defineScene(uint8_t area, uint8_t scene, const bool states[RELAY_CHANNEL_COUNT]);
    // bool removeScene(uint8_t area, uint8_t scene);

    void sendResponse(uint16_t opcode, uint16_t dst, const uint8_t *payload, uint8_t payloadLen);

    MemoryCore &flash() { return flash_; }

    uint32_t memoryAddress() const { return memoryaddress_; }

private:
    static constexpr const char *SOFTWARE_VERSION = "v0.10.0-beta";

    void applyRelayHardware(uint8_t channel);
    // void readMcuUID();

    uint32_t memoryaddress_ = MemoryAdress::Relay::SECTOR_INFO; // its the refrens address of data on memoryflash

    BusproTransport &bus_;
    MemoryCore &flash_;

    uint16_t deviceAddress_ = 0x0141;
    uint8_t fuid_[12];
    uint8_t uid_[12];
    uint16_t devType_ = RELAY_TYPE; // 4R device type per HDL spec

    uint8_t sceneCount = 0;
    uint8_t sceneActive = 0;
    uint8_t relayPins_[RELAY_CHANNEL_COUNT];
    bool relayState_[RELAY_CHANNEL_COUNT] = {false};
    bool activeHigh_;

    bool relayEnable_[RELAY_CHANNEL_COUNT] = {false};
    uint8_t relayDelay_[RELAY_CHANNEL_COUNT] = {0};
    uint8_t relayProtect_[RELAY_CHANNEL_COUNT] = {0};

    RelayController controllerFunction;

    /////////////////////////////// BASIC INFORMATION ///////////////////////////////

    /////////////////////////////// ZONE SETTING ///////////////////////////////

    /////////////////////////////// SCENE SETTING ///////////////////////////////

    /////////////////////////////// CURTAIN ///////////////////////////////

    void handleReadZone(const BusproFrame &frame);
    void handleModifyZone(const BusproFrame &frame);
    void handleReadZoneRemark(const BusproFrame &frame);
    void handleModifyZoneRemark(const BusproFrame &frame);
    void handleReadSceneRemark(const BusproFrame &frame);
    void handleModifySceneRemark(const BusproFrame &frame);

    // Device
    // void handleSearchRequest(const BusproFrame &frame);
    // void handleModifyDeviceRemark(const BusproFrame &frame);

    // void handleReadMacaddress(const BusproFrame &frame);
    // void handleModifyMacaddress(const BusproFrame &frame);

    // Channel configuration
    void handleReadChannelRemark(const BusproFrame &frame);
    void handleModifyChannelRemark(const BusproFrame &frame);

    void handleReadChannelEnable(const BusproFrame &frame);
    void handleModifyChannelEnable(const BusproFrame &frame);

    void handleReadChannelOndelay(const BusproFrame &frame);
    void handleModifyChannelOndelay(const BusproFrame &frame);

    void handleReadChannelOnprotect(const BusproFrame &frame);
    void handleModifyChannelOnprotect(const BusproFrame &frame);

    void handleSceneRead(const BusproFrame &frame);
    void handleSceneModify(const BusproFrame &frame);

    void handleSceneResumeENRead(const BusproFrame &frame);
    void handleSceneResumeENModify(const BusproFrame &frame);

    void handleSceneResumeNumRead(const BusproFrame &frame);
    void handleSceneResumeNumModify(const BusproFrame &frame);

    void handleCurtainRead(const BusproFrame &frame);
    void handleCurtainModify(const BusproFrame &frame);

    // Status
    void handleReadStatusRequest(const BusproFrame &frame);

    /////////////////////////////// UNIVERSAL REQUEST ///////////////////////////////

    void handleReadFirmware(const BusproFrame &frame);
    void handleReadHardware(const BusproFrame &frame);
    void handleFindDevice(const BusproFrame &frame);

    void handleSearchDevice(const BusproFrame &frame);

    void handleReadMacaddress(const BusproFrame &frame);
    void handleModifyMacaddress(const BusproFrame &frame);

    void handleReadDeviceRemark(const BusproFrame &frame);
    void handleModifyDeviceRemark(const BusproFrame &frame);
};
