#include "ItemIcons.h"

std::string getItemIconPath(uint32_t itemId)
{
    switch (itemId)
    {
    case 1:
        return "assets/currency/chaos_orb.png";
    case 2:
        return "assets/currency/divine_orb.png";
    case 3:
        return "assets/currency/exalted_orb.png";
    case 4:
        return "assets/currency/orb_of_alteration.png";
    case 5:
        return "assets/currency/scroll_of_wisdom.png";
    case 6:
        return "assets/uniques/starforge.png";
    case 7:
        return "assets/uniques/voltaxic_rift.png";
    case 8:
        return "assets/uniques/starkonja.png";
    case 9:
        return "assets/uniques/facebreaker.png";
    case 10:
        return "assets/uniques/volls_protector.png";
    case 11:
        return "assets/uniques/blood_dance.png";
    case 12:
        return "assets/uniques/call_of_the_brotherhood.png";
    default:
        return "";
    }
}
