#include "camera.hpp"

#include "otto/system.hpp"

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <future>
#include <mutex>
#include <iostream>

#define BASE_DIRECTORY "/stak/user"
#define FASTCAMD_DIR "/stak/sdk/otto-sdk/fastcmd/"
#define GIF_TEMP_DIR BASE_DIRECTORY "/gif_temp/"
#define OUTPUT_DIR BASE_DIRECTORY "/output/"

namespace otto {
  int num_frames = 0;

  std::shared_future<void> gifProcessorFuture;
  std::mutex running;

  static int get_next_file_number() {
    DIR *dirp;
    struct dirent *dp;
    int highest_number = 0;

    dirp = opendir(OUTPUT_DIR);
    while ((dp = readdir(dirp)) != NULL) {
      char num_buffer[5];
      char *pos = strstr(dp->d_name, ".gif");
      int offset = (int)(pos - dp->d_name);
      int len = strlen(dp->d_name);
      if ((pos) && (pos > dp->d_name + 4) && (offset == (len - 4))) {
        strncpy(num_buffer, pos - 4, 4);
        int number = atoi(num_buffer) + 1;
        if (number > highest_number) highest_number = number;
      }
    }
    closedir(dirp);
    return highest_number;
  }

  void startCamera() {
    std::cout << system("mkdir -p " GIF_TEMP_DIR) << std::endl;
    std::cout << system("mkdir -p " OUTPUT_DIR) << std::endl;
    std::cout << system(FASTCAMD_DIR "start_camd.sh") << std::endl;
  }

  void stopCamera() {
    return;
    if( isProcessingGif() ) gifProcessorFuture.wait();
    ottoSystemCallProcess(FASTCAMD_DIR "stop_camd.sh");
    ottoSystemCallProcess("rm -Rf " GIF_TEMP_DIR);
  }

  void captureFrame() {
    return;

    if( isProcessingGif() ) return;

    ottoSystemCallProcess(FASTCAMD_DIR "do_capture.sh");
    num_frames++;
    if( num_frames >= 30 ) createFinalGif();
  }

  void createFinalGif() {
    // this shouldn't ever happen anyways.
    if( isProcessingGif() ) return;
    gifProcessorFuture = std::async(std::launch::async, []() {
      running.lock();
      static char system_call_string[1024];

      // return the number to use for the next image file
      int file_number = get_next_file_number();

      // create system call string based on calculated file number
      sprintf(system_call_string,
        "gifsicle --colors 256 " GIF_TEMP_DIR "*.gif > " OUTPUT_DIR
        "gif_%04i.gif ; chown pi:pi " OUTPUT_DIR "$FNAME",
        file_number);

      // run processing call
      ottoSystemCallProcess(system_call_string);
      running.unlock();
    });
  }
  int isProcessingGif() {
    if( running.try_lock() ) {
      running.unlock();
      return false;
    }
    return true;
  }

} // otto
