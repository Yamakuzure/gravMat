#pragma once
#ifndef PWX_GRAVMAT_MATTER_H_INCLUDED
#define PWX_GRAVMAT_MATTER_H_INCLUDED 1

#include <cmath>

// Here we need it, sfmlui.cpp::initSFML() will create it:
#include "colormap.h"


/** @class CMatter
  * @brief Simple class to hold matter data and not so simple move it
  *
  * Tasks: Save matter data, calculate impulse, perform movement
**/
class CMatter : public pwx::Lockable
{
  double posX, posY, posZ; //!< Position in positional coordinates away from the center
  double impX, impY, impZ; //!< Impulse in m/kg*s²
  double accX, accY, accZ; //!< Acceleration in m/s²
  double movX, movY, movZ; //!< Movement in m/s
  double distance;         //!< Distance to the center in positional coordinates
  double mass;             //!< Mass in kg
  double radius;           //!< Radius in meters
  double ringRadius;       //!< Radius factor of the ring when exploding, based on radius
  double ringMass;         //!< Mass of the explosion ring in kg

  // Access methods within this class and between objects

  // add the given impulses (in Newton) to the currently set
  void addImpulse (double X, double Y, double Z)
    {
      impX += X; impY += Y; impZ += Z;
    }

  // Helper methods:
  // manipulate the colors given with a simplex noise offset
  PWX_INLINE void addSimplexOffset(ENVIRONMENT *env,
                                   double x, double y, double z,
                                   bool isMass, bool isRing,
                                   uint8_t &r, uint8_t &g, uint8_t &b);
  // manipulate z and range given with a simplex noise offset
  PWX_INLINE void addSimplexOffset(ENVIRONMENT *env,
                                   double x, double y, double &z,
                                   bool isDust, bool isRing, double &range);


  /* --- inline methods --- */
  /// @brief return true if x/y are on the projection plane
  bool isOnPlane(ENVIRONMENT *env, int32_t x, int32_t y) PWX_WARNUNUSED
    {
      return (env && (x >= 0) && (x < env->scrWidth) && (y >= 0) && (y < env->scrHeight));
    }

  /// @brief Sets the radius according to the unified density
  void setRadius(ENVIRONMENT *env)
    {
      assert (env && "ERROR: setRadius called without valid env!");
      assert (env && env->universe && "ERROR: setRadius called without valid universe!");
      /* Because p     = m / V
       * and  => p * V = m
       * and  => V     = m / p
       * we can thus get from
       *         V = (4 * PI * r³) / 3
       * rather straight to
       *         =>   3 * V                    = 4 * PI * r³
       * and     =>  (3 * V) / (4 * PI)        = r³
       * finally => ((3 * V) / (4 * PI)) ^ 1/3 = r (And r is our mass radius here)
      */
      if (env && env->universe)
        radius   = pwx_pow(  (3. * (mass / env->universe->UnitDensBase)) // first part ; Note: the density is unified
                           / (4. * M_PIl), // second part
                            1. / 3.); // This results in the radius in meters
      else
        radius = 0.;
    }

  /* --- outline methods --- */
  // Note: Although isFront and isVisible are one-liners, isFront needs the full sMassPixel struct and isVisible needs isFront
  PWX_INLINE bool    isFront    (ENVIRONMENT *env, int32_t x, int32_t y, double z) PWX_WARNUNUSED;
  PWX_INLINE bool    isVisible  (ENVIRONMENT *env, int32_t x, int32_t y, double z) PWX_WARNUNUSED;
  PWX_INLINE int32_t projectUnit(ENVIRONMENT *env, int32_t x, int32_t y, double z,
                                 double vR, double dR, double dMR, uint8_t r, uint8_t g, uint8_t b) PWX_WARNUNUSED;


public:
  /** @brief default ctor that should be used to create matter units
    *
    * If a movement is given it will be distributed to the three axis according to the units position
    *
    * @param[in] env Pointer to environment struct, must not be NULL
    * @param[in] x X-Coordinate in drawing positions
    * @param[in] y Y-Coordinate in drawing positions
    * @param[in] z Z-Coordinate in drawing positions
    * @param[in] movement Absolute movement in m/s
  **/
  explicit CMatter (ENVIRONMENT * env, double X, double Y, double Z, double movement):
    posX(X), posY(Y), posZ(Z),
    impX(0.0), impY(0.0), impZ(0.0),
    accX(0.0), accY(0.0), accZ(0.0),
    movX(0.0), movY(0.0), movZ(0.0),
    distance(0.0), mass(1.0), radius(0.0), ringRadius(0.0), ringMass(0.0)
    {
      assert (env && "ERROR: CMatter ctor called without valid env!");
      assert (env && env->universe && "ERROR: CMatter ctor called without valid universe!");

      mass = env->universe->Mass2Kg; // Mass2Kg is initialized being color index mass 1 in kg.

      double allPos = std::abs(posX) + std::abs(posY) + std::abs(posZ);
      distance = ::pwx::absDistance(posX, posY, posZ, 0., 0., 0.);

      // The initial movement is the signed ratio of axis position to full distance of the total movement
      movX = (posX / allPos) * movement; // Note: As the first part of the calculation in
      movY = (posY / allPos) * movement; //       brackets is a ratio, and the second is
      movZ = (posZ / allPos) * movement; //       movement in m/s, movX/Y/Z will be m/s

      setRadius(env);

      // The movement must not exceed the speed of light, of course
      if (std::abs(movX) > env->universe->c) movX = SIGN(movX) * env->universe->c;
      if (std::abs(movY) > env->universe->c) movY = SIGN(movY) * env->universe->c;
      if (std::abs(movZ) > env->universe->c) movZ = SIGN(movZ) * env->universe->c;

      // If the new z-coordinate of this unit is smaller than recorded, it needs to be noted
      if (env->doDynamic)
        {
          env->lock();
          if (posZ < env->minZ)
            env->minZ = posZ;
          env->unlock();
        }
    }

  /// @brief empty ctor, only to be used for loading matter units.
  explicit CMatter ():
    posX(0.0), posY(0.0), posZ(0.0),
    impX(0.0), impY(0.0), impZ(0.0),
    accX(0.0), accY(0.0), accZ(0.0),
    movX(0.0), movY(0.0), movZ(0.0),
    distance(0.0), mass(0.0), radius(0.0), ringRadius(0.0), ringMass(0.0)
    { }

  /// @brief default dtor, does nothing.
  ~CMatter() {}

  /// @brief return true if this unit is destroyed
  bool   destroyed() PWX_WARNUNUSED
    {
      return 1.0 > mass ;
    }

  /// @brief return the posZ value, this is needed to determine minZ while loading if dynamic camera is used
  double getPosZ() const { return posZ; }

  /// @brief return true if this unit is finally gone
  bool   gone(ENVIRONMENT *env) PWX_WARNUNUSED
    {
      assert (env && "ERROR: gone() called without valid env!");
      assert (env && env->universe && "ERROR: gone() called without valid universe!");
      return ringRadius >= env->universe->RingRadMax ;
    }

  /// @brief return the positive difference of the distance of this to rhs
  double distDiff(CMatter *rhs) PWX_WARNUNUSED
    {
      double result = distance;
      if (rhs)
        result -= rhs->distance;
      return abs(result);
    }

  /// @brief return the radius in meters
  double getRadius () const { return radius; }


  // Access methods:
  /// @brief reset the impulse values *before* calculating new gravitation
  void resetImpulse()
    {
      impX = 0.;
      impY = 0.;
      impZ = 0.;
    }

  // Work Methods
  /* --- These are the methods that represent the main workflow.
   * 1.: Apply gravitational force between two units
   * 2.: Apply the generated impulses
   * 3.: Move the unit
   * 4.: Sort all units by Z-Position
   * 5.: Check the position between two units and merge them if they meet
   * 6.: Project it to the projection plane
   *
   * - Position 4 is done from the outside, the container does it.
  */
  void    applyGravitation (ENVIRONMENT *env, CMatter *rhs);
  void    applyImpulses    (ENVIRONMENT *env);
  void    applyMovement    (ENVIRONMENT *env);
  void    applyCollision   (ENVIRONMENT *env, CMatter *rhs);
  int32_t project          (ENVIRONMENT *env) PWX_WARNUNUSED;

  /// @brief return true if this distance to the center is larger than the one of rhs
  bool operator>(CMatter &rhs)
    {
      return distance > rhs.distance;
    }

  std::ifstream& load(std::ifstream &is);
  std::ostream&  save(std::ostream &os) const;
};

std::ifstream& operator>>(std::ifstream &is, CMatter &unit);
std::ostream&  operator<<(std::ostream &os, CMatter &unit);

#endif // PWX_GRAVMAT_MATTER_H_INCLUDED

