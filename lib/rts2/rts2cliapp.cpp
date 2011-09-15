/* 
 * Command Line Interface application sceleton.
 * Copyright (C) 2003-2007 Petr Kubanek <petr@kubanek.net>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#include "rts2cliapp.h"

#include <iostream>

Rts2CliApp::Rts2CliApp (int in_argc, char **in_argv):rts2core::App (in_argc, in_argv)
{
}

void Rts2CliApp::afterProcessing ()
{
}

int Rts2CliApp::run ()
{
	int ret;
	ret = init ();
	if (ret)
		return ret;
	ret = doProcessing ();
	afterProcessing ();
	return ret;
}