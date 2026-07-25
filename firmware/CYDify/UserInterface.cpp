#include "UserInterface.h"

#include <Arduino.h>
#include <lvgl.h>

#include "DashboardPages.h"

namespace {

constexpr uint8_t EQUALIZER_BAR_COUNT = 7;

lv_obj_t* screenRoot = nullptr;
lv_obj_t* backgroundImage = nullptr;
lv_obj_t* backgroundShade = nullptr;
lv_image_dsc_t backgroundImageDescriptor = {};
lv_obj_t* albumArt = nullptr;
lv_obj_t* albumImage = nullptr;
lv_obj_t* albumPlaceholder = nullptr;
lv_image_dsc_t albumImageDescriptor = {};
lv_obj_t* titleLabel = nullptr;
lv_obj_t* artistLabel = nullptr;
lv_obj_t* albumLabel = nullptr;
lv_obj_t* progressBar = nullptr;
lv_obj_t* elapsedLabel = nullptr;
lv_obj_t* durationLabel = nullptr;
lv_obj_t* playLabel = nullptr;
lv_obj_t* likeLabel = nullptr;
lv_obj_t* volumeLabel = nullptr;
lv_obj_t* networkLabel = nullptr;
lv_obj_t* spotifyStatusLabel = nullptr;
lv_obj_t* brandLabel = nullptr;
lv_obj_t* playButtonObject = nullptr;
lv_obj_t* lyricsHeading = nullptr;
lv_obj_t* lyricsOverlay = nullptr;
lv_obj_t* lyricsTitle = nullptr;
lv_obj_t* lyricsMessage = nullptr;
lv_obj_t* lyricsProgressBar = nullptr;
lv_obj_t* lyricsLines[LyricsClient::WINDOW_SIZE] = {};
lv_obj_t* equalizerBars[EQUALIZER_BAR_COUNT] = {};
lv_obj_t* assetStatusLabel = nullptr;

UserInterface::WifiButtonHandler wifiButtonHandler = nullptr;
UserInterface::PlayerActionHandler playerActionHandler = nullptr;
UserInterface::LikedHandler likedHandler = nullptr;
UserInterface::BrightnessHandler brightnessHandler = nullptr;

uint32_t elapsedMs = 0;
uint32_t durationMs = 0;
bool isPlaying = false;
bool isLiked = false;
bool coverLoading = false;
bool backgroundLoading = false;
uint32_t assetLoadingStartedAt = 0;
uint32_t equalizerRandom = 0x43594469;
uint8_t equalizerHeights[EQUALIZER_BAR_COUNT] = {5, 9, 14, 8, 17, 11, 6};

uint32_t scaleColor(uint32_t color, uint8_t scale) {
  const uint32_t red = ((color >> 16) & 0xFF) * scale / 255;
  const uint32_t green = ((color >> 8) & 0xFF) * scale / 255;
  const uint32_t blue = (color & 0xFF) * scale / 255;
  return (red << 16) | (green << 8) | blue;
}

bool useDarkText(uint32_t color) {
  const uint32_t red = (color >> 16) & 0xFF;
  const uint32_t green = (color >> 8) & 0xFF;
  const uint32_t blue = color & 0xFF;
  return red * 299 + green * 587 + blue * 114 > 145000;
}

void applyDynamicTheme(uint32_t accent) {
  if (accent == 0) accent = 0x1ED760;
  const uint32_t backgroundTop = scaleColor(accent, 46);
  const uint32_t albumBottom = scaleColor(accent, 62);

  if (screenRoot != nullptr) {
    lv_obj_set_style_bg_color(screenRoot, lv_color_hex(backgroundTop), 0);
    lv_obj_set_style_bg_grad_color(screenRoot, lv_color_hex(0x070908), 0);
    lv_obj_set_style_bg_grad_dir(screenRoot, LV_GRAD_DIR_VER, 0);
  }
  // Marca, IP e estados de conexao usam verde semantico fixo.
  // Somente a area visual da musica acompanha a cor dominante da capa.
  if (albumArt != nullptr) {
    lv_obj_set_style_bg_color(albumArt, lv_color_hex(accent), 0);
    lv_obj_set_style_bg_grad_color(albumArt, lv_color_hex(albumBottom), 0);
  }
  if (progressBar != nullptr) {
    lv_obj_set_style_bg_color(progressBar, lv_color_hex(accent), LV_PART_INDICATOR);
  }
  if (lyricsProgressBar != nullptr) {
    lv_obj_set_style_bg_color(
        lyricsProgressBar,
        lv_color_hex(accent),
        LV_PART_INDICATOR);
  }
  if (playButtonObject != nullptr) {
    lv_obj_set_style_bg_color(playButtonObject, lv_color_hex(accent), 0);
  }
  if (playLabel != nullptr) {
    lv_obj_set_style_text_color(
        playLabel,
        lv_color_hex(useDarkText(accent) ? 0x071009 : 0xFFFFFF),
        0);
  }
  if (lyricsOverlay != nullptr) {
    lv_obj_set_style_bg_color(lyricsOverlay, lv_color_hex(scaleColor(accent, 28)), 0);
  }
  if (lyricsHeading != nullptr) {
    lv_obj_set_style_text_color(lyricsHeading, lv_color_hex(accent), 0);
  }
  for (uint8_t index = 0; index < EQUALIZER_BAR_COUNT; index++) {
    if (equalizerBars[index] != nullptr) {
      lv_obj_set_style_bg_color(equalizerBars[index], lv_color_hex(accent), 0);
    }
  }
  equalizerRandom ^= accent;
}

void formatTime(uint32_t milliseconds, char* output, size_t outputSize) {
  const uint32_t seconds = milliseconds / 1000;
  snprintf(output, outputSize, "%lu:%02lu", seconds / 60, seconds % 60);
}

void refreshTime() {
  if (elapsedLabel == nullptr || durationLabel == nullptr) return;

  char elapsed[12];
  char duration[12];
  formatTime(elapsedMs, elapsed, sizeof(elapsed));
  formatTime(durationMs, duration, sizeof(duration));
  lv_label_set_text(elapsedLabel, elapsed);
  lv_label_set_text(durationLabel, duration);

  const int32_t progress = durationMs == 0
      ? 0
      : static_cast<int32_t>((static_cast<uint64_t>(elapsedMs) * 1000) / durationMs);
  lv_bar_set_value(progressBar, constrain(progress, 0, 1000), LV_ANIM_ON);
  if (lyricsProgressBar != nullptr) {
    lv_bar_set_value(
        lyricsProgressBar,
        constrain(progress, 0, 1000),
        LV_ANIM_ON);
  }
}

void refreshLiked() {
  if (likeLabel == nullptr) return;
  lv_label_set_text(likeLabel, isLiked ? "LIKED" : "LIKE");
  lv_obj_set_style_text_color(
      likeLabel,
      lv_color_hex(isLiked ? 0x1ED760 : 0xD0D0D0),
      0);
}

void refreshAssetStatus() {
  if (assetStatusLabel == nullptr) return;
  if (!coverLoading && !backgroundLoading) {
    lv_obj_add_flag(assetStatusLabel, LV_OBJ_FLAG_HIDDEN);
    return;
  }

  const char* text = coverLoading && backgroundLoading
      ? "CARREGANDO MIDIA..."
      : (coverLoading ? "CARREGANDO CAPA..." : "CARREGANDO FUNDO...");
  lv_label_set_text(assetStatusLabel, text);
  lv_obj_remove_flag(assetStatusLabel, LV_OBJ_FLAG_HIDDEN);
}

void onPrevious(lv_event_t*) {
  DashboardPages::noteActivity();
  if (playerActionHandler != nullptr) playerActionHandler("previous");
}

void onNext(lv_event_t*) {
  DashboardPages::noteActivity();
  if (playerActionHandler != nullptr) playerActionHandler("next");
}

void onPlayPause(lv_event_t*) {
  DashboardPages::noteActivity();
  if (playerActionHandler != nullptr) {
    playerActionHandler(isPlaying ? "pause" : "play");
  }
}

void onLike(lv_event_t*) {
  DashboardPages::noteActivity();
  if (likedHandler != nullptr) likedHandler(!isLiked);
}

void onOpenLyrics(lv_event_t*) {
  DashboardPages::noteActivity();
  DashboardPages::setNavigationLocked(true);
  lv_label_set_text(lyricsMessage, "BUSCANDO LETRA...");
  lv_obj_remove_flag(lyricsMessage, LV_OBJ_FLAG_HIDDEN);
  lv_obj_remove_flag(lyricsOverlay, LV_OBJ_FLAG_HIDDEN);
}

void onCloseLyrics(lv_event_t*) {
  DashboardPages::noteActivity();
  DashboardPages::setNavigationLocked(false);
  lv_obj_add_flag(lyricsOverlay, LV_OBJ_FLAG_HIDDEN);
}

void onWifiButton(lv_event_t*) {
  DashboardPages::noteActivity();
  if (wifiButtonHandler != nullptr) wifiButtonHandler();
}

void onProgressTick(lv_timer_t*) {
  if ((coverLoading || backgroundLoading) &&
      millis() - assetLoadingStartedAt > 12000) {
    coverLoading = false;
    backgroundLoading = false;
    refreshAssetStatus();
  }
  if (!isPlaying || durationMs == 0) return;
  elapsedMs = min(elapsedMs + 1000, durationMs);
  refreshTime();
}

void onEqualizerTick(lv_timer_t*) {
  for (uint8_t index = 0; index < EQUALIZER_BAR_COUNT; index++) {
    equalizerRandom = equalizerRandom * 1664525UL + 1013904223UL;
    const uint8_t target = isPlaying
        ? static_cast<uint8_t>(5 + ((equalizerRandom >> 24) % 34))
        : static_cast<uint8_t>(4 + ((index + 1) % 3));
    equalizerHeights[index] = static_cast<uint8_t>(
        (equalizerHeights[index] * 2 + target) / 3);
    lv_obj_set_pos(
        equalizerBars[index],
        index * 5,
        42 - equalizerHeights[index]);
    lv_obj_set_size(equalizerBars[index], 3, equalizerHeights[index]);
  }
}

void createEqualizer(lv_obj_t* screen) {
  lv_obj_t* container = lv_obj_create(screen);
  lv_obj_set_pos(container, 14, 181);
  lv_obj_set_size(container, 38, 44);
  lv_obj_set_style_bg_opa(container, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(container, 0, 0);
  lv_obj_set_style_pad_all(container, 0, 0);
  lv_obj_remove_flag(container, LV_OBJ_FLAG_SCROLLABLE);

  for (uint8_t index = 0; index < EQUALIZER_BAR_COUNT; index++) {
    equalizerBars[index] = lv_obj_create(container);
    lv_obj_set_pos(
        equalizerBars[index],
        index * 5,
        42 - equalizerHeights[index]);
    lv_obj_set_size(equalizerBars[index], 3, equalizerHeights[index]);
    lv_obj_set_style_radius(equalizerBars[index], 2, 0);
    lv_obj_set_style_border_width(equalizerBars[index], 0, 0);
    lv_obj_set_style_pad_all(equalizerBars[index], 0, 0);
    lv_obj_remove_flag(equalizerBars[index], LV_OBJ_FLAG_SCROLLABLE);
  }
}

lv_obj_t* createButton(
    lv_obj_t* parent,
    int16_t x,
    int16_t y,
    int16_t width,
    int16_t height,
    uint32_t color,
    lv_event_cb_t callback) {
  lv_obj_t* button = lv_button_create(parent);
  lv_obj_set_pos(button, x, y);
  lv_obj_set_size(button, width, height);
  lv_obj_set_style_radius(button, height / 2, 0);
  lv_obj_set_style_bg_color(button, lv_color_hex(color), 0);
  lv_obj_set_style_border_width(button, 0, 0);
  lv_obj_set_style_shadow_width(button, 0, 0);
  lv_obj_add_event_cb(button, callback, LV_EVENT_CLICKED, nullptr);
  return button;
}

lv_obj_t* addCenteredLabel(lv_obj_t* parent, const char* text, uint32_t color) {
  lv_obj_t* label = lv_label_create(parent);
  lv_label_set_text(label, text);
  lv_obj_set_style_text_color(label, lv_color_hex(color), 0);
  lv_obj_center(label);
  return label;
}

void createLyricsOverlay(lv_obj_t* screen) {
  lyricsOverlay = lv_obj_create(screen);
  lv_obj_set_pos(lyricsOverlay, 0, 0);
  lv_obj_set_size(lyricsOverlay, 320, 240);
  lv_obj_set_style_radius(lyricsOverlay, 0, 0);
  lv_obj_set_style_border_width(lyricsOverlay, 0, 0);
  lv_obj_set_style_bg_color(lyricsOverlay, lv_color_hex(0x101311), 0);
  lv_obj_set_style_bg_opa(lyricsOverlay, LV_OPA_COVER, 0);
  lv_obj_remove_flag(lyricsOverlay, LV_OBJ_FLAG_SCROLLABLE);

  lyricsHeading = lv_label_create(lyricsOverlay);
  lv_label_set_text(lyricsHeading, "LETRAS");
  lv_obj_set_style_text_color(lyricsHeading, lv_color_hex(0x1ED760), 0);
  lv_obj_set_pos(lyricsHeading, 12, 10);

  lyricsTitle = lv_label_create(lyricsOverlay);
  lv_label_set_text(lyricsTitle, "Aguardando Spotify");
  lv_obj_set_width(lyricsTitle, 250);
  lv_label_set_long_mode(lyricsTitle, LV_LABEL_LONG_SCROLL_CIRCULAR);
  lv_obj_set_style_text_color(lyricsTitle, lv_color_hex(0xFFFFFF), 0);
  lv_obj_set_pos(lyricsTitle, 12, 34);

  lv_obj_t* closeButton = createButton(
      lyricsOverlay, 272, 8, 38, 38, 0x292D2A, onCloseLyrics);
  addCenteredLabel(closeButton, LV_SYMBOL_CLOSE, 0xFFFFFF);

  lyricsMessage = lv_label_create(lyricsOverlay);
  lv_label_set_text(lyricsMessage, "BUSCANDO LETRA...");
  lv_obj_set_width(lyricsMessage, 260);
  lv_obj_set_style_text_align(lyricsMessage, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_style_text_color(lyricsMessage, lv_color_hex(0xB5B8B6), 0);
  lv_obj_align(lyricsMessage, LV_ALIGN_CENTER, 0, 12);

  constexpr int16_t lineY[LyricsClient::WINDOW_SIZE] = {72, 100, 128, 156, 184};
  for (uint8_t index = 0; index < LyricsClient::WINDOW_SIZE; index++) {
    lyricsLines[index] = lv_label_create(lyricsOverlay);
    lv_label_set_text(lyricsLines[index], "");
    lv_obj_set_pos(lyricsLines[index], 12, lineY[index]);
    lv_obj_set_width(lyricsLines[index], 296);
    lv_label_set_long_mode(lyricsLines[index], LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_align(lyricsLines[index], LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(lyricsLines[index], lv_color_hex(0x737875), 0);
    lv_obj_add_flag(lyricsLines[index], LV_OBJ_FLAG_HIDDEN);
  }

  lyricsProgressBar = lv_bar_create(lyricsOverlay);
  lv_obj_set_pos(lyricsProgressBar, 12, 220);
  lv_obj_set_size(lyricsProgressBar, 296, 4);
  lv_bar_set_range(lyricsProgressBar, 0, 1000);
  lv_obj_set_style_radius(lyricsProgressBar, 2, LV_PART_MAIN);
  lv_obj_set_style_bg_color(
      lyricsProgressBar,
      lv_color_hex(0x343735),
      LV_PART_MAIN);
  lv_obj_set_style_bg_color(
      lyricsProgressBar,
      lv_color_hex(0x1ED760),
      LV_PART_INDICATOR);

  lv_obj_add_flag(lyricsOverlay, LV_OBJ_FLAG_HIDDEN);
}

}  // namespace

namespace UserInterface {

void create() {
  lv_obj_t* screen = lv_screen_active();
  screenRoot = screen;
  lv_obj_remove_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_bg_color(screen, lv_color_hex(0x090B0A), 0);
  lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);
  lv_obj_set_style_pad_all(screen, 0, 0);

  backgroundImage = lv_image_create(screen);
  lv_obj_set_pos(backgroundImage, 0, 0);
  lv_image_set_pivot(backgroundImage, 0, 0);
  lv_obj_add_flag(backgroundImage, LV_OBJ_FLAG_HIDDEN);

  // Uma camada escura garante contraste constante para textos e botoes,
  // independentemente das cores da capa.
  backgroundShade = lv_obj_create(screen);
  lv_obj_set_pos(backgroundShade, 0, 0);
  lv_obj_set_size(backgroundShade, 320, 240);
  lv_obj_set_style_radius(backgroundShade, 0, 0);
  lv_obj_set_style_border_width(backgroundShade, 0, 0);
  lv_obj_set_style_pad_all(backgroundShade, 0, 0);
  lv_obj_set_style_bg_color(backgroundShade, lv_color_hex(0x000000), 0);
  lv_obj_set_style_bg_opa(backgroundShade, LV_OPA_40, 0);
  lv_obj_remove_flag(backgroundShade, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(backgroundShade, LV_OBJ_FLAG_HIDDEN);

  brandLabel = lv_label_create(screen);
  lv_label_set_text(brandLabel, "CYDify");
  lv_obj_set_style_text_color(brandLabel, lv_color_hex(0x1ED760), 0);
  lv_obj_set_pos(brandLabel, 12, 8);

  spotifyStatusLabel = lv_label_create(screen);
  lv_label_set_text(spotifyStatusLabel, "AGUARDANDO");
  lv_obj_set_style_text_color(spotifyStatusLabel, lv_color_hex(0x969A97), 0);
  lv_obj_set_pos(spotifyStatusLabel, 64, 8);

  lv_obj_t* wifiButton = createButton(
      screen, 207, 4, 101, 26, 0x1B1E1C, onWifiButton);
  networkLabel = addCenteredLabel(
      wifiButton, LV_SYMBOL_WIFI " OFFLINE", 0x969A97);

  albumArt = lv_obj_create(screen);
  lv_obj_set_pos(albumArt, 12, 34);
  lv_obj_set_size(albumArt, 100, 100);
  lv_obj_set_style_radius(albumArt, 12, 0);
  lv_obj_set_style_border_width(albumArt, 0, 0);
  lv_obj_set_style_bg_color(albumArt, lv_color_hex(0x1ED760), 0);
  lv_obj_set_style_bg_grad_color(albumArt, lv_color_hex(0x06351A), 0);
  lv_obj_set_style_bg_grad_dir(albumArt, LV_GRAD_DIR_VER, 0);
  lv_obj_set_style_pad_all(albumArt, 0, 0);
  lv_obj_set_style_clip_corner(albumArt, true, 0);
  lv_obj_remove_flag(albumArt, LV_OBJ_FLAG_SCROLLABLE);

  albumImage = lv_image_create(albumArt);
  lv_obj_set_pos(albumImage, 0, 0);
  lv_obj_add_flag(albumImage, LV_OBJ_FLAG_HIDDEN);

  albumPlaceholder = lv_label_create(albumArt);
  lv_label_set_text(albumPlaceholder, LV_SYMBOL_AUDIO);
  lv_obj_set_style_text_color(albumPlaceholder, lv_color_hex(0xFFFFFF), 0);
  lv_obj_center(albumPlaceholder);

  titleLabel = lv_label_create(screen);
  lv_label_set_text(titleLabel, "Aguardando servidor");
  lv_obj_set_pos(titleLabel, 124, 39);
  lv_obj_set_width(titleLabel, 184);
  lv_label_set_long_mode(titleLabel, LV_LABEL_LONG_SCROLL_CIRCULAR);
  lv_obj_set_style_text_color(titleLabel, lv_color_hex(0xFFFFFF), 0);

  artistLabel = lv_label_create(screen);
  lv_label_set_text(artistLabel, "Configure o endereco");
  lv_obj_set_pos(artistLabel, 124, 64);
  lv_obj_set_width(artistLabel, 184);
  lv_label_set_long_mode(artistLabel, LV_LABEL_LONG_SCROLL_CIRCULAR);
  lv_obj_set_style_text_color(artistLabel, lv_color_hex(0xB5B8B6), 0);

  albumLabel = lv_label_create(screen);
  lv_label_set_text(albumLabel, "no portal Wi-Fi");
  lv_obj_set_pos(albumLabel, 124, 85);
  lv_obj_set_width(albumLabel, 184);
  lv_label_set_long_mode(albumLabel, LV_LABEL_LONG_SCROLL_CIRCULAR);
  lv_obj_set_style_text_color(albumLabel, lv_color_hex(0x747875), 0);

  lv_obj_t* likeButton = createButton(screen, 124, 107, 54, 26, 0x222522, onLike);
  likeLabel = addCenteredLabel(likeButton, "LIKE", 0xD0D0D0);

  lv_obj_t* lyricsButton = createButton(screen, 184, 107, 62, 26, 0x222522, onOpenLyrics);
  addCenteredLabel(lyricsButton, "LYRICS", 0xD0D0D0);

  volumeLabel = lv_label_create(screen);
  lv_label_set_text(volumeLabel, LV_SYMBOL_VOLUME_MID " --");
  lv_obj_set_style_text_color(volumeLabel, lv_color_hex(0xA7AAA8), 0);
  lv_obj_set_pos(volumeLabel, 254, 113);

  assetStatusLabel = lv_label_create(screen);
  lv_label_set_text(assetStatusLabel, "CARREGANDO MIDIA...");
  lv_obj_set_pos(assetStatusLabel, 124, 134);
  lv_obj_set_width(assetStatusLabel, 184);
  lv_obj_set_style_text_color(assetStatusLabel, lv_color_hex(0x1ED760), 0);
  lv_obj_add_flag(assetStatusLabel, LV_OBJ_FLAG_HIDDEN);

  progressBar = lv_bar_create(screen);
  lv_obj_set_pos(progressBar, 12, 153);
  lv_obj_set_size(progressBar, 296, 6);
  lv_bar_set_range(progressBar, 0, 1000);
  lv_obj_set_style_radius(progressBar, 3, LV_PART_MAIN);
  lv_obj_set_style_bg_color(progressBar, lv_color_hex(0x343735), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(progressBar, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_radius(progressBar, 3, LV_PART_INDICATOR);
  lv_obj_set_style_bg_color(progressBar, lv_color_hex(0x1ED760), LV_PART_INDICATOR);

  elapsedLabel = lv_label_create(screen);
  lv_obj_set_pos(elapsedLabel, 12, 162);
  lv_obj_set_style_text_color(elapsedLabel, lv_color_hex(0x858986), 0);

  durationLabel = lv_label_create(screen);
  lv_obj_align(durationLabel, LV_ALIGN_TOP_RIGHT, -12, 162);
  lv_obj_set_style_text_color(durationLabel, lv_color_hex(0x858986), 0);

  lv_obj_t* previousButton = createButton(screen, 68, 181, 44, 44, 0x1B1E1C, onPrevious);
  addCenteredLabel(previousButton, LV_SYMBOL_PREV, 0xFFFFFF);

  playButtonObject = createButton(screen, 132, 175, 56, 56, 0x1ED760, onPlayPause);
  playLabel = addCenteredLabel(playButtonObject, LV_SYMBOL_PLAY, 0x071009);

  lv_obj_t* nextButton = createButton(screen, 208, 181, 44, 44, 0x1B1E1C, onNext);
  addCenteredLabel(nextButton, LV_SYMBOL_NEXT, 0xFFFFFF);

  createEqualizer(screen);
  createLyricsOverlay(screen);
  DashboardPages::begin(screen);
  applyDynamicTheme(0x1ED760);
  refreshTime();
  lv_timer_create(onProgressTick, 1000, nullptr);
  lv_timer_create(onEqualizerTick, 120, nullptr);
  Serial.println("Interface Spotify criada.");
}

void setWifiButtonHandler(WifiButtonHandler handler) {
  wifiButtonHandler = handler;
  DashboardPages::setHandlers(brightnessHandler, wifiButtonHandler);
}

void setPlayerHandlers(PlayerActionHandler actionHandler, LikedHandler newLikedHandler) {
  playerActionHandler = actionHandler;
  likedHandler = newLikedHandler;
}

void setBrightnessHandler(
    BrightnessHandler handler,
    uint8_t initialBrightness) {
  brightnessHandler = handler;
  DashboardPages::setHandlers(brightnessHandler, wifiButtonHandler);
  DashboardPages::setBrightness(initialBrightness);
}

void syncBrightness(uint8_t percent) {
  DashboardPages::setBrightness(percent);
}

void setNetworkStatus(const char* text, bool connected) {
  if (networkLabel == nullptr) return;
  lv_label_set_text(networkLabel, text);
  lv_obj_set_style_text_color(
      networkLabel,
      lv_color_hex(connected ? 0x1ED760 : 0xB5B8B6),
      0);
}

void setSpotifyStatus(const char* text, bool connected) {
  if (spotifyStatusLabel == nullptr) return;
  lv_label_set_text(spotifyStatusLabel, text);
  lv_obj_set_style_text_color(
      spotifyStatusLabel,
      lv_color_hex(connected ? 0x1ED760 : 0xF59E0B),
      0);
}

void updatePlayer(const PlayerClient::State& state) {
  isPlaying = state.playing;
  isLiked = state.liked;
  elapsedMs = state.progressMs;
  durationMs = state.durationMs;
  if (!state.available) {
    setMediaLoading(false);
  }

  lv_label_set_text(titleLabel, state.available ? state.title : "Nenhuma musica");
  lv_label_set_text(artistLabel, state.available ? state.artist : "Spotify parado");
  lv_label_set_text(albumLabel, state.available ? state.album : "");
  lv_label_set_text(lyricsTitle, state.title);
  lv_label_set_text(playLabel, isPlaying ? LV_SYMBOL_PAUSE : LV_SYMBOL_PLAY);

  char volume[16];
  if (state.volumePercent >= 0) snprintf(volume, sizeof(volume), LV_SYMBOL_VOLUME_MID " %d%%", state.volumePercent);
  else snprintf(volume, sizeof(volume), LV_SYMBOL_VOLUME_MID " --");
  lv_label_set_text(volumeLabel, volume);

  refreshLiked();
  refreshTime();
  DashboardPages::updatePlayer(state);
}

void setMediaLoading(bool loading) {
  coverLoading = loading;
  backgroundLoading = loading;
  assetLoadingStartedAt = loading ? millis() : 0;
  if (albumPlaceholder != nullptr && loading) {
    lv_label_set_text(albumPlaceholder, LV_SYMBOL_REFRESH);
  } else if (albumPlaceholder != nullptr) {
    lv_label_set_text(albumPlaceholder, LV_SYMBOL_AUDIO);
  }
  refreshAssetStatus();
}

void updateAlbumArt(
    const uint16_t* pixels,
    uint16_t width,
    uint16_t height,
    uint32_t dominantColor) {
  if (albumImage == nullptr || pixels == nullptr || width == 0 || height == 0) return;

  albumImageDescriptor.header.magic = LV_IMAGE_HEADER_MAGIC;
  albumImageDescriptor.header.cf = LV_COLOR_FORMAT_RGB565;
  albumImageDescriptor.header.w = width;
  albumImageDescriptor.header.h = height;
  albumImageDescriptor.header.stride = width * sizeof(uint16_t);
  albumImageDescriptor.data_size = width * height * sizeof(uint16_t);
  albumImageDescriptor.data = reinterpret_cast<const uint8_t*>(pixels);

  lv_image_set_src(albumImage, &albumImageDescriptor);
  lv_obj_remove_flag(albumImage, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(albumPlaceholder, LV_OBJ_FLAG_HIDDEN);
  lv_obj_invalidate(albumImage);
  applyDynamicTheme(dominantColor);
  coverLoading = false;
  refreshAssetStatus();
}

void clearAlbumArt() {
  if (albumImage == nullptr || albumPlaceholder == nullptr) return;
  lv_obj_add_flag(albumImage, LV_OBJ_FLAG_HIDDEN);
  lv_label_set_text(albumPlaceholder, LV_SYMBOL_REFRESH);
  lv_obj_remove_flag(albumPlaceholder, LV_OBJ_FLAG_HIDDEN);
}

void updateBackground(
    const uint16_t* pixels,
    uint16_t width,
    uint16_t height) {
  if (backgroundImage == nullptr ||
      backgroundShade == nullptr ||
      pixels == nullptr ||
      width == 0 ||
      height == 0) {
    return;
  }

  backgroundImageDescriptor.header.magic = LV_IMAGE_HEADER_MAGIC;
  backgroundImageDescriptor.header.cf = LV_COLOR_FORMAT_RGB565;
  backgroundImageDescriptor.header.w = width;
  backgroundImageDescriptor.header.h = height;
  backgroundImageDescriptor.header.stride = width * sizeof(uint16_t);
  backgroundImageDescriptor.data_size = width * height * sizeof(uint16_t);
  backgroundImageDescriptor.data = reinterpret_cast<const uint8_t*>(pixels);

  lv_image_set_src(backgroundImage, &backgroundImageDescriptor);
  lv_image_set_pivot(backgroundImage, 0, 0);
  lv_image_set_scale_x(backgroundImage, (320UL * 256UL) / width);
  lv_image_set_scale_y(backgroundImage, (240UL * 256UL) / height);
  lv_obj_remove_flag(backgroundImage, LV_OBJ_FLAG_HIDDEN);
  lv_obj_remove_flag(backgroundShade, LV_OBJ_FLAG_HIDDEN);
  lv_obj_invalidate(backgroundImage);
  backgroundLoading = false;
  refreshAssetStatus();
}

void clearBackground() {
  if (backgroundImage != nullptr) {
    lv_obj_add_flag(backgroundImage, LV_OBJ_FLAG_HIDDEN);
  }
  if (backgroundShade != nullptr) {
    lv_obj_add_flag(backgroundShade, LV_OBJ_FLAG_HIDDEN);
  }
}

bool lyricsVisible() {
  return lyricsOverlay != nullptr &&
      !lv_obj_has_flag(lyricsOverlay, LV_OBJ_FLAG_HIDDEN);
}

void updateLyrics(const LyricsClient::Window& window) {
  if (lyricsMessage == nullptr) return;

  const bool showMessage = !window.available || window.count == 0;
  if (showMessage) {
    const char* message = window.message[0] == '\0'
        ? (window.instrumental ? "FAIXA INSTRUMENTAL" : "LETRA NAO ENCONTRADA")
        : window.message;
    lv_label_set_text(lyricsMessage, message);
    lv_obj_remove_flag(lyricsMessage, LV_OBJ_FLAG_HIDDEN);
  } else {
    lv_obj_add_flag(lyricsMessage, LV_OBJ_FLAG_HIDDEN);
  }

  for (uint8_t index = 0; index < LyricsClient::WINDOW_SIZE; index++) {
    if (index >= window.count || showMessage) {
      lv_obj_add_flag(lyricsLines[index], LV_OBJ_FLAG_HIDDEN);
      continue;
    }
    lv_label_set_text(lyricsLines[index], window.lines[index]);
    lv_obj_set_style_text_color(
        lyricsLines[index],
        lv_color_hex(index == window.currentSlot ? 0xFFFFFF : 0x737875),
        0);
    lv_obj_remove_flag(lyricsLines[index], LV_OBJ_FLAG_HIDDEN);
  }
}

}  // namespace UserInterface
