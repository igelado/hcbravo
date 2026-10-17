// SPDX-License-Identifier: LGPL-2.1-only
#include "conf.h"

#include <iostream>
#include <string_view>

int main(int argc, char ** argv) {
    if(argc == 2 && std::string_view(argv[1]) == "--help") {
        std::cout << "Usage: hcbravo-validate FILE.yaml [FILE.yaml ...]\n"
                     "Checks profile structure and values; DataRef availability requires X-Plane.\n";
        return 0;
    }
    if(argc < 2) {
        std::cerr << "Usage: hcbravo-validate FILE.yaml [FILE.yaml ...]\n";
        return 2;
    }
    bool failed = false;
    for(int i = 1; i < argc; ++i) {
        const auto result = conf::validate_file(argv[i]);
        if(result) std::cout << argv[i] << ": valid\n";
        else {
            std::cerr << argv[i] << ": " << result.error() << '\n';
            failed = true;
        }
    }
    return failed ? 1 : 0;
}
