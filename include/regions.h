#ifndef _REGIONS_H
#define _REGIONS_H

typedef enum Party {
    RED,
    BLUE,

    PARTY_COUNT
} Party;

#define PLAYER RED

typedef enum ContinentID {
    NULL_CONTINENT,

    ERIADOR,
    ROHAN_RHOVANION,
    GONDOR,
    MORDOR,

    CONTINENT_COUNT
} ContinentID;

typedef enum RegionID {
    NULL_REGION,

    // Eriador
    MITHLOND,
    FORNARNOR,
    SHIRE,
    CARDOLAN,
    IMLADRIS,
    ANGRENOST,

    // Rohan-Rhovanion
    TAUR_E_NDAEDALOS,
    LOTHLORIEN,
    FANGORN,
    ROHAN,
    RHOVANION,

    // Gondor
    PINNATH_GELIN,
    MINAS_TIRITH,
    ITHILIEN,

    // Mordor
    GORGOROTH,
    NURN,

    REGION_COUNT
} RegionID;

typedef struct Region {
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
