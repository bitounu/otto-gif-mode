#include "stak.h"
#include <otto-gfx/gfx.hpp>
#include "display.hpp"
#include "timeline.hpp"
#include "math.hpp"
#include "draw.hpp"

#include <iostream>
#include <string>
#include <chrono>
#include <dirent.h>
#include <signal.h>
#include <cstring>
#include <regex>
#include <thread>
#include <sys/stat.h>

using namespace otto;
using namespace choreograph;

static Display display = { { 96.0f, 96.0f } };

std::vector< std::string > getFilesInDirectory( std::string path, std::string match = "" ) {
  std::vector< std::string > files_in_directory;
  auto regex_filter = std::regex ( match );
  bool perform_regex = ( match.size() != 0 );

  // open directory
  DIR* directory = opendir( path.c_str( ) );

  if( !directory )
    throw std::invalid_argument( "invalid path" );

  // start reading nodes
  struct dirent* directory_entry = readdir( directory );

  // loop while a node is returned
  while( directory_entry ) {

    // copy file entry name
    std::string file_entry = std::string( directory_entry->d_name );

    // read next node
    directory_entry = readdir( directory );

    // exclude '.' and '..' from listing
    if( ( file_entry == "." ) || ( file_entry == ".." ) )
      continue;

    // push filename to vector
    if( !perform_regex || std::regex_search( file_entry, regex_filter ) ) {
      files_in_directory.push_back( file_entry );
    }
  }

  // close directory and return vector of file names
  closedir( directory );
  return files_in_directory;
}

int getNextFileNumber() {
  auto files = getFilesInDirectory("/mnt/pictures", "(\\.gif)|(\\.jpg)$");
  int maxNum = 0;
  for (const auto &file : files) {
    if (file.length() >= 8) {
      maxNum = std::max(maxNum, std::atoi(file.substr(file.size() - 8, 4).c_str()));
    }
  }
  return maxNum + 1;
}

static struct {
  uint32_t minFrame = 1;
  uint32_t maxFrame = 33;
  uint32_t nextFrame = minFrame;

  bool isRewinding = false;
  float rewindAmountMin = 0.05f;
  float rewindAmount = 0.0f;

  Output<float> frameMeterValue = nextFrame;
  const float frameMeterRadius = 100.0f;
  const float frameMeterAngleIncr = otto::PI / 6.0f;

  Output<float> rewindMeterAmount = 0.0f;
  Output<float> rewindMeterOpacity = 0.0f;

  Output<vec3> flashColor;

  Output<float> captureScreenScale = 1.0f;
  Output<float> saveScreenScale = 0.0f;

  std::chrono::steady_clock::time_point lastFrameChangeTime;

  Svg *iconRewind;

  std::thread saving_thread;

  void init() {
    iconRewind = loadSvg(std::string(stak_assets_path()) + "icon-rewind.svg", "px", 96);
  }

  void shutdown() {
    if (saving_thread.joinable()) saving_thread.join();
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
    saving_thread = std::thread([] {
      static char system_call_string[1024];
      int file_number = getNextFileNumber();
      sprintf( system_call_string, "gifsicle --loopcount --colors 256 /mnt/tmp/*.gif -o /mnt/pictures/snap_%04i.gif && rm /mnt/tmp/*.gif", file_number );

      // run processing call
      system( system_call_string );
      system("/bin/systemctl restart otto-fastcamd");
    });

    timeline.cue([this] { completeSave(); }, 1.0f);
  }

  void completeSave() {
    if (saving_thread.joinable()) saving_thread.join();

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

    if (!isFull()) {
      int pid = 0;
#if 0
      FILE *f = fopen("/var/run/otto-fastcamd.pid", "r");
      if( f == nullptr ) {
        system("/bin/systemctl restart otto-fastcamd");
        f = fopen("/var/run/otto-fastcamd.pid", "r");
        if( f == nullptr ) {
          return;
        }
      }
      // TODO(Wynter): need to signal that camera isn't running anymore!
      fscanf(f, "%d", &pid);
      fclose(f);
      kill( pid, SIGUSR1 );
#endif
      if(system("/bin/systemctl kill otto-fastcamd -s SIGUSR1") != 0) return;
    }else{
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
        fillColor( vec3(1) );
        fillText( std::to_string(frame + i) );
      }
    }
  }

  void drawRewindMeter() {
    fillColor(vec4(colorBGR(0xEC008B), rewindMeterOpacity()));
    drawProgressArc(display, rewindMeterAmount());
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

  mkdir("/mnt/tmp", S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
  mkdir("/mnt/pictures", S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);

  system("rm -f /mnt/tmp/*.gif");

  mode.init();

  return 0;
}

STAK_EXPORT int activate() {
  system("systemctl start otto-fastcamd");
}

STAK_EXPORT int deactivate() {
  system("systemctl stop otto-fastcamd");
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
