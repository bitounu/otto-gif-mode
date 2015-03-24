#include "camera.hpp"

#include "otto/system.hpp"

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BASE_DIRECTORY "/home/pi"
#define FASTCAMD_DIR BASE_DIRECTORY "/otto-sdk/fastcmd/"
#define GIF_TEMP_DIR BASE_DIRECTORY "/gif_temp/"
#define OUTPUT_DIR BASE_DIRECTORY "/output/"

namespace otto {

volatile int is_processing_gif = 0;
pthread_t pthr_process_gif;

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
  if (pthread_join(pthr_process_gif, NULL)) {
    fprintf(stderr, "Error joining gif process thread\n");
  }
  ottoSystemCallProcess(FASTCAMD_DIR "stop_camd.sh");
  ottoSystemCallProcess("rm -Rf " GIF_TEMP_DIR);
}

void captureFrame() {
  ottoSystemCallProcess(FASTCAMD_DIR "do_capture.sh")
}

// runs gifsicle to process gifs
static void *thread_process_gif(void *arg) {
  static char system_call_string[1024];

  is_processing_gif = 1;

  // return the number to use for the next image file
  int file_number = get_next_file_number();

  // create system call string based on calculated file number
  sprintf(system_call_string,
          "gifsicle --colors 256 " GIF_TEMP_DIR "*.gif > " OUTPUT_DIR
          "gif_%04i.gif ; rm " GIF_TEMP_DIR "* ; chown pi:pi " OUTPUT_DIR "$FNAME",
          file_number);

  // run processing call
  ottoSystemCallProcess(system_call_string);

  is_processing_gif = 0;
}

} // otto
