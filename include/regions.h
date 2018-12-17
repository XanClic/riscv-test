#ifndef _REGIONS_H
#define _REGIONS_H

#define HAVE_NEUTRAL

typedef enum Party {
    RED,
    BLUE,
#ifdef HAVE_NEUTRAL
    GRAY,
#endif

    PARTY_COUNT
} Party;

#define PLAYER RED

#ifdef HAVE_NEUTRAL
#define NEUTRAL GRAY
#endif

typedef enum ContinentID {
    NULL_CONTINENT,

    AKHEN,
    ANGSHIRE,
    TSUKIMOTO,
    XINDAO,

    CONTINENT_COUNT
} ContinentID;

typedef enum RegionID {
    NULL_REGION,

    // Arkhen
    NORGROST,
    LUNEN,
    BARLENIEN,
    SEEBOR,

    // Angshire
    BOTFORD,
    CWOM,
    PETONY,
    SUNDERLO,
    UXBRID,

    // Tsukimoto
    KYUUEMPI,
    SHOUZU,
    KUNUCHI,
    HIRANAGA,

    // Xindao
    YUAN_DOU,
    MU_ZHANG,
    XIAMO,

    REGION_COUNT
} RegionID;

typedef struct Region {
    const char *name;
    ContinentID continent;
    int neighbor_count; // Set at runtime
    RegionID neighbors[8];

    Party taken_by;
    struct {
        int x;
        int y;
    } troops_pos;
    int troops;
} Region;


extern struct Region regions[REGION_COUNT];
extern int continent_bonuses[CONTINENT_COUNT];


void init_region_list(void);

#endif
