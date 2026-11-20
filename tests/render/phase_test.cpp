#include <LovyanGFX.hpp>
#include "../../desk-display/firmware/DeskDisplay/apps.h"
#include <cassert>
#include <cstdio>
int main() {
  Weather w; ClockState c;
  parseWeather("OK\nX\n1\nF\nCLEAR\n0\n1\n1\n1\n1\n1\n1\nD|387|1012\nTODAY|0|1|1\n", w);
  assert(w.valid && w.sunrise == 387 && w.sunset == 1012 && !w.night && strcmp(w.days[0].name, "TODAY") == 0);
  assert(skyPhase(w, c) == PHASE_DAY);                       // clock not known yet: go by the D/N flag
  c.known = true;
  auto at = [&](int m) { c.epoch = 1794787200u / 86400 * 86400 + m * 60; return skyPhase(w, c); };
  assert(at(0) == PHASE_NIGHT && at(351) == PHASE_NIGHT && at(352) == PHASE_DAWN && at(387) == PHASE_DAWN && at(422) == PHASE_DAWN);
  assert(at(423) == PHASE_DAY && at(700) == PHASE_DAY && at(976) == PHASE_DAY && at(977) == PHASE_DUSK && at(1047) == PHASE_DUSK);
  assert(at(1048) == PHASE_NIGHT && at(1439) == PHASE_NIGHT);
  // the sun stays on screen all day and night, left to right
  int lastX = -1;
  for (int m = 352; m <= 1047; ++m) { at(m); SceneInfo i = skyInfo(w, c); sunOnArc(w, c, i); assert(i.sunX >= 36 && i.sunX <= 284 && i.sunY >= 34 && i.sunY <= 66 && i.sunX >= lastX); lastX = i.sunX; }
  lastX = -1;
  for (int k = 0; k < 1440 - (1047 - 352) - 1; ++k) { int m = (1048 + k) % 1440; at(m); SceneInfo i = skyInfo(w, c); assert(i.phase == PHASE_NIGHT); sunOnArc(w, c, i); assert(i.sunX >= 36 && i.sunX <= 284 && i.sunY >= 34 && i.sunY <= 66 && i.sunX >= lastX); lastX = i.sunX; }
  // older companion: no sun times, only the flag
  parseWeather("OK\nX\n1\nF\nCLEAR\n0\n1\n1\n1\n1\n1\n1\nN\nTODAY|0|1|1\nTUE|4|2|3\n", w);
  assert(w.sunrise == -1 && w.night && at(700) == PHASE_NIGHT && strcmp(w.days[1].name, "TUE") == 0 && w.days[1].icon == 4);
  // nonsense sun times are ignored
  parseWeather("OK\nX\n1\nF\nCLEAR\n0\n1\n1\n1\n1\n1\n1\nD|900|300\nTODAY|0|1|1\n", w);
  assert(w.sunrise == -1 && at(700) == PHASE_DAY);
  // no weather at all: assumed 6:30 and 18:30
  Weather none; w = none;
  assert(at(390) == PHASE_DAWN && at(720) == PHASE_DAY && at(1110) == PHASE_DUSK && at(1300) == PHASE_NIGHT);
  // settings rows
  Settings s; char v[14]; bool sw, on;
  assert(s.weatherC && !s.statsF && s.cover == 2 && s.visualizer && s.clockSky && s.sunGlow && s.saver == 2);
  settingValue(s, SET_WEATHER_UNIT, v, sizeof v, sw, on); assert(strcmp(v, "C") == 0);
  settingValue(s, SET_STATS_UNIT, v, sizeof v, sw, on); assert(strcmp(v, "C") == 0);
  settingValue(s, SET_SAVER, v, sizeof v, sw, on); assert(strcmp(v, "5 MIN") == 0);
  settingAdvance(s, SET_SAVER); settingValue(s, SET_SAVER, v, sizeof v, sw, on); assert(strcmp(v, "15 MIN") == 0);
  settingAdvance(s, SET_SAVER); settingValue(s, SET_SAVER, v, sizeof v, sw, on); assert(strcmp(v, "OFF") == 0 && sw && !on);
  settingAdvance(s, SET_SAVER); settingValue(s, SET_SAVER, v, sizeof v, sw, on); assert(strcmp(v, "1 MIN") == 0);
  settingValue(s, SET_FULL_ART, v, sizeof v, sw, on); assert(strcmp(v, "AUTO") == 0);
  settingAdvance(s, SET_FULL_ART); settingValue(s, SET_FULL_ART, v, sizeof v, sw, on); assert(strcmp(v, "ON TAP") == 0 && s.cover == 1);
  settingAdvance(s, SET_FULL_ART); settingValue(s, SET_FULL_ART, v, sizeof v, sw, on); assert(strcmp(v, "OFF") == 0 && s.cover == 0);
  settingAdvance(s, SET_FULL_ART); assert(s.cover == 2);
  for (int r : { SET_VISUALIZER, SET_CLOCK_SKY, SET_SUN_GLOW }) {
    settingValue(s, r, v, sizeof v, sw, on); assert(sw && on && strcmp(v, "ON") == 0);
    settingAdvance(s, r); settingValue(s, r, v, sizeof v, sw, on); assert(sw && !on && strcmp(v, "OFF") == 0);
    settingAdvance(s, r);
  }
  assert(settingsPages() == 3 && settingAt(1, 40) == SET_CLOCK_SKY && settingAt(1, 120) == SET_SUN_GLOW);
  assert(settingAt(2, 120) == SET_SAVER && settingAt(2, 160) == SET_LIGHT && settingAt(2, 200) == -1);
  // sunrise colors off: the same minutes are plain day and night
  parseWeather("OK\nX\n1\nF\nCLEAR\n0\n1\n1\n1\n1\n1\n1\nD|387|1012\nTODAY|0|1|1\n", w);
  sunGlowOn = false;
  assert(at(370) == PHASE_NIGHT && at(400) == PHASE_DAY && at(1000) == PHASE_DAY && at(1030) == PHASE_NIGHT);
  sunGlowOn = true;
  assert(at(370) == PHASE_DAWN && at(1030) == PHASE_DUSK);
  // moving sky off: the clock goes back to its plain screen
  fullColorMode = true; sceneReady = true; clockSkyOn = false; assert(!clockImmersive(c));
  clockSkyOn = true; assert(clockImmersive(c));
  printf("phase and settings checks passed\n");
}
