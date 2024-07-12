#pragma once
struct ContentItem {
    char* path;
    int type;
    char* display_name;
    void* ui_item;
    void* ui_icon;
    void* ui_description;
};