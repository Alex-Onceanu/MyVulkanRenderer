#include "window.hpp"
#ifdef VULKAN_RENDERER
#include <vulkan/vulkan.hpp>
#endif
#include <GLFW/glfw3.h>


namespace
{
    void (*callback)(void);

    void framebufferResizeCallback(GLFWwindow* window, int width, int height)
    {
        (*callback)();
    }
}

sk::Window::Window(int __WINDOW_WIDTH, int __WINDOW_HEIGHT)
{
    glfwInit();

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
    window = glfwCreateWindow(__WINDOW_WIDTH, __WINDOW_HEIGHT, "Hello triangle !", nullptr, nullptr);
    glfwSetFramebufferSizeCallback(window, framebufferResizeCallback);
    glfwSetWindowAspectRatio(window, __WINDOW_WIDTH, __WINDOW_HEIGHT);
    
    const int MIN_WIDTH = 160;
    const int MIN_HEIGHT = MIN_WIDTH * __WINDOW_HEIGHT / __WINDOW_WIDTH;
    glfwSetWindowSizeLimits(window, MIN_WIDTH, MIN_HEIGHT, GLFW_DONT_CARE, GLFW_DONT_CARE);
}

sk::Window::~Window()
{
    glfwDestroyWindow(window);
    glfwTerminate();
}

bool sk::Window::isAlive()
{
    glfwPollEvents();
    return not glfwWindowShouldClose(window);
}

void sk::Window::waitForEvent()
{
    glfwWaitEvents();
}

GLFWwindow* sk::Window::getPtr()
{
    return window;
}

void sk::Window::getSize(int& pWidth, int& pHeight)
{
    glfwGetFramebufferSize(window, &pWidth, &pHeight);
}

void sk::Window::setResizeCallback(void (*__callback)(void))
{
    callback = __callback;
}

std::vector<const char*> sk::Window::getRequiredExtensions()
{
    // Ici on parle de instance extensions, pas device extensions
    // GLFW a lui aussi besoin de certaines extensions vulkan pour établir le lien vulkan-fenêtre
    // par exemple GLFW requiert "VK_KHR_surface" pour créer le type abstrait "VkSurfaceKHR"
    uint32_t glfwExtensionCount = 0;
    const char** glfwExtensions;
    glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
    
    std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);
    
#ifdef DEBUG
    extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
#endif
    return extensions;
}

#ifdef VULKAN_RENDERER
vk::SurfaceKHR sk::Window::createSurface(VkInstance instance)
{
    VkSurfaceKHR tmpSurface;
    if(glfwCreateWindowSurface(instance, window, nullptr, &tmpSurface) != VK_SUCCESS)
    {
        throw std::runtime_error("GLFW failed to create window surface !");
    }
    return vk::SurfaceKHR(tmpSurface);
}
#endif