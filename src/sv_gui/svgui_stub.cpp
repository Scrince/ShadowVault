#include <sv/exit_codes.hpp>

#include <cstdio>

int main() {
    std::fprintf(stderr, "GUI requires Qt6; use svcli\n");
    return sv::exit_codes::general_error;
}