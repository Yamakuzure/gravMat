#pragma once
#ifndef GRAVMAT_DUSTPIXEL_H_INCLUDED
#define GRAVMAT_DUSTPIXEL_H_INCLUDED 1

#include "masspixel.h"

/* Note on minimum dust sphere ranges:
 * Dust spheres are transparent and must not be less then 1/255 of a set maximum range.
 * The set maximum range is needed, because it is difficult to compare floating
 * point values. Without a limit an endless loop might be created when trying
 * to split a very small part of a dust sphere because the comparison says that they
 * overlap while they don't.
 * For the transparency the range of a dust sphere compared to the maximum range set is
 * taken. As colors are RGB values with one byte each, a ratio of 1/255 of the
 * dust sphere range to its maximum range would mean, that any of RGB if set to 255 would
 * become 1. Any ratio below this would set the color part to zero, meaning the
 * full color, even bright white, would become black and therefore invisible.
 * This ratio of 1/255 is used for the hard coded limits, when calculating the
 * transparency later a soft limit of 0.5% (ratio 1/200) is used instead.
*/
/// @brief The minimum allowed maximum range for dust sphere pixels
const double Min_Dust_MaxRange = 0.0001; // Any max range below this is simply non-existent.

/// @brief The minimum allowed range for dust sphere pixels
const double Min_Dust_Range = Min_Dust_MaxRange / 255.; // Any range below this is simply invisible.

/** @brief simple struct for information on pixels representing dust spheres.
  *
  * This simple struct derives from sMassPixel and adds a pointer to
  * allow chains of dust sphere pixels that are ordered by their z position.
  * Additionally thickness of the dust sphere at this position and its opacity
  * are recorded.
  *
  * The dust sphere pixels are stored as a "blind backside queue". This means, that
  * the queue is filled from back to front, only adding new dust sphere pixels if
  * the queue is full. This technique has three big advantages. A) A look at
  * the root item is enough to tell whether the queue is full or not. B) We
  * never need a pointer to the previous item, neither temporary nor fixed.
  * C) All handling of the data can be kept extremely simple. This reduces
  * memory usage and increases speed. Having to go through a large queue to
  * find the starting dust sphere pixel when there is none, would waste time. To
  * see at once whether there actually is a relevant dust sphere pixel, we use the
  * following trick: Invalidated dust sphere pixels have a z value of -1. But the
  * root pixel, the one that is stored in zDustMap, is invalidated by a value
  * of -2 until the first drawable pixel is stored. With this a quick look at
  * the beginning of the queue tells us whether there is anything relevant for
  * drawing.
  *
  * The opacity of a dust sphere pixel is calculated from the dust sphere thickness at
  * this very position and the maximum thickness the dust sphere can have in total.
  * With these two values we can: A) Split dust spheres into two areas when a minor dust sphere
  * moves into this one, B) see quickly if the dust sphere is complete or shortened, and
  * C) calculate an exact opacity value. This looks a bit like overkill. But the
  * dust sphere is a dust sphere illuminated by the matter unit. Thus when a dust sphere moves
  * into another, the dust density must be added up at this length to have a
  * correct result.
  **/
struct sDustPixel : public sMassPixel
  {
    double      range;    //!< range this dust sphere pixel reaches into space
    double      maxRange; //!< maximum range of the full dust sphere (aka dust sphere radius). Opacity then is range/maxRange.
    sDustPixel *next;     //!< Chain start

    explicit sDustPixel(): sMassPixel(), range(-1.0), maxRange(0.0), next(NULL)
      { /* nothing to be done here */ }

    /// @brief dtor that deletes the whole chain!
    ~sDustPixel()
      {
        if (next)
          delete next; // It will delete its next and that its next and so on.
      } // End of dtor

    /// @brief Invalidate it all
    void invalidate()
      {
        sMassPixel::invalidate();
        range    = -1.0;
        maxRange =  0.0;
      }

    /** @brief set all values or invalidate all
      * To invalidate set @a aZ to anything not larger than zero
      * or either @a aRange or @a aMaxRange or both to anything
      * not larger than Min_Dust_Range
    **/
    void setAll(double aZ, uint8_t aR, uint8_t aG, uint8_t aB, double aRange, double aMaxRange)
      {
        if ((aZ > 0.) && (aRange > Min_Dust_Range) && (aMaxRange > Min_Dust_MaxRange))
          {
            sMassPixel::setAll(aZ, aR, aG, aB);

            range = aRange;
            maxRange = aMaxRange;
          }
        else
          invalidate();
      }

    /// @brief Copy by construction, next is not copied
    sDustPixel(sDustPixel &rhs): sMassPixel(rhs),
      range(rhs.range), maxRange(rhs.maxRange), next(NULL)
      {  }

    /// @brief Copy by assignment, next is not changed
    sDustPixel &operator=(sDustPixel &rhs)
      {
        if (&rhs != this)
          {
            sMassPixel::operator=(rhs);
            range    = rhs.range;
            maxRange = rhs.maxRange;
          }

        return *this;
      }
  };

#endif // GRAVMAT_DUSTPIXEL_H_INCLUDED

