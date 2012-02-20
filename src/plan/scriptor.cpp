/*
 * Scriptor body.
 * Copyright (C) 2007 Petr Kubanek <petr@kubanek.net>
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

#include "device.h"
#include "rts2devcliphot.h"

#include "rts2script/execcli.h"
#include "rts2script/scripttarget.h"

#include <map>
#include <stdexcept>

#define OPT_EXPAND_PATH       OPT_LOCAL + 101
#define OPT_GEN               OPT_LOCAL + 102

/** Prefix for variables holding script for device **/
#define DEV_SCRIPT_PREFIX     "SC_"

namespace rts2script
{

/**
 * This is main scriptor body. Scriptor is used to run on RTS2 device list,
 * reading script generated by script-gen command. Script generator is passed
 * as argument system state. Scriptor generator can then base its
 * decision on system state.
 *
 * @author Petr Kubanek <petr@kubanek.net>
 */
class Scriptor:public rts2core::Device, public ScriptInterface
{
	private:
		rts2core::ValueInteger *scriptCount;
		rts2core::ValueString *expandPath;
		rts2core::ValueSelection *scriptGen;

		std::map <std::string, rts2core::ValueString *> scriptVar;

		rts2script::ScriptTarget *currentTarget;
	protected:
		virtual int init ();
		virtual int processOption (int in_opt);
		virtual int willConnect (NetworkAddress * in_addr);
		virtual rts2core::DevClient *createOtherType (rts2core::Connection *conn, int other_device_type);
		virtual void deviceReady (rts2core::Connection * conn);
	public:
		Scriptor (int argc, char **argv);
		virtual ~Scriptor (void);

		virtual void postEvent (rts2core::Event * event);

		virtual int findScript (std::string in_deviceName, std::string & buf);
		virtual void getPosition (struct ln_equ_posn *posn, double JD);
};

}

using namespace rts2script;

Scriptor::Scriptor (int argc, char **argv):rts2core::Device (argc, argv, DEVICE_TYPE_SCRIPTOR, "SCRIPTOR"), ScriptInterface ()
{
	createValue (scriptCount, "script_count", "number of scripts execuced", false);
	createValue (expandPath, "expand_path", "expand path for new images", false, RTS2_VALUE_WRITABLE);
	expandPath->setValueCharArr ("%f");

	createValue (scriptGen, "script_generator", "command which gets state and generates next script", false);
	scriptGen->addSelVal ("/etc/rts2/scriptor");

	addOption (OPT_EXPAND_PATH, "expand-path", 1, "path used for filename expansion");
	addOption (OPT_GEN, "script-gen", 1, "script generator");

	currentTarget = NULL;
}


Scriptor::~Scriptor (void)
{
}

int Scriptor::init ()
{
	int ret;
	ret = rts2core::Device::init ();
	if (ret)
		return ret;

	currentTarget = new rts2script::ScriptTarget (this);
	currentTarget->moveEnded ();

	return 0;
}

int Scriptor::processOption (int in_opt)
{
	switch (in_opt)
	{
		case OPT_EXPAND_PATH:
			expandPath->setValueCharArr (optarg);
			break;
		case OPT_GEN:
			scriptGen->addSelVal (optarg);
			break;
		default:
			return rts2core::Device::processOption (in_opt);
	}
	return 0;
}

int Scriptor::willConnect (NetworkAddress * in_addr)
{
	if (in_addr->getType () < getDeviceType ())
		return 1;
	return 0;
}

rts2core::DevClient * Scriptor::createOtherType (rts2core::Connection *conn, int other_device_type)
{
	switch (other_device_type)
	{
		case DEVICE_TYPE_CCD:
			return new DevClientCameraExec (conn, expandPath);
		default:
			return rts2core::Device::createOtherType (conn, other_device_type);
	}
}

void Scriptor::deviceReady (rts2core::Connection * conn)
{
	// add variable for this device..
	rts2core::ValueString *stringVal;
	try
	{
		createValue (stringVal, (std::string (DEV_SCRIPT_PREFIX) + std::string (conn->getName())).c_str (), std::string ("Script value for ") + std::string (conn->getName ()), true);
		updateMetaInformations (stringVal);
		scriptVar[std::string (conn->getName ())] = stringVal;
	}
	catch (rts2core::Error (&er))
	{
	}

	conn->postEvent (new rts2core::Event (EVENT_SET_TARGET, (void *) currentTarget));
//	conn->postEvent (new rts2core::Event (EVENT_OBSERVE));
}

void Scriptor::postEvent (rts2core::Event * event)
{
	switch (event->getType ())
	{
		case EVENT_SCRIPT_ENDED:
			postEvent (new rts2core::Event (EVENT_SET_TARGET, (void *) currentTarget));
			break;
		case EVENT_SCRIPT_STARTED:
//			postEvent (new rts2core::Event (EVENT_OBSERVE));
			break;
	}
	rts2core::Device::postEvent (event);
}

int Scriptor::findScript (std::string in_deviceName, std::string & buf)
{
	std::ostringstream cmd;
	cmd << scriptGen->getSelName () << " " << getMasterState () << " " << in_deviceName;
	logStream (MESSAGE_DEBUG) << "Calling '" << cmd.str () << "'." << sendLog;
	FILE *gen = popen (cmd.str().c_str (), "r");

	char *filebuf = NULL;
	size_t len;
	ssize_t ret = getline (&filebuf, &len, gen);
	if (ret == -1)
		return -1;
	// replace \n
	filebuf[ret - 1] = '\0';
	buf = std::string (filebuf);
	logStream (MESSAGE_DEBUG) << "Script '" << buf << "'." << sendLog;
	pclose (gen);

	std::map <std::string, rts2core::ValueString *>::iterator iter = scriptVar.find (in_deviceName);
	if (iter != scriptVar.end ())
	{
		rts2core::ValueString *val = (*iter).second;
		val->setValueString (filebuf);
		sendValueAll (val);
	}

	free (filebuf);
	return 0;
}

void Scriptor::getPosition (struct ln_equ_posn *posn, double JD)
{
	posn->ra = 20;
	posn->dec = 20;
}

int main (int argc, char **argv)
{
	Scriptor scriptor (argc, argv);
	return scriptor.run ();
}
