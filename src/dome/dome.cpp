#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "status.h"

#include "dome.h"

Rts2DevDome::Rts2DevDome (int argc, char **argv):
Rts2Device (argc, argv, DEVICE_TYPE_DOME, 5552, "DOME")
{
  char *states_names[1] = { "dome" };
  setStateNames (1, states_names);

  sw_state = -1;

  observingPossible = 0;
}

int
Rts2DevDome::init ()
{
  return Rts2Device::init ();
}

Rts2Conn *
Rts2DevDome::createConnection (int in_sock, int conn_num)
{
  return new Rts2DevConnDome (in_sock, this);
}

int
Rts2DevDome::checkOpening ()
{
  if ((getState (0) & DOME_DOME_MASK) == DOME_OPENING)
    {
      long ret;
      ret = isOpened ();
      syslog (LOG_DEBUG, "isOPenede ret:%li", ret);
      if (ret >= 0)
	{
	  setTimeout (ret);
	  return 0;
	}
      if (ret == -1)
	{
	  endOpen ();
	  maskState (0, DOME_DOME_MASK, DOME_OPENED,
		     "opening finished with error");
	}
      if (ret == -2)
	{
	  if (endOpen ())
	    {
	      maskState (0, DOME_DOME_MASK, DOME_OPENED,
			 "dome opened with error");
	    }
	  else
	    {
	      maskState (0, DOME_DOME_MASK, DOME_OPENED, "dome opened");
	    }
	}
    }
  else if ((getState (0) & DOME_DOME_MASK) == DOME_CLOSING)
    {
      long ret;
      ret = isClosed ();
      if (ret >= 0)
	{
	  setTimeout (ret);
	  return 0;
	}
      if (ret == -1)
	{
	  endClose ();
	  maskState (0, DOME_DOME_MASK, DOME_CLOSED,
		     "closing finished with error");
	}
      if (ret == -2)
	{
	  if (endClose ())
	    {
	      maskState (0, DOME_DOME_MASK, DOME_CLOSED,
			 "dome closed with error");
	    }
	  else
	    {
	      maskState (0, DOME_DOME_MASK, DOME_CLOSED, "dome closed");
	    }
	}
    }
  setTimeout (10 * USEC_SEC);
  return 0;
}

int
Rts2DevDome::idle ()
{
  checkOpening ();
  return Rts2Device::idle ();
}

int
Rts2DevDome::ready (Rts2Conn * conn)
{
  int ret;
  ret = ready ();
  if (ret)
    {
      conn->sendCommandEnd (DEVDEM_E_HW, "dome not ready");
      return -1;
    }
  return 0;
}

int
Rts2DevDome::info (Rts2Conn * conn)
{
  int ret;
  ret = info ();
  if (ret)
    {
      conn->sendCommandEnd (DEVDEM_E_HW, "dome not ready");
      return -1;
    }
  conn->sendValue ("dome", sw_state);
  return 0;
}

int
Rts2DevDome::baseInfo (Rts2Conn * conn)
{
  int ret;
  ret = baseInfo ();
  if (ret)
    {
      conn->sendCommandEnd (DEVDEM_E_HW, "dome not ready");
      return -1;
    }
  conn->sendValue ("model", domeModel);
  return 0;
}

int
Rts2DevDome::observing ()
{
  observingPossible = 1;
  if ((getState (0) & DOME_DOME_MASK) != DOME_OPENED)
    return openDome ();
  return 0;
}

int
Rts2DevDome::standby ()
{
  if ((getState (0) & DOME_DOME_MASK) != DOME_CLOSED)
    return closeDome ();
  return 0;
}

int
Rts2DevDome::off ()
{
  if ((getState (0) & DOME_DOME_MASK) != DOME_CLOSED)
    return closeDome ();
  return 0;
}

int
Rts2DevDome::setMasterStandby ()
{
  if ((getMasterState () & SERVERD_STANDBY_MASK) != SERVERD_STANDBY)
    {
      sendMaster ("standby");
    }
}

int
Rts2DevDome::changeMasterState (int new_state)
{
  observingPossible = 0;
  if ((new_state & SERVERD_STANDBY_MASK) == SERVERD_STANDBY)
    {
      switch (new_state & SERVERD_STATUS_MASK)
	{
	case SERVERD_DUSK:
	case SERVERD_NIGHT:
	case SERVERD_DAWN:
	  return standby ();
	default:
	  return off ();
	}
    }
  switch (new_state)
    {
    case SERVERD_NIGHT:
      return observing ();
    case SERVERD_DUSK:
    case SERVERD_DAWN:
      return standby ();
    default:
      return off ();
    }
}

int
Rts2DevConnDome::commandAuthorized ()
{
  if (isCommand ("open"))
    {
      CHECK_PRIORITY;
      return master->openDome ();
    }
  else if (isCommand ("close"))
    {
      CHECK_PRIORITY;
      return master->closeDome ();
    }
  return Rts2DevConn::commandAuthorized ();
}
