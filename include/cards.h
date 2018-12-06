#ifndef _CARDS_H
#define _CARDS_H

#include <regions.h>
#include <stdbool.h>


typedef enum CardDesign {
    CARD_INFANTRY,
    CARD_CAVALRY,
    CARD_ARTILLERY,

    CARD_DESIGN_COUNT,
    CARD_WILDCARD = CARD_DESIGN_COUNT
} CardDesign;

typedef struct Card {
    RegionID region;
    CardDesign design;
    bool selected;
} Card;


void setup_card_stack(void);
void draw_card_from_stack(Party p);
int party_hand_size(Party p);
const Card *party_hand_card(Party p, int index);
void party_hand_select_card(Party p, int index);
void party_hand_deselect_card(Party p, int index);
void party_hand_drop_card(Party p, int index);
void join_party_hands(Party winner, Party loser);

#endif
