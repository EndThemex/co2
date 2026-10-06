// Quick inspector: read first non-empty content row from the simulator
// and print column ranges of "#" runs.
//
// Build (compile-only):
//   cl /nologo /std:c++latest /EHsc /O2 inspect.cpp
//
// Or just read stdin from `ui_sim 1`.

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

int main()
{
    std::vector<std::string> rows;
    char line[1024];
    while (std::fgets(line, sizeof(line), stdin))
    {
        size_t n = std::strlen(line);
        while (n > 0 && (line[n-1] == '\n' || line[n-1] == '\r')) line[--n] = '\0';
        rows.emplace_back(line);
    }

    // Print all rows numbered, plus "#" runs for every row.
    for (size_t i = 0; i < rows.size(); ++i)
    {
        const std::string &r = rows[i];
        std::printf("[%2zu] %s\n", i, r.c_str());
    }
    return 0;
}