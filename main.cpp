/** @file main.cpp
  *
  * @brief main file for gravMat
  *
  * This is the main file for gravMat, a small program that renders matter
  * in a Universe distributed via Simplex Noise and moving according to
  * the gravity between all matter particles.
  *
  * (c) 2007-2012 Sven Eden, PrydeWorX
  * @author Sven Eden, PrydeWorX - Hamburg, Germany
  *         yamakuzure@users.sourceforge.net
  *         http://pwxlib.sourceforge.net
  *
  *  This program is free software: you can redistribute it and/or modify
  *  it under the terms of the GNU General Public License as published by
  *  the Free Software Foundation, either version 3 of the License, or
  *  (at your option) any later version.
  *
  *  This program is distributed in the hope that it will be useful,
  *  but WITHOUT ANY WARRANTY; without even the implied warranty of
  *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  *  GNU General Public License for more details.
  *
  *  You should have received a copy of the GNU General Public License
  *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
  *
  @verbatim
  * History and Changelog:
  * ----------------------
  * Version   Date        Maintainer      Change(s)
  * 0.0.1     2011-07-20  sed, PrydeWorX  initial design
  * 0.1.0     2011-09-02  sed, PrydeWorX  First working version
  * 0.8.1     2011-09-30  sed, PrydeWorX  Version bump to new pwxLib release version
  * 0.8.1.1   2011-10-05  sed, PrydeWorX  Added a third distribution mode --shockwave
  * 0.8.1.2   2011-10-06  sed, PrydeWorX  Drawing of units has been completely rewritten
  * 0.8.2     2011-10-07  sed, PrydeWorX  Version bump to new pwxLib release version
  * 0.8.2.1   2011-10-15  sed, PrydeWorX  Complete rewrite of the rendering
  * 0.8.3     2011-10-16  sed, PrydeWorX  Version bump to new pwxLib release version
  * 0.8.3.1   2011-10-17  sed, PrydeWorX  Corrected the help text, had old WiP title
  * 0.8.3.2   2011-11-30  sed, PrydeWorX  The rendering has been rewritten and is now a simple ray tracer
  * 0.8.3.3   2011-12-01  sed, PrydeWorX  Eventually fixed the halo pixel projection
  *                                       Fixed a bug that could lead to a crash if a halo is consumed by a split
  *                                       Changed the rendering system to deep view
  * 0.8.3.4   2011-12-03  sed, PrydeWorX  The rendering finally works as wanted.
  * 0.8.3.5   2011-12-05  sed, PrydeWorX  Fixed initial offset calculation and eventually got the shockwave
  *                                       mode right.
  * 0.8.3.6   2011-12-08  sed, PrydeWorX  Eventually finished switching from blind values to kg/m (MKS system of units)
  *                                       Fixed the workloop, it never calculated collisions correctly after
  *                                       loading a later state
  * 0.8.3.7   2011-12-11  sed, PrydeWorX  The projection is done by env only now and might a) move pixels and b) shorten
  *                                       halo ranges accordingly.
  *                                       The projection offset is no longer calculated using the center, but the nearest
  *                                       mass position of the unit.
  *                                       The window now refreshes after being minimized or overlapped
  *                                       Stats are now displayed fully everywhere.
  * 0.8.3.8   2011-12-30  sed, PrydeWorX  Finalized renderer and added dynamic constants to add MKS system of units to the
  *                                       physics engine.
  * 0.8.3.9   2012-01-04  sed, PrydeWorX  The calculation of the mass radius has been adapted to MKS.
  *                                       Finally got the maxZ calculation right and eventually silenced the moaning of
  *                                       cppcheck
  * 0.8.3.10  2012-01-10  sed, PrydeWorX  Fixed and simplified initial maximum offset values
  * 0.8.3.11  2012-01-11  sed, PrydeWorX  Changed matter initialization to reflect new MKS system
  *                                       Eventually I found the time to add an icon...
  * 0.8.3.12  2012-02-14  sed, PrydeWorX  Changed the colormap to use the new pwxLib component CWaveColor
  * 0.8.3.13  2012-02-15  sed, PrydeWorX  As Halos are Dust Spheres, the code has been changed to reflect that.
  *                                       Dust Spheres span fully around the masses now.
  * 0.8.3.14  2012-02-16  sed, PrydeWorX  Finished changing halos to dust spheres.
  * 0.8.3.15  2012-02-24  sed, PrydeWorX  Fully applied time scaling in a way the calculations can be stayed being done
  *                                       each second. Further the saving between cycles is possible now without losing
  *                                       information.
  * 0.8.5     2012-03-01  sed, PrydeWorX  Version bump to new pwxLib release version
  * 0.8.5.1   2012-03-13  sed, PrydeWorX  Saving is now done every minute or before a new gravitation calculation is needed.
  *                                       Units are no longer cleaned up unless a frame has been drawn.
  * 0.8.5.2   2012-03-20  sed, PrydeWorX  Added an option --dyncam to have the camera being moved dynamically towards the
  *                                       scene with the original distance being the maximum distance.
  * 0.8.6     2012-04-??  sed, PrydeWorX  Version bump to new pwxLib release version
  @endverbatim
**/

#include "main.h"
#include "sfmlui.h"  // singled out, has only relevance to main.cpp!
#include "consoleui.h" // only main has to put messages onto the console with these functions

#include <pwxLib/tools/Exception.h>

int main ( int argc, char** argv )
{
  int32_t result = EXIT_SUCCESS;
  ENVIRONMENT * env = NULL;

  try
    {
      srand(static_cast<uint32_t>(time(NULL)));
      env = new ENVIRONMENT(rand() % 25000 + 25000);
      result = processArguments(env, argc, argv);
    }
  catch (std::bad_alloc &e)
    {
      cerr << "ERROR : Unable to create environment!" << endl;
      cerr << "REASON: \"" << e.what() << "\"" << endl;
      env = NULL;
      result = EXIT_FAILURE;
    }

  try
    {
      if ((EXIT_SUCCESS == result) && env && env->doWork)
        {
          // A : Initialize Display:
          result = initSFML(env);

          // B : Enter WorkLoop:
          if (EXIT_SUCCESS == result)
            result = workLoop(env);
        } // End of there is work to be done

      // Clean out environment
      if (env) delete (env);
      env = NULL;
    }
  catch (pwx::Exception &e)
    {
      cerr << "\n =============================== " << endl;
      cerr << "Uncaught mrf exception \"" << e.name() << "\" from " << e.where() << endl;
      cerr << "Message    : " << e.what() << endl;
      cerr << "Description: " << e.desc() << endl;
      cerr << "Full Func  : " << e.pfunc() << endl;
      cerr << " ------------------------------- " << endl;
      cerr << "Trace:" << endl;
      cerr << e.trace() << endl;
      cerr << " =============================== " << endl;
      result = EXIT_FAILURE;
    }
  catch (std::exception &e)
    {
      // Oh no...
      cerr << "\n =============================== " << endl;
      cerr << "Uncaught std exception : \"" << e.what() << "\"" << endl;
      cerr << " =============================== " << endl;
      result = EXIT_FAILURE;
    }
  catch (...)
    {
      cerr << "\n =============================== " << endl;
      cerr << "PANIC! Unknown exception encountered!" << endl;
      cerr << " =============================== " << endl;
      result = EXIT_FAILURE;
    }

  // No matter what, clean up:
  cleanup();

  return result;
}
