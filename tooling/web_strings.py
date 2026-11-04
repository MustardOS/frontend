import re
import sys
from html.parser import HTMLParser

ATTRIBUTES = {"placeholder", "aria-label", "title", "alt"}
SKIPPED_TAGS = {"script", "style", "code", "pre", "kbd", "samp"}
BUTTONS = re.compile(r"^(A|B|X|Y|[LR][1-3])$")
NAMES = {"MustardOS", "Pickles", "Wasabi", "RetroArch", "Discord", "IRC", "Syncthing", "muos.local"}
HELPERS = re.compile(r'(?:\b(?:showToast|action)\(|\blabel: )"([^"]*)"')


class PageStrings(HTMLParser):
    def __init__(self):
        super().__init__(convert_charrefs=True)
        self.skipping = 0
        self.found = []

    def add(self, text):
        text = re.sub(r"\s+", " ", text).strip()
        if not text or not any(c.isalpha() for c in text):
            return
        if BUTTONS.match(text) or text in NAMES:
            return
        self.found.append(text)

    def add_attributes(self, attrs):
        for name, value in attrs:
            if name in ATTRIBUTES and value:
                self.add(value)

    def handle_starttag(self, tag, attrs):
        if tag in SKIPPED_TAGS:
            self.skipping += 1
        self.add_attributes(attrs)

    def handle_startendtag(self, tag, attrs):
        self.add_attributes(attrs)

    def handle_endtag(self, tag):
        if tag in SKIPPED_TAGS and self.skipping:
            self.skipping -= 1

    def handle_data(self, data):
        if not self.skipping:
            self.add(data)


def script_strings(source):
    found = []
    for text in HELPERS.findall(source):
        text = text.strip()
        if text and any(c.isalpha() for c in text) and not BUTTONS.match(text) and text not in NAMES:
            found.append(text)
    return found


def main():
    seen = set()
    for path in sys.argv[1:]:
        with open(path, encoding="utf-8") as handle:
            source = handle.read()
        if path.endswith(".js"):
            found = script_strings(source)
        else:
            parser = PageStrings()
            parser.feed(source)
            found = parser.found
        for text in found:
            if text not in seen:
                seen.add(text)
                print(text)


if __name__ == "__main__":
    main()
