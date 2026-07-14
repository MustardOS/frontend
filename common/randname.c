#include "randname.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>

const char *const randname_adjectives[RANDNAME_ADJECTIVE_COUNT] = {
    "Silent",   "Turbo",   "Crimson", "Cosmic", "Rusty",    "Golden",   "Shadow", "Electric",  "Ancient", "Rogue",
    "Frozen",   "Blazing", "Mystic",  "Iron",   "Neon",     "Quiet",    "Wild",   "Lucky",     "Dizzy",   "Grumpy",
    "Sneaky",   "Bold",    "Feral",   "Solar",  "Lunar",    "Toxic",    "Crispy", "Glitchy",   "Rowdy",   "Noble",
    "Savage",   "Jolly",   "Spicy",   "Frosty", "Radiant",  "Wicked",   "Nimble", "Cheeky",    "Stormy",  "Vivid",
    "Phantom",  "Rapid",   "Sturdy",  "Plucky", "Zesty",    "Dusty",    "Molten", "Vintage",   "Fuzzy",   "Epic",
    "Amber",    "Arcade",  "Breezy",  "Bright", "Brisk",    "Chunky",   "Cobalt", "Copper",    "Dapper",  "Daring",
    "Dreamy",   "Emerald", "Fizzy",   "Fluffy", "Frantic",  "Galactic", "Giddy",  "Hasty",     "Hyper",   "Icy",
    "Indigo",   "Jazzy",   "Kinetic", "Mellow", "Midnight", "Mini",     "Minty",  "Mossy",     "Peachy",  "Pixelated",
    "Polished", "Prickly", "Quirky",  "Retro",  "Scarlet",  "Shiny",    "Sleepy", "Sly",       "Snazzy",  "Speedy",
    "Stellar",  "Sunny",   "Swift",   "Tiny",   "Toasty",   "Tricky",   "Velvet", "Whimsical", "Wobbly",  "Wooden"
};

const char *const randname_nouns[RANDNAME_NOUN_COUNT] = {
    "Falcon",   "Pixel",   "Wizard",   "Goblin",     "Raptor",     "Cartridge",  "Ninja",   "Badger",   "Comet",
    "Joystick", "Phoenix", "Turtle",   "Reactor",    "Wolf",       "Rocket",     "Beetle",  "Dragon",   "Otter",
    "Blaster",  "Panda",   "Cyborg",   "Ferret",     "Guardian",   "Sprocket",   "Viper",   "Muffin",   "Hamster",
    "Golem",    "Gremlin", "Bandit",   "Meteor",     "Walrus",     "Sentinel",   "Gadget",  "Yeti",     "Raccoon",
    "Titan",    "Pickle",  "Kraken",   "Marmot",     "Vortex",     "Biscuit",    "Panther", "Circuit",  "Griffin",
    "Weasel",   "Nomad",   "Pretzel",  "Cobra",      "Yak",        "Albatross",  "Axolotl", "Bilby",    "Bunyip",
    "Button",   "Cactus",  "Capybara", "Cassowary",  "Cockatoo",   "Controller", "Dingo",   "Dolphin",  "Dumpling",
    "Echidna",  "Emu",     "Gecko",    "Gherkin",    "Koala",      "Kookaburra", "Lantern", "Lynx",     "Mantis",
    "Moose",    "Moth",    "Narwhal",  "Numbat",     "Octopus",    "Penguin",    "Pigeon",  "Platypus", "Possum",
    "Puffin",   "Pumpkin", "Quokka",   "Robot",      "Salamander", "Sausage",    "Shark",   "Sloth",    "Sparrow",
    "Starship", "Switch",  "Taco",     "Tardigrade", "Toucan",     "Waffle",     "Wombat",  "Zapper",   "Zeppelin",
    "Mustard"
};

static size_t randname_random_index() {
    const unsigned long range = (unsigned long) RAND_MAX + 1UL;
    const unsigned long limit = range - range % RANDNAME_COUNT;
    unsigned long value;

    do {
        value = (unsigned long) rand();
    } while (value >= limit);

    return value % RANDNAME_COUNT;
}

int randname_format(
    char *out, const size_t out_size, const size_t adjective_index, const size_t noun_index, const char *separator
) {

    if (out == NULL || out_size == 0U || adjective_index >= RANDNAME_ADJECTIVE_COUNT
        || noun_index >= RANDNAME_NOUN_COUNT) {
        return -1;
    }

    out[0] = '\0';

    if (separator == NULL) {
        separator = RANDNAME_SEP;
    }

    const int written =
        snprintf(out, out_size, "%s%s%s", randname_adjectives[adjective_index], separator, randname_nouns[noun_index]);

    if (written < 0 || (size_t) written >= out_size) {
        out[0] = '\0';
        return -1;
    }

    return 0;
}

int randname_generate_with_separator(char *out, const size_t out_size, const char *separator) {
    const size_t adjective_index = randname_random_index();
    const size_t noun_index = randname_random_index();

    return randname_format(out, out_size, adjective_index, noun_index, separator);
}

int randname_generate(char *out, const size_t out_size) {
    return randname_generate_with_separator(out, out_size, RANDNAME_SEP);
}

int randname_from_id(char *out, const size_t out_size, const size_t id) {

    if (id >= RANDNAME_COMBINATION_COUNT) {
        if (out != NULL && out_size > 0U) {
            out[0] = '\0';
        }
        return -1;
    }

    const size_t adjective_index = id / RANDNAME_NOUN_COUNT;
    const size_t noun_index = id % RANDNAME_NOUN_COUNT;

    return randname_format(out, out_size, adjective_index, noun_index, RANDNAME_SEP);
}
