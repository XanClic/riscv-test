#include <assert.h>
#include <nonstddef.h>
#define IN_REGIONS_C
#include <regions.h>
#include <stdbool.h>


struct Region regions[REGION_COUNT] = {
    [NORGROST] = {
        .name = "Norgrost",
        .continent = AKHEN,
        .neighbors = { LUNEN,
                       KYUUEMPI },
        .troops_pos = { 192, 67 },
    },

    [LUNEN] = {
        .name = "Lunen",
        .continent = AKHEN,
        .neighbors = { NORGROST, BARLENIEN, SEEBOR,
                       BOTFORD },
        .troops_pos = { 193, 280 },
    },

    [BARLENIEN] = {
        .name = "Barlenien",
        .continent = AKHEN,
        .neighbors = { LUNEN, SEEBOR,
                       CWOM },
        .troops_pos = { 240, 369 },
    },

    [SEEBOR] = {
        .name = "Seebor",
        .continent = AKHEN,
        .neighbors = { LUNEN, BARLENIEN,
                       PETONY,
                       YUAN_DOU },
        .troops_pos = { 311, 350 },
    },


    [BOTFORD] = {
        .name = "Botford",
        .continent = ANGSHIRE,
        .neighbors = { LUNEN,
                       CWOM, SUNDERLO },
        .troops_pos = { 75, 581 },
    },

    [CWOM] = {
        .name = "Cwom",
        .continent = ANGSHIRE,
        .neighbors = { BARLENIEN,
                       BOTFORD, PETONY, SUNDERLO, UXBRID },
        .troops_pos = { 254, 657 },
    },

    [PETONY] = {
        .name = "Petony",
        .continent = ANGSHIRE,
        .neighbors = { SEEBOR,
                       CWOM, UXBRID,
                       YUAN_DOU },
        .troops_pos = { 410, 674 },
    },

    [SUNDERLO] = {
        .name = "Sunderlo",
        .continent = ANGSHIRE,
        .neighbors = { BOTFORD, CWOM, UXBRID },
        .troops_pos = { 184, 768 },
    },

    [UXBRID] = {
        .name = "Uxbrid",
        .continent = ANGSHIRE,
        .neighbors = { CWOM, PETONY, SUNDERLO,
                       MU_ZHANG },
        .troops_pos = { 333, 780 },
    },


    [KYUUEMPI] = {
        .name = "Kyuuempi",
        .continent = TSUKIMOTO,
        .neighbors = { NORGROST,
                       SHOUZU, KUNUCHI },
        .troops_pos = { 1037, 118 },
    },

    [SHOUZU] = {
        .name = "Shouzu",
        .continent = TSUKIMOTO,
        .neighbors = { KYUUEMPI, KUNUCHI, HIRANAGA,
                       YUAN_DOU, MU_ZHANG, XIAMO },
        .troops_pos = { 1003, 446 },
    },

    [KUNUCHI] = {
        .name = "Kunuchi",
        .continent = TSUKIMOTO,
        .neighbors = { KYUUEMPI, SHOUZU, HIRANAGA },
        .troops_pos = { 1178, 309 },
    },

    [HIRANAGA] = {
        .name = "Hiranaga",
        .continent = TSUKIMOTO,
        .neighbors = { SHOUZU, KUNUCHI,
                       XIAMO },
        .troops_pos = { 1246, 403 },
    },


    [YUAN_DOU] = {
        .name = "Yuan Dou",
        .continent = XINDAO,
        .neighbors = { SEEBOR,
                       PETONY,
                       SHOUZU },
        .troops_pos = { 705, 433 },
    },

    [MU_ZHANG] = {
        .name = "Mu Zhang",
        .continent = XINDAO,
        .neighbors = { UXBRID,
                       SHOUZU,
                       XIAMO },
        .troops_pos = { 1116, 777 },
    },

    [XIAMO] = {
        .name = "Xiamo",
        .continent = XINDAO,
        .neighbors = { SHOUZU, HIRANAGA,
                       MU_ZHANG },
        .troops_pos = { 1239, 627 },
    },
};


int continent_bonuses[CONTINENT_COUNT] = {
    [AKHEN] = 2,
    [ANGSHIRE] = 3,
    [TSUKIMOTO] = 2,
    [XINDAO] = 2,
};


void init_region_list(void)
{
    for (RegionID i = 1; i < REGION_COUNT; i++) {
        for (int j = 0;
             j < (int)ARRAY_SIZE(regions[i].neighbors) &&
             regions[i].neighbors[j] != NULL_REGION;
             j++)
        {
            regions[i].neighbor_count++;

            bool symmetric = false;
            RegionID oci = regions[i].neighbors[j];
            for (int ocj = 0;
                 ocj < (int)ARRAY_SIZE(regions[oci].neighbors) &&
                 regions[oci].neighbors[ocj] != NULL_REGION;
                 ocj++)
            {
                if (regions[oci].neighbors[ocj] == i) {
                    symmetric = true;
                    break;
                }
            }
            assert(symmetric);
        }
    }
}
