#include <iostream>
#include <cassert>
#include <memory>

#define GLFW_INCLUDE_VULKAN
#define GLFW_VULKAN_STATIC
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "document/canvas.h"
#include "ui/ui.h"

using std::unique_ptr;
using std::out_ptr;

VkSampleCountFlagBits max_sample_count;

static VKAPI_ATTR VkBool32 VKAPI_CALL debug_callback(
    VkDebugUtilsMessageSeverityFlagBitsEXT severity,
    VkDebugUtilsMessageTypeFlagsEXT,
    const VkDebugUtilsMessengerCallbackDataEXT* callback_data,
    void*
) {
    if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
        std::cerr << "validation layer error: " << callback_data->pMessage <<
            std::endl;
        // ignore error caused by Nsight
        if (strcmp(callback_data->pMessageIdName, "Loader Message") != 0)
            throw std::runtime_error("vulkan error");
    } else if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
        std::cout << "validation layer warning: " << callback_data->pMessage <<
            std::endl;
    }

    return VK_FALSE;
}

void glfw_check(int code) {
    if (code == GLFW_TRUE) {
        return;
    } else {
        throw std::runtime_error("Failed to initialize GLFW");
    }
}

struct unique_glfw {
    unique_glfw() { glfw_check(glfwInit()); }
    ~unique_glfw() { glfwTerminate(); }
};

struct glfw_window_deleter {
    typedef GLFWwindow* pointer;
    void operator()(GLFWwindow *window) {
        glfwDestroyWindow(window);
    }
};

using unique_window = unique_ptr<GLFWwindow, glfw_window_deleter>;

int main() {
    unique_glfw glfw;

    int window_width = 1280, window_height = 720;

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
    unique_window window{glfwCreateWindow(
        window_width, window_height, "Refdraw", nullptr, nullptr
    )};

    // set up error handling
    VkDebugUtilsMessengerCreateInfoEXT debug_utils_messenger_create_info{
        .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
        .messageSeverity =
            VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
        .messageType =
            VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
        .pfnUserCallback = debug_callback,
        .pUserData = nullptr
    };

    VkApplicationInfo application_info{
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName = "Refdraw",
        .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
        .pEngineName = "No Engine",
        .engineVersion = VK_MAKE_VERSION(1, 0, 0),
        .apiVersion = VK_API_VERSION_1_0
    };

    // look up extensions needed by GLFW
    uint32_t glfw_extension_count = 0;
    auto glfw_extensions =
        glfwGetRequiredInstanceExtensions(&glfw_extension_count);

    // loop up supported extensions
    uint32_t supported_extension_count = 0;
    vkEnumerateInstanceExtensionProperties(
        nullptr, &supported_extension_count, nullptr
    );
    auto supported_extensions =
        std::make_unique<VkExtensionProperties[]>(supported_extension_count);
    vkEnumerateInstanceExtensionProperties(
        nullptr, &supported_extension_count, supported_extensions.get()
    );

    // check support for layers
    const char* enabled_layers[]{
        "VK_LAYER_KHRONOS_validation",
    };

    uint32_t layer_count;
    vkEnumerateInstanceLayerProperties(&layer_count, nullptr);
    auto layers = std::make_unique<VkLayerProperties[]>(layer_count);
    vkEnumerateInstanceLayerProperties(&layer_count, layers.get());
    for (const char* enabled_layer : enabled_layers) {
        bool supported = false;
        for (auto i = 0u; i < layer_count; i++) {
            auto equal = strcmp(enabled_layer, layers[i].layerName);
            if (equal == 0) {
                supported = true;
                break;
            }
        }
        if (!supported) {
            throw std::runtime_error("enabled layer not supported");
        }
    }

    // create instance
    const char *required_extensions[]{
        VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
    };
    auto extension_count =
        std::size(required_extensions) + glfw_extension_count;
    auto extensions = std::make_unique<const char*[]>(extension_count);
    std::copy(
        glfw_extensions, glfw_extensions + glfw_extension_count,
        extensions.get()
    );
    std::copy(
        required_extensions,
        required_extensions + std::size(required_extensions),
        extensions.get() + glfw_extension_count
    );
    unique_instance instance;
    {
        VkInstanceCreateInfo createInfo{
            .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
            .pNext = &debug_utils_messenger_create_info,
            .pApplicationInfo = &application_info,
            .enabledLayerCount = std::size(enabled_layers),
            .ppEnabledLayerNames = enabled_layers,
            .enabledExtensionCount = static_cast<uint32_t>(extension_count),
            .ppEnabledExtensionNames = extensions.get(),
        };
        check(vkCreateInstance(
            &createInfo, nullptr, out_ptr(instance)
        ));
    }
    current_instance = instance.get();

    // create surface
    unique_surface surface;
    check(glfwCreateWindowSurface(
        instance.get(), window.get(), nullptr, out_ptr(surface)
    ));

    // look for available devices
    VkPhysicalDevice physical_device;

    uint32_t device_count = 0;
    check(vkEnumeratePhysicalDevices(instance.get(), &device_count, nullptr));
    if (device_count == 0) {
        throw std::runtime_error("no Vulkan capable GPU found");
    }
    {
        auto devices = std::make_unique<VkPhysicalDevice[]>(device_count);
        check(vkEnumeratePhysicalDevices(
            instance.get(), &device_count, devices.get()
        ));
        // TODO: check for VK_KHR_swapchain support
        physical_device = devices[0]; // just pick the first one for now
    }

    // get properties of physical device
    max_sample_count = VK_SAMPLE_COUNT_1_BIT;
    {
        VkPhysicalDeviceProperties physical_device_properties;
        vkGetPhysicalDeviceProperties(
            physical_device, &physical_device_properties
        );

        VkSampleCountFlags sample_count_falgs =
            physical_device_properties.limits.framebufferColorSampleCounts &
            physical_device_properties.limits.framebufferDepthSampleCounts &
            physical_device_properties.limits.framebufferStencilSampleCounts;

        for (auto bit : {
            VK_SAMPLE_COUNT_64_BIT, VK_SAMPLE_COUNT_32_BIT,
            VK_SAMPLE_COUNT_16_BIT, VK_SAMPLE_COUNT_8_BIT,
            VK_SAMPLE_COUNT_4_BIT, VK_SAMPLE_COUNT_2_BIT,
        }) {
            if (sample_count_falgs & bit) {
                max_sample_count = bit;
                break;
            }
        }
    }
    
    ::ui ui(physical_device, surface.get());
    static ::canvas canvas{{2048, 2048, ui.tiles.buffer}};
    static color c = {0, 0, 0, 255};

    glfwSetKeyCallback(
        window.get(), 
        [](GLFWwindow* window, int key, int scancode, int action, int mods) {
            if (action == GLFW_PRESS) {
                int stack = mods & GLFW_MOD_SHIFT ? 1 : 0;
                if (key == GLFW_KEY_Z && mods & GLFW_MOD_CONTROL) {
                    canvas.undo(stack);
                } else if (key == GLFW_KEY_Y && mods & GLFW_MOD_CONTROL) {
                    canvas.redo(stack);
                }

                if (key == GLFW_KEY_R) {
                    c = {255, 0, 0, 255};
                } else if (key == GLFW_KEY_G) {
                    c = {0, 255, 0, 255};
                } else if (key == GLFW_KEY_B) {
                    c = {0, 0, 255, 255};
                } else if (key == GLFW_KEY_K) {
                    c = {0, 0, 0, 255};
                }
            }
        }
    );

    bool in_stroke = false;

    while (!glfwWindowShouldClose(window.get())) {
        if (
            glfwGetMouseButton(window.get(), GLFW_MOUSE_BUTTON_LEFT) == 
            GLFW_PRESS
        ) {
            double x, y;
            glfwGetCursorPos(window.get(), &x, &y);
            canvas.add_stroke_point(
                {
                    static_cast<float>(x),
                    static_cast<float>(y),
                    10.0f,
                    c
                },
                false
            );
            in_stroke = true;
        }

        if (
            glfwGetMouseButton(window.get(), GLFW_MOUSE_BUTTON_LEFT) == 
            GLFW_RELEASE && in_stroke
        ) {
            canvas.add_stroke_point(
                {
                    0, 0, 0, 0
                },
                true
            );
            in_stroke = false;
        }

        ui.render();

        glfwPollEvents();
    }

    return 0;
}
