#include "ui_muxcredits.h"
#include "../../common/common.h"
#include "../../common/config.h"

#define MU_STYLE(o, prop, val) lv_obj_set_style_##prop((o), (val), MU_OBJ_MAIN_DEFAULT)
#define MU_FOCUS(o, prop, val) lv_obj_set_style_##prop((o), (val), MU_OBJ_MAIN_FOCUS)

#define SONG_TITLE   "Supporter Music"
#define SONG_TRACK   "Final Frontier"
#define SONG_ARTIST  "Nimn One"
#define SONG_STRING  "\nTrack - " SONG_TRACK "\nArtist - " SONG_ARTIST "\n\n\n"
#define SONG_REBOOT  "Your device will now reboot..."
#define SONG_BLESSED "Have a blessed day..."

#define COL_BG_HEX       0xA5B2B5
#define COL_FOCUS_HEX    0xF8E008
#define COL_TITLE_YELLOW 0xF7E318
#define COL_TEXT_AMBER   0xDDA200
#define COL_CON_HEX      0x87C97C
#define COL_CMD_HEX      0xFF0000
#define COL_ENF_HEX      0xFF4500
#define COL_WIZ_HEX      0xED6900
#define COL_HERO_HEX     0xFFDD22
#define COL_KNIGHT_HEX   0xED6900

#define NM_CON    "%s\nContributors"
#define NM_CMD    "Commanders"
#define NM_ENF    "\n\nEnforcers"
#define NM_WIZ    "Wizards"
#define NM_HERO   "Heroes"
#define NM_KNIGHT "Knights"

#define LANG_MESSAGE  "\nThe people listed throughout have been amazing supporters and contributors to MustardOS and shaping its future.\n\nFrom the bottom of my heart, thank you for your time, ideas, testing, and support. I appreciate you all more than a simple \"thank you\" could express!"
#define LANG_SPECIAL  "Special Thanks"
#define LANG_EVERYONE "A heartfelt thanks to artificers, druids, porters, theme creators, and translators for your contributions to MustardOS. Your dedication has enriched this project, and I am truly grateful for all your contributions and testing!"
#define LANG_SUPPORT  "My One True Supporter"
#define LANG_BONGLE   "To my wonderful wife, Mrs. Bongle. Whose amazing support, guidance, and patience has been my light throughout this entire adventure. You mean everything to me and I love you more than words could ever express!"
#define LANG_THANKS   "Thank you for choosing MustardOS"
#define LANG_DONATE   "You can support us by donating or subscribing which helps the development of this project.\n\nThis project is done as a hobby!"
#define LANG_QRCODE   "Scan the below QR Code to take you to the Ko-fi page!"

static const char *commanders[] = {
        "xonglebongle",
        "antikk",
        "corey",
        "bitter_bizarro",
        NULL
};

static const char *enforcers[] = {
        "acmeplus",
        "bgelmini",
        "ilfordhp5",
        "duncanyoyo1",
        "illumini_85",
        "delibirb77",
        NULL
};

static const char *wizards[] = {
        "xquader",
        "ee1000",
        "kloptops",
        "thegammasqueeze",
        ".tokyovigilante",
        "joyrider3774",
        ".cebion",
        "irilivibi",
        "vagueparade",
        "shengy.",
        "siliconexarch",
        "shauninman",
        "johnnyonflame",
        "snowram",
        "habbening",
        "trngaje",
        "skorpy",
        "stanley_00",
        "ajmandourah",
        "bcat24",
        "xanxic",
        "midwan",
        "arkun_",
        "mikhailzrick",
        "retrogfx_",
        "aeverdyn",
        "kitfox618",
        "spycat88",
        "sundownersport",
        NULL
};

static const char *hero_one[] = {
        "romus85",
        "x_tremis",
        "lmarcomiranda",
        "timecarp",
        "intelliaim",
        "kentonftw",
        "bazkart",
        "msx6011",
        "btreecat",
        "teggydave",
        "zazouboy",
        "robbiet480",
        "luzfcb",
        "brohsnbluffs",
        "zaka1w3",
        "superzu",
        "nico_linber_36894",
        "pr0j3kt2501",
        "rosemelody254",
        "bigbossman0816",
        "meowman_",
        "kaeltis",
        "raouldook.",
        "paletochen",
        "benjaminbercy",
        "snesfan1",
        "asiaclonk",
        "jimmycrackedcorn_4711",
        "hueykablooey",
        "mrwhistles",
        "losermatic",
        NULL
};

static const char *hero_two[] = {
        "ivar2028",
        ".dririan",
        "spivvmeister",
        "sol6_vi",
        "qpla",
        "amos_06286",
        "techagent",
        "meanagar",
        "roundpi",
        "chiefwally_73445",
        "scy0n",
        "luckyphil",
        "nahck",
        "mach5682",
        "foamygames",
        "xraygoggles",
        "hybrid_sith",
        "mxdamp",
        "ownedmumbles",
        "exe0237",
        "kernelkritic",
        "verctexius",
        "misfitsorbet",
        "izzythefool",
        "bigolpeewee",
        ".zerohalo",
        "milkworlds",
        "amildinconvenience.",
        "kularose",
        ".rayman30",
        NULL
};

static const char *knight_one[] = {
        "notflacko",
        "jdanteq_18123",
        "skyarcher",
        "hai6266",
        "galloc",
        "_maxwellsdemon",
        "status.quo.exile",
        "phyrex",
        "kiko_lake",
        "arkholt",
        "julas8799",
        "fibroidjames",
        "allepac",
        "pakwan8234",
        "drisc",
        "clempurp9868",
        "aj15",
        "retrogamecorps",
        "rbndr_",
        "sanelessone",
        "hanswurstsenior",
        NULL
};

static const char *knight_two[] = {
        "billynaing",
        "nuke_67641",
        "zauberpony",
        "frickinfrogs",
        "smittywerbenjaegermanjensen9250", // Award for longest user name goes to...
        "wizardfights",
        "_wizdude",
        "stin87",
        "surge_84306",
        "phlurblepoot",
        "mrcee1503",
        "splendid88_98891",
        "yomama78",
        "admiralthrawn_1",
        "penpen2crayon",
        "jupyter.",
        "bburbank",
        "crownlessk",
        "johnunsc",
        "obitomatheus",
        NULL
};

static const char *contributors[] = {
        "bitter_bizarro",
        "bmdhacks",
        "xonglebongle",
        NULL
};

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

static inline void zero_pad(lv_obj_t *o) {
    lv_obj_set_style_pad_all(o, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_row(o, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_column(o, 0, MU_OBJ_MAIN_DEFAULT);
}

static lv_obj_t *section(lv_obj_t *parent, bool scroll_ver) {
    lv_obj_t *c = lv_obj_create(parent);
    lv_obj_remove_style_all(c);

    lv_obj_set_width(c, lv_pct(100));
    lv_obj_set_height(c, LV_SIZE_CONTENT);

    if (scroll_ver) {
        lv_obj_set_scroll_dir(c, LV_DIR_VER);
        lv_obj_set_style_max_height(c, lv_pct(100), 0);
    }

    lv_obj_set_x(c, lv_pct(0));
    lv_obj_set_y(c, lv_pct(0));

    MU_STYLE(c, bg_color, lv_color_hex(COL_BG_HEX));
    MU_STYLE(c, bg_opa, 0);

    lv_obj_set_align(c, LV_ALIGN_CENTER);
    lv_obj_set_flex_flow(c, LV_FLEX_FLOW_COLUMN_WRAP);
    lv_obj_set_flex_align(c, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_add_flag(c, LV_OBJ_FLAG_OVERFLOW_VISIBLE);
    lv_obj_clear_flag(c, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);

    zero_pad(c);
    MU_STYLE(c, opa, 0);

    return c;
}

static void container(lv_obj_t *lbl, const lv_font_t *font, lv_color_t col, int pad_top, int pad_bottom, int y) {
    lv_label_set_text(lbl, "");
    lv_obj_set_width(lbl, lv_pct(100));
    lv_obj_set_height(lbl, LV_SIZE_CONTENT);

    if (y) lv_obj_set_y(lbl, y);

    lv_obj_set_align(lbl, LV_ALIGN_TOP_MID);
    lv_obj_set_scroll_dir(lbl, LV_DIR_HOR);

    MU_STYLE(lbl, text_color, col);
    MU_STYLE(lbl, text_opa, LV_OPA_COVER);
    MU_STYLE(lbl, text_align, LV_TEXT_ALIGN_CENTER);
    MU_STYLE(lbl, text_font, font);

    MU_STYLE(lbl, bg_color, lv_color_hex(COL_BG_HEX));
    MU_STYLE(lbl, bg_opa, 0);
    MU_STYLE(lbl, bg_grad_color, lv_color_hex(0x0));
    MU_STYLE(lbl, bg_main_stop, 0);
    MU_STYLE(lbl, bg_grad_stop, 200);
    MU_STYLE(lbl, bg_grad_dir, LV_GRAD_DIR_HOR);

    MU_STYLE(lbl, border_color, lv_color_hex(COL_BG_HEX));
    MU_STYLE(lbl, border_opa, 0);
    MU_STYLE(lbl, border_width, 5);
    MU_STYLE(lbl, border_side, LV_BORDER_SIDE_LEFT);

    zero_pad(lbl);

    if (pad_top) MU_STYLE(lbl, pad_top, pad_top);
    if (pad_bottom) MU_STYLE(lbl, pad_bottom, pad_bottom);

    MU_FOCUS(lbl, text_color, lv_color_hex(COL_FOCUS_HEX));
    MU_FOCUS(lbl, text_opa, LV_OPA_COVER);
}

static lv_obj_t *title(lv_obj_t *parent, const char *text, const lv_font_t *font, lv_color_t col, int pad_top, int pad_bottom, int y) {
    lv_obj_t *lbl = lv_label_create(parent);

    container(lbl, font, col, pad_top, pad_bottom, y);
    lv_label_set_text(lbl, text);

    return lbl;
}

static lv_obj_t *content(lv_obj_t *parent, const char *text, lv_color_t col, int width_pct, int y) {
    lv_obj_t *lbl = lv_label_create(parent);
    lv_label_set_text(lbl, "");

    lv_obj_set_width(lbl, lv_pct(width_pct));
    lv_obj_set_height(lbl, LV_SIZE_CONTENT);

    if (y) lv_obj_set_y(lbl, y);

    lv_obj_set_align(lbl, LV_ALIGN_CENTER);
    lv_obj_set_scroll_dir(lbl, LV_DIR_HOR);

    lv_label_set_text(lbl, text);

    MU_STYLE(lbl, text_color, col);
    MU_STYLE(lbl, text_opa, LV_OPA_COVER);
    MU_STYLE(lbl, text_letter_space, 0);
    MU_STYLE(lbl, text_line_space, 2);
    MU_STYLE(lbl, text_align, LV_TEXT_ALIGN_CENTER);

    MU_STYLE(lbl, border_color, lv_color_hex(COL_BG_HEX));
    MU_STYLE(lbl, border_opa, 0);
    MU_STYLE(lbl, border_width, 5);
    MU_STYLE(lbl, border_side, LV_BORDER_SIDE_LEFT);

    zero_pad(lbl);

    MU_FOCUS(lbl, text_color, lv_color_hex(COL_FOCUS_HEX));
    MU_FOCUS(lbl, text_opa, LV_OPA_COVER);

    return lbl;
}

static lv_obj_t *name(lv_obj_t *parent, const char **list, lv_color_t hex) {
    lv_obj_t *wrap = lv_obj_create(parent);
    lv_obj_remove_style_all(wrap);

    lv_obj_set_size(wrap, lv_pct(90), LV_SIZE_CONTENT);
    MU_STYLE(wrap, bg_opa, 0);

    lv_obj_set_flex_flow(wrap, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_flex_align(wrap, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START);

    lv_obj_set_style_pad_column(wrap, 16, 0);
    lv_obj_set_style_pad_row(wrap, 4, 0);

    for (size_t i = 0; list[i]; ++i) {
        const char *txt = list[i];
        lv_obj_t *lbl = lv_label_create(wrap);

        int re_col = 0;
        if (txt && (txt[0] == LV_TXT_COLOR_CMD[0] || txt[0] == '#')) re_col = 1;
        lv_label_set_recolor(lbl, re_col);

        MU_STYLE(lbl, text_color, hex);
        MU_STYLE(lbl, text_opa, LV_OPA_COVER);
        lv_label_set_text(lbl, txt);
    }

    return wrap;
}

typedef struct {
    const char *title;
    const char **names;
    uint32_t color_hex;
} crew_sec_t;

void init_muxcredits(const lv_font_t *header_font) {
    ui_scrCredits = lv_obj_create(NULL);
    lv_obj_clear_flag(ui_scrCredits, LV_OBJ_FLAG_SCROLLABLE);

    MU_STYLE(ui_scrCredits, bg_color, lv_color_hex(0x000000));
    MU_STYLE(ui_scrCredits, bg_opa, LV_OPA_COVER);

    ui_conCredits = lv_obj_create(ui_scrCredits);
    lv_obj_remove_style_all(ui_conCredits);

    lv_obj_set_width(ui_conCredits, lv_pct(100));
    lv_obj_set_height(ui_conCredits, lv_pct(100));

    lv_obj_set_align(ui_conCredits, LV_ALIGN_CENTER);
    lv_obj_clear_flag(ui_conCredits, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);

    ui_conStart = section(ui_conCredits, false);

    ui_lblStartTitle = title(ui_conStart, LANG_THANKS, header_font, lv_color_hex(COL_TITLE_YELLOW), 0, 0, 0);
    ui_lblStartMessage = content(ui_conStart, LANG_MESSAGE, lv_color_hex(COL_TEXT_AMBER), 90, 0);

    ui_conOfficial = section(ui_conCredits, true);

    ui_lblCommanderTitle = lv_label_create(ui_conOfficial);
    container(ui_lblCommanderTitle, header_font, lv_color_hex(COL_CMD_HEX), 0, 0, 0);

    lv_label_set_text(ui_lblCommanderTitle, NM_CMD);
    name(ui_conOfficial, commanders, lv_color_hex(COL_CMD_HEX));

    ui_lblEnforcerTitle = lv_label_create(ui_conOfficial);
    container(ui_lblEnforcerTitle, header_font, lv_color_hex(COL_ENF_HEX), 0, 0, 0);

    lv_label_set_text(ui_lblEnforcerTitle, NM_ENF);
    name(ui_conOfficial, enforcers, lv_color_hex(COL_ENF_HEX));

    static const crew_sec_t SECTIONS_AFTER_OFFICIAL[] = {
            {NM_WIZ,    wizards,    COL_WIZ_HEX},
            {NM_HERO,   hero_one,   COL_HERO_HEX},
            {NM_HERO,   hero_two,   COL_HERO_HEX},
            {NM_KNIGHT, knight_one, COL_KNIGHT_HEX},
            {NM_KNIGHT, knight_two, COL_KNIGHT_HEX},
    };

    for (size_t i = 0; i < (sizeof SECTIONS_AFTER_OFFICIAL / sizeof SECTIONS_AFTER_OFFICIAL[0]); ++i) {
        const crew_sec_t *s = &SECTIONS_AFTER_OFFICIAL[i];

        lv_obj_t *con = section(ui_conCredits, true);
        lv_obj_t *ttl = lv_label_create(con);

        container(ttl, header_font, lv_color_hex(s->color_hex), 0, 0, 0);
        lv_label_set_text(ttl, s->title);

        name(con, s->names, lv_color_hex(s->color_hex));
        if (i == 0) {
            ui_conWizard = con;
            ui_lblWizardTitle = ttl;
        } else if (i == 1) {
            ui_conHeroOne = con;
            ui_lblHeroTitleOne = ttl;
        } else if (i == 2) {
            ui_conHeroTwo = con;
            ui_lblHeroTitleTwo = ttl;
        } else if (i == 3) {
            ui_conKnightOne = con;
            ui_lblKnightTitleOne = ttl;
        } else {
            ui_conKnightTwo = con;
            ui_lblKnightTitleTwo = ttl;
        }
    }

    ui_conContrib = section(ui_conCredits, true);
    ui_lblContribTitle = lv_label_create(ui_conContrib);

    container(ui_lblContribTitle, header_font, lv_color_hex(COL_CON_HEX), 0, 0, 0);

    lv_label_set_text_fmt(ui_lblContribTitle, NM_CON, str_replace(config.SYSTEM.VERSION, "_", " "));
    name(ui_conContrib, contributors, lv_color_hex(COL_CON_HEX));

    ui_conSpecial = section(ui_conCredits, false);

    ui_lblSpecialTitle = title(ui_conSpecial, LANG_SPECIAL, header_font, lv_color_hex(COL_CON_HEX), 0, 10, 0);
    ui_lblSpecialMid = content(ui_conSpecial, LANG_EVERYONE, lv_color_hex(COL_CON_HEX), 100, 20);

    ui_lblBongleTitle = title(ui_conSpecial, LANG_SUPPORT, header_font, lv_color_hex(COL_CON_HEX), 20, 10, 0);
    ui_lblBongleMid = content(ui_conSpecial, LANG_BONGLE, lv_color_hex(COL_CON_HEX), 100, 20);

    ui_conKofi = section(ui_conCredits, false);

    ui_lblKofiTitle = title(ui_conKofi, LANG_THANKS, header_font, lv_color_hex(COL_TITLE_YELLOW), 0, 0, 0);
    ui_lblKofiMessageOne = content(ui_conKofi, LANG_DONATE, lv_color_hex(COL_TEXT_AMBER), 100, 0);
    ui_lblKofiMessageTwo = content(ui_conKofi, LANG_QRCODE, lv_color_hex(COL_TEXT_AMBER), 100, 0);

    ui_imgKofi = lv_img_create(ui_conKofi);

    lv_img_set_src(ui_imgKofi, &ui_image_Kofi);
    lv_obj_set_width(ui_imgKofi, LV_SIZE_CONTENT);
    lv_obj_set_height(ui_imgKofi, LV_SIZE_CONTENT);

    lv_obj_set_align(ui_imgKofi, LV_ALIGN_CENTER);
    lv_obj_add_flag(ui_imgKofi, LV_OBJ_FLAG_ADV_HITTEST);

    lv_obj_clear_flag(ui_imgKofi, LV_OBJ_FLAG_SCROLLABLE);

    ui_conMusic = section(ui_conCredits, false);
    ui_lblMusicTitle = title(ui_conMusic, SONG_TITLE, header_font, lv_color_hex(COL_TITLE_YELLOW), 0, 0, 20);

    const char *song_msg = config.BOOT.FACTORY_RESET ? SONG_STRING SONG_REBOOT : SONG_STRING SONG_BLESSED;
    ui_lblMusicMessage = content(ui_conMusic, song_msg, lv_color_hex(COL_TEXT_AMBER), 100, 30);

    lv_disp_load_scr(ui_scrCredits);
}
