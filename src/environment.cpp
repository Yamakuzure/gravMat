#include "environment.h"

// we need this here to be able to call the dtor, create our matter container and do clearThreads():
#include "matter.h"

// Here the real pixel info headers have to be included
#include "dustpixel.h" // It will pull masspixel.h in

// Needed for loading and saving:
#include <fstream>
#include <pwxLib/tools/StreamHelpers.h>


/// @brief return version as c-string
const char *ENVIRONMENT::getVersion() const
{
  return (version.c_str());
}

/** @brief Default constructor **/
ENVIRONMENT::ENVIRONMENT (int32_t aSeed) :
  camDist (0.), colorMap (NULL), currFrame (0), cyclPerFrm (1. / 50.),
  doDynamic (false), doHalfX (false), doHalfY (false), doPause (false),
  doVideo (false), doWork (true), drawDust (false), dynMaxZ (1000.0),
  elaDay (0), elaHour (0), elaMin (0), elaSec (0), elaYear (),
  explode (false), fileVersion (5),
  font (NULL), fontSize (12.f), fov (90.), fps (50),
  halfHeight (200.0), halfWidth (200.0), hasUserTime (false),
#if defined(PWX_HAS_CXX11_INIT)
  image({}),
#endif
  initFinished (false), isLoaded (false),
  minZ (1000.0), maxZ (1000.0),
  numThreads (8), offX (0.0), offY (0.0), offZ (0.0), outFileFmt ("outfile_%06d.png"),
  picNum (0), saveFile (""), screen (NULL), scrHeight (400), scrWidth (400),
  secondsDone(0), secPerCycle(604800), secPerFrame(NULL),
  secPFmod(6.048e5 / static_cast<double>(fps)),
  seed (aSeed), shockwave (false),
  spxRedu (1.667), spxSmoo (1.337), spxWave (5), spxZoom (29.7633),
#if defined(PWX_HAS_CXX11_INIT)
  statClock({}),
#endif
  statCurrMove (0.), statDone (0), statMaxAccel (0.), statMaxMove (0.),
  statMaxWidth (200), statTimeEla (0.),
  thread (NULL), threadPrg (NULL), threadRun (NULL),
  universe(NULL),
  zDustMap (NULL), zMassMap (NULL),
  version ("0.8.6")
{
  memset (msg,     0, 256);
  memset (prgFmt,  0, 25);
  memset (sortFmt, 0, 64);
  memset (statMsg, 0, 256);
}


/** @brief Default destructor **/
ENVIRONMENT::~ENVIRONMENT()
{
  doWork = false;

  // Be dead safe about our threads _first_!
  if (thread)
    {
      clearThreads();
      for (int32_t tNum = 0; tNum < numThreads; ++tNum)
        {
          if (thread[tNum])
            {
              delete thread[tNum];
              thread[tNum]    = NULL;
            }
        }
      delete [] thread;
    }

  // Then clear the rest:
  if (screen)      { delete    screen; }
  if (font)        { delete    font; }
  if (colorMap)    { delete    colorMap; }
  if (secPerFrame) { delete [] secPerFrame; }
  if (threadPrg)   { delete [] threadPrg; }
  if (threadRun)   { delete [] threadRun; }
  if (universe)    { delete    universe;  }
  if (zDustMap)
    {
      for (int32_t x = 0; x < scrWidth; ++x)
        {
          if (zDustMap[x])
            {
              delete [] zDustMap[x];
            }
        }
      delete [] zDustMap;
    }
  if (zMassMap)
    {
      for (int32_t x = 0; x < scrWidth; ++x)
        {
          if (zMassMap[x])
            {
              delete [] zMassMap[x];
            }
        }
      delete [] zMassMap;
    }

  screen      = NULL;
  font        = NULL;
  colorMap    = NULL;
  secPerFrame = NULL;
  thread      = NULL;
  threadPrg   = NULL;
  threadRun   = NULL;

}


/// @brief simple function that finishes and clears all threads
void ENVIRONMENT::clearThreads()
{
  for (int32_t tNum = 0; tNum < numThreads; ++tNum)
    {
      if (threadRun[tNum] && thread[tNum])
        {
          thread[tNum]->Wait();
        }

      threadRun[tNum] = false;

      if (thread[tNum])
        {
          delete thread[tNum];
          thread[tNum] = NULL;
        }
    }
}


/// @brief return a dust sphere pixel with a lower or equal z which is the last or has a next with higher z than given
sDustPixel *ENVIRONMENT::findPrevDust (sDustPixel *start, double z)
{
  sDustPixel *result = start;
  sDustPixel *next   = result->next;

  while (next && ((next->z < 0.) || (next->z >= z)) )
    {
      result = next;
      next   = result->next;
    }
  // result now either points to the dust sphere behind z or the last unset dust sphere
  // with next having an equal or smaller z or being NULL.

  return result;
}


/** @brief invalidate dust spheres by given z position
  * This method sets all dust spheres with a larger z than @a z to z=-1, and the root
  * to -2 if this invalidates all dust spheres in the queue.
**/
void ENVIRONMENT::invalidateDustSpheres(int32_t x, int32_t y, double z)
{
  sDustPixel *curr = &zDustMap[x][y];

  if (curr->z > -1.5)
    {
      while (curr && ((curr->z < 0.) || (curr->z >= z)) )
        {
          if (curr->z > 0.)
            curr->invalidate();
          curr = curr->next;
        }

      // If the queue is completely invalidated, we note this fact in the root dust sphere:
      if (NULL == curr)
        zDustMap[x][y].z = -2.0;
      else if ( (curr->z > 0.) && ((curr->z + curr->range) > z) )
        {
          curr->range = z - curr->z;
          // It might be that this dust sphere is reduced to a too small size:
          if (!isDustLargeEnough(curr->range, curr->maxRange))
            // Invalidate it then:
            curr->invalidate();
        }
    } // End of having at least one dust sphere to invalidate.
}


/// @brief move dust sphere information up to free @a toFree of data
void ENVIRONMENT::moveDustSpheresUp (sDustPixel *toFree, int32_t x, int32_t y)
{
  sDustPixel *curr = &zDustMap[x][y];

  // Note: This method must not be called if the root item is taken! Check beforehand!
  assert ((curr->z < 0.) && "ERROR: You did not check the root item before calling moveDustSpheresUp()! HOW DARE YOU!");

  // Now simply loop through the chain until toFree is reached
  while (curr && (curr != toFree))
    {
      sDustPixel *next = curr->next;
      if (next && (next->z > 0.))
        *curr = *next;
      curr = next;
    }

  // We must have been successful:
  assert (curr && (curr == toFree) && "ERROR: You called moveDustSpheresUp() with other x/y than toFree is in!");
  // Note: toFree is meant to be overwritten now, so it is not necessary to invalidate it.
}


/// @brief split @a dust sphere into two at either the start, the middle or end, updating @a z and @a range
/// Important: the caller (must not be someone else but projectDust()) is responsible for the values to be useful!)
int32_t ENVIRONMENT::splitDust (sDustPixel *dust, int32_t x, int32_t y, double &z,
                                uint8_t r, uint8_t g, uint8_t b, double &range, double maxRange)
{
  int32_t result = EXIT_SUCCESS;

  /* Basically there are four cases to split:
   *-------------------------------------------
   *
   * Case A: Split the start of *dust:
   * <---    z + range   --->
   *            <---     *dust    --->
   * ==> Result:
   * <-z+range-><-- split --><-*dust->
   * Note: In this case dust->z and z can be equal and the ends can be equal.
   *-------------------------------------------
   *
   * Case B: z + range reaches over *dust:
   * <---          z + range           --->
   *            <-- *dust -->
   * ==> Result:
   * <-z+range-><-- split --><- new dust ->
   * Here *dust will be overwritten with the split values and the rest
   * of z+range is added as a new dust sphere.
   * Note: In this case dust->z and z can be equal but *dust stops early.
   *-------------------------------------------
   *
   * Case C: Split *dust in the middle:
   *              <-- z+range -->
   * <---              *dust               --->
   * ==> Result:
   * <-- *dust --><--- split  --><--new dust-->
   * Note: In this case z is always greater than dust->z or it would
   *       be case A and handled there. Therefore the ends can be equal.
   *-------------------------------------------
   *
   * Case D: Split the end of *dust
   *          <---     z + range    --->
   * <---    *dust     --->
   * ==> Result:
   * <-*dust-><-- split --><- z+range ->
   * Note: In this case z is always greater than dust->z and dust always
   *       stops early or it would be be case B and handled there.
  */
  bool isCaseA = false, isCaseB = false, isCaseC = false, isCaseD = false;

  // Find out which case we have:
  if (!(dust->z < z) && ((z + range) > dust->z))
    {
      if ((dust->z + dust->range) >= (z + range))
        isCaseA = true;
      else
        isCaseB = true;
    }
  else if ((z > dust->z) && ((dust->z + dust->range) > z) )
    {
      if ((dust->z + dust->range) >= (z + range))
        isCaseC = true;
      else
        isCaseD = true;
    }
  // No else, as this means there is no overlapping. Although this ought to be impossible, it is safe to be ignored.

  if (isCaseA || isCaseB || isCaseC || isCaseD)
    {
      double spStart = (isCaseA || isCaseB) ? dust->z : z;
      double spRange = isCaseA ? z + range - spStart
                     : isCaseB ? dust->range
                     : isCaseC ? range
                     : isCaseD ? dust->z + dust->range - spStart
                     : 0.0;
      // Now the split end can be set:
      double spEnd = spStart + spRange;

      // Now we have all values to calculate the combined color of the split
      double opacityA    = maxRange / spRange;
      double opacityB    = dust->maxRange / spRange;
      double spOpacity   = opacityA + opacityB;
      double spMaxRange  = spRange / spOpacity;
      bool   isLongSplit = false; // If this stays being false, not all shifts are done below
      bool   doOverwrite = false; // Set to true below if *dust has to be reduced to an unusable size
      int32_t spRed = 0, spGre = 0, spBlu = 0;
      if (isDustLargeEnough(spRange, spMaxRange))
        {
          isLongSplit = true;
          spRed = static_cast<uint32_t>(  pwx_round(static_cast<double>(      r) * (opacityA / spOpacity))
                                        + pwx_round(static_cast<double>(dust->r) * (opacityB / spOpacity)) );
          spGre = static_cast<uint32_t>(  pwx_round(static_cast<double>(      g) * (opacityA / spOpacity))
                                        + pwx_round(static_cast<double>(dust->g) * (opacityB / spOpacity)) );
          spBlu = static_cast<uint32_t>(  pwx_round(static_cast<double>(      b) * (opacityA / spOpacity))
                                        + pwx_round(static_cast<double>(dust->b) * (opacityB / spOpacity)) );
        }

      // Before we add the split, z+range and *dust need to be corrected, or the adding of the split wreaks havoc!
      if (isCaseA)
        {
          /* <---    z + range   --->
           *            <---     *dust    --->
           * ==> Result:
           * <-z+range-><-- split --><-*dust->
           */
          if (isLongSplit)
            {
              // *dust is only touched if the split is large enough
              dust->z      = spEnd;
              dust->range -= spRange;
              // It might be that dust is reduced too much. We can overwrite it then
              if (!isDustLargeEnough(dust->range, dust->maxRange))
                doOverwrite = true;
            }
          // range is always shortened, but might get invalidated
          range -= spRange;
          if (!isDustLargeEnough(range, maxRange))
            range = -1.0;
        }
      else if (isCaseB)
        {
          /* <---          z + range           --->
           *            <-- *dust -->
           * ==> Result:
           * <-z+range-><-- split --><- new dust ->
           * Note: *dust is overwritten with the values of the split, and a new dust sphere is added
           */
          double restRange = z + range - spEnd;

          // If the split starts at z, z+range is to be invalidated:
          range = spStart - z;
          if (!isDustLargeEnough(range, maxRange))
            range = -1.0;

          // Note: *dust must not be touched, it will be overwritten later.
          doOverwrite = true;

          // Now add the remaining dust sphere if large enough
          if (isDustLargeEnough(restRange, maxRange))
            result = projectDust(x, y, spEnd, r, g, b, restRange, maxRange);
          // if the rest is too small, it can be ignored.
        }
      else if (isCaseC)
        {
          /*              <-- z+range -->
           * <---              *dust               --->
           * ==> Result:
           * <-- *dust --><--- split  --><--new dust-->
           */
          double newZ = spEnd;
          double newR = dust->z + dust->range - newZ;

          dust->range = spStart - dust->z;
          // It can happen that *dust is shortened too much. If it is so we turn
          // this into case B, so *dust gets overwritten below with the split values
          if (!isDustLargeEnough(dust->range, dust->maxRange))
            doOverwrite = true;

          // If the rest is large enough, it can be added
          if (isDustLargeEnough(newR, dust->maxRange))
            result = projectDust(x, y, newZ, dust->r, dust->g, dust->b, newR, dust->maxRange);
          range  = -1.0; // Used to be damn sure it is invalidated. See projectDust() why.
        }
      else
        {
          /*          <---     z + range    --->
           * <---    *dust     --->
           * ==> Result:
           * <-*dust-><-- split --><- z+range ->
           */
          if (isLongSplit)
            // *dust is only touched if the split is large enough
            dust->range -= spRange;
          // It can happen that *dust is shortened too much. If it is so we turn
          // this into case B, so *dust gets overwritten below with the split values
          if (!isDustLargeEnough(dust->range, dust->maxRange))
            doOverwrite = true;

          range -= spRange;
          if (isDustLargeEnough(range, maxRange))
            z = spEnd;
          else
            range = -1.0;
        }

      // Finally add the split, unless case C failed with adding the second part of *dust
      if (EXIT_SUCCESS == result)
        {
          // Here we need to know whether to add or overwrite:
          if (doOverwrite)
            // This marks *dust to be overwritten with the split values and can be set by Cases C and D, too
            dust->setAll(spStart,
                         static_cast<uint8_t>(spRed & 0x000000ff),
                         static_cast<uint8_t>(spGre & 0x000000ff),
                         static_cast<uint8_t>(spBlu & 0x000000ff),
                         spRange, spMaxRange);
          else if (isLongSplit)
            // Otherwise a new dust sphere with the split values is projected:
            result = projectDust(x, y, spStart,
                                 static_cast<uint8_t>(spRed & 0x000000ff),
                                 static_cast<uint8_t>(spGre & 0x000000ff),
                                 static_cast<uint8_t>(spBlu & 0x000000ff),
                                 spRange, spMaxRange);

          // Now invalidate all dust sphere(parts) that are behind this z if the split is opaque:
          if (isLongSplit && (spRange > (0.995 * spMaxRange)))
            invalidateDustSpheres(x, y, spStart);
          // Note: All dust spheres with less than 0.5% transparency are considered to be completely opaque.
        } // end of save guard against case c failure
    } // End of having a valid situation

  return result;
}


/// @brief this little helper initializes the zMaps. Width and Height **MUST** be fixed when calling this!
int32_t ENVIRONMENT::initZMaps()
{
  int32_t result = EXIT_SUCCESS;

  try
    {
      zDustMap = new sDustPixel*[scrWidth];
      zMassMap = new sMassPixel*[scrWidth];
      for (int32_t x = 0; x < scrWidth; ++x)
        {
          zDustMap[x] = new sDustPixel[scrHeight];
          zMassMap[x] = new sMassPixel[scrHeight];
        }
    }
  catch (std::bad_alloc &e)
    {
      result = EXIT_FAILURE;
      int32_t xSize = scrWidth * scrHeight;
      cerr << "ERROR: unable to allocate " << ( (xSize * sizeof (sDustPixel)) + (xSize * sizeof (sMassPixel)) );
      cerr << " bytes for the zMaps! [" << e.what() << "]" << endl;
    }

  return result;
}


/// @brief return true if this dust sphere is large enough (check visibility first to correct range or use isDustUseful()!)
bool ENVIRONMENT::isDustLargeEnough(double range, double maxRange)
{
  return ((range > Min_Dust_Range) && (maxRange > Min_Dust_MaxRange));
}


/// @brief return true if this dust sphere is useful (visible and large enough)
bool ENVIRONMENT::isDustUseful(int32_t x, int32_t y, double &z, double &range, double maxRange)
{
  return (isDustVisible(x, y, z, range) && isDustLargeEnough(range, maxRange));
}


/// @brief return true if a dust sphere is not hidden behind a mass, shortening @a range if needed.
bool ENVIRONMENT::isDustVisible(int32_t x, int32_t y, double &z, double &range)
{
  // Precheck: If the dust sphere is completely positioned behind the camera, ignore it:
  if ((z + range) < Min_Dust_Range)
    // Note: Min_Dust_Range is used as an "Epsilon" of larger than zero here.
    return false;

  double  zMassZ = zMassMap[x][y].z;

  /* There are two modifying conditions:
   * a) The dust sphere starts with z < 0, but reaches into view via its range
   *   -> Move z and shorten range
   * b) The dust sphere starts in front of a mass but reaches into it via its range
   *   -> shorten the range
  */

  // Check a)
  if (z < Min_Dust_Range)
    {
      // Note: z+range is already tested in the Precheck above
      range = z + range - Min_Dust_Range;
      z = Min_Dust_Range;
    }

  // Check b)
  if ((zMassZ > 0.) && (z < zMassZ) && ((z + range) > zMassZ))
    range = zMassZ - z;

  if ((zMassZ < 0.) || (zMassZ > z))
    return true;
  return false;
}


/// @brief load all data from saveFile
int32_t ENVIRONMENT::load()
{
  int32_t result = EXIT_SUCCESS;

  if (saveFile.size() && pwx_file_isRW (saveFile.c_str()))
    {
      std::ifstream inFile (saveFile.c_str());

      if (inFile.is_open())
        {
          using ::pwx::StreamHelpers::readNextValue;
          using ::pwx::StreamHelpers::skipLineBreak;
          /// 1.: Load env data
          int32_t loadFileVersion = 0;
          result = readNextValue (loadFileVersion, inFile, ';') ? EXIT_SUCCESS : EXIT_FAILURE;

          // Unfortunately the safe file prior fileVersion 5 is no longer loadable:
          if (loadFileVersion < 5)
            {
              cerr << "Unfortunately save files prior version 5 are no longer supported." << endl;
              result = EXIT_FAILURE;
            }
          if (EXIT_SUCCESS == result) { result = readNextValue (initFinished,inFile, ';') ? EXIT_SUCCESS : EXIT_FAILURE; }
          if (EXIT_SUCCESS == result) { result = readNextValue (doHalfX,     inFile, ';') ? EXIT_SUCCESS : EXIT_FAILURE; }
          if (EXIT_SUCCESS == result) { result = readNextValue (doHalfY,     inFile, ';') ? EXIT_SUCCESS : EXIT_FAILURE; }
          if (EXIT_SUCCESS == result) { result = readNextValue (doDynamic,   inFile, ';') ? EXIT_SUCCESS : EXIT_FAILURE; }
          if (EXIT_SUCCESS == result) { result = readNextValue (doVideo,     inFile, ';') ? EXIT_SUCCESS : EXIT_FAILURE; }
          if (EXIT_SUCCESS == result) { result = readNextValue (explode,     inFile, ';') ? EXIT_SUCCESS : EXIT_FAILURE; }
          if (EXIT_SUCCESS == result) { result = readNextValue (fov,         inFile, ';') ? EXIT_SUCCESS : EXIT_FAILURE; }
          if (EXIT_SUCCESS == result) { result = readNextValue (fps,         inFile, ';') ? EXIT_SUCCESS : EXIT_FAILURE; }
          if (EXIT_SUCCESS == result) { result = readNextValue (halfHeight,  inFile, ';') ? EXIT_SUCCESS : EXIT_FAILURE; }
          if (EXIT_SUCCESS == result) { result = readNextValue (halfWidth,   inFile, ';') ? EXIT_SUCCESS : EXIT_FAILURE; }
          if (EXIT_SUCCESS == result) { result = readNextValue (secPFmod,    inFile, ';') ? EXIT_SUCCESS : EXIT_FAILURE; }
          if (EXIT_SUCCESS == result) { result = readNextValue (offX,        inFile, ';') ? EXIT_SUCCESS : EXIT_FAILURE; }
          if (EXIT_SUCCESS == result) { result = readNextValue (offY,        inFile, ';') ? EXIT_SUCCESS : EXIT_FAILURE; }
          if (EXIT_SUCCESS == result) { result = readNextValue (offZ,        inFile, ';') ? EXIT_SUCCESS : EXIT_FAILURE; }
          if (EXIT_SUCCESS == result) { result = readNextValue (outFileFmt,  inFile, ';') ? EXIT_SUCCESS : EXIT_FAILURE; }
          if (EXIT_SUCCESS == result) { result = skipLineBreak (inFile)                   ? EXIT_SUCCESS : EXIT_FAILURE; }
          if (EXIT_SUCCESS == result) { result = readNextValue (picNum,      inFile, ';') ? EXIT_SUCCESS : EXIT_FAILURE; }
          if (EXIT_SUCCESS == result) { result = readNextValue (currFrame,   inFile, ';') ? EXIT_SUCCESS : EXIT_FAILURE; }
          if (EXIT_SUCCESS == result) { result = readNextValue (statCurrMove,inFile, ';') ? EXIT_SUCCESS : EXIT_FAILURE; }
          if (EXIT_SUCCESS == result) { result = readNextValue (scrHeight,   inFile, ';') ? EXIT_SUCCESS : EXIT_FAILURE; }
          if (EXIT_SUCCESS == result) { result = readNextValue (scrWidth,    inFile, ';') ? EXIT_SUCCESS : EXIT_FAILURE; }
          if (EXIT_SUCCESS == result) { result = readNextValue (secPerCycle, inFile, ';') ? EXIT_SUCCESS : EXIT_FAILURE; }
          if (EXIT_SUCCESS == result) { result = readNextValue (secondsDone, inFile, ';') ? EXIT_SUCCESS : EXIT_FAILURE; }
          if (EXIT_SUCCESS == result) { result = readNextValue (seed,        inFile, ';') ? EXIT_SUCCESS : EXIT_FAILURE; }
          if (EXIT_SUCCESS == result) { result = readNextValue (spxRedu,     inFile, ';') ? EXIT_SUCCESS : EXIT_FAILURE; }
          if (EXIT_SUCCESS == result) { result = readNextValue (spxSmoo,     inFile, ';') ? EXIT_SUCCESS : EXIT_FAILURE; }
          if (EXIT_SUCCESS == result) { result = readNextValue (spxWave,     inFile, ';') ? EXIT_SUCCESS : EXIT_FAILURE; }
          if (EXIT_SUCCESS == result) { result = readNextValue (spxZoom,     inFile, ';') ? EXIT_SUCCESS : EXIT_FAILURE; }

          /// 2.: Note that we have loaded data:
          if (EXIT_SUCCESS == result) { isLoaded = true; }

          // Note: Matter is loaded in initSFML to be able to show progress
          inFile.close();
        } // End of file is open
    } // End of having file name and file is readable and writable

  if (EXIT_FAILURE == result)
    {
      cerr << "ERROR loading " << saveFile << "! It seems to be broken!" << endl;
      cerr << "Please use either an older version of gravMat, or set up a new data file." << endl;
    }

  return result;
}

/** @brief need new gravitation calculation
  *
  * @return true if a new calculation of the gravitation is needed
  */
bool ENVIRONMENT::needGravCalc()
{
  // This is just a shot in the dark until some good testing shows what is needed.
  if (statCurrMove >= universe->NeedNewGDist)
    {
      statCurrMove -= universe->NeedNewGDist;
      return true;
    }
  else
    return false;

}

/** @brief project a dust sphere pixel onto the zDustMap
  *
  * IMPORTANT: env must have been locked, and the position checked beforehand!
  */
int32_t ENVIRONMENT::projectDust (int32_t x, int32_t y, double z, uint8_t r, uint8_t g, uint8_t b, double range, double maxRange)
{
  int32_t result = EXIT_SUCCESS;

  // If this dust sphere is not useful, return early and simply ignore it.
  bool isValid = isDustUseful(x, y, z, range, maxRange);
  if (!isValid)
    return result;

  sDustPixel *root       = &zDustMap[x][y];
  sDustPixel *curr       = root;
  sDustPixel *next       = curr->next;
  bool        checkCaseA = curr->z > z ? true : false; // This makes things easier below.

  if (root->z > -1.5)
    {
      // This queue is not empty, so we have to find the position to fill
      curr = findPrevDust(root, z);
      next = curr->next;

      /* We have one of the following situations now, guaranteed by the fact, that the queue does never hold
       * two dust spheres that overlap each other:
       * Note: z+range below is shown as overlapping, but it is not said right now that it does.
       *
       * --- Case A : ---
       * There already is a dust sphere (*curr) that is further away than z. It *might* be the last dust sphere in the queue
       * <-- *next or NULL -->      <-- *curr --*>
       *                 <-- z+range -->
       * Note: *curr is possibly *root, but that doesn't matter as it is starting behind z
       * --- Case B : ---
       * The queue is either empty, *next is NULL then, or the first set item is nearer than z. In both variations
       * *curr points to an unset dust sphere then.
       * <-- *next or NULL -->                ( *curr is an unset dust sphere )
       *                 <-- z+range -->
       * --- Case C : ---
       * If the queue is full and the root item is already nearer than z, it looks like this:
       * <-- *next or NULL -->  <-- *root == *curr -->
       *                                     <-- z+range -->
       * Note: Due to the nature of a blind backside queue, Case C can *only* happen if the queue is full. If
       * it isn't, *curr would point to the last unset dust sphere, which then is Case B.
       *
       * In all cases we have to check whether the nearer item (*next in A and B, *root in C) reaches into
       * this new z+range dust sphere and split if necessary. In case A it is necessary to additionally check the
       * complete range whether it reaches in or over dust spheres that are further away than z.
       * ---
       * Note: I do know that it is difficult to picture a backside queue. Imagine standing at the back, looking
       * at the camera. The queue is then in the right order, the first set dust sphere is the one nearest to you,
       * the next will be further away from that back side position. Unfortunately one is looking at
       * dust->z + dust->range with dust->z being the far end from there. But apart from that it is not this hard.
      */

      // --------------------------------------------------------------------------------------------------------------------
      // === Step 1: === Check whether the nearer dust sphere *next (Case A/B) or *root (Case C) reaches into the new one ---
      // --------------------------------------------------------------------------------------------------------------------
      if ( next && (next->z <= z) && ((next->z + next->range) > z))
        // This is Case A or B with *next reaching into the new dust sphere
        result = splitDust (next, x, y, z, r, g, b, range, maxRange);
        // Note: splitDust sets z and range to the remaining values or invalidates them
      else if ((curr == root) && (curr->z > 0.) && (curr->z <= z) && ((curr->z + curr->range) > z))
        // This is Case C with *curr reaching into the new dust sphere
        result = splitDust (curr, x, y, z, r, g, b, range, maxRange);

      // ---------------------------------------------------------------------------------------------------------------
      // === Step 2: === Check for Case A (z+range reaching into dust spheres with larger z) as long as it is needed ---
      // ---------------------------------------------------------------------------------------------------------------
      while ( (EXIT_SUCCESS == result) && checkCaseA && isValid)
        {
          /* this is a bit trickier, because the new dust sphere could stretch over several other
           * dust spheres. To resolve this we use a simply technique I've used countless times
           * (*coughcoughthricecough*) successfully in similar (or completely different)
           * situations: The reverse reduction.
          */

          curr = findPrevDust(root, z + range);
          next = curr->next;

          /* *curr now points to the last dust sphere that starts after z+range ends with the following additional details:
           * A : *curr might be unset when there is no dust sphere behind the end of the new one
           * B : *next might be NULL if *curr is the last item
           * C : *curr might be nearer if it is the root item, the queue is full, and there is no dust sphere nearer than
           *     the new one
           * Cases B and C mean that we are done. Case A means that we are done if *next is unset or ends nearer than z.
           * Note: Case C is not to be checked, as it equals Case C in Step 1 and is therefore already handled
          */
          if (next && ((next->z + next->range) > z))
            {
              result = splitDust(next, x, y, z, r, g, b, range, maxRange);
              isValid = isDustLargeEnough(range, maxRange);
            }
          else
            checkCaseA = false;
        } // End of step 2
    } // End of split search if needed

  // ----------------------------------------------------------------------------------------------------------------
  // === Step 3: === Search again for the position the new dust sphere is to place in if we have a state to go on ---
  // ----------------------------------------------------------------------------------------------------------------
  if ( (EXIT_SUCCESS == result) && !checkCaseA && isValid)
    {
      // A (new) search is needed, even if the queue is empty, because we need the last item then
      curr = findPrevDust(root, z);
      next = curr->next;

      if (root->z < -1.5)
        // If it is empty, it won't be any longer
        root->z = -1.0;

      //--------------------------------------------------------
      // === Step 4: === Add the remaining dust sphere to the chain ---
      //--------------------------------------------------------
      /* At this point we have one of the following situations: (The same as prior step 1, but with no overlapping)
       * --- Case A : ---
       * There already is a dust sphere (*curr) that is further away than z. It *might* be the last dust sphere in the queue
       * <-- *next or NULL -->               <-- *curr --*>
       *                      <-- z+range -->
       * => To do: move the queue up if there is place or add a new dust sphere behind curr as the new curr, then setup *curr
       * --- Case B : ---
       * The queue is either empty, *next is NULL then, or the first set item is nearer than z. In both variations
       * *curr points to an unset dust sphere then.
       * <-- *next or NULL -->                ( *curr is an unset dust sphere )
       *                      <-- z+range -->
       * => To do: Nothing special to be done, just set *curr to the new values
       * --- Case C : ---
       * If the queue is full and the root item is already nearer than z, it looks like this:
       * <-- *next or NULL -->  <-- *root == *curr -->
       *                                               <-- z+range -->
       * => To do: Add a new dust sphere and make it the new *next to *root. Copy the values from *root into it and setup
       *           *root
      */

      // Do we have space or need to add a new dust sphere?
      if (root->z < 0.)
        {
          // There is enough space, this applies to Cases A and B.
          if (curr->z > 0.)
            // This is Case A, we have to move dust spheres aside to allow curr to be set up to the new values:
            moveDustSpheresUp(curr, x, y);
          // No else, as Case B has set curr to a dust sphere that is allowed to be set up
        }
      else
        {
          // we have to add a new one, this applies to Cases A and C:
          try
            {
              sDustPixel *newDust = new sDustPixel();

              // The new dust sphere is always placed behind curr
              newDust->next = next;
              curr->next    = newDust;

              // We must check whether curr is the root item.
              if ((curr == root) && (curr->z < z))
                {
                  // This applies to Case C
                  *newDust = *curr;
                  next     = newDust;
                }
              else
                // Otherwise this is Case A
                curr = newDust;
            } // end of adding a new dust sphere to the queue
          catch (std::bad_alloc &e)
            {
              cerr << "ERROR : Unable to generate a new dust sphere pixel \n";
              cerr << "Reason: " << e.what() << endl;
              result = EXIT_FAILURE;
            }
        } // End of handling a full queue

      // Now set curr to our values unless the creation of a new item was unsuccessful:
      if (EXIT_SUCCESS == result)
        {
          curr->setAll (z, r, g, b, range, maxRange);
          assert((curr->next == next) && "ERROR: curr->next does not equal next. How can this be?");
          assert (((NULL == curr->next) || (curr->next->z < curr->z) || (curr->z < 0.))
                && "ERROR: The ordering of the dust spheres here is broken!");
        }
    } // End of check for step 3 and the processing of step 4

  return result;
}

/** @brief project a mass pixel onto the zMassMap
  *
  * IMPORTANT: env must have been locked, and the position checked beforehand!
  */
void ENVIRONMENT::projectMass (int32_t x, int32_t y, double z, uint8_t r, uint8_t g, uint8_t b)
{
  // The mass pixel must be visible:
  if (z < universe->M2Pos)
    {
      z = universe->M2Pos;
      // But this means it might not be front any more
      if (!(z < zMassMap[x][y].z))
        return;
    }

  // Now check...
  assert ( (x >= 0) && (x < scrWidth) && (y >= 0) && (y < scrHeight)
        && "What on earth am I supposed to do with a pixel not on the plane?");
  assert ( ((zMassMap[x][y].z < 0.) || (z < zMassMap[x][y].z))
        && "(MASS) You did NOT check whether z is smaller than zMassMap[x][y].z! SHAME ON YOU!");

  // ...and set
  if ((zMassMap[x][y].z < 0.) || (z < zMassMap[x][y].z))
    {
      zMassMap[x][y].z = z;
      zMassMap[x][y].r = r;
      zMassMap[x][y].g = g;
      zMassMap[x][y].b = b;

      // Now invalidate all dust sphere(parts) that are behind this z
      invalidateDustSpheres(x, y, z);
    }
}


/// @brief save all data into saveFile
int32_t ENVIRONMENT::save(std::ofstream& outFile)
{
  int32_t result = EXIT_FAILURE;

  if (outFile.is_open())
    {
      /// 1.: Save env data
      outFile << ";"; // Needed for the loading to work.
      outFile << fileVersion << ";";
      outFile << initFinished << ";";
      outFile << doHalfX << ";";
      outFile << doHalfY << ";";
      outFile << doDynamic << ";";
      outFile << doVideo << ";";
      outFile << explode << ";";
      outFile << fov << ";";
      outFile << fps << ";";
      outFile << halfHeight << ";";
      outFile << halfWidth << ";";
      outFile << secPFmod << ";";
      outFile << offX << ";";
      outFile << offY << ";";
      outFile << offZ << ";";
      outFile << outFileFmt << endl << ";";
      outFile << picNum << ";";
      outFile << currFrame << ";";
      outFile << statCurrMove << ";";
      outFile << scrHeight << ";";
      outFile << scrWidth << ";";
      outFile << secPerCycle << ";";
      outFile << secondsDone << ";";
      outFile << seed << ";";
      outFile << spxRedu << ";";
      outFile << spxSmoo << ";";
      outFile << spxWave << ";";
      outFile << spxZoom << ";";
      result = EXIT_SUCCESS;
    } // End of file is open

  if (outFile.bad())
    result = EXIT_FAILURE;

  return result;
}

/// @brief determine the dynMaxZ value for dynamic camera movement
void ENVIRONMENT::setDynamicZ()
{
  if (doDynamic)
    {
      double xMinZ = std::abs(minZ);
      dynMaxZ = (xMinZ < maxZ) ? xMinZ : maxZ; // Do _NOT_ move backwards behind maxZ!
    }
}


/// @brief simple function to start all threads with a given function
void ENVIRONMENT::startThreads (void (*aCb) (void *))
{
  for (int32_t tNum = 0; tNum < numThreads; ++tNum)
    {
      lock();
      threadEnv *env = new threadEnv (this, tNum);
      thread[tNum] = new sf::Thread (aCb, env);
      thread[tNum]->Launch();
      unlock();
    }
}
