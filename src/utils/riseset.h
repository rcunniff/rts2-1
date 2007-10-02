#ifndef __RTS_RISESET__
#define __RTS_RISESET__

#include <libnova/ln_types.h>
#include <time.h>

#ifdef __cplusplus
extern "C"
{
#endif

  int next_event (struct ln_lnlat_posn *observer, time_t * start_time,
		  int *curr_type, int *type, time_t * ev_time,
		  double night_horizon, double day_horizon);

#ifdef __cplusplus
};
#endif

#endif /* __RTS_RISESET__ */
