#include "otto.h"
#include "gfx.hpp"

#include "entityx/entityx.h"
#include "gtx/rotate_vector.hpp"
#include "gtx/string_cast.hpp"

// Debug
#include <iostream>
#include <string>

#define BASE_DIRECTORY "/home/pi"
#define FASTCAMD_DIR BASE_DIRECTORY "/otto-sdk/fastcmd/"
#define GIF_TEMP_DIR BASE_DIRECTORY "/gif_temp/"
#define OUTPUT_DIR BASE_DIRECTORY "/output/"

using namespace otto;

static const int screenWidth = 96;
static const int screenHeight = 96;
static const vec2 screenSize = { screenWidth, screenHeight };

struct GifModeData {
  uint32_t minFrame = 1;
  uint32_t maxFrame = 32;

  float currentFrame = 55.0f;
} mode;

STAK_EXPORT int init() {
  loadFont(std::string(otto_assets_path()) + "232MKSD-round-medium.ttf");
  std::cout << "gif loaded" << std::endl;

  startCamera();
  return 0;
}

STAK_EXPORT int shutdown() {
  stopCamera();
  return 0;
}

STAK_EXPORT int update(float dt) {
  return 0;
}

static void startCamera() {
  ottoSystemCallProcess( "mkdir -p " GIF_TEMP_DIR );
  ottoSystemCallProcess( "mkdir -p " OUTPUT_DIR );
  ottoSystemCallProcess( FASTCAMD_DIR "start_camd.sh");
}
static void stopCamera() {
  if( pthread_join( pthr_process_gif, NULL ) ) {
    fprintf(stderr, "Error joining gif process thread\n");
  }
  ottoSystemCallProcess( FASTCAMD_DIR "stop_camd.sh");
  ottoSystemCallProcess("rm -Rf " GIF_TEMP_DIR);
}
static void captureFrame() {
  ottoSystemCallProcess( FASTCAMD_DIR "do_capture.sh" )
}

// runs gifsicle to process gifs
static void* thread_process_gif(void* arg) {
    static char system_call_string[1024];

    is_processing_gif = 1;
    
    // return the number to use for the next image file
    int file_number = get_next_file_number();

    beep(512, 30);
    nanosleep((struct timespec[]){{0, 200000000L}}, NULL);
    beep(512, 30);

    // create system call string based on calculated file number
    sprintf( system_call_string, "gifsicle --colors 256 " GIF_TEMP_DIR "*.gif > " OUTPUT_DIR "gif_%04i.gif ; rm " GIF_TEMP_DIR "* ; chown pi:pi " OUTPUT_DIR "$FNAME", file_number );

    // run processing call
    system( system_call_string );

    is_processing_gif = 0;

    beep(256, 30);
}

static void drawFrameNumber() {
  fontSize(28);
  textAlign(ALIGN_CENTER | ALIGN_BASELINE);
  fillText(std::to_string(static_cast<int>(mode.currentFrame)));
}

static void drawFrameNumberMax() {
  fontSize(18);
  textAlign(ALIGN_CENTER | ALIGN_TOP);
  fillText(std::to_string(mode.maxFrame));
}

static void drawAperture(const vec3 &color, float apothem, int numBlades) {
  ScopedTransform xf;
  rotate(apothem * 0.015f);

  std::vector<vec2> pnts;
  pnts.emplace_back(0.0f, apothem / std::cos(float(M_PI) / numBlades));
  for (int i = 1; i < numBlades; ++i) {
    pnts.push_back(glm::rotate(pnts.back(), 2.0f * float(M_PI) / numBlades));
  }

  beginPath();
  rect(screenSize * -0.5f, screenSize);
  moveTo(pnts.front());
  for (const auto &p : pnts) lineTo(p);
  fillColor(color);
  fill();

  beginPath();
  const auto *prev = &pnts.back();
  for (const auto &p : pnts) {
    auto v = glm::normalize(p - *prev);
    // std::cout << glm::to_string(v) << " " << glm::to_string(p) << " " << glm::to_string(prev) << std::endl;
    moveTo(*prev);
    lineTo(*prev + v * 96.0f);
    prev = &p;
  }
  strokeColor(glm::mix(color, vec3(0), 0.5f));
  strokeWidth(2);
  stroke();
}

STAK_EXPORT int draw() {
  static const mat3 defaultMatrix = { 0.0, -1.0, 0.0, 1.0, -0.0, 0.0, 0.0, screenHeight, 1.0 };

  clearColor(0, 0, 0);
  clear(0, 0, screenWidth, screenHeight);

  setTransform(defaultMatrix);

  // NOTE(ryan): Apply a circular mask to simulate a round display. We may want to move this to
  // otto-sdk so that the mask is enforced.
  fillMask(0, 0, screenWidth, screenHeight);
  beginPath();
  circle(48.0f, 48.0f, 48.0f);
  beginMask();
  fill();
  endMask();
  enableMask();

  ScopedMask mask = { screenWidth, screenHeight };

  beginPath();
  rect(0, 0, 96, 96);
  fillColor(1, 0, 0);
  fill();

  {
    translate(vec2(screenSize * 0.5f));
    {
      ScopedTransform xf;
      translate(0, -9.0f);

      strokeColor(1, 1, 1, 0.5f);
      fillColor(vec3(1));

      beginPath();
      moveTo(-22, 0);
      lineTo(22, 0);
      strokeWidth(2.0f);
      stroke();

      pushTransform();
      translate(0, 10);
      drawFrameNumber();
      popTransform();

      pushTransform();
      translate(0, -4);
      fillColor(1, 1, 1, 0.9f);
      drawFrameNumberMax();
      popTransform();
    }

    auto radius = 48.0f * (mode.currentFrame - std::floor(mode.currentFrame));
    drawAperture(vec3(0.5f, 0, 0.75f), radius, 6);
  }

  return 0;
}

STAK_EXPORT int crank_rotated(int amount) {
  mode.currentFrame += amount > 0 ? 0.1f : -0.1f;
  mode.currentFrame =
      std::max<float>(mode.minFrame, std::min<float>(mode.maxFrame, mode.currentFrame));


  captureFrame();

  return 0;
}

STAK_EXPORT int shutter_button_pressed() {
  captureFrame();
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
