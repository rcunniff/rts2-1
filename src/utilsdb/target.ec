#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "target.h"

#include <syslog.h>

EXEC SQL include sqlca;

void
Target::logMsg (const char *message)
{
  printf ("%s\n", message);
}

void
Target::logMsg (const char *message, int num)
{
  printf ("%s %i\n", message, num);
}

void
Target::logMsg (const char *message, long num)
{
  printf ("%s %li\n", message, num);
}

void
Target::logMsg (const char *message, double num)
{
  printf ("%s %f\n", message, num);
}

void
Target::logMsg (const char *message, const char *val)
{
  printf ("%s %s\n", message, val);
}

void
Target::logMsgDb (const char *message)
{
  printf ("SQL error: %i %s (at %s)\n", sqlca.sqlcode, sqlca.sqlerrm.sqlerrmc, message);
}

Target::Target (int in_tar_id, struct ln_lnlat_posn *in_obs)
{
  observer = in_obs;
  selected = 0;

  obs_id = -1;
  img_id = 0;
  target_id = in_tar_id;
  bonus = -1;
}

Target::~Target (void)
{
  endObservation ();
}

int
Target::startObservation (struct ln_equ_posn *position)
{
  EXEC SQL BEGIN DECLARE SECTION;
  int d_tar_id = target_id;
  int d_obs_id;
  double d_obs_ra;
  double d_obs_dec;
  float d_obs_alt;
  float d_obs_az;
  EXEC SQL END DECLARE SECTION;

  struct ln_hrz_posn hrz;

  getPosition (position);
  selected++;

  if (obs_id > 0) // we already observe that target
    return 1;

  d_obs_ra = position->ra;
  d_obs_dec = position->dec;
  ln_get_hrz_from_equ (position, observer, ln_get_julian_from_sys (), &hrz);
  d_obs_alt = hrz.alt;
  d_obs_az = hrz.az;

  EXEC SQL
  SELECT
    nextval ('obs_id')
  INTO
    :d_obs_id;
  EXEC SQL
  INSERT INTO
    observations
  (
    tar_id,
    obs_id,
    obs_start,
    obs_ra,
    obs_dec,
    obs_alt,
    obs_az
  )
  VALUES
  (
    :d_tar_id,
    :d_obs_id,
    now (),
    :d_obs_ra,
    :d_obs_dec,
    :d_obs_alt,
    :d_obs_az
  );
  EXEC SQL COMMIT;
  if (sqlca.sqlcode != 0)
  {
    logMsgDb ("cannot insert observation start to db");
    return -1;
  }
  obs_id = d_obs_id;
  return 0;
}

int
Target::endObservation ()
{
  EXEC SQL BEGIN DECLARE SECTION;
  int d_obs_id = obs_id;
  EXEC SQL END DECLARE SECTION;
  if (obs_id > 0)
  {
    EXEC SQL
    UPDATE
      observations
    SET
      obs_end = now ()
    WHERE
      obs_id = :d_obs_id;
    obs_id = -1;
    if (sqlca.sqlcode != 0)
    {
      EXEC SQL ROLLBACK;
      logMsgDb ("cannot end observation");
      return -1;
    }
    EXEC SQL COMMIT;
  }
  return 0;
}

int
Target::beforeMove ()
{
  return 0;
}

/**
 * Move to designated target, get astrometry, proof that target was acquired with given margin.
 *
 * @return 0 if I can observe futher, -1 if observation was canceled
 */
int
Target::acquire ()
{

}

int
Target::observe ()
{

}

/**
 * Return script for camera exposure.
 *
 * @param target        target id
 * @param camera_name   name of the camera
 * @param script        script
 * 
 * return -1 on error, 0 on success
 */
int
Target::getDBScript (int target, const char *camera_name, char *script)
{
  EXEC SQL BEGIN DECLARE SECTION;
  int tar_id = target;
  const char *cam_name = camera_name;
  VARCHAR sc_script[2000];
  int sc_indicator;
  EXEC SQL END DECLARE SECTION;
  EXEC SQL 
  SELECT
    script
  INTO
    :sc_script :sc_indicator
  FROM
    scripts
  WHERE
    tar_id = :tar_id
    AND camera_name = :cam_name;
  if (sqlca.sqlcode)
    goto err;
  if (sc_indicator < 0)
    goto err;
  strncpy (script, sc_script.arr, sc_script.len);
  script[sc_script.len] = '\0';
  return 0;
#undef test_sql
err:
  printf ("err db_get_script: %li %s\n", sqlca.sqlcode,
	  sqlca.sqlerrm.sqlerrmc);
  script[0] = '\0';
  return -1;
}


/**
 * Return script for camera exposure.
 *
 * @param device_name	camera device for script
 * @param buf		buffer for script
 *
 * @return 0 on success, < 0 on error
 */
int
Target::getScript (const char *device_name, char *buf)
{
  char obs_type_str[2];
  char *s;
  int ret;
  obs_type_str[0] = obs_type;
  obs_type_str[1] = 0;

  ret = getDBScript (target_id, device_name, buf);
  if (!ret)
    return 0;

  s = "E 10";
  strncpy (buf, s, MAX_COMMAND_LENGTH);
  return 0;
}

int
Target::getAltAz (struct ln_hrz_posn *hrz, double JD)
{
  int ret;
  struct ln_equ_posn object;

  ret = getPosition (&object, JD);
  if (ret)
    return ret;
  ln_get_hrz_from_equ (&object, observer, JD, hrz);
  return 0;
}

int
Target::postprocess ()
{

}

/****
 * 
 *   Return -1 if target is not suitable for observing,
 *   othrwise return 0.
 */
int
Target::considerForObserving (ObjectCheck *checker, double JD)
{
  // horizont constrain..
  struct ln_equ_posn curr_position;
  double gst = ln_get_mean_sidereal_time (JD);
  if (getPosition (&curr_position, JD))
  {
    changePriority (-100, JD + 1);
    return -1;
  }
  if (!checker->is_good (gst, curr_position.ra, curr_position.dec))
  {
    struct ln_rst_time rst;
    int ret;
    ret = getRST (&rst, JD);
    if (ret == -1)
    {
      // object doesn't rise, let's hope tomorrow it will rise
      changePriority (-100, JD + 1);
      return -1;
    }
    // object is above horizont, but checker reject it..let's see what
    // will hapens in 10 minutes 
    if (rst.rise < JD)
    {
      changePriority (-100, JD + 1.0/8640.0);
      return -1;
    }
    changePriority (-100, rst.rise);
    return -1;
  }
  // target was selected for observation
  return 0;
}

int
Target::dropBonus ()
{
  EXEC SQL BEGIN DECLARE SECTION;
  int db_tar_id;
  EXEC SQL END DECLARE SECTION;

  EXEC SQL UPDATE 
    targets 
  SET tar_bonus = NULL, tar_bonus_time = NULL
  WHERE tar_id = :db_tar_id;

  return !!sqlca.sqlcode;
}

int
Target::changePriority (int pri_change, double validJD)
{
  EXEC SQL BEGIN DECLARE SECTION;
  int db_tar_id = target_id;
  int db_priority_change = pri_change;
  long int db_next_t;
  EXEC SQL END DECLARE SECTION;
  ln_get_timet_from_julian (validJD, &db_next_t);
  EXEC SQL UPDATE 
    targets
  SET
    tar_bonus = tar_bonus + :db_priority_change,
    tar_bonus_time = abstime(:db_next_t)
  WHERE
    tar_id = db_tar_id;
  if (!sqlca.sqlcode)
    return -1;
  return 0;
}

Target *createTarget (int in_tar_id, struct ln_lnlat_posn *in_obs)
{
  EXEC SQL BEGIN DECLARE SECTION;
  int db_tar_id = in_tar_id;
  char db_type_id;
  EXEC SQL END DECLARE SECTION;

  try
  {
    EXEC SQL
    SELECT
      type_id
    INTO
      :db_type_id
    FROM
      targets
    WHERE
      tar_id = :db_tar_id;
  
    if (sqlca.sqlcode)
      throw &sqlca;

    // get more informations about target..
    switch (db_type_id)
    {
      // calibration targets..
      case TYPE_DARK:
        return new DarkTarget (in_tar_id, in_obs);
      case TYPE_FLAT:
        return new FlatTarget (in_tar_id, in_obs);
      case TYPE_FOCUSING:
        return new FocusingTarget (in_tar_id, in_obs);
      case TYPE_ELLIPTICAL:
	return new EllTarget (in_tar_id, in_obs);
      case TYPE_GRB:
        return new TargetGRB (in_tar_id, in_obs);
      case TYPE_SWIFT_FOV:
        return new TargetSwiftFOV (in_tar_id, in_obs);
      default:
        return new ConstTarget (in_tar_id, in_obs);
    }
  }
  catch (struct sqlca_t *sqlerr)
  {
    syslog (LOG_ERR, "Cannot create target: %i sqlcode: %i %s",
    db_tar_id, sqlerr->sqlcode, sqlerr->sqlerrm.sqlerrmc);
  }
  return NULL;
}
