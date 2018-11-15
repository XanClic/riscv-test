#ifndef _COUNTRIES_H
#define _COUNTRIES_H

typedef enum Party {
    RED,
    BLUE,

    PARTY_COUNT
} Party;

typedef enum ContinentID {
    NULL_CONTINENT,

    ERIADOR,
    ROHAN_RHOVANION,
    GONDOR,
    MORDOR,

    CONTINENT_COUNT
} ContinentID;

typedef enum CountryID {
    NULL_COUNTRY,

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

    COUNTRY_COUNT
} CountryID;

typedef struct Country {
    ContinentID continent;
    int neighbor_count; // Set at runtime
    CountryID neighbors[8];

    Party taken_by;
    struct {
        int x;
        int y;
    } troops_pos;
    int troops;
} Country;


extern struct Country countries[COUNTRY_COUNT];


void init_country_list(void);

#endif
