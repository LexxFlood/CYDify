#pragma once

#include "PlayerClient.h"
#include "LyricsClient.h"

namespace UserInterface {

using WifiButtonHandler = void (*)();
using PlayerActionHandler = void (*)(const char* action);
using LikedHandler = void (*)(bool liked);
using BrightnessHandler = void (*)(uint8_t percent);

void create();
void setWifiButtonHandler(WifiButtonHandler handler);
void setPlayerHandlers(PlayerActionHandler actionHandler, LikedHandler likedHandler);
void setBrightnessHandler(
    BrightnessHandler handler,
    uint8_t initialBrightness);
void syncBrightness(uint8_t percent);
void setNetworkStatus(const char* text, bool connected);
void setSpotifyStatus(const char* text, bool connected);
void updatePlayer(const PlayerClient::State& state);
void setMediaLoading(bool loading);
void updateAlbumArt(
    const uint16_t* pixels,
    uint16_t width,
    uint16_t height,
    uint32_t dominantColor);
void clearAlbumArt();
void updateBackground(
    const uint16_t* pixels,
    uint16_t width,
    uint16_t height);
void clearBackground();
bool lyricsVisible();
void updateLyrics(const LyricsClient::Window& window);

}  // namespace UserInterface
