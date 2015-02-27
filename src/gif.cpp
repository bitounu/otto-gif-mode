#include "stak.h"
#include "gfx.hpp"

#include "entityx/entityx.h"

// Debug
#include <iostream>

#define STAK_EXPORT extern "C"

using namespace glm;
using namespace otto;

static const int screenWidth = 96;
static const int screenHeight = 96;

STAK_EXPORT int init() {
  loadFont("assets/232MKSD-RoundMedium.ttf");

  return 0;
}

STAK_EXPORT int shutdown() {
  return 0;
}

STAK_EXPORT int update(float dt) {
  return 0;
}

STAK_EXPORT int draw() {
  static const mat3 defaultMatrix = { 0.0, -1.0, 0.0, 1.0, -0.0, 0.0, 0.0, screenHeight, 1.0 };

  clearColor(0, 0, 0);
  clear(0, 0, 96, 96);

  setTransform(defaultMatrix);

  // NOTE(ryan): Apply a circular mask to simulate a round display. We may want to move this to
  // stak-sdk so that the mask is enforced.
  fillMask(0, 0, screenWidth, screenHeight);
  beginPath();
  circle(48.0f, 48.0f, 48.0f);
  beginMask();
  fill();
  endMask();
  enableMask();

  ScopedMask mask = { screenWidth, screenHeight };
  // Draw mode specific stuff

  return 0;
}

STAK_EXPORT int crank_rotated(int amount) {
  return 0;
}

STAK_EXPORT int shutter_button_pressed() {
  return 0;
}

STAK_EXPORT int shutter_button_released() {
  return 0;
}

STAK_EXPORT int power_button_pressed() {
  return 0;
}

STAK_EXPORT int power_button_released() {
  return 0;
}

STAK_EXPORT int crank_pressed() {
  return 0;
}

STAK_EXPORT int crank_released() {
  return 0;
}
