#pragma once

#include <Arduino.h>

namespace AlbumArtClient {

using ImageHandler = void (*)(
    const uint16_t* pixels,
    uint16_t width,
    uint16_t height,
    uint32_t dominantColor);
using ClearHandler = void (*)();

using BackgroundHandler = void (*)(
    const uint16_t* pixels,
    uint16_t width,
    uint16_t height);

void begin(
    ImageHandler imageHandler,
    ClearHandler clearHandler,
    BackgroundHandler backgroundHandler,
    ClearHandler clearBackgroundHandler);
void setTrack(const char* trackId);
void update();

}  // namespace AlbumArtClient
