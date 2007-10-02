/*!
 * @file Calculate rise and set times.
 *
 * Used for calculating of next events (dusk, night, dawn, day).
 *
 * @author petr
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <libnova/julian_day.h>
#include <libnova/solar.h>
#include <math.h>
#include <stdio.h>

#include "riseset.h"
#include "status.h"
#include "rts2config.h"

int
next_naut (double jd, struct ln_lnlat_posn *observer, struct ln_rst_time *rst,
	   struct ln_rst_time *rst_naut, int *sun_rs, double night_horizon,
	   double day_horizon)
{
  double t_jd = jd - 1;
  int sun_naut;
  Rts2Config *config;
  config = Rts2Config::instance ();

  rst_naut->rise = rst_naut->transit = rst_naut->set = 0;
  rst->rise = rst->transit = rst->set = 0;
  *sun_rs = 0;
  // find first next day, on which nautic sunset is occuring
  do
    {
      struct ln_rst_time t_rst;
      sun_naut =
	ln_get_solar_rst_horizon (t_jd, observer, night_horizon, &t_rst);
      if (!rst_naut->rise && jd < t_rst.rise)
	rst_naut->rise = t_rst.rise;
      if (!rst_naut->transit && jd < t_rst.transit)
	rst_naut->transit = t_rst.transit;
      if (!rst_naut->set && jd < t_rst.set)
	rst_naut->set = t_rst.set;
      if (!ln_get_solar_rst_horizon (t_jd, observer, day_horizon, &t_rst))
	{
	  *sun_rs = 1;
	  if (!rst->set && jd < t_rst.set)
	    rst->set = t_rst.set;
	  if (!rst->transit && jd < t_rst.transit)
	    rst->transit = t_rst.transit;
	  if (!rst->rise && jd < t_rst.rise)
	    rst->rise = t_rst.rise;
	}
      t_jd++;
    }
  while (sun_naut != 0 || !rst_naut->rise || !rst_naut->transit
	 || !rst_naut->set || ((!rst->rise || !rst->transit || !rst->set)
			       && *sun_rs == 1));

  return 0;
}

int
next_event (struct ln_lnlat_posn *observer, time_t * start_time,
	    int *curr_type, int *type, time_t * ev_time, double night_horizon,
	    double day_horizon)
{
  double jd_time = ln_get_julian_from_timet (start_time);
  struct ln_rst_time rst, rst_naut;

  int sun_rs;
  double eve_time = 7200;	// seconds
  double mor_time = 1800;	// seconds

  Rts2Config *config;
  config = Rts2Config::instance ();

  next_naut (jd_time, observer, &rst, &rst_naut, &sun_rs, night_horizon,
	     day_horizon);

  // jd_time < rst_naut.rise && jd_time < rst_naut.transit && jd_time < rst_naut.set
  if (rst_naut.rise <= rst_naut.set)
    {
      *curr_type = SERVERD_NIGHT;
      *type = SERVERD_DAWN;
      ln_get_timet_from_julian (rst_naut.rise, ev_time);
    }
  else
    {
      if (sun_rs)
	{
	  if (rst.set < rst.rise)
	    {
	      config->getDouble ("observatory", "evening_time", eve_time);
	      eve_time /= 86400.0;	// get from config, convert to days 
	      config->getDouble ("observatory", "morning_time", mor_time);
	      mor_time /= 86400.0;
	      if (jd_time > rst.set - eve_time)
		{
		  *curr_type = SERVERD_EVENING;
		  *type = SERVERD_DUSK;
		  ln_get_timet_from_julian (rst.set, ev_time);
		}
	      else if (jd_time < rst.rise + mor_time - 1.0)
		{
		  *curr_type = SERVERD_MORNING;
		  *type = SERVERD_DAY;
		  ln_get_timet_from_julian (rst.rise + mor_time - 1.0,
					    ev_time);
		}
	      else
		{
		  *curr_type = SERVERD_DAY;
		  *type = SERVERD_EVENING;
		  ln_get_timet_from_julian (rst.set - eve_time, ev_time);
		}
	    }
	  else
	    {
	      if (rst_naut.rise < rst.rise)
		{
		  *curr_type = SERVERD_DUSK;
		  *type = SERVERD_NIGHT;
		  ln_get_timet_from_julian (rst_naut.set, ev_time);
		}
	      else
		{
		  *curr_type = SERVERD_DAWN;
		  *type = SERVERD_MORNING;
		  ln_get_timet_from_julian (rst.rise, ev_time);
		}
	    }

	}
      else
	{
	  // rst_naut.rise > rst_naut.set
	  if (jd_time < rst_naut.transit)
	    {
	      *curr_type = SERVERD_DAWN;
	      *type = SERVERD_DUSK;
	      ln_get_timet_from_julian (rst_naut.transit, ev_time);
	    }
	  else
	    {
	      *curr_type = SERVERD_DUSK;
	      *type = SERVERD_NIGHT;
	      ln_get_timet_from_julian (rst_naut.set, ev_time);
	    }
	}
    }
  return 0;
}
