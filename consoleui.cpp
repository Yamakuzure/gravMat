#include "consoleui.h"

#include <pwxLib/tools/Args.h>
using namespace pwx::args::constants;
using pwx::args::addArgCb;
using pwx::args::addArgBool;
using pwx::args::addArgDouble;
using pwx::args::addArgInt32;
using pwx::args::addArgInt64;
using pwx::args::addArgString;

#include <pwxLib/tools/MathHelpers.h>
using pwx::MathHelpers::degToRad;

#include <pwxLib/tools/StreamHelpers.h>
using pwx::StreamHelpers::to_int64;

#include "matter.h"

// Local callback to have one single method to organize the display of help/version
void cbHelpVersion(const char *arg, void *env)
  {
    if (arg && strlen(arg) && env)
      {
        ENVIRONMENT *xEnv = static_cast<ENVIRONMENT *>(env);
        if (STREQ(arg, "help"))
          showHelp(xEnv);
        else if (STREQ(arg, "version"))
          showVersion(xEnv);
        xEnv->doWork = false;
      }
  }

// Local callback to have one single method to handle the time scale aliases
void cbSecPerCycle(const char *arg, void *aEnv)
  {
    if (arg && strlen(arg) && aEnv)
      {
        ENVIRONMENT *xEnv = reinterpret_cast<ENVIRONMENT *>(aEnv);
        xEnv->hasUserTime = true;
        if      (STREQ(arg, "second"     )) xEnv->secPerCycle =           1;
        else if (STREQ(arg, "minute"     )) xEnv->secPerCycle =          60;
        else if (STREQ(arg, "hour"       )) xEnv->secPerCycle =        3600;
        else if (STREQ(arg, "day"        )) xEnv->secPerCycle =       86400;
        else if (STREQ(arg, "week"       )) xEnv->secPerCycle =      604800;
        else if (STREQ(arg, "month"      )) xEnv->secPerCycle =     2592000;
        else if (STREQ(arg, "year"       )) xEnv->secPerCycle =    31536000;
        else if (STREQ(arg, "decade"     )) xEnv->secPerCycle =   315360000;
        else if (STREQ(arg, "century"    )) xEnv->secPerCycle =  3153600000;
        else if (STREQ(arg, "millennium" )) xEnv->secPerCycle = 31536000000;
        else
          xEnv->hasUserTime = false;
      }
  }

// Local callback to notify env that a user has set their own time scale value
void cbUserTime(const char *arg, void *aEnv)
  {
    if (arg && strlen(arg) && aEnv)
      {
        ENVIRONMENT *xEnv = reinterpret_cast<ENVIRONMENT *>(aEnv);
        xEnv->hasUserTime = true;
        int64_t userTime = to_int64(arg);
        if (userTime > 0)
          xEnv->secPerCycle = userTime;
        else
          xEnv->hasUserTime = false;
      }
  }

int32_t checkOutFileFmt(ENVIRONMENT* env)
{
  int32_t result = EXIT_SUCCESS;

  if (env->outFileFmt.size() > 4)
    {
      std::string fNameBase = "";
      std::string fNameExt  = "";

#if defined(_MSC_VER)
      char fileName[256] = "";
      char fileExt[8]    = "";
      _splitpath_s(env->outFileFmt.c_str(), NULL, 0, NULL, 0, fileName, 255, fileExt, 7);
      fNameBase = fileName;
      fNameExt  = fileExt;
      if (fNameExt[0] == '.')
        // Yeah, MSVC++ does that...
        fNameExt.erase(0,1);
#else
      fNameBase = basename(env->outFileFmt.c_str());
      size_t extPos = fNameBase.find_last_of('.');
      if (extPos != fNameBase.npos)
        {
          fNameExt = fNameBase.substr(extPos + 1);
          fNameBase.erase(extPos);
        }
#endif
      // Now fNameBase should have the file name without suffix and fNameExt the suffix
      if (fNameBase.size() && fNameExt.size())
        {
          // Test 1: Check suffix. png, bmp and jpg are allowed
          if ( (fNameExt != "bmp")
            && (fNameExt != "png")
            && (fNameExt != "jpg") )
            {
              cerr << "ERROR: The suffix \"" << fNameExt << "\" is not supported!" << endl;
              cerr << "       Supported are bmp, png and jpg." << endl;
              result = EXIT_FAILURE;
            }

          // Test 2: Check whether there is only one or no %
          size_t firstPos = fNameBase.find_first_of('%');
          size_t lastPos  = fNameBase.find_last_of('%');
          if (firstPos != lastPos)
            {
              cerr << "ERROR: More than one formatting part detected in \"" << fNameBase << "." << fNameExt << "\"" << endl;
              result = EXIT_FAILURE;
            }
          // Test 3: Check whether the format found is a valid numeric one (%*d)
          else if (firstPos != fNameBase.npos)
            {
              bool isFound = false;
              bool isLegal = true;
              try
                {
                  while (!isFound && isLegal)
                    {
                      const char xCh = fNameBase[++firstPos]; // This'll throw if firstPos goes beyond the strings size
                      if (xCh == 'd')
                        // The d is what we need to find after the % ...
                        isFound = true;
                      else if ((xCh < 0x30) || (xCh > 0x39))
                        // ... but there is nothing but numbers allowed between % and the 'd'
                        isLegal = false;
                    }
                }
              PWX_CATCH_AND_FORGET(std::out_of_range)
              // Now we tell the user if his format is illegal:
              if (!isFound || !isLegal)
                {
                  result = EXIT_FAILURE;
                  if (!isFound)
                    cerr << "ERROR: No 'd' found in format!" << endl;
                  if (!isLegal)
                    cerr << "ERROR: Only numbers are allowed in a %[0-9]d format!" << endl;
                }
            } // end of having exactly one '%'
          // Otherwise append %06d format:
          else
            {
              size_t fnLen  = env->outFileFmt.size();
              env->outFileFmt.erase(fnLen - fNameExt.size() - 1);
              env->outFileFmt += "_%06d.";
              env->outFileFmt += fNameExt;
            }
        } // End of having both fileName and suffix
      else
        result = EXIT_FAILURE;
    } // end of having an outFileFmt with more than 4 characters
  else if (env->outFileFmt.size() > 0)
    {
      cout << "Warning: " << env->outFileFmt << " is not a valid file format and ignored." << endl;
      env->outFileFmt.clear();
    }

  if (EXIT_FAILURE == result)
    cerr << "ERROR: \"" << env->outFileFmt << "\" is not a valid outfile!" << endl;

  return result;
}

int32_t processArguments(ENVIRONMENT * env, int argc, char * argv[])
{
  int32_t result = EXIT_SUCCESS;
  assert(env);

  // As we won't see anything if a second stays being a second, a time scaler is needed
  int32_t FoV       = 90;

  // -- normal arguments ---
  addArgBool  ("",  "dyncam", -2, "Dynamically move the camera towards the nearest unit, if it is in front of the camera", &env->doDynamic, ETT_TRUE);
  addArgBool  ("",  "explode", -2, "Matter is not distributed but explodes from the center", &env->explode, ETT_TRUE);
  addArgString("",  "file", -2, "File to load at program start from and to save on program end into", 1, "path", &env->saveFile, ETT_STRING);
  addArgInt32 ("",  "fov", -2, "field of vision (default 90, range 10-179)", 1, "value", &FoV, ETT_INT, 10, 179);
  addArgInt32 ("",  "fps", -2, "Set FPS between 1 and 200 (default 50)", 1, "FPS", &env->fps, ETT_INT, 1, 200);
  addArgBool  ("",  "halfX", -2, "Only create a matter unit for every second X coordinate", &env->doHalfX, ETT_TRUE);
  addArgBool  ("",  "halfY", -2, "Only create a matter unit for every second Y coordinate", &env->doHalfY, ETT_TRUE);
  addArgInt32 ("",  "height", -2, "Set window height (minimum 100)", 1, "height", &env->scrHeight, ETT_INT, 100, maxInt32Limit);
  addArgCb    ("",  "help", -2, "Show this help and exit", 0, NULL, cbHelpVersion, env);
  addArgBool  ("",  "shockwave", -2, "Matter is distributed in some kind of local shock waves", &env->shockwave, ETT_TRUE);
  addArgCb    ("",  "version", -2, "Show the programs version and exit", 0, NULL, cbHelpVersion, env);
  addArgInt32 ("",  "width", -2, "Set window width (minimum 100)", 1, "width", &env->scrWidth, ETT_INT, 100, maxInt32Limit);
  addArgString("o", "outfile", -2, "Format string for the output file. The default is \"outfile_%06d.png\". Supported are bmp, png and jpg.", 1, "pattern", &env->outFileFmt, ETT_STRING);
  addArgInt32 ("s", "seed", -2, "Set seed", 1, "value", &env->seed, ETT_INT, 0, maxInt32Limit);
  addArgInt32 ("t", "threads", -2, "Set number of threads (minimum 4, default 8)", 1, "num", &env->numThreads, ETT_INT, 4, maxInt32Limit);
  addArgDouble("R", "reduct", -2, "Set the reduction on each wave (minimum 1.0, default 1.667)", 1, "value", &env->spxRedu, ETT_FLOAT, 1.0, 1000000.0);
  addArgDouble("S", "smooth", -2, "Set the smoothing on each wave (minimum 1.0, default 1.337)", 1, "value", &env->spxSmoo, ETT_FLOAT, 1.0, 1000000.0);
  addArgInt32 ("W", "waves", -2, "Set number of waves (minimum 1, default 5)", 1, "value", &env->spxWave, ETT_INT, 1, maxInt32Limit);
  addArgDouble("Z", "zoom", -2, "Set the zoom factor (minimum 0.001, default 29.7633)", 1, "value", &env->spxZoom, ETT_FLOAT, 0.001, 1000000.0);

  // --- normal options with special passages in the help text ---
  addArgDouble("x", "",   0, "", 1, NULL, &env->offX, ETT_FLOAT, -1000000.0, 1000000.0);
  addArgDouble("y", "",   0, "", 1, NULL, &env->offY, ETT_FLOAT, -1000000.0, 1000000.0);
  addArgDouble("z", "",   0, "", 1, NULL, &env->offZ, ETT_FLOAT, -1000000.0, 1000000.0);

  // --- special options - env->secPerCycle ---
  addArgCb    ("",  "second",     1, "Alias for     1 second  : second",           0, NULL, cbSecPerCycle, env);
  addArgCb    ("",  "minute",     1, "Alias for    60 seconds : second",           0, NULL, cbSecPerCycle, env);
  addArgCb    ("",  "hour",       1, "Alias for    60 minutes : second",           0, NULL, cbSecPerCycle, env);
  addArgCb    ("",  "day",        1, "Alias for    24 hours   : second (*)",       0, NULL, cbSecPerCycle, env);
  addArgCb    ("",  "week",       1, "Alias for     7 days    : second (default)", 0, NULL, cbSecPerCycle, env);
  addArgCb    ("",  "month",      1, "Alias for    30 days    : second",           0, NULL, cbSecPerCycle, env);
  addArgCb    ("",  "year",       1, "Alias for   365 days    : second",           0, NULL, cbSecPerCycle, env);
  addArgCb    ("",  "decade",     1, "Alias for    10 years   : second",           0, NULL, cbSecPerCycle, env);
  addArgCb    ("",  "century",    1, "Alias for   100 years   : second",           0, NULL, cbSecPerCycle, env);
  addArgCb    ("",  "millennium", 1, "Alias for 1,000 years   : second",           0, NULL, cbSecPerCycle, env);
  // Note: We set "T" last, so it gets processed last
  addArgCb    ("T", "timescale", -2, "Set the time scaling factor. (default 1 hour)", 1, "sec", cbUserTime, env);


  if (pwx::args::loadArgs(argc, argv) >= 0)
    pwx::args::procArgs();

  /* First generate values that might overwrite loaded values if set too late */
  env->fov = static_cast<double>(FoV);
  if (!env->hasUserTime)
    {
      if (env->explode)
        // In explosion mode, the timescale value has a different default:
        env->secPerCycle = 86400; // A day instead of a week in seconds
      else
        env->secPerCycle = 604800;
    }

  if (0 != pwx::args::getErrorCount())
    {
      result = EXIT_FAILURE;
      cout << "The following errors occured:" << endl;
      for (size_t i = 0; i < pwx::args::getErrorCount(); ++i)
        cout << pwx::StreamHelpers::adjRight(2,0) << (i + 1) << ".: " << pwx::args::getError(i) << endl;
      cout << endl;
      showHelp(env);
    }
  // Otherwise load our datafile if we have one:
  else if (env->saveFile.size())
    {
      PWX_TRY(result = env->load())
      PWX_THROW_FURTHER
    }

  // check outFileFmt first, it is the most obvious error
  if (EXIT_SUCCESS == result)
    result = checkOutFileFmt(env);

  // Apply screen half height/width, maxZ, zMaps, fov and fps
  if (EXIT_SUCCESS == result)
    {
      // First width and height have to be set to a value of the power of 2
      env->scrWidth  -= env->scrWidth  % 2;
      env->scrHeight -= env->scrHeight % 2;
      // Now apply data
      env->halfHeight = env->scrHeight / 2.0;
      env->halfWidth  = env->scrWidth / 2.0;
      /* The camera distance (cZ) is the distance from the camera to the projection plane.
       * The half width of the projection plane (cX) is env->halfWidth and the angle
       * between cZ and the hypotenuse (cH) is half of FoV (alpha). We know alpha and cX
       * and need cZ:
       * cX = sin(alpha) * cH => cX / sin(alpha) = cH
       * cZ = cos(alpha) * cH
      */
      env->camDist    = std::cos(degToRad(env->fov / 2.))
                      * (env->halfWidth / std::sin(degToRad(env->fov / 2.)));

      /* Before creating our universe (meaning all the physical helper constants ;), maxZ
       * is to be determined. The idea is, that all units "start" with a distance of one
       * AU, no matter whether halfX/Y is chosen or not.
       * One AU is 149,597,870,691.0 m. We therefore need to know how many pixel positions
       * these are first, before we can calculate back, as the ratio between this and our
       * 1 or two pixels on the projection plane is the projection multiplier used to get maxZ
      */
      double SunMass = 1.99e30; // Mass in kg (J: 1.90e27) (15Mj: 2.85e28)
      double SunRad  = 6.96e8;  // meters
      double MinPos  = 3.75     // This means 7.5 pixels diameter if the sun would be positioned on the projection
                     / SunRad;  // plane, the result is one meter in positional coordinates.
      double AUinPos = MinPos * 149597870691.0L; // This is the number of Positions per AU
      /* The divisor now needed to get the max Z position is a reversal from what is to be
       * expected. Our units are initialized using the positions on the projection plane.
       * We use those without caring in how many meters (or AU) a distance of 1 would translate,
       * to achieve an initial state where all matter is nicely projected on our plane. This
       * means we have to calculate back our projected positions to real positions before creating
       * the unit.
       * Two units that are one AU apart (AUinPos) shall scale to be one pixel apart on our
       * projection plane in normal, two pixels in halfX mode.
       * The divisor is therefore 1 (or 2) / AUinPos, resulting in the number to multiply
       * X- and Y-Coordinates with to get the projected coordinates 1 (or 2) if their X
       * or Y is AUinPos and Z is zero.
      */
      double maxZdiv = (env->doHalfX || env->doHalfY ? 2. : 1. ) / AUinPos;
      /* As the multiplier m is:
       *   m  = aZ / bZ and
       *   bZ = aZ + Z
       * (aZ = env->camDist and Z = env->maxZ which we want to find)
       * We can get Z by transforming to
       *    m            = aZ / (aZ + Z)  | * (aZ + Z)
       * => m * (aZ + Z) = aZ             | / m
       * =>      aZ + Z  = aZ / m         | - aZ
       * =>           Z  = (aZ / m) - aZ
      */
      env->maxZ    = (env->camDist / maxZdiv) - env->camDist;
      env->dynMaxZ = env->maxZ;

      // Now initialize our universe constants:
      try
        {
          env->universe = new UNIVERSE(MinPos, SunRad, SunMass);
        }
      catch (std::bad_alloc &e)
        {
          result = EXIT_FAILURE;
          cout << "ERROR: unable to allocate space for the universe constants! [" << e.what() << "]" << endl;
        }
    }

  // Now it is time to init our z-maps
  if (EXIT_SUCCESS == result)
    result = env->initZMaps();

  // Apply numThreads for the threads and threadPrg numbers:
  if (EXIT_SUCCESS == result)
    {
      try
        {
          env->thread    = new sf::Thread*[env->numThreads];
          env->threadPrg = new int32_t[env->numThreads];
          env->threadRun = new bool[env->numThreads];
          for (int32_t i = 0; i < env->numThreads; ++i)
            {
              env->thread[i]    = NULL;
              env->threadPrg[i] = 0;
              env->threadRun[i] = false;
            }
        }
      catch (std::bad_alloc &e)
        {
          result = EXIT_FAILURE;
          cout << "ERROR: unable to allocate " << env->numThreads << " integers for thread progress! [";
          cout << e.what() << "]" << endl;
        }
    } // End of applying thread data

  // Now the time modifiers and values have to be initialized
  if (EXIT_SUCCESS == result)
    {
      double xFPS     = static_cast<double>(pwx_round(env->fps));
      env->cyclPerFrm = 1. / xFPS;
      env->secPFmod   = static_cast<double>(env->secPerCycle) / xFPS;

      // Now init our second per frame array
      try
        {
          double  xSec = 0.0;

          env->secPerFrame = new int64_t[env->fps];

          /* Note: The workflow will check the current index to see whether a frame is to be drawn or not.
           * After that the index is raised by one, and _checked again_, so if we have a scale of 50 frames
           * per second, all of them will bei 'ceil'ed to the next second.
           * This means that no actions need to be taken against neither high nor low time scaling.
          */

          // Simply add up the second fractions to catch rounding errors that would be there for sure otherwise.
          for (int32_t i = 0; i < env->fps; ++i)
            {
              xSec += env->secPFmod;
              env->secPerFrame[i] = static_cast<int64_t>(ceil(xSec)) % env->secPerCycle;
              // Note: The mod is done, because the last second is actually second 0 in the next cycle
            }

          assert( (static_cast<int32_t>(pwx_round(xSec)) == env->secPerCycle)
                && "ERROR: frame seconds do not add up to seconds per cycle!");
        }
      catch (std::bad_alloc &e)
        {
          result = EXIT_FAILURE;
          cout << "ERROR: unable to allocate " << env->fps << " integers for time management! [";
          cout << e.what() << "]" << endl;
        }

      // As this is only needed for scenarios with low secPerCycle values (a frame in less than
      // a second) it has to be clipped to 1.0 max. But not earlier, the full value was needed above.
      if (env->secPFmod > 1.0)
        env->secPFmod = 1.0;

    } // End of applying time values


  return (result);
}

void showHelp(ENVIRONMENT * env)
{
  int32_t spw = 6;  // Short option print width
  int32_t lpw = 22; // Long option print width
  int32_t dpw = 51; // Description print width

  cout << "Gravitation Matters - ";
  showVersion(env);
  cout << "------------------------";
  showVerDash(env);
  cout << endl;
  cout << "  Usage:" << endl;
  cout << "gravMat [options]" << endl << endl;
  cout << "The default behavior, when no options are given, is to open a 400x400 window," << endl;
  cout << "distribute material in a scattered spiral around a strong center and let it" << endl;
  cout << "flow until the escape key is pressed or only one matter unit is left." << endl;
  cout << endl << "  Options:" << endl;
  cout << "x/y/z   <value>             Set offset of the specified dimension." << endl;
  pwx::args::printArgHelp(cout, "dyncam", spw, lpw, dpw);
  pwx::args::printArgHelp(cout, "explode", spw, lpw, dpw);
  pwx::args::printArgHelp(cout, "fov", spw, lpw, dpw);
  pwx::args::printArgHelp(cout, "fps", spw, lpw, dpw);
  pwx::args::printArgHelp(cout, "file", spw, lpw, dpw);
  cout << "   Note: Data will be saved before each gravitation calculation. If anything" << endl;
  cout << "         goes wrong when loading data on program start, a new set of data" << endl;
  cout << "         will be created and the old file overwritten." << endl;
  pwx::args::printArgHelp(cout, "halfX", spw, lpw, dpw);
  pwx::args::printArgHelp(cout, "halfY", spw, lpw, dpw);
  pwx::args::printArgHelp(cout, "height", spw, lpw, dpw);
  pwx::args::printArgHelp(cout, "help", spw, lpw, dpw);
  pwx::args::printArgHelp(cout, "o", spw, lpw, dpw);
  pwx::args::printArgHelp(cout, "R", spw, lpw, dpw);
  pwx::args::printArgHelp(cout, "s", spw, lpw, dpw);
  pwx::args::printArgHelp(cout, "S", spw, lpw, dpw);
  pwx::args::printArgHelp(cout, "T", spw, lpw, dpw);
  cout << "   The following alias arguments are available for setting the time scale factor:" << endl;
  pwx::args::printArgHelp(cout, "second",     spw, lpw, dpw);
  pwx::args::printArgHelp(cout, "minute",     spw, lpw, dpw);
  pwx::args::printArgHelp(cout, "hour",       spw, lpw, dpw);
  pwx::args::printArgHelp(cout, "day",        spw, lpw, dpw);
  pwx::args::printArgHelp(cout, "week",       spw, lpw, dpw);
  pwx::args::printArgHelp(cout, "month",      spw, lpw, dpw);
  pwx::args::printArgHelp(cout, "year",       spw, lpw, dpw);
  pwx::args::printArgHelp(cout, "decade",     spw, lpw, dpw);
  pwx::args::printArgHelp(cout, "century",    spw, lpw, dpw);
  pwx::args::printArgHelp(cout, "millennium", spw, lpw, dpw);
  cout << "   Higher time scale factors can be set according to your needs." << endl;
  cout << "   (*): In explosion mode, a day is the default instead of a week." << endl;
  pwx::args::printArgHelp(cout, "shockwave", spw, lpw, dpw);
  pwx::args::printArgHelp(cout, "version", spw, lpw, dpw);
  pwx::args::printArgHelp(cout, "width", spw, lpw, dpw);
  pwx::args::printArgHelp(cout, "W", spw, lpw, dpw);
  pwx::args::printArgHelp(cout, "Z", spw, lpw, dpw);
}

void showVersion(ENVIRONMENT * env)
{
  cout << "Version " << env->getVersion() << endl;
}

void showVerDash(ENVIRONMENT * env)
{
  int32_t length = 8 + strlen(env->getVersion());
  for (int32_t i = 0; i < length; ++i)
    cout << "-";
}
