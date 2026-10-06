// ============================================================================
// UI simulator entry point: renders pages on the PC console (no hardware).
// Usage:
//   ui_sim                -> interactive, press Enter to advance
//   ui_sim --all          -> dump every page in one shot
//   ui_sim <n>            -> render only page n (1-based), repeat
//   ui_sim <n> <count>    -> render page n, <count> times (1-based)
// ============================================================================

#include "ui_layout.h"
#include "ui_pages.h"
#include "canvas_console.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

static const Ui::SensorData PREVIEW_DATA{/*temperature*/ 23.4f, /*humidity*/ 55.0f,
                                         /*tvoc*/ 1187, /*eco2*/ 1742, /*aqi*/ 4};

static void print_usage()
{
    std::printf("Usage:\n");
    std::printf("  ui_sim                interactive: press Enter to advance each page\n");
    std::printf("  ui_sim --all          dump every page in one shot\n");
    std::printf("  ui_sim <n>            render page n (1..%u), once\n",
                (unsigned)Ui::PAGE_COUNT);
    std::printf("  ui_sim <n> <count>    render page n (1..%u), <count> times\n",
                (unsigned)Ui::PAGE_COUNT);
}

static void render_one(ConsoleCanvas &c, uint8_t pageIdx)
{
    std::printf("\n=== Page %u/%u : %s ===\n",
                (unsigned)(pageIdx + 1), (unsigned)Ui::PAGE_COUNT,
                Ui::PAGES[pageIdx].title);
    c.clear();
    Ui::PAGES[pageIdx].draw(c, PREVIEW_DATA, pageIdx);
    c.flush();
}

static int parse_uint(const char *s, int &out)
{
    if (!s || !*s)
        return 0;
    char *end = nullptr;
    long v = std::strtol(s, &end, 10);
    if (end == s || (end && *end != '\0'))
        return 0;
    out = (int)v;
    return 1;
}

int main(int argc, char **argv)
{
    ConsoleCanvas c;

    // No args: legacy interactive mode.
    if (argc == 1)
    {
        for (uint8_t i = 0; i < Ui::PAGE_COUNT; ++i)
        {
            render_one(c, i);
            if (i + 1 < Ui::PAGE_COUNT)
            {
                std::printf("\n[Enter] next  ");
                std::fflush(stdout);
                std::getchar();
            }
        }
        return 0;
    }

    // --all: dump every page.
    if (std::strcmp(argv[1], "--all") == 0)
    {
        for (uint8_t i = 0; i < Ui::PAGE_COUNT; ++i)
            render_one(c, i);
        return 0;
    }

    // --help / -h
    if (std::strcmp(argv[1], "--help") == 0 || std::strcmp(argv[1], "-h") == 0)
    {
        print_usage();
        return 0;
    }

    // <n> or <n> <count>: render one specific page.
    int n = 0, count = 1;
    if (!parse_uint(argv[1], n))
    {
        std::fprintf(stderr, "Error: invalid page number '%s'\n\n", argv[1]);
        print_usage();
        return 2;
    }
    if (argc >= 3 && !parse_uint(argv[2], count))
    {
        std::fprintf(stderr, "Error: invalid repeat count '%s'\n\n", argv[2]);
        print_usage();
        return 2;
    }

    if (n < 1 || n > (int)Ui::PAGE_COUNT)
    {
        std::fprintf(stderr, "Error: page %d out of range (1..%u)\n\n",
                     n, (unsigned)Ui::PAGE_COUNT);
        print_usage();
        return 2;
    }
    if (count < 1)
        count = 1;

    uint8_t pageIdx = (uint8_t)(n - 1);
    for (int i = 0; i < count; ++i)
    {
        if (count > 1)
            std::printf("\n--- render #%d ---\n", i + 1);
        render_one(c, pageIdx);
    }

    return 0;
}