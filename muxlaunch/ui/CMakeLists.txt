SET(SOURCES ui_scrLaunch.c
    ui.c
    ui_comp_hook.c
    ui_helpers.c
    ui_events.c
    ui_img_nothing_png.c
    ui_img_muoskofi_png.c
    ui_img_muoslogotop_png.c
    ui_img_space_png.c
    fonts/ui_font_Awesome.c
    fonts/ui_font_AwesomeBig.c
    fonts/ui_font_AwesomeBrand.c
    fonts/ui_font_AwesomeBrandSmall.c
    fonts/ui_font_AwesomeSmall.c
    fonts/ui_font_Gamepad.c
    fonts/ui_font_GamepadNav.c
    fonts/ui_font_NotoSans.c
    fonts/ui_font_NotoSansBig.c
    fonts/ui_font_NotoSansSmall.c)

add_library(ui ${SOURCES})
