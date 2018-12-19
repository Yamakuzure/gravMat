#pragma once
#ifndef GRAVMAT_MASSPIXEL_H_INCLUDED
#define GRAVMAT_MASSPIXEL_H_INCLUDED 1


/** @brief simple struct for information on pixels representing masses.
  *
  * Instead of having display information in different places, and thus
  * partly only temporary, this simple struct helps to keep everything
  * together.
  * Instead of drawing masses, they are projected and their mass pixels
  * are recorded. Masses and dust spheres then can be put together in a second
  * run, with exact color information instead of some kind of estimation.
  *
  * This struct only holds the color and z value. sDustPixel, which
  * derives from this struct, adds some more, dust sphere specific, information,
  * that are not needed here.
**/
struct sMassPixel
  {
    uint8_t r; //!< Red value of this pixels color
    uint8_t g; //!< Green value of this pixels color
    uint8_t b; //!< Blue value of this pixels color
    double  z; //!< Position on a virtual z-axis, where 0.0 is the camera itself, and everything visible is larger than 0.0

    explicit sMassPixel():r(0),g(0),b(0),z(-2.0) { }
    virtual ~sMassPixel() { }

    /// @brief Invalidate it all
    virtual void invalidate()
      {
        // Note: We can't use setAll(), it would create an endless recursion.
        z = -1.0;
        r = 0;
        g = 0;
        b = 0;
      }

    /// @brief set all values or invalidate by setting @a aZ to anything not larger than zero
    virtual void setAll(double aZ, uint8_t aR, uint8_t aG, uint8_t aB)
      {
        if (aZ > 0.)
          {
            z = aZ;
            r = aR;
            g = aG;
            b = aB;
          } // End of having legal values
        else
          invalidate();
      }

    /// @brief Copy by assignment
    sMassPixel &operator=(sMassPixel &rhs)
      {
        if (&rhs != this)
          {
            z = rhs.z;
            r = rhs.r;
            g = rhs.g;
            b = rhs.b;
          }

        return *this;
      }
  };

#endif // GRAVMAT_MASSPIXEL_H_INCLUDED

