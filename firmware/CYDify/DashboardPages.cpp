#include "DashboardPages.h"

#include <time.h>

#include "CydifyClockFont.h"
#include "HardwareConfig.h"

namespace {

enum class Page : uint8_t {
  Home,
  Info,
  Stats,
  Settings,
  Sleep,
};

lv_obj_t* rootScreen = nullptr;
lv_obj_t* infoPage = nullptr;
lv_obj_t* statsPage = nullptr;
lv_obj_t* settingsPage = nullptr;
lv_obj_t* sleepPage = nullptr;
lv_obj_t* infoTitle = nullptr;
lv_obj_t* infoArtist = nullptr;
lv_obj_t* infoAlbum = nullptr;
lv_obj_t* infoRelease = nullptr;
lv_obj_t* infoTrack = nullptr;
lv_obj_t* infoDuration = nullptr;
lv_obj_t* infoPopularity = nullptr;
lv_obj_t* infoDevice = nullptr;
lv_obj_t* statsTodayTime = nullptr;
lv_obj_t* statsTracks = nullptr;
lv_obj_t* statsArtists = nullptr;
lv_obj_t* statsAlbums = nullptr;
lv_obj_t* statsWeekTime = nullptr;
lv_obj_t* statsTopArtist = nullptr;
lv_obj_t* statsTopTrack = nullptr;
lv_obj_t* brightnessSlider = nullptr;
lv_obj_t* brightnessValue = nullptr;
lv_obj_t* systemInfo = nullptr;
lv_obj_t* clockLabel = nullptr;
lv_obj_t* dateLabel = nullptr;
lv_obj_t* sleepTrackLabel = nullptr;

DashboardPages::BrightnessHandler onBrightness = nullptr;
DashboardPages::WifiHandler wifiHandler = nullptr;

Page currentPage = Page::Home;
bool playerIsPlaying = false;
bool previousPlaying = false;
bool navigationLocked = false;
uint32_t idleSince = 0;

lv_obj_t* addLabel(
    lv_obj_t* parent,
    const char* text,
    int16_t x,
    int16_t y,
    int16_t width,
    uint32_t color) {
  lv_obj_t* label = lv_label_create(parent);
  lv_label_set_text(label, text);
  lv_obj_set_pos(label, x, y);
  lv_obj_set_width(label, width);
  lv_obj_set_style_text_color(label, lv_color_hex(color), 0);
  lv_label_set_long_mode(label, LV_LABEL_LONG_DOT);
  return label;
}

lv_obj_t* createPage(const char* title, const char* hint) {
  lv_obj_t* page = lv_obj_create(rootScreen);
  lv_obj_set_pos(page, 0, 0);
  lv_obj_set_size(page, 320, 240);
  lv_obj_set_style_radius(page, 0, 0);
  lv_obj_set_style_border_width(page, 0, 0);
  lv_obj_set_style_pad_all(page, 0, 0);
  lv_obj_set_style_bg_color(page, lv_color_hex(0x0D100E), 0);
  lv_obj_set_style_bg_grad_color(page, lv_color_hex(0x172019), 0);
  lv_obj_set_style_bg_grad_dir(page, LV_GRAD_DIR_VER, 0);
  lv_obj_remove_flag(page, LV_OBJ_FLAG_SCROLLABLE);

  addLabel(page, "CYDify", 12, 9, 60, 0x1ED760);
  addLabel(page, title, 76, 9, 160, 0xFFFFFF);
  lv_obj_t* hintLabel = addLabel(page, hint, 238, 9, 70, 0x747875);
  lv_obj_set_style_text_align(hintLabel, LV_TEXT_ALIGN_RIGHT, 0);
  return page;
}

void setObjectX(void* object, int32_t value) {
  lv_obj_set_x(static_cast<lv_obj_t*>(object), value);
}

void animateIn(lv_obj_t* page, int16_t fromX) {
  lv_obj_remove_flag(page, LV_OBJ_FLAG_HIDDEN);
  lv_obj_set_x(page, fromX);

  lv_anim_t animation;
  lv_anim_init(&animation);
  lv_anim_set_var(&animation, page);
  lv_anim_set_values(&animation, fromX, 0);
  lv_anim_set_duration(&animation, 190);
  lv_anim_set_path_cb(&animation, lv_anim_path_ease_out);
  lv_anim_set_exec_cb(&animation, setObjectX);
  lv_anim_start(&animation);
}

void hidePages() {
  lv_obj_add_flag(infoPage, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(statsPage, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(settingsPage, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(sleepPage, LV_OBJ_FLAG_HIDDEN);
}

void showPage(Page page, int16_t fromX = 320) {
  if (page == currentPage &&
      !(page == Page::Home && DashboardPages::sleepVisible())) {
    return;
  }

  hidePages();
  currentPage = page;
  idleSince = millis();

  if (page == Page::Info) animateIn(infoPage, fromX);
  else if (page == Page::Stats) animateIn(statsPage, fromX);
  else if (page == Page::Settings) animateIn(settingsPage, fromX);
  else if (page == Page::Sleep) animateIn(sleepPage, 0);
}

void onGesture(lv_event_t*) {
  lv_indev_t* input = lv_indev_active();
  if (input == nullptr || currentPage == Page::Sleep || navigationLocked) return;

  DashboardPages::noteActivity();
  const lv_dir_t direction = lv_indev_get_gesture_dir(input);
  if (direction == LV_DIR_LEFT) {
    if (currentPage == Page::Home) showPage(Page::Info, 320);
    else if (currentPage == Page::Info) showPage(Page::Stats, 320);
    else if (currentPage == Page::Stats) showPage(Page::Settings, 320);
  } else if (direction == LV_DIR_RIGHT) {
    if (currentPage == Page::Settings) showPage(Page::Stats, -320);
    else if (currentPage == Page::Stats) showPage(Page::Info, -320);
    else if (currentPage == Page::Info) showPage(Page::Home, -320);
  }
}

void onBrightnessChanged(lv_event_t* event) {
  const int32_t value = lv_slider_get_value(
      static_cast<lv_obj_t*>(lv_event_get_target(event)));
  char text[12];
  snprintf(text, sizeof(text), "%ld%%", static_cast<long>(value));
  lv_label_set_text(brightnessValue, text);
  DashboardPages::noteActivity();
}

void onBrightnessReleased(lv_event_t*) {
  if (onBrightness != nullptr) {
    onBrightness(static_cast<uint8_t>(lv_slider_get_value(brightnessSlider)));
  }
}

void onWifiPressed(lv_event_t*) {
  DashboardPages::noteActivity();
  if (wifiHandler != nullptr) wifiHandler();
}

void onWake(lv_event_t*) {
  showPage(Page::Home);
}

void updateClock() {
  const time_t now = time(nullptr);
  if (now < 1609459200) {
    lv_label_set_text(clockLabel, "--:--");
    lv_label_set_text(dateLabel, "SINCRONIZANDO RELOGIO");
    return;
  }

  struct tm localTime;
  localtime_r(&now, &localTime);
  char clockText[8];
  char dateText[32];
  constexpr const char* WEEKDAYS[] = {
      "domingo", "segunda", "terca", "quarta",
      "quinta", "sexta", "sabado"};
  strftime(clockText, sizeof(clockText), "%H:%M", &localTime);
  snprintf(
      dateText,
      sizeof(dateText),
      "%s, %02d/%02d/%04d",
      WEEKDAYS[localTime.tm_wday],
      localTime.tm_mday,
      localTime.tm_mon + 1,
      localTime.tm_year + 1900);
  lv_label_set_text(clockLabel, clockText);
  lv_label_set_text(dateLabel, dateText);
}

void onSystemTick(lv_timer_t*) {
  updateClock();

  char details[120];
  snprintf(
      details,
      sizeof(details),
      "VERSAO  %s\nHEAP LIVRE  %lu KB\nPSRAM  NAO DISPONIVEL\nDESCANSO  90 SEG",
      HardwareConfig::APP_VERSION,
      static_cast<unsigned long>(ESP.getFreeHeap() / 1024));
  lv_label_set_text(systemInfo, details);

  if (!playerIsPlaying &&
      !navigationLocked &&
      currentPage != Page::Settings &&
      currentPage != Page::Sleep &&
      millis() - idleSince >= HardwareConfig::SLEEP_MODE_TIMEOUT_MS) {
    showPage(Page::Sleep, 0);
  }
}

void createInfoPage() {
  infoPage = createPage("MUSIC INFO", "1/3  >");

  infoTitle = addLabel(
      infoPage, "Nenhuma musica", 12, 36, 296, 0xFFFFFF);
  infoArtist = addLabel(
      infoPage, "Spotify parado", 12, 57, 296, 0xB5B8B6);
  lv_obj_set_style_text_align(infoTitle, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_style_text_align(infoArtist, LV_TEXT_ALIGN_CENTER, 0);

  lv_obj_t* albumCaption = addLabel(
      infoPage, "ALBUM", 12, 82, 296, 0x1ED760);
  infoAlbum = addLabel(infoPage, "--", 12, 99, 296, 0xFFFFFF);
  lv_obj_set_style_text_align(albumCaption, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_style_text_align(infoAlbum, LV_TEXT_ALIGN_CENTER, 0);

  lv_obj_t* releaseCaption = addLabel(
      infoPage, "LANCAMENTO", 12, 124, 142, 0x747875);
  lv_obj_t* durationCaption = addLabel(
      infoPage, "DURACAO", 166, 124, 142, 0x747875);
  infoRelease = addLabel(infoPage, "--", 12, 142, 142, 0xFFFFFF);
  infoDuration = addLabel(infoPage, "--", 166, 142, 142, 0xFFFFFF);

  lv_obj_t* trackCaption = addLabel(
      infoPage, "FAIXA", 12, 165, 142, 0x747875);
  lv_obj_t* popularityCaption = addLabel(
      infoPage, "POPULARIDADE", 166, 165, 142, 0x747875);
  infoTrack = addLabel(infoPage, "--", 12, 183, 142, 0xFFFFFF);
  infoPopularity = addLabel(infoPage, "--", 166, 183, 142, 0xFFFFFF);

  lv_obj_t* centeredLabels[] = {
      releaseCaption,
      durationCaption,
      infoRelease,
      infoDuration,
      trackCaption,
      popularityCaption,
      infoTrack,
      infoPopularity,
  };
  for (lv_obj_t* label : centeredLabels) {
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
  }

  infoDevice = addLabel(
      infoPage, "DISPOSITIVO  --", 12, 214, 296, 0xB5B8B6);
  lv_obj_set_style_text_align(infoDevice, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_add_flag(infoPage, LV_OBJ_FLAG_HIDDEN);
}

void createStatsPage() {
  statsPage = createPage("ESTATISTICAS", "<  2/3  >");

  lv_obj_t* todayCaption = addLabel(
      statsPage, "TEMPO OUVINDO HOJE", 12, 36, 296, 0x1ED760);
  lv_obj_set_style_text_align(todayCaption, LV_TEXT_ALIGN_CENTER, 0);

  statsTodayTime = addLabel(
      statsPage, "0m 00s", 12, 54, 296, 0xFFFFFF);
  lv_obj_set_style_text_align(statsTodayTime, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_style_text_font(statsTodayTime, &cydify_font_clock_32, 0);

  lv_obj_t* tracksCaption = addLabel(
      statsPage, "MUSICAS", 12, 97, 92, 0x747875);
  lv_obj_t* artistsCaption = addLabel(
      statsPage, "ARTISTAS", 114, 97, 92, 0x747875);
  lv_obj_t* albumsCaption = addLabel(
      statsPage, "ALBUNS", 216, 97, 92, 0x747875);
  statsTracks = addLabel(statsPage, "0", 12, 115, 92, 0xFFFFFF);
  statsArtists = addLabel(statsPage, "0", 114, 115, 92, 0xFFFFFF);
  statsAlbums = addLabel(statsPage, "0", 216, 115, 92, 0xFFFFFF);

  lv_obj_t* centeredMetrics[] = {
      tracksCaption,
      artistsCaption,
      albumsCaption,
      statsTracks,
      statsArtists,
      statsAlbums,
  };
  for (lv_obj_t* label : centeredMetrics) {
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
  }

  lv_obj_t* weekCaption = addLabel(
      statsPage, "ESTA SEMANA", 12, 139, 142, 0x747875);
  statsWeekTime = addLabel(statsPage, "0m", 166, 139, 142, 0xFFFFFF);
  lv_obj_set_style_text_align(weekCaption, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_style_text_align(statsWeekTime, LV_TEXT_ALIGN_CENTER, 0);

  lv_obj_t* artistCaption = addLabel(
      statsPage, "TOP ARTISTA", 12, 163, 296, 0x747875);
  statsTopArtist = addLabel(
      statsPage, "Ainda sem dados", 12, 180, 296, 0xFFFFFF);
  lv_obj_set_style_text_align(artistCaption, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_style_text_align(statsTopArtist, LV_TEXT_ALIGN_CENTER, 0);

  lv_obj_t* trackCaption = addLabel(
      statsPage, "TOP MUSICA", 12, 200, 296, 0x747875);
  statsTopTrack = addLabel(
      statsPage, "Ainda sem dados", 12, 217, 296, 0xFFFFFF);
  lv_obj_set_style_text_align(trackCaption, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_style_text_align(statsTopTrack, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_add_flag(statsPage, LV_OBJ_FLAG_HIDDEN);
}

void createSettingsPage() {
  settingsPage = createPage("SETTINGS", "<  3/3");

  addLabel(settingsPage, "BRILHO DA TELA", 12, 43, 180, 0xFFFFFF);
  brightnessValue = addLabel(
      settingsPage, "85%", 254, 43, 54, 0x1ED760);
  lv_obj_set_style_text_align(brightnessValue, LV_TEXT_ALIGN_RIGHT, 0);

  brightnessSlider = lv_slider_create(settingsPage);
  lv_obj_set_pos(brightnessSlider, 12, 70);
  lv_obj_set_size(brightnessSlider, 296, 12);
  lv_slider_set_range(brightnessSlider, 5, 100);
  lv_obj_set_style_bg_color(
      brightnessSlider, lv_color_hex(0x343735), LV_PART_MAIN);
  lv_obj_set_style_bg_color(
      brightnessSlider, lv_color_hex(0x1ED760), LV_PART_INDICATOR);
  lv_obj_set_style_bg_color(
      brightnessSlider, lv_color_hex(0xFFFFFF), LV_PART_KNOB);
  lv_obj_add_event_cb(
      brightnessSlider,
      onBrightnessChanged,
      LV_EVENT_VALUE_CHANGED,
      nullptr);
  lv_obj_add_event_cb(
      brightnessSlider,
      onBrightnessReleased,
      LV_EVENT_RELEASED,
      nullptr);

  lv_obj_t* wifiButton = lv_button_create(settingsPage);
  lv_obj_set_pos(wifiButton, 12, 101);
  lv_obj_set_size(wifiButton, 140, 38);
  lv_obj_set_style_radius(wifiButton, 19, 0);
  lv_obj_set_style_bg_color(wifiButton, lv_color_hex(0x1ED760), 0);
  lv_obj_add_event_cb(wifiButton, onWifiPressed, LV_EVENT_CLICKED, nullptr);
  lv_obj_t* wifiLabel = lv_label_create(wifiButton);
  lv_label_set_text(wifiLabel, LV_SYMBOL_WIFI " CONFIGURAR");
  lv_obj_set_style_text_color(wifiLabel, lv_color_hex(0x071009), 0);
  lv_obj_center(wifiLabel);

  systemInfo = addLabel(
      settingsPage, "CARREGANDO SISTEMA...", 12, 153, 296, 0xB5B8B6);
  lv_label_set_long_mode(systemInfo, LV_LABEL_LONG_WRAP);
  addLabel(
      settingsPage,
      "Deslize para a direita para voltar",
      12,
      219,
      296,
      0x747875);
  lv_obj_add_flag(settingsPage, LV_OBJ_FLAG_HIDDEN);
}

void createSleepPage() {
  sleepPage = lv_obj_create(rootScreen);
  lv_obj_set_pos(sleepPage, 0, 0);
  lv_obj_set_size(sleepPage, 320, 240);
  lv_obj_set_style_radius(sleepPage, 0, 0);
  lv_obj_set_style_border_width(sleepPage, 0, 0);
  lv_obj_set_style_bg_color(sleepPage, lv_color_hex(0x030504), 0);
  lv_obj_set_style_bg_opa(sleepPage, LV_OPA_COVER, 0);
  lv_obj_remove_flag(sleepPage, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_event_cb(sleepPage, onWake, LV_EVENT_CLICKED, nullptr);

  lv_obj_t* sleepBrand = addLabel(
      sleepPage, "CYDify", 12, 20, 296, 0x1ED760);
  lv_obj_set_style_text_align(sleepBrand, LV_TEXT_ALIGN_CENTER, 0);

  clockLabel = addLabel(sleepPage, "--:--", 12, 64, 296, 0xFFFFFF);
  lv_obj_set_style_text_align(clockLabel, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_style_text_font(clockLabel, &cydify_font_clock_32, 0);

  dateLabel = addLabel(
      sleepPage, "SINCRONIZANDO RELOGIO", 12, 120, 296, 0xB5B8B6);
  lv_obj_set_style_text_align(dateLabel, LV_TEXT_ALIGN_CENTER, 0);
  sleepTrackLabel = addLabel(
      sleepPage, "Spotify em descanso", 12, 158, 296, 0x747875);
  lv_obj_set_style_text_align(sleepTrackLabel, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_t* wakeHint = addLabel(
      sleepPage, "TOQUE PARA VOLTAR", 12, 204, 296, 0x1ED760);
  lv_obj_set_style_text_align(wakeHint, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_add_flag(sleepPage, LV_OBJ_FLAG_HIDDEN);
}

void enableGestureBubble(lv_obj_t* object) {
  const uint32_t childCount = lv_obj_get_child_count(object);
  for (uint32_t index = 0; index < childCount; index++) {
    lv_obj_t* child = lv_obj_get_child(object, index);
    lv_obj_add_flag(child, LV_OBJ_FLAG_GESTURE_BUBBLE);
    enableGestureBubble(child);
  }
}

}  // namespace

namespace DashboardPages {

void begin(lv_obj_t* screen) {
  rootScreen = screen;
  idleSince = millis();
  createInfoPage();
  createStatsPage();
  createSettingsPage();
  createSleepPage();
  enableGestureBubble(rootScreen);
  lv_obj_remove_flag(brightnessSlider, LV_OBJ_FLAG_GESTURE_BUBBLE);
  lv_obj_add_event_cb(rootScreen, onGesture, LV_EVENT_GESTURE, nullptr);
  lv_timer_create(onSystemTick, 1000, nullptr);
  updateClock();
}

void setHandlers(
    BrightnessHandler brightnessHandler,
    WifiHandler wifiHandler) {
  onBrightness = brightnessHandler;
  ::wifiHandler = wifiHandler;
}

void setBrightness(uint8_t percent) {
  const uint8_t safePercent = constrain(
      percent,
      static_cast<uint8_t>(5),
      static_cast<uint8_t>(100));
  lv_slider_set_value(brightnessSlider, safePercent, LV_ANIM_OFF);
  char text[12];
  snprintf(text, sizeof(text), "%u%%", safePercent);
  lv_label_set_text(brightnessValue, text);
}

void updatePlayer(const PlayerClient::State& state) {
  lv_label_set_text(infoTitle, state.available ? state.title : "Nenhuma musica");
  lv_label_set_text(infoArtist, state.available ? state.artist : "Spotify parado");
  lv_label_set_text(infoAlbum, state.available ? state.album : "--");
  lv_label_set_text(infoRelease, state.releaseDate[0] ? state.releaseDate : "--");

  char value[160];
  if (state.trackNumber > 0) {
    snprintf(
        value,
        sizeof(value),
        "D%u T%u",
        state.discNumber,
        state.trackNumber);
  } else {
    snprintf(value, sizeof(value), "--");
  }
  lv_label_set_text(infoTrack, value);

  const uint32_t seconds = state.durationMs / 1000;
  snprintf(value, sizeof(value), "%lu:%02lu", seconds / 60, seconds % 60);
  lv_label_set_text(infoDuration, value);

  if (state.popularity >= 0) {
    snprintf(value, sizeof(value), "%d/100", state.popularity);
  } else {
    snprintf(value, sizeof(value), "--");
  }
  lv_label_set_text(infoPopularity, value);

  snprintf(
      value,
      sizeof(value),
      "DISPOSITIVO  %s%s%s",
      state.device[0] ? state.device : "--",
      state.deviceType[0] ? " / " : "",
      state.deviceType);
  lv_label_set_text(infoDevice, value);

  const uint32_t todayMinutes = state.todayListeningSeconds / 60;
  if (todayMinutes >= 60) {
    snprintf(
        value,
        sizeof(value),
        "%luh %02lum",
        static_cast<unsigned long>(todayMinutes / 60),
        static_cast<unsigned long>(todayMinutes % 60));
  } else {
    snprintf(
        value,
        sizeof(value),
        "%lum %02lus",
        static_cast<unsigned long>(todayMinutes),
        static_cast<unsigned long>(state.todayListeningSeconds % 60));
  }
  lv_label_set_text(statsTodayTime, value);

  snprintf(value, sizeof(value), "%u", state.tracksToday);
  lv_label_set_text(statsTracks, value);
  snprintf(value, sizeof(value), "%u", state.artistsToday);
  lv_label_set_text(statsArtists, value);
  snprintf(value, sizeof(value), "%u", state.albumsToday);
  lv_label_set_text(statsAlbums, value);

  const uint32_t weekMinutes = state.weekListeningSeconds / 60;
  if (weekMinutes >= 60) {
    snprintf(
        value,
        sizeof(value),
        "%luh %02lum",
        static_cast<unsigned long>(weekMinutes / 60),
        static_cast<unsigned long>(weekMinutes % 60));
  } else {
    snprintf(
        value,
        sizeof(value),
        "%lum",
        static_cast<unsigned long>(weekMinutes));
  }
  lv_label_set_text(statsWeekTime, value);
  lv_label_set_text(
      statsTopArtist,
      state.topArtist[0] ? state.topArtist : "Ainda sem dados");
  lv_label_set_text(
      statsTopTrack,
      state.topTrack[0] ? state.topTrack : "Ainda sem dados");

  lv_label_set_text(
      sleepTrackLabel,
      state.title[0] ? state.title : "Spotify em descanso");

  previousPlaying = playerIsPlaying;
  playerIsPlaying = state.playing;
  if (playerIsPlaying) {
    idleSince = millis();
    if (!previousPlaying && currentPage != Page::Home) {
      showPage(Page::Home, -320);
    }
  } else if (previousPlaying) {
    idleSince = millis();
  }
}

void noteActivity() {
  idleSince = millis();
}

void setNavigationLocked(bool locked) {
  navigationLocked = locked;
  noteActivity();
}

bool sleepVisible() {
  return currentPage == Page::Sleep &&
      sleepPage != nullptr &&
      !lv_obj_has_flag(sleepPage, LV_OBJ_FLAG_HIDDEN);
}

}  // namespace DashboardPages
