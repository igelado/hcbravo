// SPDX-License-Identifier: LGPL-2.1-only
#include <windows.h>
#include <cstdlib>

int main(int argc, char ** argv);

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
    return main(__argc, __argv);
}
