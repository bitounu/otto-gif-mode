#include "stak.h"
#include "gfx.hpp"
#include "display.hpp"
#include "timeline.hpp"
#include "util.hpp"

#include "gtx/rotate_vector.hpp"

#include <iostream>
#include <string>
#include <chrono>

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

  bool isRewinding = false;
  float rewindAmountMin = 0.05f;
  float rewindAmount = 0.0f;

  Output<float> frameMeterValue = nextFrame;
  const float frameMeterRadius = 100.0f;
  const float frameMeterAngleIncr = pi / 6.0f;

  Output<float> rewindMeterAmount = 0.0f;
  Output<float> rewindMeterOpacity = 0.0f;

  Output<vec3> flashColor;

  Output<float> captureScreenScale = 1.0f;
  Output<float> saveScreenScale = 0.0f;

  std::chrono::steady_clock::time_point lastFrameChangeTime;

  Svg *iconRewind;

  void init() {
    iconRewind = loadSvg(std::string(stak_assets_path()) + "icon-rewind.svg", "px", 96);
  }

  bool isFull() { return nextFrame > maxFrame; }
  bool isEmpty() { return nextFrame == minFrame; }

  void setMeterFrame(uint32_t frame, bool immediate = false) {
    // NOTE(ryan): Bail if the value is the same to avoid resetting the last frame timer.
    if (frameMeterValue() == frame) return;

    auto now = std::chrono::steady_clock::now();
    auto timeSinceLastFrame =
        std::chrono::duration_cast<std::chrono::milliseconds>(now - lastFrameChangeTime).count();
    lastFrameChangeTime = now;

    // Tween to the next frame
    float duration = (immediate || timeSinceLastFrame < 300.0f) ? 0.0f : 0.33f;
    timeline.apply(&frameMeterValue).then<RampTo>(frame, duration, EaseOutQuad());
  }

  void save() {
    timeline.apply(&captureScreenScale).then<RampTo>(0.0f, 0.15f, EaseInQuad());
    timeline.apply(&saveScreenScale)
        .then<Hold>(0.0f, 0.15f)
        .then<RampTo>(1.0f, 0.15f, EaseOutQuad());

    // TODO(ryan): Replace this with actual saving / GIF creation.
    timeline.cue([this] { completeSave(); }, 2.0f);
  }

  void completeSave() {
    rewindAmount = 0.0f;
    nextFrame = minFrame;

    setMeterFrame(minFrame, true);

    timeline.apply(&saveScreenScale).then<RampTo>(0.0f, 0.15f, EaseInQuad());
    timeline.apply(&captureScreenScale)
        .then<Hold>(0.0f, 0.15f)
        .then<RampTo>(1.0f, 0.15f, EaseOutQuad());
  }

  void captureFrame() {
    static const vec3 flashColors[] = { colorBGR(0x00ADEF), colorBGR(0xEC008B),
                                        colorBGR(0xFFF100) };
    static size_t colorIndex = 0;

    if (isFull()) {
      // NOTE(ryan): Bounce the meter to indicate the reel is full
      timeline.apply(&frameMeterValue)
          .then<RampTo>(maxFrame + 1.15f, 0.1f, EaseOutQuad())
          .then<RampTo>(maxFrame + 1, 0.2f, EaseInQuad());
      return;
    }

    setMeterFrame(++nextFrame);

    // Flash
    colorIndex = (colorIndex + 1) % 3;
    timeline.apply(&flashColor)
        .then<Hold>(flashColors[colorIndex], 0.0f)
        .then<RampTo>(vec3(), 0.3f, EaseOutQuad());
  }

  void rewind(float amount) {
    rewindAmount = clamp(rewindAmount + amount, 0.0f, 1.0f);
    timeline.apply(&rewindMeterAmount).then<RampTo>(rewindAmount, 0.05f);

    if (rewindAmount < rewindAmountMin) {
      stopRewinding();
      setMeterFrame(nextFrame);
    } else if (rewindAmount > 0.99f) {
      save();
      stopRewinding();
    } else {
      setMeterFrame(nextFrame - 1);
    }
  }

  void startRewinding() {
    isRewinding = true;
    rewindAmount = 0.0f;
    timeline.apply(&rewindMeterAmount).then<Hold>(0.0f, 0.0f);
    timeline.apply(&rewindMeterOpacity).then<RampTo>(1.0f, 0.25f, EaseOutQuad());
    rewind(0.05f);
  }

  void stopRewinding() {
    isRewinding = false;
    timeline.apply(&rewindMeterOpacity).then<RampTo>(0.0f, 0.25f, EaseInQuad());
  }

  void wind(float amount) {
    if (amount > 0.0f) {
      captureFrame();
    } else if (amount < 0.0f && !isEmpty()) {
      startRewinding();
    }
  }

  void drawFrameMeter() {
    ScopedTransform xf;
    translate(0, -frameMeterRadius);

    fontSize(40);
    textAlign(ALIGN_CENTER | ALIGN_BASELINE);

    // NOTE(ryan): Draw one frame before and after the current frame
    const int frame = std::round(frameMeterValue());
    const float frameAngle = (frame - frameMeterValue()) * frameMeterAngleIncr;
    float angle;
    for (int i = -1; i <= 1; ++i) {
      if (frame + i > maxFrame + 1) continue;

      ScopedTransform xf;
      angle = frameAngle + i * frameMeterAngleIncr;
      rotate(angle);
      translate(0, frameMeterRadius);

      float brightness = 1.0f - std::abs(angle) / frameMeterAngleIncr;
      ScopedColorTransform ct(vec4(vec3(1.0f), brightness), vec4(0.0f));

      if (frame + i == maxFrame + 1) {
        translate(0, 17);
        rotate(std::min(0.0f, frameAngle) * -5.0f);
        drawSvg(iconRewind);
      }
      else {
        fillColor(vec3(1));
        fillText(std::to_string(frame + i));
      }
    }
  }

  void drawRewindMeter() {
    float r = display.bounds.size.x * 0.5f - 8.0f;
    float a = halfPi + rewindMeterAmount() * twoPi;

    auto color = vec4(colorBGR(0xEC008B), rewindMeterOpacity());

    ScopedMask mask(display.bounds.size);
    ScopedFillRule fr(VG_NON_ZERO);
    beginMask();

    beginPath();
    arc(vec2(), vec2(r * 2.0f), a, halfPi);
    strokeWidth(3.0f);
    strokeCap(VG_CAP_BUTT);
    strokeColor(color);
    stroke();

    beginPath();
    circle(0, r, 3);
    circle(glm::rotate(vec2(r, 0), a), 3);
    fillColor(color);
    fill();

    endMask();

    beginPath();
    rect(display.bounds.size * -0.5f, display.bounds.size);
    fillColor(color);
    fill();
  }

  void drawCaptureScreen() {
    beginPath();
    rect(display.bounds.size * -0.5f, display.bounds.size);
    fillColor(flashColor());
    fill();

    drawFrameMeter();

    // Rewinding
    if (rewindMeterOpacity > 0.0f) drawRewindMeter();

    translate(0, -11);

    beginPath();
    moveTo(-15, -3);
    lineTo(0, 3);
    lineTo(0, -3);
    lineTo(15, 3);
    strokeWidth(2);
    strokeCap(VG_CAP_ROUND);
    strokeColor(vec4(1, 1, 1, 0.35f));
    stroke();

    fillColor(vec4(1, 1, 1, 0.35f));
    fontSize(20);
    textAlign(ALIGN_CENTER | ALIGN_TOP);
    fillText(std::to_string(maxFrame), 0, -5);
  }

  void drawSaveScreen() {
    fontSize(20);
    textAlign(ALIGN_CENTER | ALIGN_MIDDLE);
    fillColor(vec3(1));
    fillText("saving");
  }

  void draw() {
    if (captureScreenScale() > 0.0f) {
      scale(captureScreenScale());
      drawCaptureScreen();
    }
    else if (saveScreenScale() > 0.0f) {
      scale(saveScreenScale());
      drawSaveScreen();
    }
  }
} mode;

STAK_EXPORT int init() {
  loadFont(std::string(stak_assets_path()) + "232MKSD-round-medium.ttf");
  mode.init();
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
    translate(display.bounds.size * 0.5f);
    mode.draw();
  });
  return 0;
}

STAK_EXPORT int crank_rotated(int amount) {
  if (!display.wake() && mode.captureScreenScale() == 1.0f) {
    if (mode.isRewinding)
      mode.rewind(amount * -0.05f);
    else
      mode.wind(amount);
  }
  return 0;
}

STAK_EXPORT int shutter_button_pressed() {
  if (!display.wake() && !mode.isRewinding && mode.captureScreenScale() == 1.0f)
    mode.captureFrame();
  return 0;
}

STAK_EXPORT int shutter_button_released() {
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
