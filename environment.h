#ifndef PWX_GRAVMAT_ENVIRONMENT_H
#define PWX_GRAVMAT_ENVIRONMENT_H

#include <string>
using std::string;

#include "main.h"

// Universe constants (to be instantiated on initialization)
#include "universe.h"

// CColorMap is forward to keep SFML out
class CColorMap;

// Same with CMatter:
class CMatter;

// The Pixel Info classes have their implementation in the header and therefore should be forwarded anyway:
struct sDustPixel;
struct sMassPixel;

/** @struct ENVIRONMENT
  * @brief struct to keep general values together that are used in the programs functions
**/
struct ENVIRONMENT: public pwx::Lockable, private pwx::Uncopyable
{
  double            camDist;     //!< The distance of the camera (eye) to the projection plane (window) according to fov
  CColorMap        *colorMap;    //!< Map to generate colors from
  int32_t           currFrame;   //!< Which frame of a cycle is currently the next to draw
  double            cyclPerFrm;  //!< How man cycles (fraction) are done per frame, used for explosion ring size increase
  bool              doDynamic;   //!< If set to true (--dyncam), the camera is moved towards the nearest unit.
  bool              doHalfX;     //!< Set to true by --halfX and skips every second X pixel
  bool              doHalfY;     //!< Set to true by --halfY and skips every second Y pixel
  bool              doPause;     //!< Toggled with the pause key while running
  bool              doVideo;     //!< Set to true if the output is a video
  bool              doWork;      //!< is set to false if no work is to be done
  bool              drawDust;    //!< set to false for the first, and to true for the second drawing run
  double            dynMaxZ;     //!< the used maxZ for projection, equals maxZ unless --dyncam is set
  int32_t           elaDay;      //!< How many days have been processed
  int32_t           elaHour;     //!< How many hours have been processed
  int32_t           elaMin;      //!< How many minutes have been processed
  int64_t           elaSec;      //!< How many seconds have been processed
  int32_t           elaYear;     //!< How many years have been processed
  bool              explode;     //!< Use explosion algorithm to distribute matter units
  int32_t           fileVersion; //!< This is set in the ctor. In later versions, it'll help loading old data.
  sf::Font         *font;        //!< The font to be used for the display
  float             fontSize;    //!< Base size of the font, used to determine the text box sizes
  double            fov;         //!< Field of vision, defaults to 90.0 degrees
  int32_t           fps;         //!< Set FPS, argument --fps to override (default 50)
  double            halfHeight;  //!< Half the screen height for perspective calculation as double
  double            halfWidth;   //!< Half the screen width for perspective calculation as double
  bool              hasUserTime; //!< Set to true if the timescale or one of their aliases is used, so the default isn't applied
  sf::Image         image;       //!< The image to be rendered
  bool              initFinished;//!< Set to true once the first gravitational calculation is done
  bool              isLoaded;    //!< Set to true if we successfully loaded data from a file
  double            minZ;        //!< set while moving it is used to move the projection plane if --dyncam is used
  double            maxZ;        //!< used for perspective calculation
  char              msg[256];    //!< message to display at the bottom of the screen
  int32_t           numThreads;  //!< Number of threads to spawn for the workloop calculations. Default is 8
  double            offX;        //!< x-offset
  double            offY;        //!< y-offset
  double            offZ;        //!< z-offset
  ::std::string     outFileFmt;  //!< The format string for the output files.
  int32_t           picNum;      //!< Number of the picture currently displayed on screen
  char              prgFmt[25];  //!< Dynamic progress format string set up according to the maximum number of units
  ::std::string     saveFile;    //!< Name of the (optional) save file to load from and save into.
  sf::RenderWindow *screen;      //!< the screen to be created
  int32_t           scrHeight;   //!< height of the screen
  int32_t           scrWidth;    //!< screen width
  int64_t           secondsDone; //!< The number of seconds already calculated in the current frame
  int64_t           secPerCycle; //!< How many seconds are calculated for one cycle (or all frames)
  int64_t          *secPerFrame; //!< Dynamic array for the number of frames to catch rounding errors
  double            secPFmod;    //!< Used to modify impulse and movement for low secPerCycle scenarios
  int32_t           seed;        //!< If set by command line argument, sets a new seed for RNG
  bool              shockwave;   //!< Use shock wave algorithm to initialize matter units
  char              sortFmt[64]; //!< Special format string for the (obscure) sorting status message
  double            spxRedu;     //!< Simplex Reduction Value, defaults to 1.0
  double            spxSmoo;     //!< Simplex Smooth Value, defaults to 1.0
  int32_t           spxWave;     //!< Simplex Waves Value, defaults to 1
  double            spxZoom;     //!< Simplex Zoom, defaults to 4.0
  sf::Clock         statClock;   //!< used to determine the time elapsed for the message line (bottom)
  double            statCurrMove;//!< Currently sum of maximum movements. Used to know when a new grav calc is needed
  int32_t           statDone;    //!< Record Progress
  double            statMaxAccel;//!< Maximum observed acceleration in m/s²
  double            statMaxMove; //!< Maximum observed movement in m/s
  uint32_t          statMaxWidth;//!< Width of the status lines, will be maxed out for a "quieter" display
  float             statTimeEla; //!< Used to only update the stat lines once (top) per second
  char              statMsg[256];//!< Text for the stats in the top left corner
  sf::Thread      **thread;      //!< The threads themselves.
  volatile int32_t *threadPrg;   //!< Threads write their progress in this
  volatile bool    *threadRun;   //!< Threads set it to true when they start and to false when they end
  UNIVERSE         *universe;    //!< Collection of constants describing this very universe for further physics calculations
  sDustPixel      **zDustMap;    //!< Map over pixels which represent a dust sphere point
  sMassPixel      **zMassMap;    //!< Map over pixels which represent a mass point

  /* --- non-struct methods --- */
  const char * getVersion() const;

private:
  string version;

public:
  /* Default constructor */
  explicit ENVIRONMENT(int32_t aSeed = 0);

  // The dtor is outline
  ~ENVIRONMENT();

  // Helper to clear all threads:
  void clearThreads();
  // Helper to initialize the zMaps
  int32_t initZMaps();
  // Helper to load working state from saveFile:
  int32_t load();
  // Return true if the current movement requires a new calculation of the gravitation
  bool needGravCalc();
  // Project a dust sphere pixel unto the zDustMap
  int32_t projectDust(int32_t x, int32_t y, double z,
                      uint8_t r, uint8_t g, uint8_t b,
                      double range, double maxRange)   PWX_WARNUNUSED;
  // Project a mass pixel unto the zMassMap
  void    projectMass(int32_t x, int32_t y, double z,
                      uint8_t r, uint8_t g, uint8_t b);
  // Helper to save the current state into saveFile:
  int32_t save(std::ofstream& outFile);
  // Helper to set dynMaxZ
  void    setDynamicZ();
  // Helper Method to start all threads:
  void    startThreads(void (*aCb)(void*));

private:
  // find a dust sphere pixel with a lower z that is either the last one or has a next with a larger z than given
  PWX_INLINE sDustPixel * findPrevDust(sDustPixel *start, double z) PWX_WARNUNUSED;
  // Invalidate all dust spheres up to a specific z:
  PWX_INLINE void invalidateDustSpheres(int32_t x, int32_t y, double z);
  // return true if range and maxRange are large enough
  PWX_INLINE bool isDustLargeEnough(double range, double maxRange) PWX_WARNUNUSED;
  // return true if the dust sphere is useful, thus visible and large enough (after correcting range if needed)
  PWX_INLINE bool isDustUseful(int32_t x, int32_t y, double &z, double &range, double maxRange) PWX_WARNUNUSED;
  // return true if the dust sphere is not blocked by a mass, correct range if it reaches into a mass
  PWX_INLINE bool isDustVisible(int32_t x, int32_t y, double &z, double &range) PWX_WARNUNUSED;
  // Move all dust spheres up unto a specific one to make space in between for a new one
  PWX_INLINE void moveDustSpheresUp(sDustPixel *toFree, int32_t x, int32_t y);
  // split a dust sphere into two, adding up colors and opacities
  PWX_INLINE int32_t splitDust(sDustPixel *dust, int32_t x, int32_t y, double &z,
                               uint8_t r, uint8_t g, uint8_t b,
                               double &range, double maxRange) PWX_WARNUNUSED;

  /* --- no copying! --- */
  ENVIRONMENT(ENVIRONMENT&);
  ENVIRONMENT &operator=(ENVIRONMENT&);
};

/// @brief tiny little helpers to let threads know their number
struct threadEnv
{
  ENVIRONMENT *env;
  int32_t      threadNum;   //!< This is used to tell threads which number they are for displaying the progress

  explicit threadEnv(ENVIRONMENT *aEnv, int32_t aNum):env(aEnv),threadNum(aNum) {}
  ~threadEnv() { env = NULL; }
private:
  threadEnv(threadEnv &src PWX_UNUSED):env(NULL),threadNum(0) {}
  threadEnv &operator=(threadEnv &src PWX_UNUSED) {return *this; }
};

#endif // PWX_GRAVMAT_ENVIRONMENT_H

