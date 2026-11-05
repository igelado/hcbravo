// SPDX-License-Identifier: LGPL-2.1-only
#include <algorithm>
#include <chrono>
#include <future>
#include <iostream>
#include <string_view>

#include <GLFW/glfw3.h>
#include <imgui.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl3.h>

#include "dataref-catalog.h"
#include "profile-editor.h"

int main(int argc, char ** argv) {
    if(argc == 2 && std::string_view(argv[1]) == "--help") {
        std::cout << "Usage: hcbravo-profile-editor [--profiles-dir DIRECTORY]\n";
        return 0;
    }
    if(argc != 1 && (argc != 3 || std::string_view(argv[1]) != "--profiles-dir")) {
        std::cerr << "Usage: hcbravo-profile-editor [--profiles-dir DIRECTORY]\n";
        return 2;
    }
    profile_editor editor(argc == 3 ? argv[2] : "conf");

    glfwSetErrorCallback([](int, const char * error) { std::cerr << "GLFW: " << error << '\n'; });
    if(!glfwInit()) return 1;
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
    int width = profile_editor::initial_width;
    int height = profile_editor::initial_height;
    if(auto * monitor = glfwGetPrimaryMonitor()) {
        int available_width, available_height;
        glfwGetMonitorWorkarea(monitor, nullptr, nullptr, &available_width, &available_height);
        width = std::min(width, std::max(1, available_width - 40));
        height = std::min(height, std::max(1, available_height - 40));
    }
    auto * window = glfwCreateWindow(width, height, "HCBravo Aircraft Profile Editor", nullptr, nullptr);
    if(window == nullptr) {
        glfwTerminate();
        return 1;
    }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    editor.load_fonts();
    ImGui::GetIO().IniFilename = nullptr;
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 150");

    std::stop_source cancel_catalog;
    auto catalog = std::async(std::launch::async, download_dataref_catalog, cancel_catalog.get_token());
    std::string lookup_status = "Loading the public X-Plane DataRef catalog...";
    while(!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        if(catalog.valid() && catalog.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
            try {
                editor.set_dataref_lookup([paths = catalog.get()](const char * path) {
                    return paths.contains(path);
                }, "the public X-Plane catalog");
                lookup_status = "Checking the public X-Plane catalog. Custom DataRefs may be absent.";
            }
            catch(const std::exception & error) {
                lookup_status = std::string("Online DataRef lookup unavailable. ") + error.what();
            }
        }
        int width, height;
        glfwGetFramebufferSize(window, &width, &height);
        if(width == 0 || height == 0) {
            glfwWaitEventsTimeout(0.1);
            continue;
        }

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        if(editor.draw({}, lookup_status.c_str())) glfwSetWindowShouldClose(window, true);
        ImGui::Render();

        glViewport(0, 0, width, height);
        glClearColor(0.08f, 0.08f, 0.08f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(window);
    }

    cancel_catalog.request_stop();
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwDestroyWindow(window);
    glfwTerminate();
}
