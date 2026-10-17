// SPDX-License-Identifier: LGPL-2.1-only
//
// tests/XPSDK/XPLM/XPLMUtilities.h
// XPlane Plugin for HoneyComb Bravo Throttle Controller
//
// Copyright (C) 2005 Isaac Gelado

#ifndef XPLMUTILITIES_H_
#define XPLMUTILITIES_H_

#include <cstdio>
#include <string>
#include <cstring>
#include <unordered_map>
#include <memory>
#include <vector>
#include <algorithm>

using XPLMCommandRef = void *;
enum XPLMCommandPhase { xplm_CommandBegin, xplm_CommandContinue, xplm_CommandEnd };
using XPLMCommandCallback_f = int (*)(XPLMCommandRef, XPLMCommandPhase, void *);

namespace xplm_test {
inline std::string log;
inline std::unordered_map<std::string, int> features;
// Directory listing served by XPLMGetDirectoryContents.
inline std::vector<std::string> directory_files {"evot.yaml"};
inline int directory_page_limit = 0;   // 0 means only the caller's buffers limit a page
inline bool directory_stall = false;   // report a truncated listing without returning files
inline int directory_calls = 0;
struct command {
    XPLMCommandCallback_f handler = nullptr;
    void * refcon = nullptr;
};
inline std::unordered_map<std::string, std::unique_ptr<command>> commands;
inline void invoke(const std::string & name, XPLMCommandPhase phase) {
    const auto & command = commands.at(name);
    command->handler(command.get(), phase, command->refcon);
}
}

static inline
void XPLMDebugString(const char * message) noexcept {
    xplm_test::log += message;
    std::fputs(message, stderr);
}

static inline void XPLMSetErrorCallback(void (*)(const char *)) {}
static inline void XPLMEnableFeature(const char * name, int enabled) { xplm_test::features[name] = enabled; }
static inline void XPLMReloadPlugins() {}
static inline XPLMCommandRef XPLMCreateCommand(const char * name, const char *) {
    auto & command = xplm_test::commands[name];
    if(!command) command = std::make_unique<xplm_test::command>();
    return command.get();
}
static inline void XPLMRegisterCommandHandler(XPLMCommandRef ref, XPLMCommandCallback_f handler, int, void * refcon) {
    auto * command = static_cast<xplm_test::command *>(ref);
    command->handler = handler;
    command->refcon = refcon;
}
static inline void XPLMUnregisterCommandHandler(XPLMCommandRef ref, XPLMCommandCallback_f, int, void *) {
    static_cast<xplm_test::command *>(ref)->handler = nullptr;
}
static inline char * XPLMExtractFileAndPath(char * path) {
    auto slash = std::strrchr(path, '/');
    if(slash) *slash = '\0';
    return slash;
}
static inline int XPLMGetDirectoryContents(const char *, int first, char * buffer, int buffer_size,
    char ** indexes, int index_size, int * total, int * count) {
    ++xplm_test::directory_calls;
    const int all = static_cast<int>(xplm_test::directory_files.size());
    *total = all;
    *count = 0;
    if(xplm_test::directory_stall) return 0;
    int limit = index_size;
    if(xplm_test::directory_page_limit > 0) limit = (std::min)(limit, xplm_test::directory_page_limit);
    int used = 0;
    for(int n = first; n < all && *count < limit; ++n) {
        const auto & name = xplm_test::directory_files[n];
        if(used + static_cast<int>(name.size()) + 1 > buffer_size) break;
        std::memcpy(buffer + used, name.c_str(), name.size() + 1);
        indexes[(*count)++] = buffer + used;
        used += static_cast<int>(name.size()) + 1;
    }
    return first + *count >= all ? 1 : 0;
}

#endif
