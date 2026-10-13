#pragma once

#include <cstdint>
#include <climits>
#include <cstddef>

struct ZoneInfo
{
    uint8_t remark[32];
    uint8_t scenePoweronEn;
    uint8_t scenePoweronNum;
    uint8_t reserved[30];
};

struct ZoneScene
{
    uint8_t remark[32];
    uint8_t channelStates[32];
    uint8_t Runmm;
    uint8_t Runss;
    uint8_t reserved[14];
};

static_assert(sizeof(ZoneInfo) == 64);
static_assert(sizeof(ZoneScene) == 80);

static_assert(offsetof(ZoneInfo, scenePoweronEn) == 32);
static_assert(offsetof(ZoneInfo, scenePoweronNum) == 33);
static_assert(offsetof(ZoneScene, channelStates) == 32);

template <
    uint32_t MainSector,
    uint8_t ChannelCount>
class ZoneStorage
{
public:
    using Info = ZoneInfo;
    using Scene = ZoneScene;

    static constexpr uint32_t SECTOR_SIZE = 0x1000;
    static constexpr uint32_t MAIN_SECTOR = MainSector;

    static constexpr uint32_t BASE_ADDRESS = (MAIN_SECTOR + 1) * SECTOR_SIZE;

    static constexpr uint32_t INFO_OFFSET = 0x0000;
    static constexpr uint32_t SCENE_OFFSET = 0x0060;

    static constexpr uint8_t CHANNEL_COUNT = ChannelCount;
    static constexpr uint8_t ZONE_COUNT = CHANNEL_COUNT;
    static constexpr uint16_t SCENE_COUNT = uint16_t(CHANNEL_COUNT) * 2;

    static constexpr uint32_t INFO_SIZE = sizeof(Info);
    static constexpr uint32_t SCENE_SIZE = sizeof(Scene);

    static_assert(INFO_OFFSET + INFO_SIZE <= SECTOR_SIZE, "Info exceeds sector size");

    static_assert(SCENE_OFFSET + SCENE_COUNT * SCENE_SIZE <= SECTOR_SIZE, "Scenes exceed sector size");

    // All public indexes start at 1.

    static constexpr bool validZone(uint8_t zone)
    {
        return zone >= 1 && zone <= ZONE_COUNT;
    }

    static constexpr bool validScene(uint16_t scene)
    {
        return scene >= 0 && scene <= SCENE_COUNT;
    }

    static constexpr bool validChannel(uint8_t channel)
    {
        return channel >= 1 && channel <= CHANNEL_COUNT;
    }

    static constexpr uint32_t zoneInfoAddress(uint8_t zone)
    {
        if (!validZone(zone))
            return UINT32_MAX;

        return BASE_ADDRESS + uint32_t(zone - 1) * SECTOR_SIZE + INFO_OFFSET;
    }

    static constexpr uint32_t sceneAddress(uint8_t zone, uint16_t scene)
    {
        if (!validZone(zone) || !validScene(scene))
            return UINT32_MAX;

        return BASE_ADDRESS + uint32_t(zone - 1) * SECTOR_SIZE + SCENE_OFFSET + uint32_t(scene) * SCENE_SIZE;
    }

    static constexpr uint32_t channelStatesAddress(uint8_t zone, uint16_t scene)
    {
        const uint32_t address = sceneAddress(zone, scene);

        if (address == UINT32_MAX)
            return UINT32_MAX;

        return address + offsetof(Scene, channelStates);
    }

    static constexpr uint32_t channelStateAddress(uint8_t zone, uint16_t scene, uint8_t channel)
    {
        if (!validChannel(channel))
            return UINT32_MAX;

        const uint32_t address =
            channelStatesAddress(zone, scene);

        if (address == UINT32_MAX)
            return UINT32_MAX;

        return address + uint32_t(channel - 1);
    }

    static constexpr uint32_t sceneRunTimeAddress(uint8_t zone, uint16_t scene)
    {
        const uint32_t address = sceneAddress(zone, scene);

        if (address == UINT32_MAX)
            return UINT32_MAX;

        return address + offsetof(Scene, Runmm);
    }

    static constexpr uint16_t runTimeInSeconds(uint8_t minutes, uint8_t seconds)
    {
        if (minutes > 60 || seconds > 60)
            return 0;

        const uint16_t total = uint16_t(minutes) * 60 + seconds;

        return total <= 3600 ? total : 0;
    }

    static constexpr uint32_t scenePoweronEnAddress(uint8_t zone)
    {
        const uint32_t address = zoneInfoAddress(zone);

        if (address == UINT32_MAX)
            return UINT32_MAX;

        return address + offsetof(Info, scenePoweronEn);
    }

    static constexpr uint32_t scenePoweronNumAddress(uint8_t zone)
    {
        const uint32_t address = zoneInfoAddress(zone);

        if (address == UINT32_MAX)
            return UINT32_MAX;

        return address + offsetof(Info, scenePoweronNum);
    }

    static constexpr uint32_t zoneRemarkAddress(uint8_t zone)
    {
        return zoneInfoAddress(zone);
    }

    static constexpr uint32_t sceneRemarkAddress(uint8_t zone, uint16_t scene)
    {
        const uint32_t address = sceneAddress(zone, scene);

        if (address == UINT32_MAX)
            return UINT32_MAX;

        return address + offsetof(Scene, remark);
    }
};