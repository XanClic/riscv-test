#include <assert.h>
#include <cards.h>
#include <stdlib.h>
#include <string.h>


typedef struct Hand {
    Card cards[12];
    int card_count;
} Hand;


// Two wildcards for 42 regions, so one per 21
#define WILDCARD_COUNT \
    (REGION_COUNT - 1 + 10) / 21

#define STACK_SIZE \
    (REGION_COUNT - 1 + WILDCARD_COUNT)


static Card stack[STACK_SIZE];
static int stack_i;

static Hand hands[PARTY_COUNT];


void setup_card_stack(void)
{
    CardDesign d = 0;

    for (RegionID r = 1; r < REGION_COUNT; r++) {
        stack[r - 1].region = r;
        stack[r - 1].design = d;

        d = (d + 1) % CARD_DESIGN_COUNT;
    }

    for (int i = REGION_COUNT - 1; i < STACK_SIZE; i++) {
        stack[i].region = NULL_REGION;
        stack[i].design = CARD_WILDCARD;
    }

    shuffle(stack, STACK_SIZE, sizeof(stack[0]));

    stack_i = 0;
}


void draw_card_from_stack(Party p)
{
    if (stack_i >= STACK_SIZE) {
        return;
    }

    /* You can have at most six cards in hand (5 after defeating an
     * opponent, then 6 after drawing a card; and you can only draw
     * once per turn, so before doing so, you must have less than 6).
     */
    assert(hands[p].card_count < 6);
    hands[p].cards[hands[p].card_count++] = stack[stack_i++];
}


int party_hand_size(Party p)
{
    return hands[p].card_count;
}


const Card *party_hand_card(Party p, int index)
{
    assert(index >= 0 && index < hands[p].card_count);
    return &hands[p].cards[index];
}


void party_hand_select_card(Party p, int index)
{
    assert(index >= 0 && index < hands[p].card_count);
    hands[p].cards[index].selected = true;
}


void party_hand_deselect_card(Party p, int index)
{
    assert(index >= 0 && index < hands[p].card_count);
    hands[p].cards[index].selected = false;
}


void party_hand_drop_card(Party p, int index)
{
    assert(index >= 0 && index < hands[p].card_count);
    memmove(hands[p].cards + index,
            hands[p].cards + index + 1,
            (hands[p].card_count - index - 1) * sizeof(Card));
    hands[p].card_count--;
}


void join_party_hands(Party winner, Party loser)
{
    /* The winner has not drawn a card this turn yet (because the
     * battle phase has not ended yet), so they may not have more
     * than five cards in hand -- but it does not really matter.  6
     * is really the maximum, so use that as the limit. */
    assert(hands[winner].card_count <= 6);
    assert(hands[loser].card_count <= 6);

    memcpy(hands[winner].cards + hands[winner].card_count,
           hands[loser].cards,
           hands[loser].card_count * sizeof(Card));

    hands[winner].card_count += hands[loser].card_count;
    hands[loser].card_count = 0;
}
