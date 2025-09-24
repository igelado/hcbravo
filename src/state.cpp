#include <XPLM/XPLMPlugin.h>
#include <XPLM/XPLMUtilities.h>

#include <expected>

#include <hidapi.h>

#include "state.h"

std::expected<state, int>
state::init() noexcept
{
    XPLMDebugString("Initializing HID");
    int res = hid_init();
    if(res < 0) {
        XPLMDebugString("Failed to initialize HID");
        return std::unexpected(1);
    }
    auto hid_handle = hid_open(0x294b, 0x1901, nullptr);
    if(hid_handle == nullptr) {
        XPLMDebugString("Failed to Open HoneyComb Bravo Quadrant");
        return std::unexpected(1);
    }

    auto commands = commands::init();
    if(commands.has_value() == false) return std::unexpected(commands.error());

    return state(hid_handle, std::move(commands.value()));
}

static const size_t FILES_PATH_SIZE = 64;
static const size_t FILES_BUFFER_SIZE = 4096;
static const size_t FILES_INDEX_SIZE = FILES_BUFFER_SIZE / FILES_PATH_SIZE;
static char files_buffer[FILES_BUFFER_SIZE];
static char * files_indexes[FILES_INDEX_SIZE];

static const char * plane_icao_label_ = "sim/aircraft/view/acf_ICAO";

state::state(hid_device * hid, commands::ptr_type && commands) :
    hid_(hid),
    cmds_(std::move(commands)),
    plane_icao_data_ref_(
        XPLMFindDataRef(plane_icao_label_)
    ),
    plane_(std::nullopt)
{
    memset(&this->leds_, 0, sizeof(led_state));


    XPLMDebugString("Reading Plugin Configuration Files");
    auto id = XPLMGetMyID();
    static char path[256];
    XPLMGetPluginInfo(id, nullptr, path, nullptr, nullptr);
    XPLMExtractFileAndPath(path);
    const char * sep = XPLMGetDirectorySeparator();
    std::string config_file_path = std::string(path).append(sep).append("conf");
    int index = 0;
    int total_conf_files = 0;
    do {
        int file_count = 0;
        XPLMGetDirectoryContents(
            config_file_path.c_str(),  index, files_buffer, FILES_BUFFER_SIZE,
            files_indexes, FILES_INDEX_SIZE, &total_conf_files, &file_count
        );
        for(auto n = 0; n < file_count; ++n) {
            std::string config_file = config_file_path + std::string(sep) + std::string(files_indexes[n]);
            profile_ptr prof = profile::from_yaml(config_file);
            for(const auto &model : prof->models()) {
                profile_map_.emplace(model, prof);
            }
        }
        total_conf_files += file_count;
    } while(index != total_conf_files - 1);


}



bool
state::load_plane() {
    static char icao_name[64];
    int ret = XPLMGetDatab(plane_icao_data_ref_, icao_name, 0, 64);
    if(ret < 64) icao_name[ret] = '\0';

    auto profile = profile_map_.find(icao_name);
    if(profile != profile_map_.end()) {
        plane_.emplace(profile->second);
        return true;
    }
    return false;
}

