// SPDX-License-Identifier: LGPL-2.1-only
#include "dataref-catalog.h"

#include <stdexcept>

#ifdef HCBRAVO_ONLINE_DATAREFS
#include <memory>

#include <curl/curl.h>
#include <yaml-cpp/yaml.h>

namespace {

size_t append_response(char * data, size_t size, size_t count, void * context) noexcept {
    constexpr size_t limit = 12 * 1024 * 1024;
    auto & response = *static_cast<std::string *>(context);
    if(size == 0 || count > (limit - response.size()) / size) return 0;
    const size_t bytes = size * count;
    try {
        response.append(data, bytes);
        return bytes;
    }
    catch(...) {
        return 0;
    }
}

int cancel_download(void * context, curl_off_t, curl_off_t, curl_off_t, curl_off_t) {
    return static_cast<const std::stop_token *>(context)->stop_requested() ? 1 : 0;
}

void collect_paths(const YAML::Node & entry, std::unordered_set<std::string> & paths, int depth) {
    if(depth > 40 || !entry.IsMap()) throw std::runtime_error("Unexpected DataRef catalog format.");
    if(const auto children = entry["children"]) {
        if(!children.IsSequence()) throw std::runtime_error("Unexpected DataRef catalog format.");
        for(const auto & child : children) collect_paths(child, paths, depth + 1);
    } else if(const auto name = entry["name"]; name.IsScalar()) {
        paths.insert(name.as<std::string>());
    }
}

} // namespace
#endif

std::unordered_set<std::string> download_dataref_catalog(std::stop_token stop) {
#ifdef HCBRAVO_ONLINE_DATAREFS
    std::unique_ptr<CURL, decltype(&curl_easy_cleanup)> request(curl_easy_init(), curl_easy_cleanup);
    if(!request) throw std::runtime_error("Could not initialize the catalog download.");

    std::string response;
    curl_easy_setopt(request.get(), CURLOPT_URL,
                     "https://developer.x-plane.com/wp-content/plugins/datarefs-app/datarefs.json");
    curl_easy_setopt(request.get(), CURLOPT_ACCEPT_ENCODING, "");
    curl_easy_setopt(request.get(), CURLOPT_FAILONERROR, 1L);
    curl_easy_setopt(request.get(), CURLOPT_CONNECTTIMEOUT, 5L);
    curl_easy_setopt(request.get(), CURLOPT_TIMEOUT, 30L);
    curl_easy_setopt(request.get(), CURLOPT_NOSIGNAL, 1L);
    curl_easy_setopt(request.get(), CURLOPT_WRITEFUNCTION, append_response);
    curl_easy_setopt(request.get(), CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(request.get(), CURLOPT_NOPROGRESS, 0L);
    curl_easy_setopt(request.get(), CURLOPT_XFERINFOFUNCTION, cancel_download);
    curl_easy_setopt(request.get(), CURLOPT_XFERINFODATA, &stop);
    const auto result = curl_easy_perform(request.get());
    if(result != CURLE_OK) throw std::runtime_error(curl_easy_strerror(result));

    const auto document = YAML::Load(response);
    std::unordered_set<std::string> paths;
    collect_paths(document["drefs"], paths, 0);
    if(paths.empty()) throw std::runtime_error("The DataRef catalog is empty.");
    return paths;
#else
    (void)stop;
    throw std::runtime_error("This build has no libcurl support.");
#endif
}
