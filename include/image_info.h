#ifndef __RTS_IMAGE_INFO__
#define __RTS_IMAGE_INFO__

#include "camera_info.h"
#include "telescope_info.h"
#include "dome_info.h"

#include <time.h>

#define TARGET_LIGHT		1
#define TARGET_DARK		2
#define TARGET_FLAT		3
#define TARGET_FLAT_DARK	4

struct image_info
{
  char *camera_name;
  char *telescope_name;
  char *dome_name;
  struct telescope_info telescope;
  struct camera_info camera;
  struct dome_info dome;
  time_t exposure_time;
  float exposure_length;
  int target_type;
  int target_id;
  int observation_id;
};

#endif /* __RTS_IMAGE_INFO__ */
