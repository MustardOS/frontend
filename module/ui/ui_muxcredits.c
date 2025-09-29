#include "ui_muxcredits.h"
#include "../../common/common.h"
#include "../../common/config.h"

#define SONG_TRACK "Final Frontier"
#define SONG_ARTIST "Nimn One"

lv_obj_t *ui_scrCredits;

lv_obj_t *ui_conCredits;
lv_obj_t *ui_conStart;
lv_obj_t *ui_conOfficial;
lv_obj_t *ui_conWizard;
lv_obj_t *ui_conHeroOne;
lv_obj_t *ui_conHeroTwo;
lv_obj_t *ui_conKnightOne;
lv_obj_t *ui_conKnightTwo;
lv_obj_t *ui_conContrib;
lv_obj_t *ui_conSpecial;
lv_obj_t *ui_conKofi;
lv_obj_t *ui_conMusic;

lv_obj_t *ui_imgKofi;

lv_obj_t *ui_lblStartTitle;
lv_obj_t *ui_lblStartMessage;

lv_obj_t *ui_lblCommanderTitle;
lv_obj_t *ui_lblCommanderCrew;

lv_obj_t *ui_lblEnforcerTitle;
lv_obj_t *ui_lblEnforcerCrew;

lv_obj_t *ui_lblWizardTitle;
lv_obj_t *ui_lblWizardCrew;

lv_obj_t *ui_lblHeroTitleOne;
lv_obj_t *ui_lblHeroCrewOne;
lv_obj_t *ui_lblHeroTitleTwo;
lv_obj_t *ui_lblHeroCrewTwo;

lv_obj_t *ui_lblKnightTitleOne;
lv_obj_t *ui_lblKnightCrewOne;
lv_obj_t *ui_lblKnightTitleTwo;
lv_obj_t *ui_lblKnightCrewTwo;

lv_obj_t *ui_lblContribTitle;
lv_obj_t *ui_lblContribCrew;

lv_obj_t *ui_lblSpecialTitle;
lv_obj_t *ui_lblSpecialMid;

lv_obj_t *ui_lblBongleTitle;
lv_obj_t *ui_lblBongleMid;

lv_obj_t *ui_lblKofiTitle;
lv_obj_t *ui_lblKofiMessageOne;
lv_obj_t *ui_lblKofiMessageTwo;

lv_obj_t *ui_lblMusicTitle;
lv_obj_t *ui_lblMusicMessage;

static lv_obj_t *name_flow(lv_obj_t *parent, const char **list, lv_color_t hex) {
    lv_obj_t *wrap = lv_obj_create(parent);
    lv_obj_remove_style_all(wrap);

    lv_obj_set_size(wrap, lv_pct(90), LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(wrap, 0, 0);
    lv_obj_set_flex_flow(wrap, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_flex_align(wrap, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_column(wrap, 16, 0);
    lv_obj_set_style_pad_row(wrap, 4, 0);

    for (size_t i = 0; list[i]; ++i) {
        lv_obj_t *lbl = lv_label_create(wrap);
        lv_label_set_text(lbl, list[i]);
        lv_obj_set_style_text_color(lbl, hex, 0);
        lv_obj_set_style_text_opa(lbl, 255, 0);
    }

    return wrap;
}

const char *commanders[] = {
        "xonglebongle", "antikk", "corey", "bitter_bizarro",
        NULL
};

const char *enforcers[] = {
        "acmeplus", "bgelmini", "ilfordhp5", "duncanyoyo1", "illumini_85", "delibirb77",
        NULL
};

const char *wizards[] = {
        "xquader", "ee1000", "kloptops", "thegammasqueeze", ".tokyovigilante", "joyrider3774",
        ".cebion", "irilivibi", "vagueparade", "shengy.", "siliconexarch", "shauninman",
        "johnnyonflame", "snowram", "habbening", "trngaje", "skorpy", "stanley_00",
        "ajmandourah", "bcat24", "xanxic", "midwan", "arkun_", "mikhailzrick",
        "retrogfx_", "aeverdyn", "spycat88",
        NULL
};

const char *hero_one[] = {
        "romus85", "x_tremis", "lmarcomiranda", "timecarp", "intelliaim", "kentonftw",
        "bazkart", "msx6011", "joeysretrohandhelds", "btreecat", "teggydave", "zazouboy",
        "robbiet480", "rabite890", "luzfcb", "brohsnbluffs", "bigbossman0816", "superzu",
        "nico_linber_36894", "nico_linber", "pr0j3kt2501", "meowman_", "zaka1w3", "kaeltis",
        "reitw", "raouldook.", "paletochen", "benjaminbercy", "suribii", "tobitaibuta",
        "opinion_panda", "hueykablooey", "mrwhistles", ".zerohalo", "losermatic", "milkworlds",
        "rosemelody254", "lilaclobotomies",
        NULL
};

const char *hero_two[] = {
        "ivar2028", ".dririan", "spivvmeister", "sol6_vi", "izzythefool", "n3vurmynd",
        "qpla", "supremedialects", "amos_06286", "techagent", "meanagar", "roundpi",
        "turner74.", "chiefwally_73445", "bigfoothenders", "scy0n", "luckyphil", "nahck",
        "mach5682", "foamygames", "xraygoggles", "hybrid_sith", "mxdamp", "ownedmumbles",
        "existentialrose", "techyysean", "cliffupperlip", "roymustang", "kernelkritic", "jelzer.",
        "misfitsorbet", "verctexius", "bigolpeewee", "amildinconvenience.", "kalamer.", "yotakon",
        "jimmycrackedcorn_4711",
        NULL
};

const char *knight_one[] = {
        "allanc5963", "notflacko", "jdanteq_18123", "skyarcher", "hai6266", "galloc",
        "ben819.", "_maxwellsdemon", "status.quo.exile", "phyrex", "delored", "kiko_lake",
        "arkholt", "julas8799", ".starship9", "fibroidjames", "allepac", "pakwan8243",
        "heyitscap.", "drisc", "clempurp9868", "aj15", "retrogamecorps", "biffoutt",
        "rbndr_", "sanelessone", ".retrogamingmonkey", "billynaing", "gasqbduv", ".manny.",
        NULL
};

const char *knight_two[] = {
        "_wizdude", "blkxltng", "surge_84306", "phlurblepoot", "mrcee1503", "splendid88_98891",
        "yomama78", "pixelgabz", "admiralthrawn_1", "penpen2crayon", "jupyter.", "lander95",
        "bburbank", "crownlessk", "johnunsc", "nuke_67641", "misjbaksel", "zauberpony",
        "dreadpirates", "artur_ditu", "wizardfights", "stin87", "obitomatheus", "johnrich340",
        "frickinfrogs", "smittywerbenjaegermanjensen9250", "leberwurstmitsauregurken",
        NULL
};

const char *contributors[] = {
        "antikk", "bitter_bizarro", "imcokeman", "shijikori", "xonglebongle",
        NULL
};

void init_muxcredits(const lv_font_t *header_font) {
    ui_scrCredits = lv_obj_create(NULL);
    lv_obj_clear_flag(ui_scrCredits, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(ui_scrCredits, lv_color_hex(0x0), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_opa(ui_scrCredits, 255, MU_OBJ_MAIN_DEFAULT);

    ui_conCredits = lv_obj_create(ui_scrCredits);
    lv_obj_remove_style_all(ui_conCredits);
    lv_obj_set_width(ui_conCredits, lv_pct(100));
    lv_obj_set_height(ui_conCredits, lv_pct(100));
    lv_obj_set_align(ui_conCredits, LV_ALIGN_CENTER);
    lv_obj_clear_flag(ui_conCredits, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);

    ui_conStart = lv_obj_create(ui_conCredits);
    lv_obj_remove_style_all(ui_conStart);
    lv_obj_set_width(ui_conStart, lv_pct(100));
    lv_obj_set_height(ui_conStart, LV_SIZE_CONTENT);
    lv_obj_set_x(ui_conStart, lv_pct(0));
    lv_obj_set_y(ui_conStart, lv_pct(0));
    lv_obj_set_style_bg_color(ui_conStart, lv_color_hex(0xA5B2B5), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_opa(ui_conStart, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_align(ui_conStart, LV_ALIGN_CENTER);
    lv_obj_set_flex_flow(ui_conStart, LV_FLEX_FLOW_COLUMN_WRAP);
    lv_obj_set_flex_align(ui_conStart, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_add_flag(ui_conStart, LV_OBJ_FLAG_OVERFLOW_VISIBLE);
    lv_obj_clear_flag(ui_conStart, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_left(ui_conStart, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_right(ui_conStart, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_top(ui_conStart, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_bottom(ui_conStart, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_row(ui_conStart, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_column(ui_conStart, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_opa(ui_conStart, 0, MU_OBJ_MAIN_DEFAULT);

    ui_lblStartTitle = lv_label_create(ui_conStart);
    lv_label_set_text(ui_lblStartTitle, "");
    lv_obj_set_width(ui_lblStartTitle, lv_pct(100));
    lv_obj_set_height(ui_lblStartTitle, LV_SIZE_CONTENT);
    lv_obj_set_x(ui_lblStartTitle, 0);
    lv_obj_set_y(ui_lblStartTitle, 0);
    lv_obj_set_align(ui_lblStartTitle, LV_ALIGN_TOP_MID);
    lv_label_set_text(ui_lblStartTitle, "Thank you for choosing MustardOS");
    lv_obj_set_scroll_dir(ui_lblStartTitle, LV_DIR_HOR);
    lv_obj_set_style_text_color(ui_lblStartTitle, lv_color_hex(0xF7E318), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_text_opa(ui_lblStartTitle, 255, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_text_align(ui_lblStartTitle, LV_TEXT_ALIGN_CENTER, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_text_font(ui_lblStartTitle, header_font, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_color(ui_lblStartTitle, lv_color_hex(0xA5B2B5), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_opa(ui_lblStartTitle, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_grad_color(ui_lblStartTitle, lv_color_hex(0x0), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_main_stop(ui_lblStartTitle, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_grad_stop(ui_lblStartTitle, 200, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_grad_dir(ui_lblStartTitle, LV_GRAD_DIR_HOR, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_color(ui_lblStartTitle, lv_color_hex(0xA5B2B5), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_opa(ui_lblStartTitle, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_width(ui_lblStartTitle, 5, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_side(ui_lblStartTitle, LV_BORDER_SIDE_LEFT, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_left(ui_lblStartTitle, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_right(ui_lblStartTitle, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_top(ui_lblStartTitle, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_bottom(ui_lblStartTitle, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_row(ui_lblStartTitle, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_column(ui_lblStartTitle, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_text_color(ui_lblStartTitle, lv_color_hex(0xF8E008), MU_OBJ_MAIN_FOCUS);
    lv_obj_set_style_text_opa(ui_lblStartTitle, 255, MU_OBJ_MAIN_FOCUS);

    ui_lblStartMessage = lv_label_create(ui_conStart);
    lv_label_set_text(ui_lblStartMessage, "");
    lv_obj_set_width(ui_lblStartMessage, lv_pct(90));
    lv_obj_set_height(ui_lblStartMessage, LV_SIZE_CONTENT);
    lv_obj_set_x(ui_lblStartMessage, 0);
    lv_obj_set_y(ui_lblStartMessage, 0);
    lv_obj_set_align(ui_lblStartMessage, LV_ALIGN_CENTER);
    lv_label_set_text(ui_lblStartMessage,
                      "\nThe people listed throughout have been amazing supporters and contributors to MustardOS "
                      "and shaping its future.\n\nFrom the bottom of my heart, thank you for your time, ideas, "
                      "testing, and support. I appreciate you all more than a simple \"thank you\" could express!");
    lv_obj_set_scroll_dir(ui_lblStartMessage, LV_DIR_HOR);
    lv_obj_set_style_text_color(ui_lblStartMessage, lv_color_hex(0xDDA200), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_text_opa(ui_lblStartMessage, 255, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_text_letter_space(ui_lblStartMessage, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_text_line_space(ui_lblStartMessage, 2, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_text_align(ui_lblStartMessage, LV_TEXT_ALIGN_CENTER, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_color(ui_lblStartMessage, lv_color_hex(0xA5B2B5), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_opa(ui_lblStartMessage, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_width(ui_lblStartMessage, 5, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_side(ui_lblStartMessage, LV_BORDER_SIDE_LEFT, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_left(ui_lblStartMessage, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_right(ui_lblStartMessage, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_top(ui_lblStartMessage, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_bottom(ui_lblStartMessage, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_row(ui_lblStartMessage, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_column(ui_lblStartMessage, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_text_color(ui_lblStartMessage, lv_color_hex(0xF8E008), MU_OBJ_MAIN_FOCUS);
    lv_obj_set_style_text_opa(ui_lblStartMessage, 255, MU_OBJ_MAIN_FOCUS);

    ui_conOfficial = lv_obj_create(ui_conCredits);
    lv_obj_remove_style_all(ui_conOfficial);
    lv_obj_set_width(ui_conOfficial, lv_pct(100));
    lv_obj_set_height(ui_conOfficial, LV_SIZE_CONTENT);
    lv_obj_set_scroll_dir(ui_conOfficial, LV_DIR_VER);
    lv_obj_set_style_max_height(ui_conOfficial, lv_pct(100), 0);
    lv_obj_set_x(ui_conOfficial, lv_pct(0));
    lv_obj_set_y(ui_conOfficial, lv_pct(0));
    lv_obj_set_style_bg_color(ui_conOfficial, lv_color_hex(0xA5B2B5), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_opa(ui_conOfficial, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_align(ui_conOfficial, LV_ALIGN_CENTER);
    lv_obj_set_flex_flow(ui_conOfficial, LV_FLEX_FLOW_COLUMN_WRAP);
    lv_obj_set_flex_align(ui_conOfficial, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_add_flag(ui_conOfficial, LV_OBJ_FLAG_OVERFLOW_VISIBLE);
    lv_obj_clear_flag(ui_conOfficial, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_left(ui_conOfficial, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_right(ui_conOfficial, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_top(ui_conOfficial, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_bottom(ui_conOfficial, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_row(ui_conOfficial, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_column(ui_conOfficial, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_opa(ui_conOfficial, 0, MU_OBJ_MAIN_DEFAULT);

    ui_lblCommanderTitle = lv_label_create(ui_conOfficial);
    lv_label_set_text(ui_lblCommanderTitle, "");
    lv_obj_set_width(ui_lblCommanderTitle, lv_pct(100));
    lv_obj_set_height(ui_lblCommanderTitle, LV_SIZE_CONTENT);
    lv_obj_set_align(ui_lblCommanderTitle, LV_ALIGN_TOP_MID);
    lv_label_set_text(ui_lblCommanderTitle, "Commanders");
    lv_obj_set_scroll_dir(ui_lblCommanderTitle, LV_DIR_HOR);
    lv_obj_set_style_text_color(ui_lblCommanderTitle, lv_color_hex(0xFF0000), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_text_opa(ui_lblCommanderTitle, 255, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_text_align(ui_lblCommanderTitle, LV_TEXT_ALIGN_CENTER, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_text_font(ui_lblCommanderTitle, header_font, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_color(ui_lblCommanderTitle, lv_color_hex(0xA5B2B5), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_opa(ui_lblCommanderTitle, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_grad_color(ui_lblCommanderTitle, lv_color_hex(0x0), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_main_stop(ui_lblCommanderTitle, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_grad_stop(ui_lblCommanderTitle, 200, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_grad_dir(ui_lblCommanderTitle, LV_GRAD_DIR_HOR, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_color(ui_lblCommanderTitle, lv_color_hex(0xA5B2B5), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_opa(ui_lblCommanderTitle, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_width(ui_lblCommanderTitle, 5, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_side(ui_lblCommanderTitle, LV_BORDER_SIDE_LEFT, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_left(ui_lblCommanderTitle, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_right(ui_lblCommanderTitle, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_top(ui_lblCommanderTitle, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_bottom(ui_lblCommanderTitle, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_row(ui_lblCommanderTitle, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_column(ui_lblCommanderTitle, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_text_color(ui_lblCommanderTitle, lv_color_hex(0xF8E008), MU_OBJ_MAIN_FOCUS);
    lv_obj_set_style_text_opa(ui_lblCommanderTitle, 255, MU_OBJ_MAIN_FOCUS);

    name_flow(ui_conOfficial, commanders, lv_color_hex(0xFF0000));

    ui_lblEnforcerTitle = lv_label_create(ui_conOfficial);
    lv_label_set_text(ui_lblEnforcerTitle, "");
    lv_obj_set_width(ui_lblEnforcerTitle, lv_pct(100));
    lv_obj_set_height(ui_lblEnforcerTitle, LV_SIZE_CONTENT);
    lv_obj_set_align(ui_lblEnforcerTitle, LV_ALIGN_TOP_MID);
    lv_label_set_text(ui_lblEnforcerTitle, "\nEnforcers");
    lv_obj_set_scroll_dir(ui_lblEnforcerTitle, LV_DIR_HOR);
    lv_obj_set_style_text_color(ui_lblEnforcerTitle, lv_color_hex(0xFF4500), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_text_opa(ui_lblEnforcerTitle, 255, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_text_align(ui_lblEnforcerTitle, LV_TEXT_ALIGN_CENTER, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_text_font(ui_lblEnforcerTitle, header_font, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_color(ui_lblEnforcerTitle, lv_color_hex(0xA5B2B5), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_opa(ui_lblEnforcerTitle, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_grad_color(ui_lblEnforcerTitle, lv_color_hex(0x0), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_main_stop(ui_lblEnforcerTitle, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_grad_stop(ui_lblEnforcerTitle, 200, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_grad_dir(ui_lblEnforcerTitle, LV_GRAD_DIR_HOR, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_color(ui_lblEnforcerTitle, lv_color_hex(0xA5B2B5), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_opa(ui_lblEnforcerTitle, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_width(ui_lblEnforcerTitle, 5, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_side(ui_lblEnforcerTitle, LV_BORDER_SIDE_LEFT, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_left(ui_lblEnforcerTitle, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_right(ui_lblEnforcerTitle, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_top(ui_lblEnforcerTitle, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_bottom(ui_lblEnforcerTitle, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_row(ui_lblEnforcerTitle, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_column(ui_lblEnforcerTitle, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_text_color(ui_lblEnforcerTitle, lv_color_hex(0xF8E008), MU_OBJ_MAIN_FOCUS);
    lv_obj_set_style_text_opa(ui_lblEnforcerTitle, 255, MU_OBJ_MAIN_FOCUS);

    name_flow(ui_conOfficial, enforcers, lv_color_hex(0xFF4500));

    ui_conWizard = lv_obj_create(ui_conCredits);
    lv_obj_remove_style_all(ui_conWizard);
    lv_obj_set_width(ui_conWizard, lv_pct(100));
    lv_obj_set_height(ui_conWizard, LV_SIZE_CONTENT);
    lv_obj_set_scroll_dir(ui_conWizard, LV_DIR_VER);
    lv_obj_set_style_max_height(ui_conWizard, lv_pct(100), 0);
    lv_obj_set_x(ui_conWizard, lv_pct(0));
    lv_obj_set_y(ui_conWizard, lv_pct(0));
    lv_obj_set_style_bg_color(ui_conWizard, lv_color_hex(0xA5B2B5), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_opa(ui_conWizard, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_align(ui_conWizard, LV_ALIGN_CENTER);
    lv_obj_set_flex_flow(ui_conWizard, LV_FLEX_FLOW_COLUMN_WRAP);
    lv_obj_set_flex_align(ui_conWizard, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_add_flag(ui_conWizard, LV_OBJ_FLAG_OVERFLOW_VISIBLE);
    lv_obj_clear_flag(ui_conWizard, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_left(ui_conWizard, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_right(ui_conWizard, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_top(ui_conWizard, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_bottom(ui_conWizard, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_row(ui_conWizard, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_column(ui_conWizard, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_opa(ui_conWizard, 0, MU_OBJ_MAIN_DEFAULT);

    ui_lblWizardTitle = lv_label_create(ui_conWizard);
    lv_label_set_text(ui_lblWizardTitle, "");
    lv_obj_set_width(ui_lblWizardTitle, lv_pct(100));
    lv_obj_set_height(ui_lblWizardTitle, LV_SIZE_CONTENT);
    lv_obj_set_align(ui_lblWizardTitle, LV_ALIGN_TOP_MID);
    lv_label_set_text(ui_lblWizardTitle, "Wizards");
    lv_obj_set_scroll_dir(ui_lblWizardTitle, LV_DIR_HOR);
    lv_obj_set_style_text_color(ui_lblWizardTitle, lv_color_hex(0xED6900), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_text_opa(ui_lblWizardTitle, 255, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_text_align(ui_lblWizardTitle, LV_TEXT_ALIGN_CENTER, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_text_font(ui_lblWizardTitle, header_font, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_color(ui_lblWizardTitle, lv_color_hex(0xA5B2B5), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_opa(ui_lblWizardTitle, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_grad_color(ui_lblWizardTitle, lv_color_hex(0x0), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_main_stop(ui_lblWizardTitle, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_grad_stop(ui_lblWizardTitle, 200, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_grad_dir(ui_lblWizardTitle, LV_GRAD_DIR_HOR, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_color(ui_lblWizardTitle, lv_color_hex(0xA5B2B5), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_opa(ui_lblWizardTitle, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_width(ui_lblWizardTitle, 5, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_side(ui_lblWizardTitle, LV_BORDER_SIDE_LEFT, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_left(ui_lblWizardTitle, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_right(ui_lblWizardTitle, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_top(ui_lblWizardTitle, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_bottom(ui_lblWizardTitle, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_row(ui_lblWizardTitle, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_column(ui_lblWizardTitle, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_text_color(ui_lblWizardTitle, lv_color_hex(0xF8E008), MU_OBJ_MAIN_FOCUS);
    lv_obj_set_style_text_opa(ui_lblWizardTitle, 255, MU_OBJ_MAIN_FOCUS);

    name_flow(ui_conWizard, wizards, lv_color_hex(0xED6900));

    ui_conHeroOne = lv_obj_create(ui_conCredits);
    lv_obj_remove_style_all(ui_conHeroOne);
    lv_obj_set_width(ui_conHeroOne, lv_pct(100));
    lv_obj_set_height(ui_conHeroOne, LV_SIZE_CONTENT);
    lv_obj_set_scroll_dir(ui_conHeroOne, LV_DIR_VER);
    lv_obj_set_style_max_height(ui_conHeroOne, lv_pct(100), 0);
    lv_obj_set_x(ui_conHeroOne, lv_pct(0));
    lv_obj_set_y(ui_conHeroOne, lv_pct(0));
    lv_obj_set_style_bg_color(ui_conHeroOne, lv_color_hex(0xA5B2B5), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_opa(ui_conHeroOne, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_align(ui_conHeroOne, LV_ALIGN_CENTER);
    lv_obj_set_flex_flow(ui_conHeroOne, LV_FLEX_FLOW_COLUMN_WRAP);
    lv_obj_set_flex_align(ui_conHeroOne, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_add_flag(ui_conHeroOne, LV_OBJ_FLAG_OVERFLOW_VISIBLE);
    lv_obj_clear_flag(ui_conHeroOne, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_left(ui_conHeroOne, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_right(ui_conHeroOne, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_top(ui_conHeroOne, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_bottom(ui_conHeroOne, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_row(ui_conHeroOne, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_column(ui_conHeroOne, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_opa(ui_conHeroOne, 0, MU_OBJ_MAIN_DEFAULT);

    ui_lblHeroTitleOne = lv_label_create(ui_conHeroOne);
    lv_label_set_text(ui_lblHeroTitleOne, "");
    lv_obj_set_width(ui_lblHeroTitleOne, lv_pct(100));
    lv_obj_set_height(ui_lblHeroTitleOne, LV_SIZE_CONTENT);
    lv_obj_set_align(ui_lblHeroTitleOne, LV_ALIGN_TOP_MID);
    lv_label_set_text(ui_lblHeroTitleOne, "Heroes");
    lv_obj_set_scroll_dir(ui_lblHeroTitleOne, LV_DIR_HOR);
    lv_obj_set_style_text_color(ui_lblHeroTitleOne, lv_color_hex(0xFFDD22), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_text_opa(ui_lblHeroTitleOne, 255, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_text_align(ui_lblHeroTitleOne, LV_TEXT_ALIGN_CENTER, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_text_font(ui_lblHeroTitleOne, header_font, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_color(ui_lblHeroTitleOne, lv_color_hex(0xA5B2B5), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_opa(ui_lblHeroTitleOne, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_grad_color(ui_lblHeroTitleOne, lv_color_hex(0x0), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_main_stop(ui_lblHeroTitleOne, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_grad_stop(ui_lblHeroTitleOne, 200, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_grad_dir(ui_lblHeroTitleOne, LV_GRAD_DIR_HOR, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_color(ui_lblHeroTitleOne, lv_color_hex(0xA5B2B5), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_opa(ui_lblHeroTitleOne, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_width(ui_lblHeroTitleOne, 5, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_side(ui_lblHeroTitleOne, LV_BORDER_SIDE_LEFT, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_left(ui_lblHeroTitleOne, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_right(ui_lblHeroTitleOne, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_top(ui_lblHeroTitleOne, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_bottom(ui_lblHeroTitleOne, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_row(ui_lblHeroTitleOne, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_column(ui_lblHeroTitleOne, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_text_color(ui_lblHeroTitleOne, lv_color_hex(0xF8E008), MU_OBJ_MAIN_FOCUS);
    lv_obj_set_style_text_opa(ui_lblHeroTitleOne, 255, MU_OBJ_MAIN_FOCUS);

    name_flow(ui_conHeroOne, hero_one, lv_color_hex(0xFFDD22));

    ui_conHeroTwo = lv_obj_create(ui_conCredits);
    lv_obj_remove_style_all(ui_conHeroTwo);
    lv_obj_set_width(ui_conHeroTwo, lv_pct(100));
    lv_obj_set_height(ui_conHeroTwo, LV_SIZE_CONTENT);
    lv_obj_set_scroll_dir(ui_conHeroTwo, LV_DIR_VER);
    lv_obj_set_style_max_height(ui_conHeroTwo, lv_pct(100), 0);
    lv_obj_set_x(ui_conHeroTwo, lv_pct(0));
    lv_obj_set_y(ui_conHeroTwo, lv_pct(0));
    lv_obj_set_style_bg_color(ui_conHeroTwo, lv_color_hex(0xA5B2B5), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_opa(ui_conHeroTwo, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_align(ui_conHeroTwo, LV_ALIGN_CENTER);
    lv_obj_set_flex_flow(ui_conHeroTwo, LV_FLEX_FLOW_COLUMN_WRAP);
    lv_obj_set_flex_align(ui_conHeroTwo, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_add_flag(ui_conHeroTwo, LV_OBJ_FLAG_OVERFLOW_VISIBLE);
    lv_obj_clear_flag(ui_conHeroTwo, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_left(ui_conHeroTwo, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_right(ui_conHeroTwo, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_top(ui_conHeroTwo, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_bottom(ui_conHeroTwo, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_row(ui_conHeroTwo, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_column(ui_conHeroTwo, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_opa(ui_conHeroTwo, 0, MU_OBJ_MAIN_DEFAULT);

    ui_lblHeroTitleTwo = lv_label_create(ui_conHeroTwo);
    lv_label_set_text(ui_lblHeroTitleTwo, "");
    lv_obj_set_width(ui_lblHeroTitleTwo, lv_pct(100));
    lv_obj_set_height(ui_lblHeroTitleTwo, LV_SIZE_CONTENT);
    lv_obj_set_align(ui_lblHeroTitleTwo, LV_ALIGN_TOP_MID);
    lv_label_set_text(ui_lblHeroTitleTwo, "Heroes");
    lv_obj_set_scroll_dir(ui_lblHeroTitleTwo, LV_DIR_HOR);
    lv_obj_set_style_text_color(ui_lblHeroTitleTwo, lv_color_hex(0xFFDD22), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_text_opa(ui_lblHeroTitleTwo, 255, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_text_align(ui_lblHeroTitleTwo, LV_TEXT_ALIGN_CENTER, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_text_font(ui_lblHeroTitleTwo, header_font, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_color(ui_lblHeroTitleTwo, lv_color_hex(0xA5B2B5), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_opa(ui_lblHeroTitleTwo, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_grad_color(ui_lblHeroTitleTwo, lv_color_hex(0x0), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_main_stop(ui_lblHeroTitleTwo, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_grad_stop(ui_lblHeroTitleTwo, 200, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_grad_dir(ui_lblHeroTitleTwo, LV_GRAD_DIR_HOR, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_color(ui_lblHeroTitleTwo, lv_color_hex(0xA5B2B5), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_opa(ui_lblHeroTitleTwo, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_width(ui_lblHeroTitleTwo, 5, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_side(ui_lblHeroTitleTwo, LV_BORDER_SIDE_LEFT, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_left(ui_lblHeroTitleTwo, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_right(ui_lblHeroTitleTwo, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_top(ui_lblHeroTitleTwo, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_bottom(ui_lblHeroTitleTwo, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_row(ui_lblHeroTitleTwo, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_column(ui_lblHeroTitleTwo, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_text_color(ui_lblHeroTitleTwo, lv_color_hex(0xF8E008), MU_OBJ_MAIN_FOCUS);
    lv_obj_set_style_text_opa(ui_lblHeroTitleTwo, 255, MU_OBJ_MAIN_FOCUS);

    name_flow(ui_conHeroTwo, hero_two, lv_color_hex(0xFFDD22));

    ui_conKnightOne = lv_obj_create(ui_conCredits);
    lv_obj_remove_style_all(ui_conKnightOne);
    lv_obj_set_width(ui_conKnightOne, lv_pct(100));
    lv_obj_set_height(ui_conKnightOne, LV_SIZE_CONTENT);
    lv_obj_set_scroll_dir(ui_conKnightOne, LV_DIR_VER);
    lv_obj_set_style_max_height(ui_conKnightOne, lv_pct(100), 0);
    lv_obj_set_x(ui_conKnightOne, lv_pct(0));
    lv_obj_set_y(ui_conKnightOne, lv_pct(0));
    lv_obj_set_style_bg_color(ui_conKnightOne, lv_color_hex(0xA5B2B5), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_opa(ui_conKnightOne, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_align(ui_conKnightOne, LV_ALIGN_CENTER);
    lv_obj_set_flex_flow(ui_conKnightOne, LV_FLEX_FLOW_COLUMN_WRAP);
    lv_obj_set_flex_align(ui_conKnightOne, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_add_flag(ui_conKnightOne, LV_OBJ_FLAG_OVERFLOW_VISIBLE);
    lv_obj_clear_flag(ui_conKnightOne, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_left(ui_conKnightOne, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_right(ui_conKnightOne, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_top(ui_conKnightOne, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_bottom(ui_conKnightOne, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_row(ui_conKnightOne, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_column(ui_conKnightOne, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_opa(ui_conKnightOne, 0, MU_OBJ_MAIN_DEFAULT);

    ui_lblKnightTitleOne = lv_label_create(ui_conKnightOne);
    lv_label_set_text(ui_lblKnightTitleOne, "");
    lv_obj_set_width(ui_lblKnightTitleOne, lv_pct(100));
    lv_obj_set_height(ui_lblKnightTitleOne, LV_SIZE_CONTENT);
    lv_obj_set_align(ui_lblKnightTitleOne, LV_ALIGN_TOP_MID);
    lv_label_set_text(ui_lblKnightTitleOne, "Knights");
    lv_obj_set_scroll_dir(ui_lblKnightTitleOne, LV_DIR_HOR);
    lv_obj_set_style_text_color(ui_lblKnightTitleOne, lv_color_hex(0xED6900), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_text_opa(ui_lblKnightTitleOne, 255, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_text_align(ui_lblKnightTitleOne, LV_TEXT_ALIGN_CENTER, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_text_font(ui_lblKnightTitleOne, header_font, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_color(ui_lblKnightTitleOne, lv_color_hex(0xA5B2B5), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_opa(ui_lblKnightTitleOne, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_grad_color(ui_lblKnightTitleOne, lv_color_hex(0x0), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_main_stop(ui_lblKnightTitleOne, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_grad_stop(ui_lblKnightTitleOne, 200, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_grad_dir(ui_lblKnightTitleOne, LV_GRAD_DIR_HOR, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_color(ui_lblKnightTitleOne, lv_color_hex(0xA5B2B5), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_opa(ui_lblKnightTitleOne, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_width(ui_lblKnightTitleOne, 5, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_side(ui_lblKnightTitleOne, LV_BORDER_SIDE_LEFT, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_left(ui_lblKnightTitleOne, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_right(ui_lblKnightTitleOne, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_top(ui_lblKnightTitleOne, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_bottom(ui_lblKnightTitleOne, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_row(ui_lblKnightTitleOne, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_column(ui_lblKnightTitleOne, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_text_color(ui_lblKnightTitleOne, lv_color_hex(0xF8E008), MU_OBJ_MAIN_FOCUS);
    lv_obj_set_style_text_opa(ui_lblKnightTitleOne, 255, MU_OBJ_MAIN_FOCUS);

    name_flow(ui_conKnightOne, knight_one, lv_color_hex(0xED6900));

    ui_conKnightTwo = lv_obj_create(ui_conCredits);
    lv_obj_remove_style_all(ui_conKnightTwo);
    lv_obj_set_width(ui_conKnightTwo, lv_pct(100));
    lv_obj_set_height(ui_conKnightTwo, LV_SIZE_CONTENT);
    lv_obj_set_scroll_dir(ui_conKnightTwo, LV_DIR_VER);
    lv_obj_set_style_max_height(ui_conKnightTwo, lv_pct(100), 0);
    lv_obj_set_x(ui_conKnightTwo, lv_pct(0));
    lv_obj_set_y(ui_conKnightTwo, lv_pct(0));
    lv_obj_set_style_bg_color(ui_conKnightTwo, lv_color_hex(0xA5B2B5), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_opa(ui_conKnightTwo, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_align(ui_conKnightTwo, LV_ALIGN_CENTER);
    lv_obj_set_flex_flow(ui_conKnightTwo, LV_FLEX_FLOW_COLUMN_WRAP);
    lv_obj_set_flex_align(ui_conKnightTwo, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_add_flag(ui_conKnightTwo, LV_OBJ_FLAG_OVERFLOW_VISIBLE);
    lv_obj_clear_flag(ui_conKnightTwo, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_left(ui_conKnightTwo, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_right(ui_conKnightTwo, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_top(ui_conKnightTwo, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_bottom(ui_conKnightTwo, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_row(ui_conKnightTwo, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_column(ui_conKnightTwo, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_opa(ui_conKnightTwo, 0, MU_OBJ_MAIN_DEFAULT);

    ui_lblKnightTitleTwo = lv_label_create(ui_conKnightTwo);
    lv_label_set_text(ui_lblKnightTitleTwo, "");
    lv_obj_set_width(ui_lblKnightTitleTwo, lv_pct(100));
    lv_obj_set_height(ui_lblKnightTitleTwo, LV_SIZE_CONTENT);
    lv_obj_set_align(ui_lblKnightTitleTwo, LV_ALIGN_TOP_MID);
    lv_label_set_text(ui_lblKnightTitleTwo, "Knights");
    lv_obj_set_scroll_dir(ui_lblKnightTitleTwo, LV_DIR_HOR);
    lv_obj_set_style_text_color(ui_lblKnightTitleTwo, lv_color_hex(0xED6900), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_text_opa(ui_lblKnightTitleTwo, 255, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_text_align(ui_lblKnightTitleTwo, LV_TEXT_ALIGN_CENTER, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_text_font(ui_lblKnightTitleTwo, header_font, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_color(ui_lblKnightTitleTwo, lv_color_hex(0xA5B2B5), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_opa(ui_lblKnightTitleTwo, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_grad_color(ui_lblKnightTitleTwo, lv_color_hex(0x0), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_main_stop(ui_lblKnightTitleTwo, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_grad_stop(ui_lblKnightTitleTwo, 200, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_grad_dir(ui_lblKnightTitleTwo, LV_GRAD_DIR_HOR, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_color(ui_lblKnightTitleTwo, lv_color_hex(0xA5B2B5), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_opa(ui_lblKnightTitleTwo, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_width(ui_lblKnightTitleTwo, 5, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_side(ui_lblKnightTitleTwo, LV_BORDER_SIDE_LEFT, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_left(ui_lblKnightTitleTwo, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_right(ui_lblKnightTitleTwo, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_top(ui_lblKnightTitleTwo, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_bottom(ui_lblKnightTitleTwo, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_row(ui_lblKnightTitleTwo, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_column(ui_lblKnightTitleTwo, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_text_color(ui_lblKnightTitleTwo, lv_color_hex(0xF8E008), MU_OBJ_MAIN_FOCUS);
    lv_obj_set_style_text_opa(ui_lblKnightTitleTwo, 255, MU_OBJ_MAIN_FOCUS);

    name_flow(ui_conKnightTwo, knight_two, lv_color_hex(0xED6900));

    ui_conContrib = lv_obj_create(ui_conCredits);
    lv_obj_remove_style_all(ui_conContrib);
    lv_obj_set_width(ui_conContrib, lv_pct(100));
    lv_obj_set_height(ui_conContrib, LV_SIZE_CONTENT);
    lv_obj_set_scroll_dir(ui_conContrib, LV_DIR_VER);
    lv_obj_set_style_max_height(ui_conContrib, lv_pct(100), 0);
    lv_obj_set_x(ui_conContrib, lv_pct(0));
    lv_obj_set_y(ui_conContrib, lv_pct(0));
    lv_obj_set_style_bg_color(ui_conContrib, lv_color_hex(0xA5B2B5), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_opa(ui_conContrib, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_align(ui_conContrib, LV_ALIGN_CENTER);
    lv_obj_set_flex_flow(ui_conContrib, LV_FLEX_FLOW_COLUMN_WRAP);
    lv_obj_set_flex_align(ui_conContrib, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_add_flag(ui_conContrib, LV_OBJ_FLAG_OVERFLOW_VISIBLE);
    lv_obj_clear_flag(ui_conContrib, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_left(ui_conContrib, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_right(ui_conContrib, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_top(ui_conContrib, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_bottom(ui_conContrib, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_row(ui_conContrib, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_column(ui_conContrib, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_opa(ui_conContrib, 0, MU_OBJ_MAIN_DEFAULT);

    ui_lblContribTitle = lv_label_create(ui_conContrib);
    lv_label_set_text(ui_lblContribTitle, "");
    lv_obj_set_width(ui_lblContribTitle, lv_pct(100));
    lv_obj_set_height(ui_lblContribTitle, LV_SIZE_CONTENT);
    lv_obj_set_align(ui_lblContribTitle, LV_ALIGN_TOP_MID);
    lv_label_set_text_fmt(ui_lblContribTitle, "%s\nContributors", str_replace(config.SYSTEM.VERSION, "_", " "));
    lv_obj_set_scroll_dir(ui_lblContribTitle, LV_DIR_HOR);
    lv_obj_set_style_text_color(ui_lblContribTitle, lv_color_hex(0x87C97C), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_text_opa(ui_lblContribTitle, 255, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_text_align(ui_lblContribTitle, LV_TEXT_ALIGN_CENTER, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_text_font(ui_lblContribTitle, header_font, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_color(ui_lblContribTitle, lv_color_hex(0xA5B2B5), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_opa(ui_lblContribTitle, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_grad_color(ui_lblContribTitle, lv_color_hex(0x0), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_main_stop(ui_lblContribTitle, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_grad_stop(ui_lblContribTitle, 200, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_grad_dir(ui_lblContribTitle, LV_GRAD_DIR_HOR, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_color(ui_lblContribTitle, lv_color_hex(0xA5B2B5), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_opa(ui_lblContribTitle, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_width(ui_lblContribTitle, 5, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_side(ui_lblContribTitle, LV_BORDER_SIDE_LEFT, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_left(ui_lblContribTitle, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_right(ui_lblContribTitle, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_top(ui_lblContribTitle, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_bottom(ui_lblContribTitle, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_row(ui_lblContribTitle, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_column(ui_lblContribTitle, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_text_color(ui_lblContribTitle, lv_color_hex(0x87C97C), MU_OBJ_MAIN_FOCUS);
    lv_obj_set_style_text_opa(ui_lblContribTitle, 255, MU_OBJ_MAIN_FOCUS);

    name_flow(ui_conContrib, contributors, lv_color_hex(0x87C97C));

    ui_conSpecial = lv_obj_create(ui_conCredits);
    lv_obj_remove_style_all(ui_conSpecial);
    lv_obj_set_width(ui_conSpecial, lv_pct(100));
    lv_obj_set_height(ui_conSpecial, LV_SIZE_CONTENT);
    lv_obj_set_x(ui_conSpecial, lv_pct(0));
    lv_obj_set_y(ui_conSpecial, lv_pct(0));
    lv_obj_set_align(ui_conSpecial, LV_ALIGN_CENTER);
    lv_obj_set_flex_flow(ui_conSpecial, LV_FLEX_FLOW_COLUMN_WRAP);
    lv_obj_set_flex_align(ui_conSpecial, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_add_flag(ui_conSpecial, LV_OBJ_FLAG_OVERFLOW_VISIBLE);
    lv_obj_clear_flag(ui_conSpecial, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_left(ui_conSpecial, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_right(ui_conSpecial, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_top(ui_conSpecial, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_bottom(ui_conSpecial, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_row(ui_conSpecial, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_column(ui_conSpecial, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_opa(ui_conSpecial, 0, MU_OBJ_MAIN_DEFAULT);

    ui_lblSpecialTitle = lv_label_create(ui_conSpecial);
    lv_label_set_text(ui_lblSpecialTitle, "");
    lv_obj_set_height(ui_lblSpecialTitle, LV_SIZE_CONTENT);
    lv_obj_set_width(ui_lblSpecialTitle, lv_pct(100));
    lv_obj_set_x(ui_lblSpecialTitle, 0);
    lv_obj_set_y(ui_lblSpecialTitle, 0);
    lv_obj_set_align(ui_lblSpecialTitle, LV_ALIGN_TOP_MID);
    lv_label_set_text(ui_lblSpecialTitle, "Special Thanks");
    lv_obj_set_scroll_dir(ui_lblSpecialTitle, LV_DIR_HOR);
    lv_obj_set_style_text_color(ui_lblSpecialTitle, lv_color_hex(0x87C97C), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_text_opa(ui_lblSpecialTitle, 255, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_text_align(ui_lblSpecialTitle, LV_TEXT_ALIGN_CENTER, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_text_font(ui_lblSpecialTitle, header_font, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_color(ui_lblSpecialTitle, lv_color_hex(0xA5B2B5), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_opa(ui_lblSpecialTitle, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_grad_color(ui_lblSpecialTitle, lv_color_hex(0x0), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_main_stop(ui_lblSpecialTitle, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_grad_stop(ui_lblSpecialTitle, 200, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_grad_dir(ui_lblSpecialTitle, LV_GRAD_DIR_HOR, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_color(ui_lblSpecialTitle, lv_color_hex(0xA5B2B5), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_opa(ui_lblSpecialTitle, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_width(ui_lblSpecialTitle, 5, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_side(ui_lblSpecialTitle, LV_BORDER_SIDE_LEFT, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_left(ui_lblSpecialTitle, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_right(ui_lblSpecialTitle, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_top(ui_lblSpecialTitle, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_bottom(ui_lblSpecialTitle, 10, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_row(ui_lblSpecialTitle, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_column(ui_lblSpecialTitle, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_text_color(ui_lblSpecialTitle, lv_color_hex(0x87C97C), MU_OBJ_MAIN_FOCUS);
    lv_obj_set_style_text_opa(ui_lblSpecialTitle, 255, MU_OBJ_MAIN_FOCUS);

    ui_lblSpecialMid = lv_label_create(ui_conSpecial);
    lv_label_set_text(ui_lblSpecialMid, "");
    lv_obj_set_width(ui_lblSpecialMid, lv_pct(100));
    lv_obj_set_height(ui_lblSpecialMid, LV_SIZE_CONTENT);
    lv_obj_set_x(ui_lblSpecialMid, 0);
    lv_obj_set_y(ui_lblSpecialMid, 20);
    lv_obj_set_align(ui_lblSpecialMid, LV_ALIGN_CENTER);
    lv_label_set_text(ui_lblSpecialMid,
                      "A heartfelt thanks to artificers, druids, porters, theme creators, and translators "
                      "for your contributions to MustardOS. Your dedication has enriched this project, and I am "
                      "truly grateful for all your contributions and testing!"
    );
    lv_obj_set_scroll_dir(ui_lblSpecialMid, LV_DIR_HOR);
    lv_obj_set_style_text_color(ui_lblSpecialMid, lv_color_hex(0x87C97C), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_text_opa(ui_lblSpecialMid, 255, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_text_letter_space(ui_lblSpecialMid, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_text_line_space(ui_lblSpecialMid, 3, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_text_align(ui_lblSpecialMid, LV_TEXT_ALIGN_CENTER, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_color(ui_lblSpecialMid, lv_color_hex(0xA5B2B5), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_opa(ui_lblSpecialMid, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_width(ui_lblSpecialMid, 5, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_side(ui_lblSpecialMid, LV_BORDER_SIDE_LEFT, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_left(ui_lblSpecialMid, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_right(ui_lblSpecialMid, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_top(ui_lblSpecialMid, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_bottom(ui_lblSpecialMid, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_row(ui_lblSpecialMid, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_column(ui_lblSpecialMid, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_text_color(ui_lblSpecialMid, lv_color_hex(0x87C97C), MU_OBJ_MAIN_FOCUS);
    lv_obj_set_style_text_opa(ui_lblSpecialMid, 255, MU_OBJ_MAIN_FOCUS);

    ui_lblBongleTitle = lv_label_create(ui_conSpecial);
    lv_label_set_text(ui_lblBongleTitle, "");
    lv_obj_set_height(ui_lblBongleTitle, LV_SIZE_CONTENT);
    lv_obj_set_width(ui_lblBongleTitle, lv_pct(100));
    lv_obj_set_x(ui_lblBongleTitle, 0);
    lv_obj_set_y(ui_lblBongleTitle, 0);
    lv_obj_set_align(ui_lblBongleTitle, LV_ALIGN_TOP_MID);
    lv_label_set_text(ui_lblBongleTitle, "My One True Supporter");
    lv_obj_set_scroll_dir(ui_lblBongleTitle, LV_DIR_HOR);
    lv_obj_set_style_text_color(ui_lblBongleTitle, lv_color_hex(0x87C97C), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_text_opa(ui_lblBongleTitle, 255, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_text_align(ui_lblBongleTitle, LV_TEXT_ALIGN_CENTER, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_text_font(ui_lblBongleTitle, header_font, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_color(ui_lblBongleTitle, lv_color_hex(0xA5B2B5), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_opa(ui_lblBongleTitle, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_grad_color(ui_lblBongleTitle, lv_color_hex(0x0), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_main_stop(ui_lblBongleTitle, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_grad_stop(ui_lblBongleTitle, 200, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_grad_dir(ui_lblBongleTitle, LV_GRAD_DIR_HOR, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_color(ui_lblBongleTitle, lv_color_hex(0xA5B2B5), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_opa(ui_lblBongleTitle, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_width(ui_lblBongleTitle, 5, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_side(ui_lblBongleTitle, LV_BORDER_SIDE_LEFT, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_left(ui_lblBongleTitle, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_right(ui_lblBongleTitle, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_top(ui_lblBongleTitle, 20, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_bottom(ui_lblBongleTitle, 10, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_row(ui_lblBongleTitle, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_column(ui_lblBongleTitle, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_text_color(ui_lblBongleTitle, lv_color_hex(0x87C97C), MU_OBJ_MAIN_FOCUS);
    lv_obj_set_style_text_opa(ui_lblBongleTitle, 255, MU_OBJ_MAIN_FOCUS);

    ui_lblBongleMid = lv_label_create(ui_conSpecial);
    lv_label_set_text(ui_lblBongleMid, "");
    lv_obj_set_width(ui_lblBongleMid, lv_pct(100));
    lv_obj_set_height(ui_lblBongleMid, LV_SIZE_CONTENT);
    lv_obj_set_x(ui_lblBongleMid, 0);
    lv_obj_set_y(ui_lblBongleMid, 20);
    lv_obj_set_align(ui_lblBongleMid, LV_ALIGN_CENTER);
    lv_label_set_text(ui_lblBongleMid,
                      "To my wonderful wife, Mrs. Bongle. Whose amazing support, guidance, and patience has been my "
                      "light throughout this entire adventure. You mean everything to me and I love you more than "
                      "words could ever express!");
    lv_obj_set_scroll_dir(ui_lblBongleMid, LV_DIR_HOR);
    lv_obj_set_style_text_color(ui_lblBongleMid, lv_color_hex(0x87C97C), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_text_opa(ui_lblBongleMid, 255, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_text_letter_space(ui_lblBongleMid, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_text_line_space(ui_lblBongleMid, 3, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_text_align(ui_lblBongleMid, LV_TEXT_ALIGN_CENTER, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_color(ui_lblBongleMid, lv_color_hex(0xA5B2B5), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_opa(ui_lblBongleMid, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_width(ui_lblBongleMid, 5, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_side(ui_lblBongleMid, LV_BORDER_SIDE_LEFT, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_left(ui_lblBongleMid, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_right(ui_lblBongleMid, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_top(ui_lblBongleMid, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_bottom(ui_lblBongleMid, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_row(ui_lblBongleMid, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_column(ui_lblBongleMid, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_text_color(ui_lblBongleMid, lv_color_hex(0x87C97C), MU_OBJ_MAIN_FOCUS);
    lv_obj_set_style_text_opa(ui_lblBongleMid, 255, MU_OBJ_MAIN_FOCUS);

    ui_conKofi = lv_obj_create(ui_conCredits);
    lv_obj_remove_style_all(ui_conKofi);
    lv_obj_set_width(ui_conKofi, lv_pct(100));
    lv_obj_set_height(ui_conKofi, LV_SIZE_CONTENT);
    lv_obj_set_x(ui_conKofi, lv_pct(0));
    lv_obj_set_y(ui_conKofi, lv_pct(0));
    lv_obj_set_align(ui_conKofi, LV_ALIGN_CENTER);
    lv_obj_set_flex_flow(ui_conKofi, LV_FLEX_FLOW_COLUMN_WRAP);
    lv_obj_set_flex_align(ui_conKofi, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_add_flag(ui_conKofi, LV_OBJ_FLAG_OVERFLOW_VISIBLE);
    lv_obj_clear_flag(ui_conKofi, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_left(ui_conKofi, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_right(ui_conKofi, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_top(ui_conKofi, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_bottom(ui_conKofi, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_row(ui_conKofi, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_column(ui_conKofi, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_opa(ui_conKofi, 0, MU_OBJ_MAIN_DEFAULT);

    ui_lblKofiTitle = lv_label_create(ui_conKofi);
    lv_label_set_text(ui_lblKofiTitle, "");
    lv_obj_set_width(ui_lblKofiTitle, lv_pct(100));
    lv_obj_set_height(ui_lblKofiTitle, LV_SIZE_CONTENT);
    lv_obj_set_align(ui_lblKofiTitle, LV_ALIGN_TOP_MID);
    lv_label_set_text(ui_lblKofiTitle, "Thank you for choosing MustardOS");
    lv_obj_set_scroll_dir(ui_lblKofiTitle, LV_DIR_HOR);
    lv_obj_set_style_text_color(ui_lblKofiTitle, lv_color_hex(0xF7E318), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_text_opa(ui_lblKofiTitle, 255, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_text_align(ui_lblKofiTitle, LV_TEXT_ALIGN_CENTER, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_text_font(ui_lblKofiTitle, header_font, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_color(ui_lblKofiTitle, lv_color_hex(0xA5B2B5), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_opa(ui_lblKofiTitle, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_grad_color(ui_lblKofiTitle, lv_color_hex(0x0), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_main_stop(ui_lblKofiTitle, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_grad_stop(ui_lblKofiTitle, 200, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_grad_dir(ui_lblKofiTitle, LV_GRAD_DIR_HOR, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_color(ui_lblKofiTitle, lv_color_hex(0xA5B2B5), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_opa(ui_lblKofiTitle, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_width(ui_lblKofiTitle, 5, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_side(ui_lblKofiTitle, LV_BORDER_SIDE_LEFT, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_left(ui_lblKofiTitle, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_right(ui_lblKofiTitle, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_top(ui_lblKofiTitle, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_bottom(ui_lblKofiTitle, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_row(ui_lblKofiTitle, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_column(ui_lblKofiTitle, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_text_color(ui_lblKofiTitle, lv_color_hex(0xF8E008), MU_OBJ_MAIN_FOCUS);
    lv_obj_set_style_text_opa(ui_lblKofiTitle, 255, MU_OBJ_MAIN_FOCUS);

    ui_lblKofiMessageOne = lv_label_create(ui_conKofi);
    lv_label_set_text(ui_lblKofiMessageOne, "");
    lv_obj_set_width(ui_lblKofiMessageOne, lv_pct(100));
    lv_obj_set_height(ui_lblKofiMessageOne, LV_SIZE_CONTENT);
    lv_obj_set_align(ui_lblKofiMessageOne, LV_ALIGN_CENTER);
    lv_label_set_text(ui_lblKofiMessageOne,
                      "You can support us by donating or subscribing which helps the "
                      "development of this project.\n\nThis project is done as a hobby!");
    lv_obj_set_scroll_dir(ui_lblKofiMessageOne, LV_DIR_HOR);
    lv_obj_set_style_text_color(ui_lblKofiMessageOne, lv_color_hex(0xDDA200), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_text_opa(ui_lblKofiMessageOne, 255, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_text_letter_space(ui_lblKofiMessageOne, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_text_line_space(ui_lblKofiMessageOne, 2, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_text_align(ui_lblKofiMessageOne, LV_TEXT_ALIGN_CENTER, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_color(ui_lblKofiMessageOne, lv_color_hex(0xA5B2B5), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_opa(ui_lblKofiMessageOne, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_width(ui_lblKofiMessageOne, 5, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_side(ui_lblKofiMessageOne, LV_BORDER_SIDE_LEFT, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_left(ui_lblKofiMessageOne, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_right(ui_lblKofiMessageOne, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_top(ui_lblKofiMessageOne, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_bottom(ui_lblKofiMessageOne, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_row(ui_lblKofiMessageOne, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_column(ui_lblKofiMessageOne, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_text_color(ui_lblKofiMessageOne, lv_color_hex(0xF8E008), MU_OBJ_MAIN_FOCUS);
    lv_obj_set_style_text_opa(ui_lblKofiMessageOne, 255, MU_OBJ_MAIN_FOCUS);

    ui_lblKofiMessageTwo = lv_label_create(ui_conKofi);
    lv_label_set_text(ui_lblKofiMessageTwo, "");
    lv_obj_set_width(ui_lblKofiMessageTwo, lv_pct(100));
    lv_obj_set_height(ui_lblKofiMessageTwo, LV_SIZE_CONTENT);
    lv_obj_set_align(ui_lblKofiMessageTwo, LV_ALIGN_CENTER);
    lv_label_set_text(ui_lblKofiMessageTwo, "Scan the below QR Code to take you to the Ko-fi page!");
    lv_obj_set_scroll_dir(ui_lblKofiMessageTwo, LV_DIR_HOR);
    lv_obj_set_style_text_color(ui_lblKofiMessageTwo, lv_color_hex(0xDDA200), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_text_opa(ui_lblKofiMessageTwo, 255, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_text_letter_space(ui_lblKofiMessageTwo, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_text_line_space(ui_lblKofiMessageTwo, 2, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_text_align(ui_lblKofiMessageTwo, LV_TEXT_ALIGN_CENTER, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_color(ui_lblKofiMessageTwo, lv_color_hex(0xA5B2B5), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_opa(ui_lblKofiMessageTwo, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_width(ui_lblKofiMessageTwo, 5, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_side(ui_lblKofiMessageTwo, LV_BORDER_SIDE_LEFT, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_left(ui_lblKofiMessageTwo, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_right(ui_lblKofiMessageTwo, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_top(ui_lblKofiMessageTwo, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_bottom(ui_lblKofiMessageTwo, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_row(ui_lblKofiMessageTwo, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_column(ui_lblKofiMessageTwo, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_text_color(ui_lblKofiMessageTwo, lv_color_hex(0xF8E008), MU_OBJ_MAIN_FOCUS);
    lv_obj_set_style_text_opa(ui_lblKofiMessageTwo, 255, MU_OBJ_MAIN_FOCUS);

    ui_imgKofi = lv_img_create(ui_conKofi);
    lv_img_set_src(ui_imgKofi, &ui_image_Kofi);
    lv_obj_set_width(ui_imgKofi, LV_SIZE_CONTENT);
    lv_obj_set_height(ui_imgKofi, LV_SIZE_CONTENT);
    lv_obj_set_align(ui_imgKofi, LV_ALIGN_CENTER);
    lv_obj_add_flag(ui_imgKofi, LV_OBJ_FLAG_ADV_HITTEST);
    lv_obj_clear_flag(ui_imgKofi, LV_OBJ_FLAG_SCROLLABLE);

    ui_conMusic = lv_obj_create(ui_conCredits);
    lv_obj_remove_style_all(ui_conMusic);
    lv_obj_set_width(ui_conMusic, lv_pct(100));
    lv_obj_set_height(ui_conMusic, LV_SIZE_CONTENT);
    lv_obj_set_x(ui_conMusic, lv_pct(0));
    lv_obj_set_y(ui_conMusic, lv_pct(0));
    lv_obj_set_align(ui_conMusic, LV_ALIGN_CENTER);
    lv_obj_set_flex_flow(ui_conMusic, LV_FLEX_FLOW_COLUMN_WRAP);
    lv_obj_set_flex_align(ui_conMusic, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_add_flag(ui_conMusic, LV_OBJ_FLAG_OVERFLOW_VISIBLE);
    lv_obj_clear_flag(ui_conMusic, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_left(ui_conMusic, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_right(ui_conMusic, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_top(ui_conMusic, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_bottom(ui_conMusic, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_row(ui_conMusic, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_column(ui_conMusic, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_opa(ui_conMusic, 0, MU_OBJ_MAIN_DEFAULT);

    ui_lblMusicTitle = lv_label_create(ui_conMusic);
    lv_label_set_text(ui_lblMusicTitle, "");
    lv_obj_set_width(ui_lblMusicTitle, lv_pct(100));
    lv_obj_set_height(ui_lblMusicTitle, LV_SIZE_CONTENT);
    lv_obj_set_x(ui_lblMusicTitle, 0);
    lv_obj_set_y(ui_lblMusicTitle, 20);
    lv_obj_set_align(ui_lblMusicTitle, LV_ALIGN_TOP_MID);
    lv_label_set_text(ui_lblMusicTitle, "Supporter Music");
    lv_obj_set_scroll_dir(ui_lblMusicTitle, LV_DIR_HOR);
    lv_obj_set_style_text_color(ui_lblMusicTitle, lv_color_hex(0xF7E318), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_text_opa(ui_lblMusicTitle, 255, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_text_align(ui_lblMusicTitle, LV_TEXT_ALIGN_CENTER, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_text_font(ui_lblMusicTitle, header_font, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_color(ui_lblMusicTitle, lv_color_hex(0xA5B2B5), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_opa(ui_lblMusicTitle, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_grad_color(ui_lblMusicTitle, lv_color_hex(0x0), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_main_stop(ui_lblMusicTitle, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_grad_stop(ui_lblMusicTitle, 200, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_grad_dir(ui_lblMusicTitle, LV_GRAD_DIR_HOR, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_color(ui_lblMusicTitle, lv_color_hex(0xA5B2B5), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_opa(ui_lblMusicTitle, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_width(ui_lblMusicTitle, 5, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_side(ui_lblMusicTitle, LV_BORDER_SIDE_LEFT, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_left(ui_lblMusicTitle, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_right(ui_lblMusicTitle, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_top(ui_lblMusicTitle, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_bottom(ui_lblMusicTitle, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_row(ui_lblMusicTitle, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_column(ui_lblMusicTitle, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_text_color(ui_lblMusicTitle, lv_color_hex(0xF8E008), MU_OBJ_MAIN_FOCUS);
    lv_obj_set_style_text_opa(ui_lblMusicTitle, 255, MU_OBJ_MAIN_FOCUS);

    ui_lblMusicMessage = lv_label_create(ui_conMusic);
    lv_label_set_text(ui_lblMusicMessage, "");
    lv_obj_set_width(ui_lblMusicMessage, lv_pct(100));
    lv_obj_set_height(ui_lblMusicMessage, LV_SIZE_CONTENT);
    lv_obj_set_x(ui_lblMusicMessage, 0);
    lv_obj_set_y(ui_lblMusicMessage, 30);
    lv_obj_set_align(ui_lblMusicMessage, LV_ALIGN_CENTER);
    if (config.BOOT.FACTORY_RESET) {
        lv_label_set_text(ui_lblMusicMessage,
                          "\nTrack - " SONG_TRACK
                          "\nArtist - " SONG_ARTIST
                          "\n\n\nYour device will now reboot...");
    } else {
        lv_label_set_text(ui_lblMusicMessage,
                          "\nTrack - " SONG_TRACK
                          "\nArtist - " SONG_ARTIST
                          "\n\n\nHave a blessed day...");
    }
    lv_obj_set_scroll_dir(ui_lblMusicMessage, LV_DIR_HOR);
    lv_obj_set_style_text_color(ui_lblMusicMessage, lv_color_hex(0xDDA200), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_text_opa(ui_lblMusicMessage, 255, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_text_letter_space(ui_lblMusicMessage, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_text_line_space(ui_lblMusicMessage, 2, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_text_align(ui_lblMusicMessage, LV_TEXT_ALIGN_CENTER, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_color(ui_lblMusicMessage, lv_color_hex(0xA5B2B5), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_opa(ui_lblMusicMessage, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_width(ui_lblMusicMessage, 5, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_side(ui_lblMusicMessage, LV_BORDER_SIDE_LEFT, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_left(ui_lblMusicMessage, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_right(ui_lblMusicMessage, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_top(ui_lblMusicMessage, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_bottom(ui_lblMusicMessage, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_row(ui_lblMusicMessage, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_column(ui_lblMusicMessage, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_text_color(ui_lblMusicMessage, lv_color_hex(0xF8E008), MU_OBJ_MAIN_FOCUS);
    lv_obj_set_style_text_opa(ui_lblMusicMessage, 255, MU_OBJ_MAIN_FOCUS);

    lv_disp_load_scr(ui_scrCredits);
}
