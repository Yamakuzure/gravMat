#pragma once
#ifndef PWX_GRAVMAT_COLORMAP_H_INCLUDED
#define PWX_GRAVMAT_COLORMAP_H_INCLUDED 1

#include <pwxLib/CWaveColor.h>

struct sColorData {
  double lowM; //!< lower border mass in MJ
  double uppM; //!< upper border mass in MJ
  double loDR, upDR, loMR, upMR; //!< upper and lower border [D]ust [R]adius modifier and [M]ax [R]ange modifier
  uint8_t loR, loG, loB; //!< RGB parts of the lower border color
  uint8_t upR, upG, upB; //!< RGB parts of the upper border color

  /// simple ctor, no controls, just pump 'em in
#if defined(PWX_HAS_CXX11_INIT)
  explicit
#endif // has PWX_HAS_CXX11_INIT
  sColorData(double lM, double uM, double lDR, double uDR, double lMR, double uMR,
             uint8_t lR, uint8_t lG, uint8_t lB, uint8_t uR, uint8_t uG, uint8_t uB):
    lowM(lM), uppM(uM), loDR(lDR), upDR(uDR), loMR(lMR), upMR(uMR), loR(lR), loG(lG), loB(lB), upR(uR), upG(uG), upB(uB)
    { }

#if !defined(PWX_HAS_CXX11_INIT)
  /// Empty ctor, only needed when the new extended initializer list is not supported
  sColorData() { /* nothing here, use init()! */ }

  /// Simple initialization, only needed ... well, you got it, right?
  void init(double lM, double uM, double lDR, double uDR, double lMR, double uMR,
            uint8_t lR, uint8_t lG, uint8_t lB, uint8_t uR, uint8_t uG, uint8_t uB)
    {
      lowM = lM;
      uppM = uM;
      loDR = lDR;
      upDR = uDR;
      loMR = lMR;
      upMR = uMR;
      loR  = lR;
      loG  = lG;
      loB  = lB;
      upR  = uR;
      upG  = uG;
      upB  = uB;
    }
#endif /// has no PWX_HAS_CXX11_INIT

  /** @brief get color by mass/MJ
    * @param[in] massInMJ The mass of the object in MJ
    * @param[out] dRMod the resulting dust range modifier, the higher the mass, the smaller the dust sphere
    * @param[out] dMMod the resulting dust max range modifier, the lower the mass, the denser the dust sphere.
    * @param[out] r the resulting red part of the color
    * @param[out] g the resulting green part of the color
    * @param[out] b the resulting blue part of the color
    * @return true if the mass fits this entries lower and upper bounds, false otherwise
  **/
  bool getWeightedColor (double massInMJ, double* dRMod, double* dMMod, uint8_t &r, uint8_t &g, uint8_t &b)
    {
      if (pwx::isBetween(massInMJ, lowM, uppM))
        {
          double dist   = uppM - lowM;
          double modLow = (massInMJ - lowM) / dist;
          double modUp  = (uppM - massInMJ) / dist;

          // Determine dRMod and dMMod
          if (dRMod)
            *dRMod = (modLow * loDR) + (modUp * upDR);
          if (dMMod)
            *dMMod = (modLow * loMR) + (modUp * upMR);

          // Add up the colors for the final RGB;
          int32_t red   = static_cast<int32_t>(pwx_round(modLow * loR) + pwx_round(modUp * upR));
          int32_t green = static_cast<int32_t>(pwx_round(modLow * loG) + pwx_round(modUp * upG));
          int32_t blue  = static_cast<int32_t>(pwx_round(modLow * loB) + pwx_round(modUp * upB));

          // Give clipped colors back
          r = red   < 0 ? 0 : red   > 255 ? 255 : static_cast<uint8_t>(red);
          g = green < 0 ? 0 : green > 255 ? 255 : static_cast<uint8_t>(green);
          b = blue  < 0 ? 0 : blue  > 255 ? 255 : static_cast<uint8_t>(blue);

          return true;
        }
      return false;
    }
};


class CColorMap : public pwx::Lockable, private pwx::Uncopyable
{
  const double c;      //!< Speed of light in m/s, used only here thus placed here to keep env and universe out
  const double pcInM;  //!< Parsec in meters, used only here thus placed here to keep env and universe out
  const double MJ;     //!< Mass of Jupiter, used only here thus placed here to keep env and universe out
  sColorData   CD[13]; //!< Array of color data entries
  const size_t size;   //!< Size of CD

public:
  /** @brief default ctor
    * @param[in] KgMassOne Amount of kg a unit of mass one has. This is the base to calculate the color map
  **/
  explicit CColorMap () : c(299792458.0), pcInM(30.857e15), MJ(1.90e27),
#if defined(PWX_HAS_CXX11_INIT)
      /* Order of the colors:
       * The ordering uses the main sequence of stars according to Morgan-Keenan.
       * Note: The base is MJ, which wich will be used to determine the internal class of an object be lower and upper
       *       bounds. The need of both bounds for each class is based upon the fact the main sequence star classes
       *       might overlap.
      */
      CD({ sColorData(
        /* - Giant planets
         *     This is a "trick" to have a valid color if it is decided to start with lower masses than fit into Class T
        */
        0.0, 13., 5., 3., 1.5, 2., 0x10, 0x02, 0x01, 0x60, 0x10, 0x10
      ), sColorData(
        /* - Class T: methane dwarfs
         *     These are cool brown dwarfs, their emission peaks in the infrared. Methane is prominent in their spectra. Brown
         *     dwarfs occupy the mass range between that of large gas giant planets and the lowest mass stars; this upper limit
         *     is between 75 and 80 Jupiter masses (MJ) (75 MJ = 1.425e+29 kg)
        */
        13.0, 80., 3., 2.5, 2., 2.1, 0x60, 0x10, 0x10, 0x80, 0x30, 0x18
      ), sColorData(
        /* - Class L
         *     These dwarfs get their designation because they are cooler than M stars and L is the remaining letter
         *     alphabetically closest to M. L does not mean lithium dwarf. They are a very dark red in color and brightest in
         *     infrared. (This is the range of 75 MJ (Class T) to 135 MJ (Class M), which shall close the gap between pure red
         *     (T) and brown (M))
        */
        75., 135., 2.5, 2.3, 2.1, 2.2, 0x78, 0x28, 0x20, 0x80, 0x60, 0x30
      ), sColorData(
        /* - Class M
         *     Class M is by far the most common class. About 76% of the main sequence stars in the solar neighborhood are Class
         *     M stars. Although most Class M stars are red dwarfs, the class also hosts most giants and some supergiants such as
         *     Antares and Betelgeuse, as well as Mira variables. Here we use only red dwarfs, as the color map is meant to
         *     connect ascending mass class with ascending color classes. Red dwarfs are very low mass stars with no more than
         *     40% of the mass of the Sun. ( Sun = 1.99e+30 kg -> 40% = 7.96e+29 kg or ~418.95 MJ. ) Range is brown to red
        */
        135., 418.95, 2.3, 2.1, 2.2, 2.3, 0x80, 0x60, 0x30, 0xE0, 0x20, 0x28
      ), sColorData(
        /* - Class K
         *     These orange dwarfs are intermediate in size between red M-type main sequence stars and yellow G-type main sequence
         *     stars. They have masses of from 0.5 to 0.8 times the mass of the Sun.
         *     ( 50% => 9.95e+29 kg (523.68 MJ) to 80% => 1.592e+30 kg (837.89 MJ) - The gap from 419 to 524MJ is filled with a
         *       color shift from red to dark orange )
        */
        // Note: This is the said gap:
        418.95, 523.68, 2.1, 2.0, 2.3, 2.4, 0xE0, 0x20, 0x28, 0xC0, 0x90, 0x00
      ), sColorData(
        // Note: And this is class K
        523.68, 837.89, 2.0, 1.2, 2.4, 2.5, 0xC0, 0x90, 0x00, 0xFF, 0xD8, 0x00
      ), sColorData(
        /* - Class G
         *     These main sequence stars of spectral type G have about 0.8 to 1.2 solar masses.
         *     ( 80% => 1.592e+30 kg (837.89 MJ) to 120% => 2.28e+30 (1200 MJ) , shift from light orange to bright yellow. )
        */
        837.89, 1200., 1.2, 0.8, 2.5, 2.5, 0xFF, 0xD8, 0x00, 0xFF, 0xFF, 0x60
      ), sColorData(
        /* - Class F
         *     Class F stars have strengthening H and K lines of Ca II. Neutral metals (Fe I, Cr I) beginning to gain on ionized
         *     metal lines by late F. Their spectra are characterized by the weaker hydrogen lines and ionized metals.
         *     Their color is white and they have from 1.0 to 1.4 times the mass of the Sun
         *     ( 100% => 1.99e30 kg (1,047,37 MJ) to 140% => 2.786e+30 kg (1,400 MJ), this is a fade from yellow to white. )
        */
        1047.37, 1400., 0.8, 0.6, 2.5, 3.0, 0xFF, 0xF0, 0x40, 0xFF, 0xFF, 0xFF
      ), sColorData(
        /* - Class A
         *     Class A stars are amongst the more common naked eye stars, and are white or bluish-white. They have strong
         *     hydrogen lines, at a maximum by A0, and also lines of ionized metals (Fe II, Mg II, Si II) at a maximum at A5.
         *     They have masses from 1.4 to 2.1 times the mass of the Sun.
         *     ( 140% => 2.786e+30 kg (1,400 MJ) to 210% => 4.179e+30 kg (2,199.47 MJ) - they fade from white to light blue )
        */
        1400., 2199.47, 0.6, 0.4, 3.0, 4.0, 0xFF, 0xFF, 0xFF, 0xE8, 0xE8, 0xFF
      ), sColorData(
        /* - Class B
         *     These stars are extremely luminous and blue, they have from 2 to 16 times the mass of the Sun.
         *     ( 200% => 3.98e+30 kg (2,094.74 MJ) to 1600% => 3.04e+31 kg (16,000 MJ)
         *       Note: The bluish-white of Class A is brought to blue up in a gap to 3,000 MJ, the color is blue then. )
        */
        // Note: this is the fast change to blue
        2094.74, 3000., 0.4, 0.1, 4.0, 6.0, 0xF4, 0xF4, 0xFF, 0xA0, 0xC8, 0xFF
      ), sColorData(
        // Note: This is the final slow blue uprise
        3000., 16000., 0.1, 0.0, 6.0, 20.0, 0xA0, 0xC8, 0xFF, 0x90, 0xA0, 0xFF
      ), sColorData(
        /* - Class O
         *     An O-type main sequence star is a main sequence having between 15 and 90 times the mass of the Sun.
         *     ( 1500% => 2.985e+31 kg (15710,53 MJ) - 9000% => 1.791e32 kg (94,263.16 MJ) - a Fade to strong blue )
        */
        15710.53, 94263.16, 0., 0., 20., 20., 0x98, 0xA4, 0xFF, 0x00, 0x60, 0xFF
      ), sColorData(
        // Note: this is no class but a possibility to fill the super masses that the whole lot might collapse into:
        // Resolution 1600 x 1200 with neither halfX nor halfY results in 1,920,000 Units, every unit has 15MJ (per default)
        // and thus the upper limit is: 28,800,000 - To be damn sure the upper limit is fixed on 50M.
        94263.16, 50.0e6, 0., 0., 20., 20., 0x00, 0x60, 0xFF, 0x40, 0x10, 0x60
      ) }),
#endif // PWX_HAS_CXX11_INIT
      size(13)
    {
#if !defined(PWX_HAS_CXX11_INIT)
      /* unfortunately MSVC++10 does not support the new extended initializer
       * lists. Therefore it is done "on foot", documentation is above.
      */
        CD[0].init(0.0, 13., 5., 3., 1.5, 2., 0x10, 0x02, 0x01, 0x60, 0x10, 0x10);
        CD[1].init(13.0, 80., 3., 2.5, 2., 2.1, 0x60, 0x10, 0x10, 0x80, 0x30, 0x18);
        CD[2].init(75., 135., 2.5, 2.3, 2.1, 2.2, 0x78, 0x28, 0x20, 0x80, 0x60, 0x30);
        CD[3].init(135., 418.95, 2.3, 2.1, 2.2, 2.3, 0x80, 0x60, 0x30, 0xE0, 0x20, 0x28);
        CD[4].init(418.95, 523.68, 2.1, 2.0, 2.3, 2.4, 0xE0, 0x20, 0x28, 0xC0, 0x90, 0x00);
        CD[5].init(523.68, 837.89, 2.0, 1.2, 2.4, 2.5, 0xC0, 0x90, 0x00, 0xFF, 0xD8, 0x00);
        CD[6].init(837.89, 1200., 1.2, 0.8, 2.5, 2.5, 0xFF, 0xD8, 0x00, 0xFF, 0xFF, 0x60);
        CD[7].init(1047.37, 1400., 0.8, 0.6, 2.5, 3.0, 0xFF, 0xF0, 0x40, 0xFF, 0xFF, 0xFF);
        CD[8].init(1400., 2199.47, 0.6, 0.4, 3.0, 4.0, 0xFF, 0xFF, 0xFF, 0xE8, 0xE8, 0xFF);
        CD[9].init(2094.74, 3000., 0.4, 0.1, 4.0, 6.0, 0xF4, 0xF4, 0xFF, 0xA0, 0xC8, 0xFF);
        CD[10].init(3000., 16000., 0.1, 0.0, 6.0, 20.0, 0xA0, 0xC8, 0xFF, 0x90, 0xA0, 0xFF);
        CD[11].init(15710.53, 94263.16, 0., 0., 20., 20., 0x98, 0xA4, 0xFF, 0x00, 0x60, 0xFF);
        CD[12].init(94263.16, 50.0e6, 0., 0., 20., 20., 0x00, 0x60, 0xFF, 0x40, 0x10, 0x60);
#endif // PWX_HAS_CXX11_INIT not defined
    }

  ~CColorMap() { /* nothing to be done */ }

  /** @brief The one and only getter method
    * @param[in] mass The mass in kg
    * @param[in] movZ The current z-movement in m/s
    * @param[in] distZ The distance from the camera in *meters*
    * @param[out] dRMod the resulting dust range modifier, the higher the mass, the smaller the dust sphere
    * @param[out] dMMod the resulting dust max range modifier, the lower the mass, the denser the dust sphere.
    * @param[out] r red portion of the resulting color
    * @param[out] g green portion of the resulting color
    * @param[out] b blue portion of the resulting color
    * @param[in] aGamma the resulting color shall be applied this gamma
  **/
  void mkColor (double mass, double movZ, double distZ, double* dRMod, double* dMMod,
                uint8_t &r, uint8_t &g, uint8_t &b, double aGamma)
  {
    assert ( (mass >= 0) && "ERROR: getColor needs a positive mass!");

    // First, we need the MJ:
    double massInMJ = mass / MJ;

    // Second, the right index and thus the base color RGB are to be found:
    uint8_t red = CD[size - 1].loR, green = CD[size - 1].loG, blue = CD[size - 1].loB;
    for (size_t i = 0; (i < size) && !CD[i].getWeightedColor(massInMJ, dRMod, dMMod, red, green, blue); ++i) ;

    // We'll work with a CWaveColor instance here:
    pwx::CWC::CWaveColor WC(red, green, blue);

    /* Third, we need the color modifications according to the Z-Movement
     * A matter unit that moves towards the camera has its light shifted towards
     * ultraviolet, while a unit that moves away has it shifted to infrared.
     *
     * Although there is a cosmological red shift and a gravitational, the only
     * relevant effect here is the Doppler effect out of pure simplicity. The
     * effect is very simple to calculate for our situation where the camera is
     * fixed. Further we ignore x and y movement.
     *
     * fE = fS / (1 - (v / c))
     * fE : resulting frequency
     * fS : original frequency
     * c  : speed of light
     * v  : speed of the unit (= movZ)
     * Note: We need to negate movZ, as the effect is the wrong way round with a
     *       negative movZ representing a movement to the camera!
    */
    for (size_t i = 0; i < WC.size(); ++i)
      {
        double wl = WC.getFrequency(i) / (1. - (-movZ / c));
        // We have to make sure the wavelength/frequency does not shift out of the visible range:
        if (wl > 788927.522) wl = 788927.522; // c / 788927.522 ~= 380 nm, the lower wavelength boundary
        if (wl < 384349.306) wl = 384349.306; // c / 384349,306 ~= 780 nm, the upper wavelength boundary
        WC.setFrequency(i, wl);
      }
    // for negative movZ fE is > fS => shift to violet,
    // for positive movZ fE is < fS => shift to red

    /* Personal Note: I do doubt that this frequency shifting will result in anything
     * clearly visible. The movements are too low for a prominent Doppler effect
     * unless the shifting changes the mixture. Perhaps, later, we could add
     * gravitation effect and/or a relativity effect.
    */

    // Now set the gamma value according to distZ and xGamma
    /* With
     * r = distance in parsec,
     * M = absolute magnitude and
     * m = apparent magnitude the distance module is:
     *    log10(r) = (m - M + 5) / 5 | * 5
     * => log10(r) * 5 = m - M + 5   | + M, - 5
     * => log10(r) * 5 + M - 5 = m   | With M = 0
     * => log10(r) * 5 - 5 = m
     * m from an M that is zero is more than enough, no real apparent magnitude is
     * needed, as we only want a *slight* gamma shift if a unit is more than 1pc away.
    */
    double pcDist = distZ / pcInM; // results in the distance in parsecs
    double xGamma = aGamma;

    // Only distance of more than one parsec are calculated:
    if (pcDist > 1.0)
      {
        double appMag = (5. * std::log10(pcDist)) - 5.; // Normalized for absolute magnitude 0.0
        xGamma *= 1.0 - (appMag / 100.);
      }
    WC.setGamma(xGamma);

    // Finally we get our target colors:
    WC.getRGB(r, g, b);
  }
};


#endif // PWX_GRAVMAT_COLORMAP_H_INCLUDED
