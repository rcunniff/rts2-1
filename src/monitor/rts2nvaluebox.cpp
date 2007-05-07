#include "rts2nvaluebox.h"
#include "nmonitor.h"

Rts2NValueBox::Rts2NValueBox (Rts2NWindow * top, Rts2Value * in_val)
{
  topWindow = top;
  val = in_val;
}

Rts2NValueBox::~Rts2NValueBox (void)
{

}

Rts2NValueBoxBool::Rts2NValueBoxBool (Rts2NWindow * top,
				      Rts2ValueBool * in_val, int x, int y):
Rts2NValueBox (top, in_val),
Rts2NSelWindow (top->getX () + x, top->getY () + y, 10, 4)
{
  maxrow = 2;
  setLineOffset (0);
  if (!in_val->getValueBool ())
    setSelRow (1);
}

keyRet Rts2NValueBoxBool::injectKey (int key)
{
  switch (key)
    {
    case KEY_ENTER:
    case K_ENTER:
      return RKEY_ENTER;
      break;
    }
  return Rts2NSelWindow::injectKey (key);
}

void
Rts2NValueBoxBool::draw ()
{
  Rts2NSelWindow::draw ();
  werase (getWriteWindow ());
  mvwprintw (getWriteWindow (), 0, 1, "true");
  mvwprintw (getWriteWindow (), 1, 1, "false");
  refresh ();
}

void
Rts2NValueBoxBool::sendValue (Rts2Conn * connection)
{
  if (!connection->getOtherDevClient ())
    return;
  connection->
    queCommand (new
		Rts2CommandChangeValue (connection->getOtherDevClient (),
					getValue ()->getName (), '=',
					getSelRow () == 0));
}

Rts2NValueBoxFloat::Rts2NValueBoxFloat (Rts2NWindow * top, Rts2ValueFloat * in_val, int x, int y):
Rts2NValueBox (top, in_val),
Rts2NWindowEditDigits (top->getX () + x, top->getY () + y, 20, 3, 1, 1, 300,
		       1)
{
  wprintw (getWriteWindow (), "%f", in_val->getValueFloat ());
}

keyRet
Rts2NValueBoxFloat::injectKey (int key)
{
  return Rts2NWindowEditDigits::injectKey (key);
}

void
Rts2NValueBoxFloat::draw ()
{
  Rts2NWindowEditDigits::draw ();
  refresh ();
}

void
Rts2NValueBoxFloat::sendValue (Rts2Conn * connection)
{
  if (!connection->getOtherDevClient ())
    return;
  char buf[200];
  char *endptr;
  mvwinnstr (getWriteWindow (), 0, 0, buf, 200);
  float tval = strtof (buf, &endptr);
  if (*endptr != '\0' && *endptr != ' ')
    {
      // log error;
      return;
    }
  connection->
    queCommand (new
		Rts2CommandChangeValue (connection->getOtherDevClient (),
					getValue ()->getName (), '=', tval));
}

Rts2NValueBoxDouble::Rts2NValueBoxDouble (Rts2NWindow * top, Rts2ValueDouble * in_val, int x, int y):
Rts2NValueBox (top, in_val),
Rts2NWindowEditDigits (top->getX () + x, top->getY () + y, 20, 3, 1, 1, 300,
		       1)
{
  wprintw (getWriteWindow (), "%f", in_val->getValueDouble ());
}

keyRet
Rts2NValueBoxDouble::injectKey (int key)
{
  return Rts2NWindowEditDigits::injectKey (key);
}

void
Rts2NValueBoxDouble::draw ()
{
  Rts2NWindowEditDigits::draw ();
  refresh ();
}

void
Rts2NValueBoxDouble::sendValue (Rts2Conn * connection)
{
  if (!connection->getOtherDevClient ())
    return;
  char buf[200];
  char *endptr;
  mvwinnstr (getWriteWindow (), 0, 0, buf, 200);
  double tval = strtod (buf, &endptr);
  if (*endptr != '\0' && *endptr != ' ')
    {
      // log error;
      return;
    }
  connection->
    queCommand (new
		Rts2CommandChangeValue (connection->getOtherDevClient (),
					getValue ()->getName (), '=', tval));
}
