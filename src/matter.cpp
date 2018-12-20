// For unit loading we use readNextValue from here:
#include <pwxLib/tools/StreamHelpers.h>

#include "environment.h"
#include "matter.h"

// This one is needed to get RNG Simplex3D offsets:
#include "sfmlui.h"

// Here the real pixel info headers have to be included
#include "dustpixel.h" // It will pull masspixel.h in

using std::ifstream;
using std::ostream;
using std::min;
using std::max;

/// @brief manipulate @a r, @a g and @a b with simplex noise set up with the other values
void CMatter::addSimplexOffset(ENVIRONMENT *env,
                               double x, double y, double z,
                               bool isMass, bool isRing,
                               uint8_t &r, uint8_t &g, uint8_t &b)
  {
    assert ((isMass || isRing) && "ERROR: Simplex Color Offset called without a mass?");
    if (isMass || isRing)
      {
        int32_t baseR = static_cast<int32_t>(r);
        int32_t baseG = static_cast<int32_t>(g);
        int32_t baseB = static_cast<int32_t>(b);

        double maxOffset = isMass ? 0.1 : 0.25;
        double zoomFac   = 1.0 - std::max(Min_Dust_Range, posZ) / (env->camDist + env->dynMaxZ);
        if (zoomFac < 1.0)
          zoomFac = 1.0;
        double offset = getSimOff(posX + x, posY + y, z, zoomFac * (isMass ? 17.337 : 3.675)) * maxOffset;

        baseR += static_cast<int32_t>(pwx_round(static_cast<double>(baseR) * offset));
        baseG += static_cast<int32_t>(pwx_round(static_cast<double>(baseG) * offset));
        baseB += static_cast<int32_t>(pwx_round(static_cast<double>(baseB) * offset));

        // Before we can write back the values, they have to be put "in range":
        if (baseR < 0) baseR = 0; else if (baseR > 255) baseR = 255;
        if (baseG < 0) baseG = 0; else if (baseG > 255) baseG = 255;
        if (baseB < 0) baseB = 0; else if (baseB > 255) baseB = 255;

        // Now write back:
        r = static_cast<uint8_t>(0x000000ff & baseR);
        g = static_cast<uint8_t>(0x000000ff & baseG);
        b = static_cast<uint8_t>(0x000000ff & baseB);
      } // End of having legal values
  }


/// @brief manipulate @a range with simplex noise set up with the other values
void CMatter::addSimplexOffset(ENVIRONMENT *env,
                               double x, double y, double &z,
                               bool isDust, bool isRing, double &range)
  {
    assert ((isDust || isRing) && "ERROR: Simplex Range Offset called without a dust sphere?");
    if (isDust || isRing)
      {
        double maxOffset = isDust ? 0.2 : 0.15;
        double zoomFac   = 1.0 - std::max(Min_Dust_Range, posZ) / (env->camDist + env->dynMaxZ);
        if (zoomFac < 1.0)
          zoomFac = 1.0;
        double offset = getSimOff(x, y, z, zoomFac * (isDust ? 23.973 : 3.375)) * maxOffset * range;

        // The offset does not only lower/raise the range, but shifts z (of course)
        z     -= offset / 2.;
        range += offset;

      } // end of having legal values
  }


/// @brief Apply gravitational force between two units (Step 1)
void CMatter::applyGravitation (ENVIRONMENT *env, CMatter *rhs)
  {
    // Now do the calculations
    if (rhs)
      {
        // Shortcuts:
        const double Pos_to_M = env->universe->Pos2M;
        const double G_Const  = env->universe->G;

        // We have positional coordinates, but need meters here:
        double lX = posX * Pos_to_M;
        double lY = posY * Pos_to_M;
        double lZ = posZ * Pos_to_M;
        double rX = rhs->posX * Pos_to_M;
        double rY = rhs->posY * Pos_to_M;
        double rZ = rhs->posZ * Pos_to_M;

        // Now for the distance:
        double dist    = pwx::absDistance(lX, lY, lZ, rX, rY, rZ);
        // Note: The result of absDistance is always positive
        double distXY  = pwx::absDistance(lX, lY, rX, rY);
        // Note 2: distXY is the flat distance needed to calculate cos(beta)

        // As the first second allows no collision, and as we do not want extreme shoots, we have to "normalize":
        if (dist   < 1.0) dist   = 1.0;
        if (distXY < 0.0) distXY = 0.0;
        // Note: distXY can of course become zero, that is no problem at all.

        // Gravitation is N = (m1*m2) / r²
        // with m being the masses in kg and
        // r being the distance in m.
        double N = G_Const * (mass / dist) * (rhs->mass / dist); // The result is in Newton. (N or kg*m/s²)

        // The Impulses are simply added up, as we start over at zero in each round.
        // As we have the positions, we do neither need sine nor cosine, we already
        // have the results at hand.
        // "alpha" is the angle between X and Y, "beta" is the angle between the XY-
        // hypotenuse and Z.
        // "dist" is the hypotenuse over all three coordinates.
        double modXY = distXY    / dist; // sin(beta)  (unsigned)
        double modX  = (rX - lX) / dist  // cos(alpha) (signed)
                     * modXY;
        double modY  = (rY - lY) / dist  // sin(alpha) (signed)
                     * modXY;
        double modZ  = (rZ - lZ) / dist; // cos(beta)  (signed)
        // Another note as this troubled me again: distXY is the distance between
        // the X and Y positions of both units. This means that they are already
        // "on the sphere" as we only want to calculate back the relations between
        // the three axis.
        /* Sphere Coordinates:
         * X = cos(alpha) * sin(beta) * N == N * modX * modXY
         * Y = sin(alpha) * sin(beta) * N == N * modY * modXY
         * Z =              cos(beta) * N == N * modZ
         */
        lock();
        addImpulse(N * modX, N * modY, N * modZ );
        unlock();

        rhs->lock();
        rhs->addImpulse(-N * modX, -N * modY, -N * modZ );
        rhs->unlock();
      } // end of having rhs.
  }

/// @brief Apply impulses
void CMatter::applyImpulses (ENVIRONMENT *env)
  {
    using std::min;
    using std::max;

    /* Impulses drag the movement in a specific direction. The workflow
     * for applying the current impulse is as follows:
     *
     * a) Determine the target speed per axis after the full impulse is applied if the unit had a mass of 1 kg
     * b) Calculate the resulting movement modifier per axis and mass
     * c) Set Impulse values to the movement modifier modified by FPS modifier
     *
    */

    /* a) Determine the target speed per axis after the full impulse is applied
     * When the full impulse is applied, a target speed is reached after one second.
     * This is done like the unit had a mass of 1 kg, because if we reduced the impulse
     * instead of the effect, the target speed would be wrong and mass induced drag
     * would not be taken into account. And further, a speed change from +10m/s with
     * an impulse of +20mkg/s² for a mass of 2kg would result in no speed change if
     * the mass is applied beforehand.
     *
     * There are three cases:
     * Case 1: mov* and imp* have different signedness
     *   ==> tgt* is mov* + imp*
     * Case 2: mov* and imp* have the same signedness, but mov* is nearer to 0 than imp*
     *   ==> tgt* is imp*
     * Case 3: mov* and imp* have the same signedness, but imp* is nearer to 0 than mov*
     *   ==> tgt* is mov*
     */
    double tgtX = SIGN(movX) != SIGN(impX) ? movX + impX // Case 1
                : std::abs(impX) > std::abs(movX) ? impX // Case 2
                : movX;                                  // Case 3
    double tgtY = SIGN(movY) != SIGN(impY) ? movY + impY // Case 1
                : std::abs(impY) > std::abs(movY) ? impY // Case 2
                : movY;                                  // Case 3
    double tgtZ = SIGN(movZ) != SIGN(impZ) ? movZ + impZ // Case 1
                : std::abs(impZ) > std::abs(movZ) ? impZ // Case 2
                : movZ;                                  // Case 3

    /* b) Calculate the resulting acceleration per axis and mass
     * The movement modifier is the difference between mov* and tgt*.
     * But now it must be divided by the mass, because we still have Newton:
     * N = kg*m / s² | we need m/s² ; the acceleration
     *    N / kg = kg*m /kg*s²
     * => N / kg = m / s²
     */
    accX = (tgtX - movX) / mass;
    accY = (tgtY - movY) / mass;
    accZ = (tgtZ - movZ) / mass;

    // c) Save the resulting acceleration if it is the largest known
    double accel = pwx::absDistance(accX, accY, accZ, 0., 0., 0.);

    if (accel > env->universe->c)
      {
        // The acceleration is to high, scale it down.
        double accMod = (env->universe->c - 1.0) / accel;
        accX *= accMod;
        accY *= accMod;
        accZ *= accMod;
        accel = pwx::absDistance(accX, accY, accZ, 0., 0., 0.);
      }

    env->lock();
    if (accel > env->statMaxAccel)
      env->statMaxAccel = accel;
    env->unlock();

    /* d) Modify the acceleration values by the FPS modifier.
     * Only a fraction of the acceleration is applied for each frame, but the frame
     * modifier is unified.
     */
    accX *= env->secPFmod;
    accY *= env->secPFmod;
    accZ *= env->secPFmod;
  }

/// @brief Move the unit
void CMatter::applyMovement (ENVIRONMENT *env)
  {
    // Step 1: Apply per Frame impulse modifier
    movX += accX;
    movY += accY;
    movZ += accZ;


    // Step 2: Save the current movement if this is the fastest mover
    double movement = pwx::absDistance(movX, movY, movZ, 0., 0., 0.);

    if (movement > env->universe->c)
      {
        // The acceleration is to high, scale it down.
        double movMod = (env->universe->c - 1.0) / movement;
        movX *= movMod;
        movY *= movMod;
        movZ *= movMod;
        movement = pwx::absDistance(movX, movY, movZ, 0., 0., 0.);
      }

    env->lock();
    if (movement > env->statMaxMove)
      env->statMaxMove = movement;
    env->unlock();

    // Step 3: Apply per Frame movement fraction
    posX += env->universe->M2Pos * movX * env->secPFmod;
    posY += env->universe->M2Pos * movY * env->secPFmod;
    posZ += env->universe->M2Pos * movZ * env->secPFmod;

    // Step 4: If the new z-coordinate of this unit is smaller than recorded, it needs to be noted
    if (env->doDynamic)
      {
        env->lock();
        if (posZ < env->minZ)
          env->minZ = posZ;
        env->unlock();
      }

    // Renew distance
    distance = pwx::absDistance(posX, posY, posZ, 0., 0., 0.);
  }


/// @brief check positions and merge if they collided (Step 6) (returns true if they collide)
/// Important:this unit must be locked and checked to be not destroyed beforehand
void CMatter::applyCollision (ENVIRONMENT *env, CMatter *rhs)
  {
    if (rhs && (rhs->mass > 1.0))
      {
        // Two units collide, if their surfaces have a distance less than one meter to each other
        double dist = pwx::absDistance(posX, posY, posZ, rhs->posX, rhs->posY, rhs->posZ)
                    - (env->universe->M2Pos * (radius + rhs->radius));
        // Note: The result of absDistance is always positive

        rhs->lock();
        if (!rhs->destroyed() && (dist < env->universe->M2Pos))
          {

            // Set mass and readjust radius on the larger of the two units and set the other to be destroyed:
            if (mass >= rhs->mass)
              {
                // This unit "wins" and annihilates rhs

                // The resulting impulse and movement is the average of both movements adjusted by their masses
                // Note: the final reduction is done later, to save two additions
                impX = (impX * mass) + (rhs->impX * rhs->mass);
                impY = (impY * mass) + (rhs->impY * rhs->mass);
                impZ = (impZ * mass) + (rhs->impZ * rhs->mass);
                movX = (movX * mass) + (rhs->movX * rhs->mass);
                movY = (movY * mass) + (rhs->movY * rhs->mass);
                movZ = (movZ * mass) + (rhs->movZ * rhs->mass);
                // The acceleration does not need to be adjusted, as it is regenerated before the next movement

                // Add rhs mass now and re-adjust impulse and movement:
                mass += rhs->mass;
                impX /= mass;
                impY /= mass;
                impZ /= mass;
                movX /= mass;
                movY /= mass;
                movZ /= mass;

                // readjust our radius:
                setRadius(env);

                // annihilate rhs:
                rhs->ringMass   = 1.0 + (mass / 2.0);
                rhs->mass       = 0.0; // This takes it out of further calculations until the container is cleaned.
                rhs->ringRadius = 0.0; // Start with a ring that begins exactly where the mass has ended
              }
            else
              {
                // rhs "wins" and annihilates this unit
                rhs->impX = (impX * mass) + (rhs->impX * rhs->mass);
                rhs->impY = (impY * mass) + (rhs->impY * rhs->mass);
                rhs->impZ = (impZ * mass) + (rhs->impZ * rhs->mass);
                rhs->movX = (movX * mass) + (rhs->movX * rhs->mass);
                rhs->movY = (movY * mass) + (rhs->movY * rhs->mass);
                rhs->movZ = (movZ * mass) + (rhs->movZ * rhs->mass);

                rhs->mass += mass;
                rhs->impX /= rhs->mass;
                rhs->impY /= rhs->mass;
                rhs->impZ /= rhs->mass;
                rhs->movX /= rhs->mass;
                rhs->movY /= rhs->mass;
                rhs->movZ /= rhs->mass;

                // readjust our radius:
                rhs->setRadius(env);

                // annihilate this unit:
                ringMass   = 1.0 + (rhs->mass / 2.0);
                mass       = 0.0; // This takes it out of further calculations until the container is cleaned.
                ringRadius = 0.0; // Start with a ring that begins exactly where the mass has ended
              }

            // Note: The position is not added. The positions are considered equal when this annihilation
            // takes place.
          } // End of collision handling
        rhs->unlock();
      } // End of having rhs
  }


/// @brief starter method that manages the projection onto the projection plane
int32_t CMatter::project(ENVIRONMENT *env)
  {
    int32_t result = EXIT_SUCCESS;
    assert(env && "ERROR: CMatter::project() called without valid env!");
    assert(env && env->universe && "ERROR: CMatter::project() called without valid env->universe!");

    // Shortcuts
    const double Kg_to_Mass = env->universe->Kg2Mass;
    const double M_to_Pos   = env->universe->M2Pos;
    const double Pos_to_M   = env->universe->Pos2M;
    const double Ring_IPC   = env->universe->RingRadIPC; // [I]Increase [P]er [C]ycle
    const double Ring_Max   = env->universe->RingRadMax;
    double viewZpos         = env->dynMaxZ + env->camDist + posZ; // Position on the virtual Z-Axis

    if ( (viewZpos > 0.)
      && ((mass > 1.0) || (ringRadius < Ring_Max) ) )
      {
        double viewXMov = posX + (M_to_Pos * radius); // X-Offset by radius
        double viewDiv  = env->camDist / viewZpos;    // The divisor that scales the mass radius on the projection plane
        double viewRad  = pwx::absDistance(viewDiv * posX,     0.,  // central X position
                                           viewDiv * viewXMov, 0.); // X offset of the radius
        // Before the absolute X/Y-positions can be calculated, viewDiv has to be recalculated to not
        // use the center, but the Z-position nearest to the camera.
        viewDiv  = env->camDist / (viewZpos - viewRad);
        // Now it is the divisor that scales units onto the projection plane

        // Before the final viewRad is known, it has to be recalculated if this is a detonation ring:
        if (1.0 > mass)
          viewRad += viewRad * ringRadius;
        int32_t viewXpos = static_cast<int32_t>(pwx_round(posX * viewDiv)
                                               + env->halfWidth);  // Absolute drawing position X value
        int32_t viewYpos = static_cast<int32_t>(pwx_round(posY * viewDiv)
                                               + env->halfHeight); // Absolute drawing position Y value
        double  dustRad  = viewRad; // Will be modified by mass through mkColor() below

        // There is no need to calculate anything further if this unit doesn't touch the projection plane:
        // Note: This does validate units that are not on the projection plane, because of their diagonal
        // position to the projection plane edges. But it would be very complex to calculate a real collision
        // between the units outer rim and the projection plane.
        if ( ((viewXpos + dustRad) > -1) && ((viewXpos - dustRad) < env->scrWidth)
          && ((viewYpos + dustRad) > -1) && ((viewYpos - dustRad) < env->scrHeight) )
          {
            uint8_t r = 0x0, g = 0x0, b = 0x0;
            double dustMaxRangeMod = 2.5; // The default results in a maximum dust sphere of 40% of the DustMaxRange

            if (mass > 0.1)
              {
                double dustRadMassMod = 1.0;
                env->colorMap->mkColor(Kg_to_Mass * mass, movZ, Pos_to_M * viewZpos,
                                       &dustRadMassMod, &dustMaxRangeMod,
                                       r, g, b, 1.0);
                dustRad += viewRad * dustRadMassMod;
                // Note: dustRad was viewRad, and the mod determines, how much larger than the viewRad the dust sphere is
                if (dustRad < 1.414214)
                  dustRad = 1.414214; // This minimum is there to smooth the display via the 8 pixels around the center
              }
            else
              {
                env->colorMap->mkColor(Kg_to_Mass * ringMass, 0., Pos_to_M * viewZpos,
                                       NULL, NULL,
                                       r, g, b, 1.5 - ((Ring_Max - ringRadius) / Ring_Max));
                dustRad += viewRad; // Here the initial dust radius is always twice the view radius
              }

            // If the colors are alright now, we can go on
            assert((r || g || b) && "ERROR: a black unit shall be rendered!");
            result = projectUnit(env, viewXpos, viewYpos, viewZpos, viewRad, dustRad, dustMaxRangeMod, r, g, b);
          } // End of being on the projection plane
      } // End of being valid to project

    // The ringRadius is raised for the full time to be x cycles, See UNIVERSE::RingRadIPC
    if ((EXIT_SUCCESS == result) && (1.0 > mass) && (ringRadius < Ring_Max))
      ringRadius += env->cyclPerFrm * Ring_IPC;

    return result;
  }


/// @brief return true if x/y/z is NOT hidden behind a mass pixel.
// Warning: x/y must be ensured to be sane!
bool CMatter::isFront(ENVIRONMENT *env, int32_t x, int32_t y, double z)
{
  // Note: z is not checked against 0.0, because env->projectMass/Dust are responsible for that
  return (env && ((env->zMassMap[x][y].z < 0.) || (z < env->zMassMap[x][y].z)) );
}


/// @brief return true if x/y/z is visible.
bool CMatter::isVisible(ENVIRONMENT *env, int32_t x, int32_t y, double z)
{
  // Note: z is not checked against 0.0, because env->projectMass/Dust are responsible for that
  return (env && isOnPlane(env, x, y) && isFront(env, x, y, z));
}

/// @brief do the projection of each pixel a units projection consists of
int32_t CMatter::projectUnit(ENVIRONMENT* env, int32_t x, int32_t y, double z, double vR,
                             double dR, double dMR, uint8_t r, uint8_t g, uint8_t b)
{
  assert ((vR > 0.) && "ERROR: viewRad MUST be larger than 0.0!");
  assert ((dR > 0.) && "ERROR: dustRad MUST be larger than 0.0!");

  int32_t result   = EXIT_SUCCESS;

  /* A note on the arguments vR/hR:
   *
   * hR is the [h]alo[R]adius and thus the maximum extend of the unit, no matter
   *   whether it is a mass unit or a detonation ring.
   * vR is the [v]iew[R]adius and therefore *different* for mass units and detonation
   *   rings.
   *   - For mass units vR is the radius the mass has. The area between vR and hR is
   *     the dust sphere.
   *   - For detonation ring vR is the radius where the ring _starts_, so it looks
   *     like being the other way round. On the other hand vR is then the end of the
   *     central dust sphere representing the consumed mass, so the usage is quite consistent.
  */

  // Shortcuts:
  double Half_Radius   = env->universe->RingRadHalf;
  double Full_Radius   = env->universe->RingRadMax;

  // Working values:
  double  stop     = 1.1 + dR; // This is the maximum offset to calculate
  double  maxRange = dR * 2. * dMR;
  double  range    = maxRange;
  double  colMod   = 1.0;   // Modifier for the color, determined by the position of the pixel relative to the edge

  // The color has to be "grayed" a bit:
  uint32_t grayPart = static_cast<uint32_t>(
                      ( static_cast<double>(r)
                      + static_cast<double>(g)
                      + static_cast<double>(b) ) / 3.);
  uint8_t dustR = static_cast<uint8_t>(0xff & ((grayPart + (static_cast<uint32_t>(r) * 2)) / 3));
  uint8_t dustG = static_cast<uint8_t>(0xff & ((grayPart + (static_cast<uint32_t>(g) * 2)) / 3));
  uint8_t dustB = static_cast<uint8_t>(0xff & ((grayPart + (static_cast<uint32_t>(b) * 2)) / 3));


  // For detonation rings we need five additional values:
  double centRange = 0., ringRange = 0., ringStop = 0., ringCent = 0.;
  bool   ringHasMass = true;

  if (1.0 > mass)
    {
      /* This is a detonation ring */
      // centRange is the maxRange value for the central dust sphere representing the consumed
      // mass' dust sphere.
      centRange  = Full_Radius / (Full_Radius - ringRadius);
      if (centRange < 1.0)
        centRange = 1.0;
      centRange *= vR * 2. * dMR;
      // ringStop is the point where the (expanding) ring stops, and is bound to the
      // calculated dust sphere radius. This means that the ring consumes the dust sphere while it
      // expands.
      ringStop = dR * (ringRadius / Full_Radius);
      if (ringStop < vR) ringStop = vR + 0.5;
      if (dR < ringStop) dR       = ringStop + 0.1;
      // ringCent is the center of the ring at two thirds length towards the ring end.
      // This will make the ring more torus looking and strengthen the expanding effect.
      ringCent = (vR / 3.) + (ringStop * 2. / 3.);
      // ringRange is a value needed when the ring radius is larger than half the maximum
      // ring radius. Then the ring is considered a fading dust sphere, fading like the central
      // remnant by increasing the maxRange.
      if (ringRadius > Half_Radius)
        {
          ringRange = Half_Radius / (ringRadius - Half_Radius);
          if (ringRange < 1.0)
            ringRange = 1.0;
          ringRange *= ringStop - vR * dMR;
          // ringHasMass is used below to easily see if the ring needs color modification or a range
          ringHasMass = false;
        }
    }

  // The first thing we have to do is to project the center pixel. This is done beforehand,
  // because it would be too much of a hassle to skip one of the zero coordinates in each run.
  // very large objects near to the camera need a lot of pixels to consider, doing superfluous
  // checks every time is not very effective. And finally the drawing of the center pixel is simple.
  env->lock();
  if (isVisible(env, x, y, z - vR))
    {
      double  currZ = z - vR;
      if (mass > 0.1)
        {
          // 1: The mass pixel preparation
          uint8_t currR = r;
          uint8_t currG = g;
          uint8_t currB = b;
          addSimplexOffset(env, x, y, currZ, true, false, currR, currG, currB);

          // 2.: Project the pixel as a mass
          if (vR > 0.5)
            env->projectMass(x, y, currZ, currR, currG, currB);
          // 3.: Project the pixel as a halo
          else
            {
              /* If the view radius is less than half a pixel, it is considered to
               * be "outshone" by what is behind it.
               * the minimum is 50% with a view radius of 0.001 (capped) and the maximum
               * is 100% (you say!) with a radius of 0.5
              */
              double calcRange = vR < 0.001 ? 0.001 : vR;
              double calcMaxRange = calcRange + (calcRange * (1.0 - (2.0 * calcRange)));
              // Note: This leads of a dust range of maximum 199.8% of vR, which results in 50,05% opacity.

              // further the dust color needs to be dimmed:
              double calcGamma = calcRange / calcMaxRange;
              dustR = static_cast<uint8_t>(0xff & static_cast<uint32_t>(pwx_round(static_cast<double>(dustR) * calcGamma)));
              dustG = static_cast<uint8_t>(0xff & static_cast<uint32_t>(pwx_round(static_cast<double>(dustG) * calcGamma)));
              dustB = static_cast<uint8_t>(0xff & static_cast<uint32_t>(pwx_round(static_cast<double>(dustB) * calcGamma)));

              // Now project the pixel
              result = env->projectDust(x, y, currZ, currR, currG, currB, calcRange, calcMaxRange);
            }

          // 4.: The dust pixel
          if (EXIT_SUCCESS == result)
            {
              currZ = z - dR;
              double currRange = 2. * dR;
              addSimplexOffset(env, x, y, currZ, true, false, currRange);
              result = env->projectDust(x, y, currZ, dustR, dustG, dustB, currRange, maxRange);
            }
        }
      else
        {
          double xRange = vR * 2.;
          addSimplexOffset(env, x, y, currZ, true, false, xRange);
          result = env->projectDust(x, y, currZ, dustR, dustG, dustB, xRange, centRange);
        }
    } // End of having no mass point in front of this one
  // Quit if we failed:
  if (EXIT_FAILURE == result)
    env->doWork = false;
  // Now unlock
  env->unlock();

  // Now we can calculate everything else in a two level loop, mirroring the result by both axis
  // Note: If the result of a dust projection is EXIT_FAILURE, then env->doWork is already false.
  for (double xOff = 1.0; (xOff < stop) && env->doWork; xOff += 1.0)
    {
      for (double yOff = 0.0; (yOff < stop) && env->doWork; yOff += 1.0)
        {
          int32_t drawX[4]  = { static_cast<int32_t>(pwx_round(x + xOff)),
                                static_cast<int32_t>(pwx_round(x - yOff)),
                                static_cast<int32_t>(pwx_round(x - xOff)),
                                static_cast<int32_t>(pwx_round(x + yOff)) };
          int32_t drawY[4]  = { static_cast<int32_t>(pwx_round(y + yOff)),
                                static_cast<int32_t>(pwx_round(y + xOff)),
                                static_cast<int32_t>(pwx_round(y - yOff)),
                                static_cast<int32_t>(pwx_round(y - xOff)) };
          // Note: Use isOnPlane() rather than isVisible(), because the latter needs the real z value
          // and that value needs a lot of calculations beforehand we can save here.
          bool    doDraw[4] = { isOnPlane(env, drawX[0], drawY[0]),
                                isOnPlane(env, drawX[1], drawY[1]),
                                isOnPlane(env, drawX[2], drawY[2]),
                                isOnPlane(env, drawX[3], drawY[3])};

          // Now do only continue if at least one of the 4 pixels is drawable:
          if (doDraw[0] || doDraw[1] || doDraw[2] || doDraw[3])
            {
              /* There are three steps before we can project the points:
               * Step 1: calculate the distance values of this pixel
               * Step 2: determine the case and the resulting mod and drawing Z values
               * Step 3: calculate colmod if it is needed
              */

              /* === Step 1: Calculate the distance value of this pixel ===
                 ===----------------------------------------------------===
              pointDist is the simple distance between pixel and the center. The simple distance determines
              the "phase" the pixel is in.
              */
              double pointDist = pwx::absDistance(xOff, yOff, 0., 0.); // Distance of the center of the pixel to 0/0

              /* === Step 2: Set the case we are in. Calculate mod and drawing Z ===
                 ===-------------------------------------------------------------===
              It is not only necessary to know in which part (or "phase") of an object we are,
              all parts share two values that are needed to project the resulting pixels.
              modZ is a multiplier for the specific radius of the sphere that allows to calculate
              massZ and dustZ. The two Z positions are the z nearer to the camera than the argument z
              where the pixel (or point) truly lies. Further more dust sphere pixels need their range,
              which is twice the z-offset of this pixel.
              */
              bool isMassPix = false, isRemnPix = false, isRingPix = false, isDustPix = false;
              double dustZ   = z;
              double massZ   = z;
              double modZ    = 0.;

              if (pointDist < vR)
                {
                  /* A note about modZ:
                   * modZ is the cosine of the angle between the hypotenuse (0/0 to x/y) and the
                   * z-offset (z - massZ) of this point. pointDist is this hypotenuse, and can
                   * be used to calculate the sine of said angle. modZ is then the cosine of that
                   * angle that we get by simply calculating the arcus sine. We do not use SCT.cos,
                   * because asin() returns radians, which std::cos happily takes.
                  */

                  // This pixel is within the inner bounds, so either it is the mass or its remnant
                  if (mass > 0.1)
                    {
                      // Here we have to add the spanning sphere
                      isDustPix = true;
                      modZ   = std::cos(std::asin(pointDist / dR));
                      range  = 2 * dR * modZ;
                      dustZ -= dR * modZ;

                      // Note: The dust pixel is calculated first, because modZ is later
                      // needed for the color modification of the mass pixel
                      isMassPix = true;
                      modZ   = std::cos(std::asin(pointDist / vR));
                      massZ -= vR * modZ;
                    }
                  else
                    {
                      isRemnPix = true;
                      modZ   = std::cos(std::asin(pointDist / vR));
                      dustZ -= vR * modZ;
                      range  = 2 * vR * modZ;
                    }
                } // End of having a pixel in the inner bounds
              else if ((1.0 > mass) && (pointDist < ringStop))
                {
                  isRingPix = true;
                  /* This is a bit more complicated, because the ring is a torus with shifted center.
                   * Generally speaking the ring consist of two half tori. The inner and the outer.
                   * As we "see" a point on a line through the torus, only the side off the center
                   * and that respective distance are relevant.
                  */
                  if (pointDist < ringCent)
                    {
                      // Inner half
                      double innRad = ringCent - vR;
                      modZ   = std::cos(std::asin(1.0 - ((pointDist - vR) / innRad)));
                      range  = 2 * innRad * modZ;
                      if (ringHasMass)
                        massZ -= innRad * modZ;
                      else
                        dustZ -= innRad * modZ;
                    }
                  else
                    {
                      // Outer half
                      double outRad = ringStop - ringCent;
                      modZ   = std::cos(std::asin((pointDist - ringCent) / outRad));
                      range  = 2 * outRad * modZ;
                      if (ringHasMass)
                        massZ -= outRad * modZ;
                      else
                        dustZ -= outRad * modZ;
                    }
                } // End of having a pixel in the detonation ring
              else if (pointDist < dR)
                {
                  // Simple. Same as with the inner bounds, but with hR instead of vR
                  isDustPix = true;
                  modZ   = std::cos(std::asin(pointDist / dR));
                  range  = 2 * dR * modZ;
                  dustZ -= dR * modZ;
                } // End of having a pixel in the dust sphere
              // No else, if none of these is true, we are finished now and do not carry on below.

              if (isMassPix || isRemnPix || isRingPix || isDustPix)
                {
                  /* === Step 3: Calculate colmod if this is a mass representing pixel ===
                     ===---------------------------------------------------------------===
                  */

                  if (isMassPix || (ringHasMass && isRingPix))
                    {
                      double edgeDist  = pwx::absDistance(
                                        xOff + (-0.5 *
                                                 (static_cast<int32_t>(yOff) > static_cast<int32_t>(xOff) ?
                                                   xOff / yOff : 1.0)), // Move from left to center the further down the pixel is
                                        yOff + (-0.5 *
                                                 (static_cast<int32_t>(xOff) > static_cast<int32_t>(yOff) ?
                                                   yOff / xOff : 1.0)), // Move from top to center the further right the pixel is
                                        0., 0.); // The center
                      // Note: We calculate the bottom right quarter of an object and mirror the results.
                      if (isMassPix)
                        colMod = 1.0 - ((edgeDist / vR) / 4.) // relative distance to the center
                               + ((vR - pointDist) / 2.); // Real point distance to the radius
                      else
                        {
                          if (pointDist < ringCent)
                            colMod = 1.0 - ((vR / edgeDist) / 4.) // relative distance to the center
                                   + ((pointDist - vR) / 2.); // Real point distance to the radius
                          else
                            colMod = 1.0 - ((edgeDist / ringStop) / 4.) // relative distance to the center
                                   + ((ringStop - pointDist) / 2.); // Real point distance to the radius
                        }

                      // Now that we have modZ and colMod, we can further manipulate colMod
                      if (colMod > 1.00) colMod = 1.00;
                      colMod *= 1.0 - (0.5 - (modZ / 2.));
                      // This means, that the color is lowered by 50% at the outer edge, and 0% at the center

                      // Normalize lower boundary:
                      if (colMod < 0.25) colMod = 0.25;
                    }


                  /* === Step 5: Generate final color and project this pixel ===
                   * ===-----------------------------------------------------===
                  Before we go through the pixels to be drawn, the resulting color has to be determined:
                  */
                  uint8_t baseR = r, baseG = g, baseB = b;

                  if (isMassPix || (isRingPix && ringHasMass))
                    {
                      // Here colMod is needed to be applied to the base color:
                      baseR = static_cast<uint8_t>(pwx_round(static_cast<double>(r) * colMod));
                      baseG = static_cast<uint8_t>(pwx_round(static_cast<double>(g) * colMod));
                      baseB = static_cast<uint8_t>(pwx_round(static_cast<double>(b) * colMod));
                    }

                  for (int32_t i = 0; i < 4; ++i)
                    {
                      env->lock();
                      if (doDraw[i])
                        {
                          // Project mass pixel first
                          if ((isMassPix || (isRingPix && ringHasMass))
                            && isVisible(env, drawX[i], drawY[i], massZ) )
                            {
                              uint8_t currR = baseR;
                              uint8_t currG = baseG;
                              uint8_t currB = baseB;
                              addSimplexOffset(env, drawX[i], drawY[i], massZ, isMassPix, isRingPix, currR, currG, currB);
                              env->projectMass(drawX[i], drawY[i], massZ, currR, currG, currB);
                            } // End of having a mass pixel

                          // Now project the dust pixel if there is one
                          if ((isDustPix || isRemnPix || (isRingPix && !ringHasMass))
                            && isVisible(env, drawX[i], drawY[i], dustZ) )
                            {
                              double currRange = range;
                              addSimplexOffset(env, drawX[i], drawY[i], dustZ, isDustPix || isRemnPix, isRingPix, currRange);

                              // If the range is shortened, dustZ is moved a bit away. We therefore have to check again:
                              if ((currRange > Min_Dust_Range) && isFront(env, drawX[i], drawY[i], dustZ))
                                {
                                  if      (isRemnPix)
                                    result = env->projectDust(drawX[i], drawY[i], dustZ, dustR, dustG, dustB, currRange, centRange);
                                  else if (isRingPix)
                                    result = env->projectDust(drawX[i], drawY[i], dustZ, dustR, dustG, dustB, currRange, ringRange);
                                  else
                                    result = env->projectDust(drawX[i], drawY[i], dustZ, dustR, dustG, dustB, currRange, maxRange);
                                }
                            } // End of having a dust sphere pixel
                        } // End of having a drawable and visible pixel
                      env->unlock();
                    } // End of projection loop
                } // End of being inside the object
            } // End of having at least one drawable pixel
        } // End of y offset loop
    } // End of x offset loop

  return result;
}


/// @brief load a unit from an ifstream
ifstream &CMatter::load (std::ifstream &is)
{
  if (is.good()) {
      bool success = true;
      uint32_t xVers = 2;
      using ::pwx::StreamHelpers::readNextValue;

      is >> xVers; // After this we have semicolon separated values
      if (success) { success = readNextValue (mass,      is, ';', false, false); }
      if (success) { success = readNextValue (radius,    is, ';', false, false); }
      if (success) { success = readNextValue (posX,      is, ';', false, false); }
      if (success) { success = readNextValue (posY,      is, ';', false, false); }
      if (success) { success = readNextValue (posZ,      is, ';', false, false); }
      if (success) { success = readNextValue (impX,      is, ';', false, false); }
      if (success) { success = readNextValue (impY,      is, ';', false, false); }
      if (success) { success = readNextValue (impZ,      is, ';', false, false); }
      if (success) { success = readNextValue (accX,      is, ';', false, false); }
      if (success) { success = readNextValue (accY,      is, ';', false, false); }
      if (success) { success = readNextValue (accZ,      is, ';', false, false); }
      if (success) { success = readNextValue (movX,      is, ';', false, false); }
      if (success) { success = readNextValue (movY,      is, ';', false, false); }
      if (success) { success = readNextValue (movZ,      is, ';', false, false); }
      if (success) { success = readNextValue (ringRadius,is, ';', false, false); }
      if (success) { success = readNextValue (ringMass,  is, ';', false, false); }

      distance = ::pwx::absDistance (posX, posY, posZ, 0., 0., 0.);
    }
  return is;
}

/// @brief save a unit to an ostream
ostream &CMatter::save (std::ostream &os) const
{
  if (os.good())
    {
      os << 2 << ";";
      os << mass       << ";" << radius << ";";
      os << posX       << ";" << posY       << ";" << posZ << ";";
      os << impX       << ";" << impY       << ";" << impZ << ";";
      os << accX       << ";" << accY       << ";" << accZ << ";";
      os << movX       << ";" << movY       << ";" << movZ << ";";
      os << ringRadius << ";" << ringMass;
    }
  return os;
}

/// @brief operator>> to shift a unit from an ifstream
ifstream &operator>> (std::ifstream &is, CMatter &unit)
{
  return unit.load (is);
}

/// @brief operator<< to shift a unit onto an ostream
ostream &operator<< (std::ostream &os, CMatter &unit)
{
  return unit.save (os);
}
