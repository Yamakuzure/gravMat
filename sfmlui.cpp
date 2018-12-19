#include <cstdarg>

#include "sfmlui.h"
#include "matter.h"

// Here the real pixel info headers have to be included
#include "dustpixel.h" // It will pull masspixel.h in

// Here is our program icon:
#include "icon.h"

// This is for the explosion initialization only:
#include <pwxLib/CSinCosTables.h>
using pwx::SCT;

#include <pwxLib/CRandom.h> // Provides pwx::RNG
using pwx::RNG;

// We need this to be able to save our matter container in env:
#define PWX_NO_MRF_INSTANCE
// Note: Remember to compile with -DPWX_THREADS !!!
#include <pwxLib/CMemRingFactory.h>
#undef PWX_NO_MRF_INSTANCE

/// @brief simply typedef to shorten things a bit...
typedef pwx::mrf::TMemRing<CMatter>    matCont;
typedef pwx::mrf::MRInterface<CMatter> matContInt;

// Global MRF:
pwx::mrf::CMemRingFactory localMRF(false, false);

// Global pointer to the matter container:
matCont *mCont;
// Global pointer to the interface array:
matContInt **mContInt;

/// @brief Do not forget to call before program ends!
void cleanup()
  {
    if (mContInt)   { delete [] mContInt; }
  }

void doEvents(ENVIRONMENT * env)
  {
    sf::Event event;

    while (env->screen->GetEvent(event))
      {
        /// 1.: Check whether to quit:
        if ( (sf::Event::Closed == event.Type)
                ||( (sf::Event::KeyPressed == event.Type)
                  &&(sf::Key::Escape       == event.Key.Code) ) )
          {
            env->screen->Close();
            env->doWork = false;
          } // End of checking event
        /// 2.: Check for window resizing and maintain aspect ratio
        else if (sf::Event::Resized == event.Type)
          {
            uint32_t newWidth  = event.Size.Width;
            uint32_t newHeight = event.Size.Height;

            // first limit size:
            if (newWidth < static_cast<uint32_t>(env->scrWidth))
              newWidth = env->scrWidth;
            if (newHeight < static_cast<uint32_t>(env->scrHeight))
              newHeight = env->scrHeight;

            // Now calculate the aspect ratios:
            double oldAspect   = static_cast<double>(env->scrWidth) / static_cast<double>(env->scrHeight);
            double newAspect   = static_cast<double>(newWidth) / static_cast<double>(newHeight);

            // Now check for wrong aspect sizes:
            if (newAspect > oldAspect)
              {
                // Here Width is drawn too large, we drag Height along:
                newHeight = static_cast<uint32_t>(pwx_round(static_cast<double>(newWidth) / oldAspect));
                env->screen->SetSize(newWidth, newHeight);
                env->screen->Display();
              }
            else if (newAspect < oldAspect)
              {
                // This means that the Height is drawn too large, drag width along:
                newWidth = static_cast<uint32_t>(pwx_round(static_cast<double>(newHeight) * oldAspect));
                env->screen->SetSize(newWidth, newHeight);
                env->screen->Display();
              }
          } // End of handling resize events
        /// 3.: Check for Pause mode
        else if ( (sf::Event::KeyPressed == event.Type)
                &&(sf::Key::Pause        == event.Key.Code) )
          {
            env->doPause = !(env->doPause);
            showMsg(env, "... paused ...");
          }

      } // End of having event

    // if we are in paused mode, the screen is to be refreshed:
    if (env->doPause)
      env->screen->Display();
  }

/// @brief just a little wrapper so matter.cpp doesn't need an own instance of RNG
double getSimOff(double x, double y, double z, double zoom)
  {
    RNG.lock();
    double offset = RNG.simplex3D(x, y, z, zoom);
    RNG.unlock();
    return offset;
  }

int32_t initSFML(ENVIRONMENT * env)
  {
    int32_t result = EXIT_SUCCESS;

    // we need a container, our own MRF does not use name and id map.
    PWX_TRY(mCont = localMRF.create(static_cast<CMatter *>(NULL)))
    PWX_THROW_FURTHER

    // Disable tracking, all items are guaranteed to be unique!
    mCont->disableTracking();

    // Further we need the dynamic array for the threads interfaces:
    try
      {
        mContInt = new matContInt*[env->numThreads];
        for (int32_t i = 0; i < env->numThreads; ++i)
          mContInt[i] = NULL;
      }
    catch (std::bad_alloc &e)
      {
        result = EXIT_FAILURE;
        cout << "ERROR: unable to allocate " << env->numThreads << " integers for thread progress! [";
        cout << e.what() << "]" << endl;
      }

    // Create Window:
    string title = "Gravitation Matters V";
    title += env->getVersion();
    title += " (c) PrydeWorX 2007-2012 (";
    title += pwx::StreamHelpers::to_string(env->numThreads);
    title += " Threads)";
    PWX_TRY(env->screen = new sf::RenderWindow(sf::VideoMode(env->scrWidth, env->scrHeight), title))
    catch (std::bad_alloc &e)
      {
        result = EXIT_FAILURE;
        cerr << "Unable to init SFML RenderWindow: \"" << e.what() << "\"" << endl;
      }

    // Set Icon:
    env->screen->SetIcon(gravmat_icon.width, gravmat_icon.height, gravmat_icon.pixel_data);

  // Load font:
  if (EXIT_SUCCESS == result)
    {
      std::string fontpath = FONT_PATH;
      fontpath += FONT_SEP;
      fontpath += FONT_NAME;

      if (fontpath.size() > 2)
        {
          try
            {
              env->font = new sf::Font;
              if (env->font->LoadFromFile(std::string(fontpath), 12))
                env->fontSize = static_cast<float>(env->font->GetCharacterSize());
              else
                {
                  cerr << "Failed to load \"" << FONT_PATH << FONT_SEP << FONT_NAME << "\"\n";
                  if (env->font) delete env->font;
                }
            }
          catch (std::bad_alloc &e)
            {
              cerr << "Error creating font : " << e.what() << endl;
            }
        }
      else
        cerr << "No font specified!\n";

      if (!env->font)
        {
          cerr << "--> Using built in Arial font instead." << endl;
          env->font = new sf::Font(sf::Font::GetDefaultFont());
        }
    }

    // Initialize our ColorMap
    if (EXIT_SUCCESS == result)
      {
        try
          {
            env->colorMap = new CColorMap();
          }
        catch (std::bad_alloc &e)
          {
            cerr << "Error initializing Color Map : " << e.what() << endl;
            result = EXIT_FAILURE;
          }
      }

    // Set the image to our screen width and height:
    result = (EXIT_SUCCESS == result) && env->image.Create(env->scrWidth, env->scrHeight) ? EXIT_SUCCESS : EXIT_FAILURE;

    // Set SCT to use life calculation, we need maximum precision!
    if ((EXIT_SUCCESS == result) && (-1 != SCT.setPrecision(-1)))
      {
        cerr << "ERROR: Setting SCT to life calculation failed! (Precision is " << SCT.getPrecision() << ")" << endl;
        result = EXIT_FAILURE;
      }

    // Initialize/load our Matter:
    if (EXIT_SUCCESS == result)
      {
        // We put everything into a large try-catch, as the error case is a break anyway
        try
          {
            int32_t maxNr  = static_cast<int32_t>(
                                (env->doHalfX ? env->halfWidth  : env->scrWidth )
                              * (env->doHalfY ? env->halfHeight : env->scrHeight) );

            // Now that we have a maximum number we can set up our progress format string:
            string maxNrStr  = ::pwx::StreamHelpers::to_string(maxNr);
            int32_t maxNrLen = maxNrStr.size();
            pwx_snprintf(env->prgFmt, 24, "%% 5.3f%%%% - %%%dd / %%%dd", maxNrLen, maxNrLen);
            pwx_snprintf(env->sortFmt, 63, "%%%dd / %%%dd sorted", maxNrLen, maxNrLen);

            if (env->isLoaded)
              {
                // We let a thread load the data so we can show a progress message
                int32_t oldNumThreads = env->numThreads;
                // Set to 1, only one is needed:
                env->numThreads = 1;

                // Start loader thread:
                env->startThreads(&thrdLoad);
                // Wait and show progress:
                waitLoad(env, "Loading: %s", maxNr);
                // End clear away the loader:
                env->clearThreads();

                // reset to previous state:
                env->numThreads = oldNumThreads;
              } // End of having to load data

            // We "ask" again, because if loading failed for some reason, we have to re-initialize
            if (!env->isLoaded)
              {
                // Create and start Threads for initializing
                env->startThreads(&thrdInit);
                // Now wait till all threads have finished
                waitThrd(env, "Initializing", maxNr);
                // Delete all finished threads:
                env->clearThreads();

                // Sort the units initially
                if (env->doWork)
                  {
                    env->startThreads(&thrdSort);
                    // We can _not_ use waitThrd() here, because the counting is done backwards!
                    waitSort(env);
                    env->clearThreads();
                  }
              } // End of initializing matter


            // Finally draw all units and show that we are ready:
            if (env->doWork)
              {
                env->setDynamicZ();
                env->startThreads(&thrdProj);
                waitThrd(env, "Projecting...");
                env->clearThreads();
                env->startThreads(&thrdDraw);
                waitThrd(env, "Tracing...   ");
                env->clearThreads();
              }
          }
        catch(pwx::Exception &e)
          {
            cerr << "pwx Exception: " << e.name() << " occurred at\n";
            cerr << e.where() << endl;
            cerr << "Reason: " << e.what() << " ; " << e.desc() << endl;
            cerr << "Trace:\n" << e.trace() << endl;
            result = EXIT_FAILURE;
          }
      } // End of initializing our matter

    return (result);
  }


// Returns the number of running threads, and adds up their progress in @a progress
int32_t running(ENVIRONMENT * env, int32_t *progress)
  {
    int32_t running  = 0;
    if (progress)
      *progress = 0;
    for (int32_t tNum = 0; tNum < env->numThreads; ++tNum)
      {
        if (env->threadRun[tNum])
          ++running;
        if (progress)
          *progress += env->threadPrg[tNum];
      }
    return running;
  }

// Simple wrapper to save without ENVIRONMENT knowing anything about MRF:
int32_t save(ENVIRONMENT * env)
  {
    int32_t result = EXIT_SUCCESS;

    if ( env->saveFile.size()
      && (!pwx_file_exists (env->saveFile.c_str())
        || pwx_file_isRW (env->saveFile.c_str())))
      {
        std::ofstream outFile (env->saveFile.c_str());

        if (outFile.is_open())
          {
            /// 1.: Save env data
            result = env->save(outFile);
            /// 2.: Save container
            if ((EXIT_SUCCESS == result) && mCont)
              {
                mCont->save (outFile);
                if (outFile.bad())
                  result = EXIT_FAILURE;
              }

            outFile.close();
          } // End of file is open
      } // End of having file name and file is readable and writable
    return result;
  }

// Determine how much time we have to wait for the current processing to achieve 10% of progress
void setSleep(float pOld, float pCur, float pMax, int32_t * toSleep, int32_t * partSleep)
{
  assert ((*toSleep > 0) && "ERROR: Never call setWait() with setSleep being zero!");

 // Without debugging on, the assert won't fail and we need the calculation here
 if (*toSleep < 1)
  *toSleep = 1;

  float pDone  = pCur - pOld; // Last amount of work done

  // However, we always assume that at least 1 unit of processing has been done
  if (pDone < 1.0)
    pDone = 1.0;

  float pRatio = pDone / static_cast<float>(*toSleep);
  float mRatio = (pMax - pCur) / pMax; // This is the ratio of the processing still to go

  // mRatio has to be clipped at 10%, we do not want less messages (until toSleep would end up <1)
  if (mRatio > 0.1) mRatio = 0.1f;

  // Now determine how many milliseconds we have to wait to get mRatio processing done:
  *toSleep = static_cast<int32_t>(pwx_round(mRatio * pMax / pRatio));

  // However, toSleep should not exceed two seconds:
  if (*toSleep > 2000) *toSleep = 2000;
  // And neither should it drop below 1 millisecond:
  if (*toSleep <    1) *toSleep =    1;

  // Now the part to sleep between event handling can be calculated:
  *partSleep = *toSleep < 20 ? 1 : *toSleep > 1000 ? 50 : *toSleep / 20;
}

void showMsg(ENVIRONMENT * env, const char * fmt, ...)
{
  env->screen->Clear();
  env->screen->Draw(sf::Sprite(env->image));

  float elapsed = env->statClock.GetElapsedTime();
  env->statTimeEla += elapsed;

  // Generate a new message if enough time has passed
  if (elapsed >= 0.25)
    {
      va_list vl;
      va_start(vl, fmt);
      pwx_vsnprintf(env->msg, 255, fmt, vl);
      va_end(vl);

      env->statClock.Reset();
    }

  /// === Text Message ===
  // Set the text for the message first, we need its size
  sf::String sMsg(env->msg, *(env->font), env->fontSize);
  sMsg.SetColor(sf::Color(0x90, 0xC0, 0xFF));
  sf::FloatRect txtRect(sMsg.GetRect());

  // Determine background box width
  uint32_t bWidth = static_cast<uint32_t>(pwx_round(txtRect.GetWidth() + 4.f));
  uint32_t bHeight= static_cast<uint32_t>(pwx_round(txtRect.GetHeight() + 4.f));

  // Max out the background box
  if (bWidth > env->statMaxWidth)
    env->statMaxWidth = bWidth;
  else
    bWidth = env->statMaxWidth;

  // Draw the text background box:
  int32_t  left   = env->scrWidth / 2 - bWidth / 2 - 2;
  sf::Vector2f msgBoxPos(static_cast<float>(left > 4 ? left - 2 : 2),
                         static_cast<float>(env->scrHeight - 2 - bHeight) );
  sf::Vector2f msgTxtPos(static_cast<float>(left > 4 ? left     : 4),
                         static_cast<float>(env->scrHeight     - bHeight) );
  sf::Image  msgBoxBg(bWidth, bHeight, sf::Color(0x60, 0x40, 0x20));
  sf::Sprite msgBox(msgBoxBg, msgBoxPos);
  env->screen->Draw(msgBox);

  // Set the text position:
  sMsg.SetPosition(msgTxtPos);

  // Draw the text:
  env->screen->Draw(sMsg);

  /// === Stats Text ===
  if (env->statTimeEla >= 1.0)
    {
      env->elaSec   = env->secondsDone;
      env->elaMin   = static_cast<int32_t>(env->elaSec / 60);
      env->elaHour  = env->elaMin / 60;
      env->elaDay   = env->elaHour / 24;
      env->elaYear  = env->elaDay / 365;
      // Now correct minutes and seconds:
      env->elaSec  -=  60 * env->elaMin;
      env->elaMin  -=  60 * env->elaHour;
      env->elaHour -=  24 * env->elaDay;
      env->elaDay  -= 365 * env->elaYear;

      // Note: For a reason I do not understand, yet, SFML does not print s², so Acc is m/ss
      pwx_snprintf(env->statMsg, 255, "[%d] %d y, % 3d d, % 2d:%02d:%02ld (Acc: %g m/ss; Mov: %g m/s)",
                   env->picNum,
                   env->elaYear, env->elaDay, env->elaHour, env->elaMin, env->elaSec,
                   env->statMaxAccel, env->statMaxMove);

      env->statTimeEla = 0.0;
    }

  // Set the text for the message first, we need its size
  sf::String sStat(env->statMsg, *(env->font), env->fontSize);
  sStat.SetColor(sf::Color(0x90, 0xC0, 0xFF));
  sf::FloatRect statRect(sStat.GetRect());

  // Determine background box width
  bWidth = static_cast<uint32_t>(pwx_round(statRect.GetWidth()  + 4.f));
  bHeight= static_cast<uint32_t>(pwx_round(statRect.GetHeight() + 4.f));

  // Max out the background box
  if (bWidth > env->statMaxWidth)
    env->statMaxWidth = bWidth;
  else
    bWidth = env->statMaxWidth;

  // Draw the text background box:
  sf::Vector2f statBoxPos(2, 2);
  sf::Vector2f statTxtPos(4, 4);
  sf::Image    statBoxBg(bWidth, bHeight, sf::Color(0x60, 0x40, 0x20));
  sf::Sprite   statBox(statBoxBg, statBoxPos);
  env->screen->Draw(statBox);

  // Set the text position:
  sStat.SetPosition(statTxtPos);

  // Draw the text:
  env->screen->Draw(sStat);

  // Finally display it:
  env->screen->Display();
}


// Returns the number of sorting threads, and adds up their unsorted count in @a unsorted
int32_t sorting(ENVIRONMENT * env, int32_t *unsorted)
  {
    int32_t sorting  = 0;

    if (unsorted)
      *unsorted = 0;

    for (int32_t tNum = 0; tNum < env->numThreads; ++tNum)
      {
        if (env->threadRun[tNum])
          ++sorting;
        // Here we ask the container interface of each thread:
        if (unsorted && mContInt[tNum])
          *unsorted += mContInt[tNum]->getUnsortedCount();
      } // End of walking through the thread list

    return sorting;
  }


// Thread Function for collision checking
void thrdCheck(void *xEnv)
  {
    threadEnv   *thrdEnv = static_cast<threadEnv *>(xEnv);
    ENVIRONMENT *env     = thrdEnv->env;
    int32_t      tNum    = thrdEnv->threadNum;

    // Kick it!
    delete thrdEnv;

    matContInt   lContInt(mCont); // Interface for lNr
    matContInt   rContInt(mCont); // Interface for rNr
    int32_t      maxUnit = lContInt.size();
    CMatter     *unit    = NULL;
    CMatter     *other   = NULL;
    bool         away    = false;

    env->lock();
    env->threadPrg[tNum] = 0;
    env->threadRun[tNum] = true;
    env->unlock();

    for (int32_t lNr = tNum; env->doWork && (lNr < maxUnit); lNr += env->numThreads)
      {
        // Get Unit to work with
        unit = lContInt[lNr];
        away = false;

        /* To not miss very large objects that might wait lurking somewhere, we have to search in
         * both directions. Once towards the center and once away from it.
         */

        // --- First loop: Search towards the center ---
        for (int32_t rNr = lNr - 1; env->doWork && !unit->destroyed() && !away && (rNr >= 0); --rNr)
          {
            // Get Unit to check against
            other = rContInt[rNr];
            double fullRange = env->universe->M2Pos // The minimum meter in positional coordinates
                             + (   env->universe->M2Pos // Now used as a multiplier, because the units
                                * (unit->getRadius() + other->getRadius()) // radii are in meters
                               );
            if (other->distDiff(unit) <= fullRange)
              {
                // They are in the same distance area, so check whether they are neighbors:
                other->lock();
                if (!other->destroyed() && !unit->destroyed())
                  other->applyCollision(env, unit);
                other->unlock();
              }
            else
              // The other items are no longer in the same distance area
              away = true;
          } // End of first loop

        // --- Second loop: Search away from center ---
        away = false;
        for (int32_t rNr = lNr + 1; env->doWork && !unit->destroyed() && !away && (rNr < maxUnit); ++rNr)
          {
            // Get Unit to check against
            other = rContInt[rNr];
            double fullRange = env->universe->M2Pos // The minimum meter in positional coordinates
                             + (   env->universe->M2Pos // Now used as a multiplier, because the units
                                * (unit->getRadius() + other->getRadius()) // radii are in meters
                               );
            if (unit->distDiff(other) <= fullRange)
              {
                // They are in the same distance area, so check whether they are neighbors:
                unit->lock();
                if (!unit->destroyed() && !other->destroyed())
                  unit->applyCollision(env, other);
                unit->unlock();
              }
            else
              // The other items are no longer in the same distance area
              away = true;
          } // End of second loop

        // Finally record our progress
        env->threadPrg[tNum]++;

        // Now if we are told to pause action, do so:
        while (env->doPause && env->doWork)
          pwx_sleep(50);
      } // End of outer loop

    // Tell env that we are finished:
    env->lock();
    env->threadRun[tNum] = false;
    env->unlock();
  }


// Thread Function for drawing only
void thrdDraw(void *xEnv)
  {
    threadEnv   *thrdEnv = static_cast<threadEnv *>(xEnv);
    ENVIRONMENT *env     = thrdEnv->env;
    int32_t      tNum    = thrdEnv->threadNum;

    // Kick it!
    delete thrdEnv;

    size_t maxX  = env->scrWidth;
    size_t maxY  = env->scrHeight;
    size_t start = tNum;
    size_t jump  = env->numThreads;

    env->lock();
    env->threadPrg[tNum] = 0;
    env->threadRun[tNum] = true;
    env->unlock();

    // Every thread draws its own pixels, there won't be any concurrency.
    for (size_t x = 0; env->doWork && (x < maxX); ++x)
      {
        for (size_t y = start; env->doWork && (y < maxY); y += jump)
          {
            uint8_t r = 0, g = 0, b = 0;
            double z = -2.0;

            // Step 1: Check Mass Map
            if (env->zMassMap[x][y].z > -0.5)
              {
                // There is a mass, get the relevant values then:
                r = env->zMassMap[x][y].r;
                g = env->zMassMap[x][y].g;
                b = env->zMassMap[x][y].b;
                z = env->zMassMap[x][y].z;
                // And reset the mass, we don't need it any more:
                env->zMassMap[x][y].invalidate();
              }

            // Step 2: Walk through the zDustMap and add up our colors
            if (env->zDustMap[x][y].z > -1.5)
              {
                sDustPixel *dust = &env->zDustMap[x][y];
                while (dust)
                  {
                    assert ( ((z < 0.) || (dust->z < z))
                          && "ERROR: zDustMap has not been correctly invalidated somewhere! Dust behind Mass detected!");
                    if ((dust->z > 0.) && ((z < 0.) || (dust->z < z)) )
                      {
                        double dustOpaq = dust->range / dust->maxRange;
                        double dustTran = 1.0 - dustOpaq;

                        if (dustTran > 0.005)
                          {
                            // The dust sphere is transparent enough, calculate the color
                            r = static_cast<uint8_t>(pwx_round( (static_cast<double>(r) * dustTran)
                                                               +(static_cast<double>(dust->r) * dustOpaq) ));
                            g = static_cast<uint8_t>(pwx_round( (static_cast<double>(g) * dustTran)
                                                               +(static_cast<double>(dust->g) * dustOpaq) ));
                            b = static_cast<uint8_t>(pwx_round( (static_cast<double>(b) * dustTran)
                                                               +(static_cast<double>(dust->b) * dustOpaq) ));
                          }
                        else
                          {
                            // The dust sphere is opaque, overwrite our colors with the dust sphere colors
                            r = dust->r;
                            g = dust->g;
                            b = dust->b;
                          }

                        // Finally invalidate this:
                        dust->invalidate();
                      } // end of having a drawable dust sphere
                    // And move on:
                    dust = dust->next;
                  } // end of looping through all dust sphere entries
                // The whole dust sphere chain is empty now:
                env->zDustMap[x][y].z = -2.0;
              } // end of having at least one dust sphere entry
            // Step 3: If we have a resulting color, draw it:
            if (r || g || b)
              {
                env->lock();
                env->image.SetPixel(x, y, sf::Color(r, g, b));
                env->unlock();
              }

            // Now if we are told to pause action, do so:
            while (env->doPause && env->doWork)
              pwx_sleep(50);
          } // end of y-loop
      } // end of x-loop

    // Tell env that we are finished:
    env->lock();
    env->threadRun[tNum] = false;
    env->unlock();
  }


// Thread Function for the initialization of the matter
void thrdInit(void *xEnv)
  {
    threadEnv   *thrdEnv = static_cast<threadEnv *>(xEnv);
    ENVIRONMENT *env     = thrdEnv->env;
    int32_t      tNum    = thrdEnv->threadNum;

    // Kick it!
    delete thrdEnv;

    // Our Interface:
    matContInt iCont(mCont);

    // These are the universal constants
    const double Base_Volume   = env->universe->UnitVolBase;
    const double Grav_Constant = env->universe->G;
    const double Mass_to_kg    = env->universe->Mass2Kg;
    const double M_to_Pos      = env->universe->M2Pos;

    /* All coordinate values refer to the projection plane (aka our window). Therefore
     * the projection plane needs to be scaled to find the maxX (Ax) and maxY (Ay)
     * of the zero z-plane, the z-coordinate of the projection plane is -1 * env->maxZ.
     * 1-point projection without any transformations reduces itself to a simple:
     * Bx = Ax * (Bz / Az) => Bx / (Bz / Az) = Ax and
     * By = Ay * (Bz / Az) => By / (Bz / Az) = Ay
     * Width Bx and By being halfWidth and halfHeight, Bz being the camDist and
     * Az being maxZ plus the camDist. All we need though, is the scaling factor to find
     * the right divisor for the x and y coordinates of our units.
    */
    double  zeroDiv = env->camDist / (env->maxZ + env->camDist);

    // The following variables are plain shortcuts to make the remaining more readable:
    bool    halfX      = env->doHalfX;
    bool    halfY      = env->doHalfY;
    bool    explode    = env->explode;
    double  maxZ       = env->maxZ * 0.4; // initially only 40%.
    double  offX       = env->offX;
    double  offY       = env->offY;
    double  offZ       = env->offZ;
    double  zoom       = env->spxZoom;
    double  smooth     = env->spxSmoo;
    double  reduct     = env->spxRedu;
    bool    shockwave  = env->shockwave;
    int32_t waves      = env->spxWave;

    // distance modifier for the non-shockwave spiral generation
    double  spiralMod  = (180.0 / M_PIl) // 2*pi*r = 360 => 2*r = 360/pi => r=180/pi ; radius a circle needs 360 units
                       * (halfX ? M_PI_2l : M_PI_4l)  // squeeze it, more if we skip x-positions
                       * (halfY ? M_PI_2l : M_PI_4l); // squeeze it, more if we skip y-positions

    // dynamic values
    /* Note on X/Y values: generally screens/windows have more width than height. If the height is set higher
     * than the width, X and Y are swapped for the calculation. This has no effect on the further positioning,
     * it is just done to ensure that the outer loop, although called X, is the larger one.
     */
    int32_t maxX      = env->scrHeight > env->scrWidth ? env->scrHeight : env->scrWidth;
    int32_t maxY      = env->scrHeight > env->scrWidth ? env->scrWidth : env->scrHeight;
    int32_t halfWidth = maxX / 2;
    int32_t halfHeight= maxY / 2;
    int32_t xStart    = maxX - (shockwave ? 1 : 0);
    int32_t xStep     = halfX ? -2 : -1;
    int32_t yStart    = maxY - (tNum * (halfY ? 2 : 1)) - (shockwave ? 1 : 0);
    int32_t yStep     = env->numThreads * (halfY ? -2 : -1);
    int32_t yStop     = shockwave ? halfHeight : 1;

    /* Maximum offsets:
     * The maximum offsets are calculated by looking at where two direct neighbors
     * would end after their reverse projection if their z position is zero.
     * Example:
     * If the camera distance is 200 and the maximum z is 1000, the ratio from
     * projection plane to zero z coordinate would be 200:1200 or 1:6.
     * The X positions 1 and 2 on the plane would become 6 and 12. The units then
     * would have a space of 6 positions between them and a maximum X offset of 3,
     * minus their radii, minus the minimum unit distance (epsilon).
     */
    double baseDist= (  pwx_pow((3. * Base_Volume) / (4. * M_PIl), 1. / 3.) // See CMatter::setRadius()
                      * M_to_Pos ) // base radius is in meters, we need position coordinates
                    + M_to_Pos; // And the minimum distance has to be protected as well
    double maxXoff = ( ( (halfX ? 2. : 1.) / zeroDiv) // The true X-coordinate if the projection coordinate is 1 (2)
                      - baseDist ) // baseDist is the minimum space one unit needs in that direction
                    / 2.; // The final offset has to be halfed, as the same offset can be applied on the neighbor
    double maxYoff = ( ( (halfY ? 2. : 1.) / zeroDiv) - baseDist ) / 2.; // Same with halfY

    // The distance depends on whether explosion mode is set or not
    double  maxDist = pwx::absDistance(0., 0., static_cast<double>(halfWidth),
                                       static_cast<double>(halfHeight)); // Top left corner to center
    double  maxMov  = 0.; // This is set to the escape velocity in explosion mode
    if (explode)
      {
        /* In explosion mode the matter is distributed in a sphere around the center of
         * our space. maxDist then is scaled down to form the maximum sphere radius, and
         * the movement of all units is set to a fraction of the needed escape velocity
         * according to the ratio of their position to the sphere radius.
         * Step one: Set maxDist to a radius that produces a nice visible sphere
         * Step two: Calculate the escape velocity according to the now set maxDist.
         *
         * Notes:
         * - The sphere shall start with 5% of the projection plane, the lower this value,
         *   the higher the initial movement;
         *   maxX is always the larger value and can be used with maxX / 2 / 20 = maxX / 40
         * - The escape velocity Ve has to be calculated for step two:
         *   Ve = sqr( 2G * M / R )
         *   with G being the constant for proportionality (gravitational constant),
         *   M being the total mass, and R the total radius.
        */
        double M = static_cast<double>((maxX * maxY) - 1) * Mass_to_kg;
        M *= (halfX ? 0.5 : 1.0) * (halfY ? 0.5 : 1.0);
        // Note: We need the true kg mass to get the true meters, or we can't scale correctly down

        maxDist = (maxX / 40.) / zeroDiv; // This is the distance in positional coordinates

        maxMov  = pwx_pow((2. * Grav_Constant * M) / (maxDist / M_to_Pos), 1. / 2.); // This is the resulting m/s
      }
    else
      // Otherwise we have to adapt maxZ as it is relevant in none-explosion mode:
      maxZ *= ((halfX ? 1.0 : 1.5) + (halfY ? 1.0 : 1.5) + (shockwave ? 1.5 : 1.0)) / 3.0;

    double  circleMod  = maxDist / ((maxX + 1) * (maxY + 1));

    env->lock();
    env->threadPrg[tNum] = 0;
    env->threadRun[tNum] = true;
    env->unlock();

    // Again x is not necessarily screen width, it is the larger one, y is the smaller one
    for (int32_t x = xStart ; env->doWork && (x > 0) ; x += xStep)
      {
        // if we do halfY we jump every second outer loop value
        int32_t yJump = halfY ? x % 2 : 0;
        for (int32_t y = yStart - yJump; env->doWork && (y > (yStop - yJump)) ; y += yStep)
          {
            try
              {
                CMatter *mUnit   = NULL;
                double circlePos = y + (maxY * x); // The position of the current x and y made linear

                /* There are two possibilities:
                 * a) in shockwave mode we distribute the matter by x and y, mirroring y at the middle
                 * b) in explode and normal mode we circle from the outside towards the center
                */
                if (shockwave)
                  {
                    // Shockwave mode is completely different

                    /* --- Top Unit --- */
                    double xPos  = x - halfWidth;
                    double yPos  = y - halfHeight;
                    double xyDist= pwx::absDistance(xPos, yPos, 0., 0.);
                    RNG.lock();
                    double zPos  = RNG.simplex2D(xyDist, xyDist, zoom) * maxZ;
                    double modX  = maxXoff * RNG.simplex3D(x  + offX, yPos, zPos + offZ, zoom);
                    double modY  = maxYoff * RNG.simplex3D(xPos, y  + offY, zPos + offZ, zoom);
                    RNG.unlock();
                    mUnit = new CMatter(env, (modX + xPos) / zeroDiv, (modY + yPos) / zeroDiv, zPos, zPos);
                    iCont.add_sorted(mUnit);
                    ++env->threadPrg[tNum];
                    mUnit = NULL;

                    /* --- Bottom (mirrored) Unit --- */
                    xPos  = halfWidth  - x - 1;
                    yPos  = halfHeight - y - (2 - yJump);
                    xyDist= pwx::absDistance(xPos, yPos, 0., 0.);
                    RNG.lock();
                    zPos  = RNG.simplex2D(xyDist, xyDist, zoom) * maxZ;
                    modX  = maxXoff * RNG.simplex3D(x  + offX, circlePos, zPos + offZ, zoom, smooth, reduct, waves);
                    modY  = maxYoff * RNG.simplex3D(circlePos, y  + offY, zPos + offZ, zoom, smooth, reduct, waves);
                    RNG.unlock();
                    mUnit = new CMatter(env, (modX + xPos) / zeroDiv, (modY + yPos) / zeroDiv, zPos, zPos);
                    iCont.add_sorted(mUnit);
                    ++env->threadPrg[tNum];
                    mUnit = NULL;
                  }
                else
                  {
                    // Normal and explode mode share a lot of their initial calculations
                    double distMod   = circlePos * circleMod;

                    // We need to control the distance, it has to sink slower if we are further away
                    double distFlow  = 2.25 - (1.25 * (distMod / maxDist));

                    double distance  = maxDist - (distMod / distFlow) + M_to_Pos;
                    // The alpha (x-y) angle is generally it's position, but in non-explosion mode we need more steps per
                    // circle the further away from the center we are.
                    double alphaMod  = explode ? 1.0 : distance / spiralMod;
                    // Apart from the circlePos, simplex noise is used to generate an offset to alpha for more "chaos"
                    RNG.lock();
                    double alpha     = pwx::MathHelpers::getNormalizedDegree(
                                       (circlePos / alphaMod)
                                     + (90.0 * RNG.simplex3D(x + offX, y + offY, circlePos + offZ,
                                                             zoom, smooth, reduct, waves)));
                    RNG.unlock();
                    assert((distance > M_to_Pos) && "ERROR: distance is not larger than M_to_Pos!");
                    assert((distance <= (maxDist + 1.0)) && "ERROR: distance is not smaller than maxDist + UMD!");
                    /* Sphere Coordinates:
                     * X = cos(alpha) * sin(beta) * distance
                     * Y = sin(alpha) * sin(beta) * distance
                     * Z =              cos(beta) * distance
                     * In non-explosion mode Z is created by simplex noise, in explosion mode we'll have a beta.
                     */
                    double sinA, cosA; // Those are needed for the x-/y-mod according to alpha
                    SCT.lock();
                    SCT.sincos(alpha, sinA, cosA);
                    SCT.unlock();
                    double xPos = distance * cosA; // Will be modified by beta angle or an X-modifier
                    double yPos = distance * sinA; // Will be modified by beta angle or an Y-modifier
                    if (explode)
                      {
                        // The beta (x-z) angle is generated by simplex noise. Unfortunately simplex noise tends to generate
                        // more values around 0.0 than towards the edges -1.0/+1.0. We therefore use three circles to produce
                        // the result
                        RNG.lock();
                        double beta  = pwx::MathHelpers::getNormalizedDegree(/* alpha + */ (
                                       1080.0 * RNG.simplex3D(xPos + offX, yPos + offY, circlePos + offZ, zoom)));
                        RNG.unlock();
                        double sinB, cosB; // Those are needed for the x-/y-mod according to beta
                        SCT.lock();
                        SCT.sincos(beta,  sinB, cosB);
                        SCT.unlock();
                        mUnit = new CMatter(env, xPos * sinB, yPos * sinB, distance * cosB,
                                            (maxMov + (maxMov * (distance / maxDist))) / 2.);
                      }
                    else
                      {
                        // The initial z position is calculated using Noise, which is a value between -1.0 and 1.0.
                        RNG.lock();
                        double z = RNG.simplex3D(x + offX, y + offY, circlePos, zoom) * maxZ;
                        RNG.unlock();

                        // The modification of x and y are simplex noise values. To not let matter units start too close
                        // to each other, it is modified.
                        RNG.lock();
                        double modX = maxXoff * RNG.simplex3D(x + offX, circleMod, z + offZ, zoom);
                        double modY = maxYoff * RNG.simplex3D(circleMod, y + offY, z + offZ, zoom);
                        RNG.unlock();
                        mUnit = new CMatter(env, (modX + xPos) / zeroDiv, (modY + yPos) / zeroDiv, z, 0.);
                      }

                    // Now add our result. If we had none, an exception would have been thrown.
                    iCont.add_sorted(mUnit); // Add sorted according to distance from center

                    // Finally record our progress
                    ++env->threadPrg[tNum];
                  } // end of non-normal mode
              }
            catch(pwx::Exception &e)
              {
                cerr << "pwx Exception: " << e.name() << " occured at\n";
                cerr << e.where() << endl;
                cerr << "Reason: " << e.what() << " ; " << e.desc() << endl;
                cerr << "Trace:\n" << e.trace() << endl;
              }
            catch (std::bad_alloc &e)
              {
                cout << "ERROR: Failed to initialize matter unit " << env->threadPrg[tNum];
                cout << " in Thread " << tNum << " [" << e.what() << "]" << endl;
              }

            // Now if we are told to pause action, do so:
            while (env->doPause && env->doWork)
              pwx_sleep(50);
          } // end of y-loop
      } // end of x-loop

    // Tell env that we are finished:
    env->lock();
    env->threadRun[tNum] = false;
    env->unlock();
  }


// Thread Function for impulse application
void thrdImpu(void *xEnv)
  {
    threadEnv   *thrdEnv = static_cast<threadEnv *>(xEnv);
    ENVIRONMENT *env     = thrdEnv->env;
    int32_t      tNum    = thrdEnv->threadNum;

    // Kick it!
    delete thrdEnv;

    matContInt   lContInt(mCont); // Interface for lNr
    int32_t      maxUnit = lContInt.size();
    int32_t      portion = static_cast<int32_t>(maxUnit / env->numThreads); // How many items we draw
    int32_t      start   = portion * tNum; // The first number to fetch
    int32_t      stop    = tNum == (env->numThreads - 1) ? maxUnit : portion * (tNum + 1); // the last number to fetch
    CMatter     *unit    = NULL;

    env->lock();
    env->threadPrg[tNum] = 0;
    env->threadRun[tNum] = true;
    env->unlock();

    for (int32_t lNr = start; env->doWork && (lNr < stop); ++lNr)
      {
        // Get Unit to work with
        unit = lContInt[lNr];

        // Apply impulses
        if (env->doWork && !unit->destroyed())
          {
            unit->applyImpulses(env);
            // Record our progress
            env->threadPrg[tNum]++;
          }


        // Now if we are told to pause action, do so:
        while (env->doPause && env->doWork)
          pwx_sleep(50);
      } // End of loop

    // Tell env that we are finished:
    env->lock();
    env->threadRun[tNum] = false;
    env->unlock();
  }


// Thread Function to load data from a file
void thrdLoad(void *xEnv)
  {
    threadEnv   *thrdEnv = static_cast<threadEnv *>(xEnv);
    ENVIRONMENT *env     = thrdEnv->env;
    int32_t      tNum    = thrdEnv->threadNum;

    assert((0 == tNum) && "ERROR: Loading must be done with exactly one thread!");

    // Kick it!
    delete thrdEnv;

    env->lock();
    env->threadPrg[tNum] = 0;
    env->threadRun[tNum] = true;
    env->unlock();

    // TMemRing can load data on its own and throws on error
    try
      {
        std::ifstream inFile;
        inFile.open(env->saveFile.c_str());

        if (inFile.is_open())
          {
            PWX_TRY(mCont->load(inFile, true))
            PWX_THROW_FURTHER
            inFile.close();
          }

        // Before we are finished, we have to determine the minimum Z-value if it is needed
        if (env->doDynamic)
          {
            matContInt   lContInt(mCont); // Interface for lNr
            int32_t      maxUnit = lContInt.size();
            CMatter     *unit    = NULL;
            for (int32_t lNr = 0; env->doWork && (lNr < maxUnit); ++lNr)
              {
                // Get Unit to work with
                unit = lContInt[lNr];

                if (unit->getPosZ() < env->minZ)
                  env->minZ = unit->getPosZ();
              } // End of reading minZ
          } // end of setting dynamic values
      } // end of loading data try
    catch(pwx::Exception &e)
      {
        cerr << "pwx exception while loading data " << e.name() << " occured at\n";
        cerr << e.where() << endl;
        cerr << "Reason: " << e.what() << " ; " << e.desc() << endl;
        cerr << "Trace:\n" << e.trace() << endl;
        env->isLoaded = false; // we need to initialize
        mCont->clear();
      }
    catch (std::exception &e)
      {
        cerr << "std exception while loading data: " << e.what() << endl;
        env->isLoaded = false; // we need to initialize
        mCont->clear();
      }

    // Tell env that we are finished:
    env->lock();
    env->threadRun[tNum] = false;
    env->unlock();
  }


// Thread Function for gravitation calculation
void thrdGrav(void *xEnv)
  {
    threadEnv   *thrdEnv = static_cast<threadEnv *>(xEnv);
    ENVIRONMENT *env     = thrdEnv->env;
    int32_t      tNum    = thrdEnv->threadNum;

    // Kick it!
    delete thrdEnv;

    matContInt   lContInt(mCont); // Interface for lNr
    matContInt   rContInt(mCont); // Interface for rNr
    int32_t      maxUnit = lContInt.size();
    CMatter     *unit    = NULL;
    CMatter     *other   = NULL;

    env->lock();
    env->threadPrg[tNum] = 0;
    env->threadRun[tNum] = true;
    env->unlock();


    /* --- clean the impulse values --- */
    for (int32_t lNr = tNum; env->doWork && (lNr < maxUnit); lNr += env->numThreads)
      {
        // Get Unit to work with...
        unit = lContInt[lNr];

        // ... and reset it
        unit->resetImpulse();

        // Now if we are told to pause action, do so:
        while (env->doPause && env->doWork)
          pwx_sleep(50);
      } // End of outer loop

    /* --- The real calculating loop --- */
    for (int32_t lNr = tNum; env->doWork && (lNr < maxUnit); lNr += env->numThreads)
      {
        // Get Unit to work with
        unit = lContInt[lNr];

        /// --- Step 1 : Apply gravitation via inner loop
        for (int32_t rNr = lNr + 1; env->doWork && !unit->destroyed() && (rNr < maxUnit); ++rNr)
          {
            // Get Unit to check against other
            other = rContInt[rNr];

            // Now check against the collision:
            if (!other->destroyed())
              // Note: the unit will do a lock just for the writing of the impulse.
              unit->applyGravitation(env, other);
          }

        /// --- Step 2 : Record our progress
        env->threadPrg[tNum]++;

        // Now if we are told to pause action, do so:
        while (env->doPause && env->doWork)
          pwx_sleep(50);
      } // End of outer loop

    // Tell env that we are finished:
    env->lock();
    env->threadRun[tNum] = false;
    env->unlock();
  }


// Thread Function for projecting the units on the projection plane
void thrdProj(void *xEnv)
  {
    threadEnv   *thrdEnv = static_cast<threadEnv *>(xEnv);
    ENVIRONMENT *env     = thrdEnv->env;
    int32_t      tNum    = thrdEnv->threadNum;

    // Kick it!
    delete thrdEnv;

    matContInt   lContInt(mCont);
    int32_t      maxUnit  = lContInt.size();
    CMatter     *unit     = NULL;
    int32_t      portion  = static_cast<int32_t>(maxUnit / env->numThreads); // How many items we draw
    int32_t      start    = portion * tNum; // The first number to fetch
    int32_t      stop     = tNum == (env->numThreads - 1) ? maxUnit : portion * (tNum + 1); // the last number to fetch

    env->lock();
    env->threadPrg[tNum] = 0;
    env->threadRun[tNum] = true;
    env->unlock();

    for (int32_t lNr = start; env->doWork && (lNr < stop); ++lNr)
      {
        // Get Unit to work with
        unit = lContInt[lNr];

        // Draw the unit
        if (env->doWork && !unit->gone(env))
          {
            if (EXIT_FAILURE == unit->project(env))
              {
                // This means we have had an exception (probably bad_alloc) and need to exit.
                env->lock();
                env->doWork = false; // this'll end all threads and the program itself.
                env->unlock();
              }
            // Record our progress
            env->threadPrg[tNum]++;
          }

        // Now if we are told to pause action, do so:
        while (env->doPause && env->doWork)
          pwx_sleep(50);
      } // End of loop

    // Tell env that we are finished:
    env->lock();
    env->threadRun[tNum] = false;
    env->unlock();
  }


// Thread Function for Movement
void thrdMove(void *xEnv)
  {
    threadEnv   *thrdEnv = static_cast<threadEnv *>(xEnv);
    ENVIRONMENT *env     = thrdEnv->env;
    int32_t      tNum    = thrdEnv->threadNum;

    // Kick it!
    delete thrdEnv;

    matContInt   lContInt(mCont); // Interface for lNr
    int32_t      maxUnit = lContInt.size();
    int32_t      portion = static_cast<int32_t>(maxUnit / env->numThreads); // How many items we draw
    int32_t      start   = portion * tNum; // The first number to fetch
    int32_t      stop    = tNum == (env->numThreads - 1) ? maxUnit : portion * (tNum + 1); // the last number to fetch
    CMatter     *unit    = NULL;

    env->lock();
    env->threadPrg[tNum] = 0;
    env->threadRun[tNum] = true;
    env->unlock();

    for (int32_t lNr = start; env->doWork && (lNr < stop); ++lNr)
      {
        // Get Unit to work with
        unit = lContInt[lNr];

        // Draw the unit
        if (env->doWork && !unit->destroyed())
          {
            unit->applyMovement(env);
            // Record our progress
            env->threadPrg[tNum]++;
          }

        // Now if we are told to pause action, do so:
        while (env->doPause && env->doWork)
          pwx_sleep(50);
      } // End of loop

    // Tell env that we are finished:
    env->lock();
    env->threadRun[tNum] = false;
    env->unlock();
  }


// Thread Function to sort according to center distance
void thrdSort(void *xEnv)
  {
    threadEnv   *thrdEnv = static_cast<threadEnv *>(xEnv);
    ENVIRONMENT *env     = thrdEnv->env;
    int32_t      tNum    = thrdEnv->threadNum;

    // Kick it!
    delete thrdEnv;

    matContInt contInt(mCont); // Interface for sorting
    mContInt[tNum] = &contInt; // waitSort needs to ask directly

    env->lock();
    env->threadPrg[tNum] = 0;
    env->threadRun[tNum] = true;
    env->unlock();

    bool needSort = true;

    while (needSort && env->doWork && contInt.size())
      {
        contInt.sort_once();
        needSort = false;

        // Now if we are told to pause action, do so:
        while (env->doPause && env->doWork)
          {
            pwx_sleep(50);
            // As the sorting got interrupted, we have to start over:
            needSort = true;
          }
      }

    // Tell env that we are finished:
    env->lock();
    env->threadRun[tNum] = false;
    mContInt[tNum] = NULL;
    env->unlock();
  }


int32_t workLoop(ENVIRONMENT * env)
  {
    int32_t    result       = EXIT_SUCCESS;
    char       picName[256] = "";
    bool       doCollision  = env->secondsDone ? true : false; // There is no collision check in the first second!
    int64_t    secInCycle   = 0; // The second in this cycle the real second belongs to
    matContInt iCont(mCont);

    // If this is a fresh start, we need a round of drawing, saving and gravitation calculation before the workLoop starts
    if (!env->isLoaded)
      {
        // 1.: Save the initial picture
        pwx_snprintf(picName, 255, env->outFileFmt.c_str(), ++env->picNum);
        showMsg(env, "saving picture %s ...", picName);
        env->image.SaveToFile(std::string(picName));
        doEvents(env);

        // 2.: Create the initial save file
        if (env->doWork && env->saveFile.size())
          {
            // Note: This means we save whenever a new gravitation calculation is needed or a minute is done
            showMsg(env, "Saving %d items...", iCont.size());
            save(env);
            doEvents(env);
          }
      } // End of initial preparations

    // 3.: Initial Gravitation
    // Step 3 is needed until env->initFinished is true, which is saved and loaded and might be false after loading
    // If someone exited the program before finishing the first round.
    if (!env->initFinished && env->doWork)
      {
        env->startThreads(&thrdGrav);
        waitThrd(env, "Gravitation");
        env->clearThreads();
        env->initFinished = true;
      }


    while (env->doWork && (EXIT_SUCCESS == result) && env->screen->IsOpened() && (mCont->size() > 1))
      {
        bool doGrav = env->needGravCalc();

        // If we shall save, we save now:
        if (env->saveFile.size() && (doGrav || !(env->secondsDone % 60)))
          {
            // Note: This means we save whenever a new gravitation calculation is needed or a minute is done
            showMsg(env, "Saving %d items...", iCont.size());
            save(env);
          }


        /* ==== Workflow ====
         * The work loop, no matter what the time scaling is, has to calculate each second.
         * Each second consists of the following steps:
         *  1. Check whether a new gravitation calculation is needed. If it is, continue with 2., otherwise with 3.
         *  2. Calculate gravitation of each unit.
         *  3. Apply the impulses on each unit to generate its current acceleration.
         *  4. Advance one second and set the relevant frame values
         *  5. Move the units
         *  6. Sort the units
         *  7. Check for collisions
         *  8. Project all units relative to the projection plane
         *  9. Trace all pixels from camera to source
         * 10. Update screen
         * 11. Save the image
         * 12. Advance to the next frame and check whether it needs the same second. If it does, continue with 5.
         * 13. Eventually clean up the units.
        **/

        /// === Step 1 ===
        /// Check whether a new gravitation calculation is needed. If it is, continue with 2., otherwise with 3.
        if (doGrav)
          {
            /// === Step 2 ===
            /// Caclulate gravitation for each unit
            env->startThreads(&thrdGrav);
            waitThrd(env, "Gravitation");
            env->clearThreads();
          }

        /// === Step 3 ===
        /// Apply the impulses on each unit to generate its current acceleration.
        if (env->doWork)
          {
            env->statMaxAccel = 0.;
            // Create and start Threads for impulses:
            env->startThreads(&thrdImpu);
            waitThrd(env, "Impulses");
            env->clearThreads();
          }

        /// === Step 4 ===
        /// Advance one second and set current frame values:
        if (env->currFrame >= env->fps)
          // Every cycle starts with frame 0
          env->currFrame = 0;
        secInCycle = ++env->secondsDone % env->secPerCycle;

        /// === Steps 5 to 12 are looped as long as a frame needs this very second to be drawn ===
        bool doMove = true;
        while (env->doWork && doMove)
          {
            doMove = false; // will be set to true below if this frame is drawn and the next has to be drawn, too

            /// === Step 5 ===
            /// Move the units
            if (env->doWork)
              {
                if (env->doDynamic)
                  env->minZ       = env->maxZ;
                env->statMaxMove  = 0.;
                env->startThreads(&thrdMove);
                waitThrd(env, "Moving");
                env->clearThreads();
                // env needs to know the maximum movement now:
                env->statCurrMove += env->statMaxMove;
              }

            /// === Step 6 ===
            /// Sort the units
            if (env->doWork)
              {
                env->startThreads(&thrdSort);
                // We can _not_ use waitThrd() here, because the counting is done backwards!
                waitSort(env);
                env->clearThreads();
              }

            /// === Step 7 ===
            /// Check for collisions
            if (env->doWork && doCollision)
              {
                env->startThreads(&thrdCheck);
                waitThrd(env, "Collisions");
                env->clearThreads();
              }

            /// Steps 8 to 13 are skipped unless the current frame needs the second the cycle is in
            if (env->doWork && (env->secPerFrame[env->currFrame] == secInCycle))
              {
                /// === Step 8 ===
                /// Project all units relative to the projection plane
                if (env->doWork)
                  {
                    // Clear image first
                    env->image.Create(env->scrWidth, env->scrHeight);

                    // Now project all masses with their dust spheres
                    env->setDynamicZ();
                    env->startThreads(&thrdProj);
                    waitThrd(env, "Projecting...");
                    env->clearThreads();
                  }

                /// === Step 9 ===
                /// Trace image pixels from camera to source
                if (env->doWork)
                  {
                    env->startThreads(&thrdDraw);
                    waitThrd(env, "Tracing...");
                    env->clearThreads();
                  }

                /// === Step 10 ===
                /// Update screen
                if (env->doWork)
                  {
                    env->screen->Clear();
                    env->screen->Draw(sf::Sprite(env->image));
                    env->screen->Display();
                    // Process events to get out early if wanted:
                    doEvents(env);
                  }

                /// === Step 11 ===
                /// Save the image
                if (env->doWork)
                  {
                    // save the image
                    pwx_snprintf(picName, 255, env->outFileFmt.c_str(), ++env->picNum);
                    // Only show message if doWork is true, or the screen might have gone away!
                    showMsg(env, "saving picture %s ...", picName);
                    env->image.SaveToFile(std::string(picName));
                  }

                /// === Step 12 ===
                /// Check whether the next frame needs to be drawn, too
                ++env->currFrame;
                if ((env->currFrame < env->fps) && (env->secPerFrame[env->currFrame] == secInCycle))
                  doMove = true;

                /// === Step 13 ===
                /// Clean up the units
                if (env->doWork)
                  {
                    int32_t    nr = mCont->size();
                    env->statDone = 0;

                    // While cleaning backwards we reduce the number of iterations necessary to renumber the
                    // matter units. That's how TMemRing works...
                    while (env->doWork && nr)
                      {
                        // Note: As we clean backwards, nr would point to the next, already checked,
                        //       Unit after a deletion. Therefore we can decrease nr on each iteration.
                        --nr;
                        if (!(nr % 100))
                          {
                            env->statDone = nr;

                            showMsg(env, "Cleaning : % 7d / % 7d", nr, mCont->size());
                          }
                        if (mCont->getData(nr)->gone(env))
                          // Deleting can not be done by the interface
                          mCont->delItem(nr);

                        if (!(nr % 10))
                          doEvents(env);
                      }
                  } /// End of step 13
              } /// End of optional drawing steps 8 to 13
          } /// End of movement loop steps 5 to 13


        // Enable collision checking
        if (env->doWork && !doCollision)
          {
            doCollision = true;
            // Now check collisions at the end of second 1:
            env->startThreads(&thrdCheck);
            waitThrd(env, "Collisions");
            env->clearThreads();
          }
      } // end main loop

    return (result);
  }

// Simple method that waits for the loader thread to finish constantly displaying progress and handling events
// fmt must be a text with ONLY ONE %s for the progress numbers in it!
void waitLoad(ENVIRONMENT *env, const char *fmt, int32_t maxNr)
  {
    int32_t maxUnit     = maxNr ? maxNr : env->scrHeight * env->scrWidth;
    int32_t running     = env->numThreads;
    float   prgCur      = 0.;
    float   prgMax      = static_cast<float>(maxUnit);
    float   prgOld      = 0.;
    int32_t fullSleep   = 1;
    int32_t partSleep   = 1;
    int32_t slept       = 1;
    char    newFmt[128] = "";

    // Sleep at least one ms
    pwx_sleep(1);

    while (env->doWork && running)
      {
        // first count up what has to be done:
        env->statDone = mCont->size();
        prgOld        = prgCur;
        prgCur        = static_cast<float>(env->statDone);
        fullSleep     = slept ? slept : 1; // What we actually did
        slept         = 1;
        running       = env->threadRun[0] ? 1 : 0;

        if (running)
          {
            // Determine how much time we can waste
            setSleep(prgOld, prgCur, prgMax, &fullSleep, &partSleep);

            // Apply progress string
            pwx_snprintf(newFmt, 127, fmt, env->prgFmt);

            // Now show the message
            showMsg(env, newFmt, 100.0 * prgCur / prgMax, env->statDone, maxUnit);

            // Third waste time fetching events
            doEvents(env);
            while (env->doWork && (slept < fullSleep) && running)
              {
                pwx_sleep(partSleep);
                slept += partSleep;
                doEvents(env);
                // Look again at the thread, maybe it has stopped?
                running = env->threadRun[0] ? 1 : 0;
              } // End of handling events and let some time pass
          } // End of thread still running
      } // end of "we are waiting"
  }

// Special variant of waitThrd that does not do any ETA calculations, but shows the min item nr, max item nr
// and sum of currently unsorted items.
void waitSort(ENVIRONMENT *env)
  {
    int32_t maxUnit   = mCont->size();
    float   prgCur    = 0.;
    float   prgMax    = static_cast<float>(maxUnit);
    float   prgOld    = 0.;
    int32_t running   = 0;
    int32_t unsorted  = 0;
    int32_t slept     = 1;
    int32_t fullSleep = 1;
    int32_t partSleep = 1;

    while (env->doWork && (running = sorting(env, &unsorted)) )
      {
        // Count up what has to be done; be aware that sorting() results in a backward count
        prgOld    = prgMax - prgCur;
        prgCur    = prgMax - unsorted;
        fullSleep = slept ? slept : 1;
        slept     = 0;

        // Determine how long to sleep
        setSleep(prgOld, prgCur, prgMax, &fullSleep, &partSleep);

        // Apply number of threads and progress string
        char newFmt[128] = "";
        pwx_snprintf(newFmt, 127, "[% 2d] %-35s", running, env->sortFmt);
        // Now show the message
        showMsg(env, newFmt, unsorted, maxUnit);

        // If the sorting is paused, the threads have to be interrupted first:
        if (env->doPause || !env->doWork)
          {
            // Here we have to interrupt all threads, they'll restart their
            // sorting once the pause mode is over, or quit if doWork is false
            for (int32_t tNum = 0; tNum < env->numThreads; ++tNum)
              {
                if (mContInt[tNum])
                  mContInt[tNum]->interruptSorting();
              } // End of notifying interfaces
          }

        // Now get events, and waste time or do pause
        doEvents(env);
        while (env->doWork && (slept < fullSleep) && sorting(env, NULL))
          {
            pwx_sleep(partSleep);
            slept += partSleep;
            doEvents(env);
            // Handle pause mode
            while (env->doPause && env->doWork)
              {
                pwx_sleep(50);
                slept += 50;
                doEvents(env);
              }
          }
      } // end of "we are waiting"
  }

// Simple method that waits for all threads to finish constantly displaying progress and handling events
// msg Should have a maximum of 13 chars
void waitThrd(ENVIRONMENT *env, const char *msg, int32_t maxNr)
  {
    int32_t maxUnit   = maxNr ? maxNr : mCont->size();
    float   prgCur    = 0.;
    float   prgMax    = static_cast<float>(maxUnit);
    float   prgOld    = 0.;
    int32_t fullSleep = 1;
    int32_t partSleep = 1;
    int32_t slept     = 1;
    int32_t currRun   = 0;

    assert((strlen(msg) < 14) && "ERROR: waitThrd should not be called with a message of more than 10 chars!");

    // Sleep at least one ms
    pwx_sleep(1);

    while (env->doWork && (currRun = running(env, &env->statDone)))
      {
        prgOld    = prgCur;
        prgCur    = static_cast<float>(env->statDone);
        fullSleep = slept ? slept : 1;
        slept     = 0;

        // Determine how long to sleep
        setSleep(prgOld, prgCur, prgMax, &fullSleep, &partSleep);

        env->statDone    = static_cast<int32_t>(prgCur);
        // Apply number of threads and progress string
        char newFmt[128] = "";
        pwx_snprintf(newFmt, 127, "[% 2d] %-13s: %16s", currRun, msg, env->prgFmt);
        // Now show the message
        showMsg(env, newFmt, 100.0 * prgCur / prgMax, env->statDone, maxUnit);

        // Third waste mSec ms fetching events or do pausing
        doEvents(env);
        while (env->doWork && (slept < fullSleep) && running(env, NULL))
          {
            pwx_sleep(partSleep);
            slept += partSleep;
            doEvents(env);
            // Handle pause mode
            while (env->doPause && env->doWork)
              {
                pwx_sleep(50);
                slept += 50;
                doEvents(env);
              }
          }
      } // end of "we are waiting"
  }
