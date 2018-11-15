#include <assert.h>
#define IN_COUNTRIES_C
#include <countries.h>
#include <nonstddef.h>
#include <stdbool.h>


struct Country countries[COUNTRY_COUNT] = {
    [MITHLOND] = {
        .continent = ERIADOR,
        .neighbors = { FORNARNOR, SHIRE, CARDOLAN },
        .troops_pos = { 170, 229 },
    },

    [FORNARNOR] = {
        .continent = ERIADOR,
        .neighbors = { MITHLOND, SHIRE, CARDOLAN, IMLADRIS,
                       TAUR_E_NDAEDALOS },
        .troops_pos = { 480, 156 },
    },

    [SHIRE] = {
        .continent = ERIADOR,
        .neighbors = { MITHLOND, FORNARNOR, CARDOLAN },
        .troops_pos = { 385, 303 },
    },

    [CARDOLAN] = {
        .continent = ERIADOR,
        .neighbors = { MITHLOND, FORNARNOR, SHIRE, IMLADRIS, ANGRENOST },
        .troops_pos = { 460, 373 },
    },

    [IMLADRIS] = {
        .continent = ERIADOR,
        .neighbors = { FORNARNOR, CARDOLAN, ANGRENOST,
                       TAUR_E_NDAEDALOS, LOTHLORIEN },
        .troops_pos = { 653, 313 },
    },

    [ANGRENOST] = {
        .continent = ERIADOR,
        .neighbors = { CARDOLAN, IMLADRIS,
                       LOTHLORIEN, FANGORN, ROHAN, PINNATH_GELIN },
        .troops_pos = { 525, 550 },
    },


    [TAUR_E_NDAEDALOS] = {
        .continent = ROHAN_RHOVANION,
        .neighbors = { FORNARNOR, IMLADRIS,
                       LOTHLORIEN, RHOVANION },
        .troops_pos = { 861, 252 },
    },

    [LOTHLORIEN] = {
        .continent = ROHAN_RHOVANION,
        .neighbors = { IMLADRIS, ANGRENOST,
                       TAUR_E_NDAEDALOS, FANGORN, ROHAN, RHOVANION },
        .troops_pos = { 718, 431 },
    },

    [FANGORN] = {
        .continent = ROHAN_RHOVANION,
        .neighbors = { ANGRENOST,
                       LOTHLORIEN, ROHAN },
        .troops_pos = { 684, 516 },
    },

    [ROHAN] = {
        .continent = ROHAN_RHOVANION,
        .neighbors = { ANGRENOST,
                       LOTHLORIEN, FANGORN, RHOVANION,
                       PINNATH_GELIN, MINAS_TIRITH, ITHILIEN },
        .troops_pos = { 766, 606 },
    },

    [RHOVANION] = {
        .continent = ROHAN_RHOVANION,
        .neighbors = { TAUR_E_NDAEDALOS, LOTHLORIEN, ROHAN,
                       ITHILIEN,
                       GORGOROTH, NURN },
        .troops_pos = { 1141, 395 },
    },


    [PINNATH_GELIN] = {
        .continent = GONDOR,
        .neighbors = { ANGRENOST,
                       ROHAN,
                       MINAS_TIRITH },
        .troops_pos = { 626, 712 },
    },

    [MINAS_TIRITH] = {
        .continent = GONDOR,
        .neighbors = { ROHAN,
                       PINNATH_GELIN, ITHILIEN },
        .troops_pos = { 817, 812 },
    },

    [ITHILIEN] = {
        .continent = GONDOR,
        .neighbors = { ROHAN, RHOVANION,
                       MINAS_TIRITH,
                       GORGOROTH, NURN },
        .troops_pos = { 940, 827 },
    },


    [GORGOROTH] = {
        .continent = MORDOR,
        .neighbors = { RHOVANION,
                       ITHILIEN,
                       NURN },
        .troops_pos = { 1093, 711 },
    },

    [NURN] = {
        .continent = MORDOR,
        .neighbors = { RHOVANION,
                       ITHILIEN,
                       GORGOROTH },
        .troops_pos = { 1170, 810 },
    },
};


void init_country_list(void)
{
    for (CountryID i = 1; i < COUNTRY_COUNT; i++) {
        for (int j = 0;
             j < (int)ARRAY_SIZE(countries[i].neighbors) &&
             countries[i].neighbors[j] != NULL_COUNTRY;
             j++)
        {
            countries[i].neighbor_count++;

            bool symmetric = false;
            CountryID oci = countries[i].neighbors[j];
            for (int ocj = 0;
                 ocj < (int)ARRAY_SIZE(countries[oci].neighbors) &&
                 countries[oci].neighbors[ocj] != NULL_COUNTRY;
                 ocj++)
            {
                if (countries[oci].neighbors[ocj] == i) {
                    symmetric = true;
                    break;
                }
            }
            assert(symmetric);
        }
    }
}
