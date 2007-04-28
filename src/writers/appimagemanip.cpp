#include "rts2appdbimage.h"
#include "../utils/rts2config.h"

#include <iostream>
#include <iomanip>

#include <list>

#define IMAGEOP_NOOP		0x00
#define IMAGEOP_ADDDATE		0x01
#define IMAGEOP_INSERT		0x02
#define IMAGEOP_TEST		0x04
#define IMAGEOP_COPY		0x08
#define IMAGEOP_MOVE		0x10
#define IMAGEOP_EVAL		0x20

class Rts2AppImageManip:public Rts2AppDbImage
{
private:
  int operation;

  void printOffset (double x, double y, Rts2Image * image);

  int addDate (Rts2Image * image);
  int insert (Rts2ImageDb * image);
  int testImage (Rts2Image * image);
  int testEval (Rts2Image * image);

    std::string copy_expr;
    std::string move_expr;
protected:
    virtual int processOption (int in_opt);
  virtual int processImage (Rts2ImageDb * image);
public:
    Rts2AppImageManip (int in_argc, char **in_argv);
};

void
Rts2AppImageManip::printOffset (double x, double y, Rts2Image * image)
{
  double sep;
  double x_out;
  double y_out;
  double ra, dec;

  image->getOffset (x, y, x_out, y_out, sep);
  image->getRaDec (x, y, ra, dec);

  std::ios_base::fmtflags old_settings =
    std::cout.setf (std::ios_base::fixed, std::ios_base::floatfield);

  int old_p = std::cout.precision (2);

  std::cout << "Rts2Image::getOffset ("
    << std::setw (10) << x << ", "
    << std::setw (10) << y << "): "
    << "RA " << LibnovaRa (ra) << " "
    << LibnovaDegArcMin (x_out)
    << " DEC " << LibnovaDec (dec) << " "
    << LibnovaDegArcMin (y_out) << " ("
    << LibnovaDegArcMin (sep) << ")" << std::endl;

  std::cout.precision (old_p);
  std::cout.setf (old_settings);
}

int
Rts2AppImageManip::addDate (Rts2Image * image)
{
  int ret;
  time_t t;
  std::cout << "Adding date " << image->getImageName () << "..";
  t = image->getExposureSec ();
  image->setValue ("DATE-OBS", &t, image->getExposureUsec (),
		   "date of observation");
  ret = image->saveImage ();
  std::cout << (ret ? "failed" : "OK") << std::endl;
  return ret;
}

int
Rts2AppImageManip::insert (Rts2ImageDb * image)
{
  return image->saveImage ();
}

int
Rts2AppImageManip::testImage (Rts2Image * image)
{
  double ra, dec, x, y;
  std::cout
    << image << std::endl
    << "average " << image->getAverage () << std::endl
    << "stdev " << image->getStdDev () << std::endl
    << "bg_stdev " << image->getBgStdDev () << std::endl
    << "Image XoA and Yoa: [" << image->getXoA ()
    << ":" << image->getYoA () << "]" << std::endl
    << "[XoA:YoA] RA: " << image->getCenterRa ()
    << " DEC: " << image->getCenterDec () << std::endl
    << "FLIP: " << image->getFlip () << std::endl
    << image->getRaDec (image->getXoA (), image->getYoA (), ra, dec)
    << "ROTANG: " << ln_rad_to_deg (image->getRotang ())
    << " (deg) XPLATE: " << image->getXPlate ()
    << " YPLATE: " << image->getYPlate () << std::endl
    << "RA and DEC of [XoA:YoA]: " << ra << ", " << dec << std::endl
    << image->getRaDec (0, 0, ra, dec)
    << "RA and DEC of [0:0]: " << ra << ", " << dec << std::endl
    << image->getRaDec (image->getWidth (), 0, ra, dec)
    << "RA and DEC of [W:0]: " << ra << ", " << dec << std::endl
    << image->getRaDec (0, image->getHeight (), ra, dec)
    << "RA and DEC of [0:H]: " << ra << ", " << dec << std::endl
    << image->getRaDec (image->getWidth (), image->getHeight (), ra, dec)
    << "RA and DEC of [W:H]: " << ra << ", " << dec << std::endl
    << "Rts2Image::getCenterRow " << image->getCenter (x, y, 3) << " " << x
    << ":" << y << std::endl
    << "Expression %b/%t/%i/%c/%f '" << image->
    expandPath (std::string ("%b/%t/%i/%c/%f")) << '\'' << std::
    endl <<
    "Expression $DATE-OBS$/%b/%e/%E/%f/%F/%t/%i/%y/%m/%d/%D/%H/%M/%S/%s.fits '"
    << image->
    expandPath (std::
		string
		("$DATE-OBS$/%b/%e/%E/%f/%F/%t/%i/%y/%m/%d/%D/%H/%M/%S/%s.fits"))
    << '\'' << std::endl;

  printOffset (image->getXoA () + 50, image->getYoA (), image);
  printOffset (image->getXoA (), image->getYoA () + 50, image);
  printOffset (image->getXoA () - 50, image->getYoA (), image);
  printOffset (image->getXoA (), image->getYoA () - 50, image);

  printOffset (152, 150, image);

  return 0;
}

int
Rts2AppImageManip::testEval (Rts2Image * image)
{
  float value, error;

  image->evalAF (&value, &error);

  std::cout << "value: " << value << " error: " << error << std::endl;

  return 0;
}

int
Rts2AppImageManip::processOption (int in_opt)
{
  switch (in_opt)
    {
    case 'c':
      operation |= IMAGEOP_COPY;
      copy_expr = optarg;
      break;
    case 'd':
      operation |= IMAGEOP_ADDDATE;
      break;
    case 'i':
      operation |= IMAGEOP_INSERT;
      break;
    case 'm':
      operation |= IMAGEOP_MOVE;
      move_expr = optarg;
      break;
    case 't':
      operation |= IMAGEOP_TEST;
      break;
    case 'e':
      operation |= IMAGEOP_EVAL;
      break;
    default:
      return Rts2AppDbImage::processOption (in_opt);
    }
  return 0;
}

int
Rts2AppImageManip::processImage (Rts2ImageDb * image)
{
  if (operation & IMAGEOP_ADDDATE)
    addDate (image);
  if (operation & IMAGEOP_INSERT)
    insert (image);
  if (operation & IMAGEOP_TEST)
    testImage (image);
  if (operation & IMAGEOP_COPY)
    image->copyImageExpand (copy_expr);
  if (operation & IMAGEOP_MOVE)
    image->renameImageExpand (move_expr);
  if (operation & IMAGEOP_EVAL)
    testEval (image);
  return 0;
}

Rts2AppImageManip::Rts2AppImageManip (int in_argc, char **in_argv):Rts2AppDbImage (in_argc,
		in_argv)
{
  Rts2Config *
    config;
  config = Rts2Config::instance ();

  operation = IMAGEOP_NOOP;

  addOption ('c', "copy", 1,
	     "copy image(s) to path expression given as argument");
  addOption ('d', "add-date", 0, "add DATE-OBS to image header");
  addOption ('i', "insert", 0, "insert/update image(s) in the database");
  addOption ('m', "move", 1,
	     "move image(s) to path expression given as argument");
  addOption ('e', "eval", 0, "image evaluation for AF purpose");
  addOption ('t', "test", 0, "test various image routines");
}

int
main (int argc, char **argv)
{
  Rts2AppImageManip app = Rts2AppImageManip (argc, argv);
  int
    ret = app.init ();
  if (ret)
    {
      return ret;
    }
  return app.run ();
}
