#pragma once
#ifndef PWX_GRAVMAT_UNIVERSE_H_INCLUDED
#define PWX_GRAVMAT_UNIVERSE_H_INCLUDED 1

#include <pwxLib/tools/DefaultDefines.h>

/** @struct UNIVERSE
  * @brief struct to keep values that describe the universe (scalers and such) out of ENVIRONMENT
**/
struct UNIVERSE: private pwx::Uncopyable
{
  // Note: although most constants are "fixed" they are not declared as being static, there won't
  // be more than one instance of this struct.
  // Note2: Some of the values depend on the calculation of others, the order is therefore not strictly
  // alphabetical.
  const double c;           //!< The speed of light
  const double G;           //!< Gravitational Constant G

  const double Mass2Kg;     //!< Multiplier to get the mass in kg out of the color index mass
  const double Kg2Mass;     //!< Multiplier to get the color mass index from CMatter::mass
  const double M2Pos;       //!< Multiplier to get the drawing positions from meters
  const double Pos2M;       //!< Multiplier to get the meters from the drawing positions

  const double UnitDensBase;//!< The pure base density, used as a base for all masses
  const double UnitVolBase; //!< Volume of mass 1, which is simply Mass2Kg (= mass 1) / UnitDensBase

  const double NeedNewGDist;//!< A value checked by env against the maximum movement of a second

  const double RingRadMax;  //!< The maximum radius a ring can have
  const double RingRadHalf; //!< Half of the maximum radius, used as a calculation helper
  const double RingRadIPC;  //!< How many radii increases does each ring have per Cycle (IPC = Increase Per Cycle)

  explicit UNIVERSE(double mInPos, double aSunRadius, double aSunMass):
    c           (299792458.0),
    G           (6.67384e-11),  // Gravitational Constant (G) SI (m³ / kg*s²) or (N*m²/kg²)
    Mass2Kg     (2.85e28),      // 15Mj, this constant is a shortcut and a one point definition with these two constants
    Kg2Mass     (1. / Mass2Kg), // the sun has a color index mass of (roughly) 69 which seems to be in order.
    M2Pos       (mInPos),       // one meter in positional coordinates
    Pos2M       (1. / M2Pos),   // amount of meters of one positional coordinate difference
    /* The unit base density is used to reverse calculate the real units radius when a specific mass is given
     * p = m / V ; and
     * V = 4/3 * pi * r³
     * So with the radius and mass of the sun we can calculate:
     *
     * Sun: 1.99e30 / ((4. / 3.) * M_PIl * 6.96e8³) = 1,409.08356106 kg/m³
     * J  : 1.90e27 / ((4. / 3.) * M_PIl * 6.98e7³) = 1,333.82460259 kg/m³
     * So the suns density is higher than jupiters. Bigger masses will result in bigger densities.
     * But as the density is needed for the collision and display relevant mass radius and nowhere
     * else in physics, I daresay it is completely in order to fix that on the density of the sun.
    */
    UnitDensBase(aSunMass / ((4. / 3.) * M_PIl * pwx_pow(aSunRadius, 3.))),
    UnitVolBase (Mass2Kg / UnitDensBase),
    // For the Needed GDist, we start with the sun diameter, see CMatter::setRadius() for the algorithm
    NeedNewGDist(pwx_pow((3. * (aSunMass / UnitDensBase)) / (4. * M_PIl), 1. / 3.) * 2.0),
    RingRadMax  (200.0),            // Currently 200 times the mass radius
    RingRadHalf (RingRadMax / 2.),  // This is just a helper, really.
    RingRadIPC  (RingRadMax / 5.)   // Simply divide the maximum radius by the number of cycles the rings shall be seen
    { /* these are consts, so nothing to be done here. */ }
};

#endif // PWX_GRAVMAT_UNIVERSE_H_INCLUDED

