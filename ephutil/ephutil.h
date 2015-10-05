#if !defined(EPHUTIL_H_)
#define EPHUTIL_H_

/*
coord_sys:
0 ... GCRS or "local GCRS"
1 ... true equator and equinox of date
2 ... true equator and CIO of date
3 ... astrometric coordinates, i.e., without light deflection or aberration.
*/

enum {
    coord_gcrs,
    coord_equ,
    coord_cio,
    coord_astro
};

enum {
    pln_mercury=1,
    pln_venus,
    pln_earth,
    pln_mars,
    pln_jupiter,
    pln_saturn,
    pln_uranus,
    pln_neptune,
    pln_pluto,
    pln_sun,
    pln_moon
};

typedef struct planet_t_ {
    int id;
    const char* name;
} planet_t;

typedef struct leap_second_t_ {
    int year;
    int month;
    int day;
    double jd;
    double tai_utc;
} leap_second_t;

/*
 * While using NOVAS, you will need access to various representations
 * of time. We place all in one structure to make it easier to maintain.
 */
typedef struct time_parameters_t_ {
    double jd_utc;
    double leapsecs;
    double jd_tt;
    double jd_ut1;
    double delta_t;
} time_parameters_t;

#define DMS_MAX 14
#define HMS_MAX 13

double get_jd_utc();
void make_time_parameters(time_parameters_t* tp, double jd_utc, double ut1_utc);
char *as_dms(char* buf, double val);
char *as_hms(char* buf, double val);

void get_rtrim(char* out, size_t max_out, const char* in_beg, const char* in_end);
int get_int(const char* cp, size_t len);
double get_double(const char* cp, size_t len);

#define NBR_OF_PLANETS 11
const planet_t the_planets[NBR_OF_PLANETS];

void init_eph_const();
void free_eph_const();
double get_eph_const(const char* nam);

/*
 * Read the title string that appears at the beginning of every JPL binary ephemeris
 * file.
 */
int get_eph_title(char* out_str, int out_len, const char* eph_name);

/* normalize a periodic value
 * return a double in the range [0.0, period) which represents
 * the "phase" of val. For example, normalize(theta, 2*PI) will
 * return the phase of theta in radians. normalize( julian_date, 7.0 )
 * will return the day-of-the-week for the given julian_date.
 */
double normalize(double val, double period);

double leapsec_tai_utc(double jd_utc);
#endif
