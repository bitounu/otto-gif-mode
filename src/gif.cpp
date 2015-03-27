#include "stak.h"
#include "gfx.hpp"
#include "display.hpp"
#include "timeline.hpp"

// Debug
#include <iostream>
#include <string>

using namespace otto;
using namespace choreograph;

static const float pi = M_PI;
static const float twoPi = M_PI * 2.0f;
static const float halfPi = M_PI / 2.0f;

static Display display = { { 96.0f, 96.0f } };

static struct {
  uint32_t minFrame = 1;
  uint32_t maxFrame = 33;
  uint32_t nextFrame = minFrame;

  float rewindAmount = 0.0f;

  Output<vec3> flashColor;
} mode;

STAK_EXPORT int init() {
  loadFont(std::string(stak_assets_path()) + "232MKSD-round-medium.ttf");
  return 0;
}

STAK_EXPORT int shutdown() {
  return 0;
}

STAK_EXPORT int update(float dt) {
  display.update([dt] { timeline.step(dt); });
  return 0;
}

STAK_EXPORT int draw() {
  display.draw([] {
    beginPath();
    rect(vec2(), display.bounds.size);
    fillColor(mode.flashColor());
    fill();

    translate(display.bounds.size * 0.5f);

    if (mode.nextFrame <= mode.maxFrame) {
      translate(0, -11);

      beginPath();
      moveTo(-15, -3);
      lineTo(0, 3);
      lineTo(0, -3);
      lineTo(15, 3);
      strokeWidth(2);
      strokeColor(vec4(1, 1, 1, 0.35f));
      stroke();

      fontSize(40);
      textAlign(ALIGN_CENTER | ALIGN_BASELINE);
      fillColor(vec4(1));
      fillText(std::to_string(mode.nextFrame), 0, 11);

      fontSize(20);
      textAlign(ALIGN_CENTER | ALIGN_TOP);
      fillColor(vec4(1, 1, 1, 0.35f));
      fillText(std::to_string(mode.maxFrame), 0, -5);
    }
    else {
      fontSize(50);
      textAlign(ALIGN_CENTER | ALIGN_MIDDLE);
      uint32_t rewindFrame = mode.maxFrame * (1.0f - mode.rewindAmount);
      fillColor(vec3(1));
      fillText(std::to_string(rewindFrame));

      const float width = 3.0f;
      const float inset = 12.0f;
      beginPath();
      arc(vec2(), display.bounds.size - inset, halfPi + mode.rewindAmount * twoPi, halfPi);
      strokeColor(vec4(1, 1, 1, 0.35f));
      strokeWidth(width);
      strokeCap(VG_CAP_BUTT);
      stroke();
    }
  });
  return 0;
}

static void captureFrame() {
  static const vec3 flashColors[] = { colorBGR(0x00ADEF), colorBGR(0xEC008B), colorBGR(0xFFF100) };
  static size_t colorIndex = 0;

  if (mode.nextFrame <= mode.maxFrame) {
    mode.nextFrame++;
    colorIndex = (colorIndex + 1) % 3;
    timeline.apply(&mode.flashColor)
        .then<Hold>(flashColors[colorIndex], 0.0f)
        .then<RampTo>(vec3(), 0.3f, EaseOutQuad());
  }
}

static void saveFrame() {
  mode.rewindAmount = 0.0f;
  mode.nextFrame = mode.minFrame;
}

static void rewind(float amount) {
  mode.rewindAmount = std::min(1.0f, mode.rewindAmount + amount);
  if (mode.rewindAmount == 1.0f) saveFrame();
}

STAK_EXPORT int crank_rotated(int amount) {
  if (amount > 0)
    captureFrame();
  else if (amount < 0 && mode.nextFrame > mode.maxFrame)
    rewind(std::abs(amount) * 0.02f);
  display.wake();
  return 0;
}

STAK_EXPORT int shutter_button_pressed() {
  captureFrame();
  display.wake();
  return 0;
}

STAK_EXPORT int shutter_button_released() {
  display.wake();
  return 0;
}

STAK_EXPORT int power_button_pressed() {
  display.wake();
  return 0;
}

STAK_EXPORT int power_button_released() {
  display.wake();
  return 0;
}

STAK_EXPORT int crank_pressed() {
  display.wake();
  return 0;
}

STAK_EXPORT int crank_released() {
  display.wake();
  return 0;
}
