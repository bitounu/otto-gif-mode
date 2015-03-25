#include "camera.hpp"

#include "otto/system.hpp"

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <future>

#define BASE_DIRECTORY "/stak/user"
#define FASTCAMD_DIR "/stak/sdk/otto-sdk/fastcmd/"
#define GIF_TEMP_DIR BASE_DIRECTORY "/gif_temp/"
#define OUTPUT_DIR BASE_DIRECTORY "/output/"

namespace otto {
int num_frames = 0;

std::future<void> gifProcessorFuture;

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
  ottoSystemCallProcess("mkdir -p " GIF_TEMP_DIR);
  ottoSystemCallProcess("mkdir -p " OUTPUT_DIR);
  ottoSystemCallProcess(FASTCAMD_DIR "start_camd.sh");
}

void stopCamera() {
  
  if( isProcessingGif() ) gifProcessorFuture.wait();
  ottoSystemCallProcess(FASTCAMD_DIR "stop_camd.sh");
  ottoSystemCallProcess("rm -Rf " GIF_TEMP_DIR);
}

void captureFrame() {
  
  if( isProcessingGif() ) return;

  ottoSystemCallProcess(FASTCAMD_DIR "do_capture.sh");
  num_frames++;
  if( num_frames >= 30 ) createFinalGif();
}

void createFinalGif() {
  // this shouldn't ever happen anyways.
  if( isProcessingGif() ) return;
  gifProcessorFuture = std::async(std::launch::async, [](){
    static char system_call_string[1024];

    // return the number to use for the next image file
    int file_number = get_next_file_number();

    // create system call string based on calculated file number
    sprintf(system_call_string,
      "gifsicle --colors 256 " GIF_TEMP_DIR "*.gif > " OUTPUT_DIR
      "gif_%04i.gif ; rm " GIF_TEMP_DIR "* ; chown pi:pi " OUTPUT_DIR "$FNAME",
      file_number);

    // run processing call
    ottoSystemCallProcess(system_call_string);
  });
}
int isProcessingGif() {
  return ( gifProcessorFuture.wait_for(std::chrono::seconds(0)) == std::future_status::ready );
}

} // otto
