/* 
 * Starlight Express CCD driver.
 * Copyright (C) 2016 Petr Kubanek <petr@kubanek.net>
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

#include "camd.h"
#include "sxccd/sxccdusb.h"

#include <libusb-1.0/libusb.h>

#define MAX_CAM   5

namespace rts2camd
{

/**
 * Class for Starlight Express camera.
 *
 * @author Petr Kubanek <petr@kubanek.net>
 */
class SX:public Camera
{
	public:
		SX (int in_argc, char **in_argv);
		virtual ~SX (void);

		virtual int processOption (int in_opt);
		virtual int initHardware ();
		virtual int initChips ();
		virtual int info ();
		virtual int startExposure ();
		virtual long isExposing ();
		virtual int doReadout ();

	private:
		const char *sxName;
		bool listNames;

		HANDLE sxHandle;
		struct t_sxccd_params caps;

		rts2core::ValueInteger *model;
		rts2core::ValueBool *interlaced;
		rts2core::ValueLong *wipeDelay;

		char *evenBuffer, *oddBuffer;
};

};

using namespace rts2camd;

SX::SX (int in_argc, char **in_argv):Camera (in_argc, in_argv)
{
	sxName = NULL;
	listNames = false;

	sxHandle = NULL;

	evenBuffer = NULL;
	oddBuffer = NULL;

	addOption ('n', NULL, 1, "camera name (for systems with multiple CCDs)");
	addOption ('l', NULL, 0, "list available camera names");
	
	createExpType ();

	createValue (model, "model", "camera model", false);
	createValue (interlaced, "interlaced", "true if using interlaced camera", true);
	createValue (wipeDelay, "wipe_delay", "[us] delay for image wipe", true);
	wipeDelay->setValueLong (130000);
}

SX::~SX ()
{
	if (sxHandle != NULL)
		sxClose (&sxHandle);
	if (evenBuffer)
		free (evenBuffer);
	if (oddBuffer)
		free (oddBuffer);
}

int SX::processOption (int in_opt)
{
	switch (in_opt)
	{
		case 'n':
			sxName = optarg;
			break;
		case 'l':
			listNames = true;
			break;
		default:
			return Camera::processOption (in_opt);
	}
	return 0;
}

int SX::initHardware ()
{
	if (getDebug ())
		sxDebug (true);

	DEVICE devices[MAX_CAM];
	const char* names[MAX_CAM];

	int camNum = sxList (devices, names, MAX_CAM);
	if (camNum <= 0)
	{
		logStream (MESSAGE_ERROR) << "cannot find any SX camera" << sendLog;
		return -1;
	}

	if (listNames)
	{
		for (int i = 0; i < camNum; i++)
			std::cerr << "camera " << i << " name '" << names[i] << "'" << std::endl;
	}

	int cn = 0;

	if (sxName != NULL)
	{
		int i;
		for (i = 0; i < camNum; i++)
		{
			if (strcmp (names[i], sxName) == 0)
			{
				cn = i;
				break;
			}
		}
		if (i == camNum)
		{
			logStream (MESSAGE_ERROR) << "cannot find camera with name " << sxName << sendLog;
			return -1;
		}
	}

	int ret = sxOpen (devices[cn], &sxHandle);
	if (!ret)
		return -1;

	memset (&caps, 0, sizeof (caps));
	ret = sxGetCameraParams (sxHandle, 0, &caps);
	if (!ret)
		return ret;

	ret = sxReset (sxHandle);
	if (!ret)
		return -1;

	model->setValueInteger (sxGetCameraModel (sxHandle));
	interlaced->setValueBool (sxIsInterlaced (model->getValueInteger ()));

	return initChips ();
}

int SX::initChips ()
{
	struct t_sxccd_params cp;
	int ret = sxGetCameraParams (sxHandle, 0, &cp);
	if (!ret)
		return -1;
	if (interlaced->getValueBool ())
	{
		setSize (cp.width, cp.height * 2, 0, 0);
		evenBuffer = (char *) malloc (cp.height / 2 * cp.width * cp.bits_per_pixel / 8);
		oddBuffer = (char *) malloc (cp.height / 2 * cp.width * cp.bits_per_pixel / 8);
	}
	else
	{
		setSize (cp.width, cp.height, 0, 0);
	}
	return Camera::initChips ();
}

int SX::info ()
{
	return Camera::info ();
}

int SX::startExposure ()
{
	int ret;

	if (interlaced->getValueBool () && binningVertical () == 1)
	{
		ret = sxClearPixels (sxHandle, CCD_EXP_FLAGS_FIELD_EVEN | CCD_EXP_FLAGS_NOWIPE_FRAME, 0);
		if (!ret)
			return -1;
		usleep (wipeDelay->getValueLong ());
		ret = sxClearPixels (sxHandle, CCD_EXP_FLAGS_FIELD_ODD | CCD_EXP_FLAGS_NOWIPE_FRAME, 0);
		if (!ret)
			return -1;
	}
	else
	{
		ret = sxClearPixels (sxHandle, CCD_EXP_FLAGS_FIELD_BOTH, 0);
		if (!ret)
			return -1;
	}
	
	ret = sxSetShutter (sxHandle, 0);
	if (!ret)
		return -1;

	ret = sxSetTimer (sxHandle, getExposure () * 100.0);
	if (!ret)
		return -1;
	
	return 0;
}

long SX::isExposing ()
{
	unsigned long ret = sxGetTimer (sxHandle);

	if (ret == 0)
		return -2;

	return ret;
}

int SX::doReadout ()
{
	int ret;
	if (interlaced)
	{
		if (binningVertical () > 1)
		{
			ret = sxLatchPixels (sxHandle, CCD_EXP_FLAGS_FIELD_BOTH, 0, chipUsedReadout->getXInt (), chipUsedReadout->getYInt (), chipUsedReadout->getWidthInt (), chipUsedReadout->getHeightInt () / 2, binningHorizontal (), binningVertical () / 2);
			if (!ret)
				return -1;
			ret = sxReadPixels (sxHandle, getDataBuffer (0), chipUsedSize ());
			if (!ret)
				return -1;
		}
		else
		{
			ret = sxLatchPixels (sxHandle, CCD_EXP_FLAGS_FIELD_EVEN | CCD_EXP_FLAGS_SPARE2, 0, chipUsedReadout->getXInt (), chipUsedReadout->getYInt () / 2, chipUsedReadout->getWidthInt (), chipUsedReadout->getHeightInt () / 2, binningHorizontal (), 1);
			if (!ret)
				return -1;
			struct timespec start_time, end_time;
			clock_gettime (CLOCK_MONOTONIC, &start_time);
			ret = sxReadPixels (sxHandle, evenBuffer, chipUsedSize () / 2);
			if (!ret)
				return -1;
			clock_gettime (CLOCK_MONOTONIC, &end_time);
			wipeDelay->setValueLong ((end_time.tv_sec - start_time.tv_sec) * 1000000 + (end_time.tv_nsec - start_time.tv_nsec + 500) / 1000000);
			ret = sxLatchPixels (sxHandle, CCD_EXP_FLAGS_FIELD_ODD | CCD_EXP_FLAGS_SPARE2, 0, chipUsedReadout->getXInt (), chipUsedReadout->getYInt () / 2, chipUsedReadout->getWidthInt (), chipUsedReadout->getHeightInt () / 2, binningHorizontal (), 1);
			if (!ret)
				return -1;
			ret = sxReadPixels (sxHandle, oddBuffer, chipUsedSize () / 2);
			if (!ret)
				return -1;
			for (int i = 0, j = 0; i < chipUsedReadout->getHeightInt (); i += 2, j++)
			{
				memcpy (getDataBuffer (0) + i * lineByteSize (), oddBuffer + (j * lineByteSize ()), lineByteSize ());
				memcpy (getDataBuffer (0) + ((i + 1) * lineByteSize ()), evenBuffer + (j * lineByteSize ()), lineByteSize ()); 
			}
		}
	}
	else
	{
		ret = sxLatchPixels (sxHandle, CCD_EXP_FLAGS_FIELD_BOTH, 0, chipUsedReadout->getXInt (), chipUsedReadout->getYInt (), chipUsedReadout->getWidthInt (), chipUsedReadout->getHeightInt (), binningHorizontal (), binningVertical ());
		if (!ret)
			return -1;
		ret = sxReadPixels (sxHandle, getDataBuffer (0), chipUsedSize ());
		if (!ret)
			return -1;
	}

	updateReadoutSpeed (getReadoutPixels ());

	ret = sendReadoutData (getDataBuffer (0), getWriteBinaryDataSize ());
	if (ret < 0)
		return -1;
	if (getWriteBinaryDataSize () == 0)
		return -2;
	return 0;
}

int main (int argc, char **argv)
{
	SX device (argc, argv);
	return device.run ();
}