
#include <string.h>
#include <math.h>

#define DEGREE    (PI/180.0)
#ifndef PI
      #define PI        3.1415926535897931160E0      /* pi */
#endif

//  ellipsoid: index into the gEllipsoid[] array, in which
//             a is the ellipsoid semimajor axis
//             invf is the inverse of the ellipsoid flattening f
//  dx, dy, dz: ellipsoid center with respect to WGS84 ellipsoid center
//    x axis is the prime meridian
//    y axis is 90 degrees east longitude
//    z axis is the axis of rotation of the ellipsoid

// The following values for dx, dy and dz were extracted from the output of
// the GARMIN PCX5 program. The output also includes values for da and df, the
// difference between the reference ellipsoid and the WGS84 ellipsoid semi-
// major axis and flattening, respectively. These are replaced by the
// data contained in the structure array gEllipsoid[], which was obtained from
// the Defence Mapping Agency document number TR8350.2, "Department of Defense
// World Geodetic System 1984."

struct DATUM {
        char *name;
        short ellipsoid;
        double dx;
        double dy;
        double dz;
};

struct ELLIPSOID {
        char *name;             // name of ellipsoid
        double a;               // semi-major axis, meters
        double invf;            // 1/f
};


struct DATUM const gDatum[] = {
//     name               ellipsoid   dx      dy       dz
      { "Adindan",                5,   -162,    -12,    206 },    // 0
      { "Afgooye",               15,    -43,   -163,     45 },    // 1
      { "Ain el Abd 1970",       14,   -150,   -251,     -2 },    // 2
      { "Anna 1 Astro 1965",      2,   -491,    -22,    435 },    // 3
      { "Arc 1950",               5,   -143,    -90,   -294 },    // 4
      { "Arc 1960",               5,   -160,     -8,   -300 },    // 5
      { "Ascension Island  58",  14,   -207,    107,     52 },    // 6
      { "Astro B4 Sorol Atoll",  14,    114,   -116,   -333 },    // 7
      { "Astro Beacon  E ",      14,    145,     75,   -272 },    // 8
      { "Astro DOS 71/4",        14,   -320,    550,   -494 },    // 9
      { "Astronomic Stn  52",    14,    124,   -234,    -25 },    // 10
      { "Australian Geod  66",    2,   -133,    -48,    148 },    // 11
      { "Australian Geod  84",    2,   -134,    -48,    149 },    // 12
      { "Bellevue (IGN)",        14,   -127,   -769,    472 },    // 13
      { "Bermuda 1957",           4,    -73,    213,    296 },    // 14
      { "Bogota Observatory",    14,    307,    304,   -318 },    // 15
      { "Campo Inchauspe",       14,   -148,    136,     90 },    // 16
      { "Canton Astro 1966",     14,    298,   -304,   -375 },    // 17
      { "Cape",                   5,   -136,   -108,   -292 },    // 18
      { "Cape Canaveral",         4,     -2,    150,    181 },    // 19
      { "Carthage",               5,   -263,      6,    431 },    // 20
      { "CH-1903",                3,    674,     15,    405 },    // 21
      { "Chatham 1971",          14,    175,    -38,    113 },    // 22
      { "Chua Astro",            14,   -134,    229,    -29 },    // 23
      { "Corrego Alegre",        14,   -206,    172,     -6 },    // 24
      { "Djakarta (Batavia)",     3,   -377,    681,    -50 },    // 25
      { "DOS 1968",              14,    230,   -199,   -752 },    // 26
      { "Easter Island 1967",    14,    211,    147,    111 },    // 27
      { "European 1950",         14,    -87,    -98,   -121 },    // 28
      { "European 1979",         14,    -86,    -98,   -119 },    // 29
      { "Finland Hayford",       14,    -78,   -231,    -97 },    // 30
      { "Gandajika Base",        14,   -133,   -321,     50 },    // 31
      { "Geodetic Datum  49",    14,     84,    -22,    209 },    // 32
      { "Guam 1963",              4,   -100,   -248,    259 },    // 33
      { "GUX 1 Astro",           14,    252,   -209,   -751 },    // 34
      { "Hjorsey 1955",          14,    -73,     46,    -86 },    // 35
      { "Hong Kong 1963",        14,   -156,   -271,   -189 },    // 36
      { "Indian Bangladesh",      6,    289,    734,    257 },    // 37
      { "Indian Thailand",        6,    214,    836,    303 },    // 38
      { "Ireland 1965",           1,    506,   -122,    611 },    // 39
      { "ISTS 073 Astro  69",    14,    208,   -435,   -229 },    // 40
      { "Johnston Island",       14,    191,    -77,   -204 },    // 41
      { "Kandawala",              6,    -97,    787,     86 },    // 42
      { "Kerguelen Island",      14,    145,   -187,    103 },    // 43
      { "Kertau 1948",            7,    -11,    851,      5 },    // 44
      { "L.C. 5 Astro",           4,     42,    124,    147 },    // 45
      { "Liberia 1964",           5,    -90,     40,     88 },    // 46
      { "Luzon Mindanao",         4,   -133,    -79,    -72 },    // 47
      { "Luzon Philippines",      4,   -133,    -77,    -51 },    // 48
      { "Mahe 1971",              5,     41,   -220,   -134 },    // 49
      { "Marco Astro",           14,   -289,   -124,     60 },    // 50
      { "Massawa",                3,    639,    405,     60 },    // 51
      { "Merchich",               5,     31,    146,     47 },    // 52
      { "Midway Astro 1961",     14,    912,    -58,   1227 },    // 53
      { "Minna",                  5,    -92,    -93,    122 },    // 54
      { "NAD27 Alaska",           4,     -5,    135,    172 },    // 55
      { "NAD27 Bahamas",          4,     -4,    154,    178 },    // 56
      { "NAD27 Canada",           4,    -10,    158,    187 },    // 57
      { "NAD27 Canal Zone",       4,      0,    125,    201 },    // 58
      { "NAD27 Caribbean",        4,     -7,    152,    178 },    // 59
      { "NAD27 Central",          4,      0,    125,    194 },    // 60
      { "NAD27 CONUS",            4,     -8,    160,    176 },    // 61
      { "NAD27 Cuba",             4,     -9,    152,    178 },    // 62
      { "NAD27 Greenland",        4,     11,    114,    195 },    // 63
      { "NAD27 Mexico",           4,    -12,    130,    190 },    // 64
      { "NAD27 San Salvador",     4,      1,    140,    165 },    // 65
      { "NAD83",                 11,      0,      0,      0 },    // 66
      { "Nahrwn Masirah Ilnd",    5,   -247,   -148,    369 },    // 67
      { "Nahrwn Saudi Arbia",     5,   -231,   -196,    482 },    // 68
      { "Nahrwn United Arab",     5,   -249,   -156,    381 },    // 69
      { "Naparima BWI",          14,     -2,    374,    172 },    // 70
      { "Observatorio 1966",     14,   -425,   -169,     81 },    // 71
      { "Old Egyptian",          12,   -130,    110,    -13 },    // 72
      { "Old Hawaiian",           4,     61,   -285,   -181 },    // 73
      { "Oman",                   5,   -346,     -1,    224 },    // 74
      { "Ord Srvy Grt Britn",     0,    375,   -111,    431 },    // 75
      { "Pico De Las Nieves",    14,   -307,    -92,    127 },    // 76
      { "Pitcairn Astro 1967",   14,    185,    165,     42 },    // 77
      { "Prov So Amrican  56",   14,   -288,    175,   -376 },    // 78
      { "Prov So Chilean  63",   14,     16,    196,     93 },    // 79
      { "Puerto Rico",            4,     11,     72,   -101 },    // 80
      { "Qatar National",        14,   -128,   -283,     22 },    // 81
      { "Qornoq",                14,    164,    138,   -189 },    // 82
      { "Reunion",               14,     94,   -948,  -1262 },    // 83
      { "Rome 1940",             14,   -225,    -65,      9 },    // 84
      { "RT 90",                  3,    498,    -36,    568 },    // 85
      { "Santo (DOS)",           14,    170,     42,     84 },    // 86
      { "Sao Braz",              14,   -203,    141,     53 },    // 87
      { "Sapper Hill 1943",      14,   -355,     16,     74 },    // 88
      { "Schwarzeck",            21,    616,     97,   -251 },    // 89
      { "South American  69",    16,    -57,      1,    -41 },    // 90
      { "South Asia",             8,      7,    -10,    -26 },    // 91
      { "Southeast Base",        14,   -499,   -249,    314 },    // 92
      { "Southwest Base",        14,   -104,    167,    -38 },    // 93
      { "Timbalai 1948",          6,   -689,    691,    -46 },    // 94
      { "Tokyo",                  3,   -128,    481,    664 },    // 95
      { "Tristan Astro 1968",    14,   -632,    438,   -609 },    // 96
      { "Viti Levu 1916",         5,     51,    391,    -36 },    // 97
      { "Wake-Eniwetok  60",     13,    101,     52,    -39 },    // 98
      { "WGS 72",                19,      0,      0,      5 },    // 99
      { "WGS 84",                20,      0,      0,      0 },    // 100
      { "Zanderij",              14,   -265,    120,   -358 }           // 101
};

struct ELLIPSOID const gEllipsoid[] = {
//      name                               a        1/f
      {  "Airy 1830",                  6377563.396, 299.3249646   },    // 0
      {  "Modified Airy",              6377340.189, 299.3249646   },    // 1
      {  "Australian National",        6378160.0,   298.25        },    // 2
      {  "Bessel 1841",                6377397.155, 299.1528128   },    // 3
      {  "Clarke 1866",                6378206.4,   294.9786982   },    // 4
      {  "Clarke 1880",                6378249.145, 293.465       },    // 5
      {  "Everest (India 1830)",       6377276.345, 300.8017      },    // 6
      {  "Everest (1948)",             6377304.063, 300.8017      },    // 7
      {  "Modified Fischer 1960",      6378155.0,   298.3         },    // 8
      {  "Everest (Pakistan)",         6377309.613, 300.8017      },    // 9
      {  "Indonesian 1974",            6378160.0,   298.247       },    // 10
      {  "GRS 80",                     6378137.0,   298.257222101 },    // 11
      {  "Helmert 1906",               6378200.0,   298.3         },    // 12
      {  "Hough 1960",                 6378270.0,   297.0         },    // 13
      {  "International 1924",         6378388.0,   297.0         },    // 14
      {  "Krassovsky 1940",            6378245.0,   298.3         },    // 15
      {  "South American 1969",        6378160.0,   298.25        },    // 16
      {  "Everest (Malaysia 1969)",    6377295.664, 300.8017      },    // 17
      {  "Everest (Sabah Sarawak)",    6377298.556, 300.8017      },    // 18
      {  "WGS 72",                     6378135.0,   298.26        },    // 19
      {  "WGS 84",                     6378137.0,   298.257223563 },    // 20
      {  "Bessel 1841 (Namibia)",      6377483.865, 299.1528128   },    // 21
      {  "Everest (India 1956)",       6377301.243, 300.8017      }     // 22
};

short nDatums = sizeof(gDatum)/sizeof(struct DATUM);

void datumParams(short datum, double *a, double *es)
{
    extern struct DATUM const gDatum[];
    extern struct ELLIPSOID const gEllipsoid[];

    double f = 1.0 / gEllipsoid[gDatum[datum].ellipsoid].invf;    // flattening
    *es = 2 * f - f * f;                                          // eccentricity^2
    *a = gEllipsoid[gDatum[datum].ellipsoid].a;                   // semimajor axis
}

int GetDatumIndex(const char *str)
{
      int i = 0;
      while (i < 102)
      {
            if(!strcmp(str, gDatum[i].name))
                  return i;
            i++;
      }

      return -1;
}


/* --------------------------------------------------------------------------------- *

 *Molodensky
 *In the listing below, the class GeodeticPosition has three members, lon, lat, and h.
 *They are double-precision values indicating the longitude and latitude in radians,
 * and height in meters above the ellipsoid.

 * The source code in the listing below may be copied and reused without restriction,
 * but it is offered AS-IS with NO WARRANTY.

 * Adapted for opencpn by David S. Register - bdbcat@yahoo.com

 * --------------------------------------------------------------------------------- */

void MolodenskyTransform (double lat, double lon, double *to_lat, double *to_lon, int from_datum_index, int to_datum_index)
{
      double from_lat = lat * DEGREE;
      double from_lon = lon * DEGREE;
      double from_f = 1.0 / gEllipsoid[gDatum[from_datum_index].ellipsoid].invf;    // flattening
      double from_esq = 2 * from_f - from_f * from_f;                               // eccentricity^2
      double from_a = gEllipsoid[gDatum[from_datum_index].ellipsoid].a;             // semimajor axis
      double dx = gDatum[from_datum_index].dx;
      double dy = gDatum[from_datum_index].dy;
      double dz = gDatum[from_datum_index].dz;
      double to_f = 1.0 / gEllipsoid[gDatum[to_datum_index].ellipsoid].invf;        // flattening
      double to_a = gEllipsoid[gDatum[to_datum_index].ellipsoid].a;                 // semimajor axis
      double da = to_a - from_a;
      double df = to_f - from_f;
      double from_h = 0;


      double slat = sin (from_lat);
      double clat = cos (from_lat);
      double slon = sin (from_lon);
      double clon = cos (from_lon);
      double ssqlat = slat * slat;
      double adb = 1.0 / (1.0 - from_f);  // "a divided by b"
      double dlat, dlon, dh;

      double rn = from_a / sqrt (1.0 - from_esq * ssqlat);
      double rm = from_a * (1. - from_esq) / pow ((1.0 - from_esq * ssqlat), 1.5);

      dlat = (((((-dx * slat * clon - dy * slat * slon) + dz * clat)
                  + (da * ((rn * from_esq * slat * clat) / from_a)))
                  + (df * (rm * adb + rn / adb) * slat * clat)))
            / (rm + from_h);

      dlon = (-dx * slon + dy * clon) / ((rn + from_h) * clat);

      dh = (dx * clat * clon) + (dy * clat * slon) + (dz * slat)
                  - (da * (from_a / rn)) + ((df * rn * ssqlat) / adb);

      *to_lon = lon + dlon/DEGREE;
      *to_lat = lat + dlat/DEGREE;
//
      return;
}

 
