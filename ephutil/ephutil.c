#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <math.h>
#include <time.h> /* only needed for time() and localtime() calls */
#include <stdio.h>
#include <novas.h>
#include "ephutil.h"

int get_eph_title(char* out_str, int out_len, const char* eph_name)
{
    ssize_t cnt;
    int fd;
    char *p = out_str;
    fd = open(eph_name, O_RDONLY);
    if (fd >= 0) {
        if ((cnt=read(fd, out_str, out_len-1)) == out_len-1) {
            for (p += cnt; p > out_str && p[-1] == ' '; --p)
                ;
        }
        close(fd);
    }
    *p = 0;
    return (int)(p - out_str);
}

const planet_t the_planets[NBR_OF_PLANETS] = {
    { 1, "Mercury" },
    { 2, "Venus" },
    { 3, "Earth" },
    { 4, "Mars" },
    { 5, "Jupiter" },
    { 6, "Saturn" },
    { 7, "Uranus" },
    { 8, "Neptune" },
    { 9, "Pluto" },
    { 10, "Sun" },
    { 11, "Moon" }
};

double normalize(double val, double period)
{
    double quot = val / period;
    return period * (quot - floor(quot));
}

/*
 * The following table is published by IERS, and is accurate through 31-Dec-2015,
 * (which would be the next opportunity to add a leap second after the one scheduled
 * for 30-Jun-2015).
 *
 * VERY IMPORTANT: Add new leap second entries *before* the dummy record at the end.
 */
static const leap_second_t leap_seconds[] = {
    { 1972, 1,  1, 2441317.5,  10.0 },
    { 1972, 7,  1, 2441499.5,  11.0 },
    { 1973, 1,  1, 2441683.5,  12.0 },
    { 1974, 1,  1, 2442048.5,  13.0 },
    { 1975, 1,  1, 2442413.5,  14.0 },
    { 1976, 1,  1, 2442778.5,  15.0 },
    { 1977, 1,  1, 2443144.5,  16.0 },
    { 1978, 1,  1, 2443509.5,  17.0 },
    { 1979, 1,  1, 2443874.5,  18.0 },
    { 1980, 1,  1, 2444239.5,  19.0 },
    { 1981, 7,  1, 2444786.5,  20.0 },
    { 1982, 7,  1, 2445151.5,  21.0 },
    { 1983, 7,  1, 2445516.5,  22.0 },
    { 1985, 7,  1, 2446247.5,  23.0 },
    { 1988, 1,  1, 2447161.5,  24.0 },
    { 1990, 1,  1, 2447892.5,  25.0 },
    { 1991, 1,  1, 2448257.5,  26.0 },
    { 1992, 7,  1, 2448804.5,  27.0 },
    { 1993, 7,  1, 2449169.5,  28.0 },
    { 1994, 7,  1, 2449534.5,  29.0 },
    { 1996, 1,  1, 2450083.5,  30.0 },
    { 1997, 7,  1, 2450630.5,  31.0 },
    { 1999, 1,  1, 2451179.5,  32.0 },
    { 2006, 1,  1, 2453736.5,  33.0 },
    { 2009, 1,  1, 2454832.5,  34.0 },
    { 2012, 7,  1, 2456109.5,  35.0 },
    { 2015, 7,  1, 2457204.5,  36.0 },
    { 9999, 1,  1, 5373119.5,  36.0 },	// dummy end record
};

/*
 * Find the number of leap seconds for a given JD UTC.
 */
double leapsec_tai_utc(double jd_utc) {
    const leap_second_t* leap;

    for (leap = leap_seconds; leap[1].jd < jd_utc; ++leap)
        ;
    return leap->tai_utc;
}

/*
 * Obtain the JD UTC from the system clock, with accuracy of 1.0 seconds.
 */
double get_jd_utc() {
    short int year = 2000;
    short int month = 1;
    short int day = 1;
    double hour = 0.0;

    time_t now;
    struct tm bdtm;

    time(&now);
    if (gmtime_r(&now, &bdtm) != NULL) {
        year = bdtm.tm_year + 1900;
        month = bdtm.tm_mon + 1;
        day = bdtm.tm_mday;
        hour = (double)(bdtm.tm_hour * 3600 + bdtm.tm_min * 60 + bdtm.tm_sec) / 3600.0;
        printf("UTC: %02d/%02d/%04d hour=%12.6f\n", month, day, year, hour);
    } else {
        printf("Error obtaining local time.\n");
    }
    return julian_date(year,month,day,hour);
}

/*
 * Initialize the time representations in tp, using the current JD UTC and
 * instantaneous offset from UT1.
 */
void make_time_parameters(time_parameters_t* tp, double jd_utc, double ut1_utc) {
    tp->jd_utc = jd_utc;
    tp->leapsecs = leapsec_tai_utc(jd_utc);

    tp->jd_tt = tp->jd_utc + (tp->leapsecs + 32.184) / 86400.0;
    tp->jd_ut1 = tp->jd_utc + ut1_utc / 86400.0;
    tp->delta_t = 32.184 + tp->leapsecs - ut1_utc;
}

extern void const_(char* nam, double* val, double* sss, int* n);

typedef struct eph_const_t_ {
    char nam[8];
    double val;
} eph_const_t;

static int cmp_eph_const(const void *p1, const void *p2) {
    return strcmp(((const eph_const_t *)p1)->nam, ((const eph_const_t *)p2)->nam);
}

static eph_const_t* eph_consts = NULL;
static int nbr_of_eph_consts = 0;
static double eph_jd[3];

void init_eph_const() {
    const int cdim = 600;
    const int nam_len = 6;

    free_eph_const();
    char* my_nam = (char*)malloc(cdim * nam_len);
    double* my_val = (double*)malloc(cdim * sizeof(double));
    const_(my_nam, my_val, eph_jd, &nbr_of_eph_consts);

    if (nbr_of_eph_consts > 0 && nbr_of_eph_consts < cdim) {
	eph_consts = (eph_const_t*)malloc(sizeof(eph_const_t) * nbr_of_eph_consts);
	if (eph_consts == NULL) {
	    printf("Error allocating ephemeris constants.\n");
	    nbr_of_eph_consts = 0;
	    goto out;
	}

	int i;
	char *p = my_nam;
	eph_const_t* dest = eph_consts;
	for (i=0; i < nbr_of_eph_consts; ++i) {
	    memset(dest->nam, 0, sizeof(dest->nam));
	    get_rtrim(dest->nam, sizeof(dest->nam), p, p + nam_len);
	    dest->val = my_val[i];
	    ++dest;
	    p += nam_len;
	}
	qsort(eph_consts, nbr_of_eph_consts, sizeof(eph_const_t), cmp_eph_const);
    } else {
	nbr_of_eph_consts = 0;
    }

out:
    free(my_val);
    free(my_nam);
}

void free_eph_const() {
    if (eph_consts != NULL) {
	free(eph_consts);
	eph_consts = NULL;
    }
    nbr_of_eph_consts = 0;
}

#if !defined(NAN)
#error NAN is not defined
#endif

double get_eph_const(const char* nam) {
    eph_const_t key, *res;
    if (eph_consts == NULL || nbr_of_eph_consts == 0) {
	return NAN;
    }
    int nam_len = strlen(nam);
    if (0 < nam_len && nam_len <= 6) {
	strcpy(key.nam, nam);
	res = bsearch(&key, eph_consts, nbr_of_eph_consts, sizeof(eph_const_t), 
		cmp_eph_const);
	if (res != NULL) {
	    return res->val;
	}
    }
    return NAN;
}
