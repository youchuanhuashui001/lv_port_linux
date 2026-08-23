SET(SOURCES screens/ui_Home.c
    screens/ui_Audio.c
    ui.c
    components/ui_comp_hook.c
    ui_helpers.c
    ui_events.c
    images/ui_img_background_png.c
    images/ui_img_audio_icon_png.c
    images/ui_img_book_icon_png.c
    images/ui_img_photo_icon_png.c
    images/ui_img_video_icon_png.c
    images/ui_img_setup_icon_png.c
    images/ui_img_audio_background_png.c
    images/ui_img_def_cover_png.c
    images/ui_img_play_png.c
    images/ui_img_pause_png.c
    images/ui_img_left_png.c
    images/ui_img_right_png.c
    images/ui_img_1385611356.c)

add_library(ui ${SOURCES})
