#define VULKAN_HPP_NO_CONSTRUCTORS
#include <vulkan/vulkan.hpp>

#include <iostream>
#include <vector>
#include <string>
#include <exception>
#include <optional>
#include <cstdint> // uint32_t
#include <limits>  // std::numeric_limits
#include <fstream>
#include <array>
#include <chrono>

#include "math.hpp"
#include "renderer.hpp"
#include "window.hpp"

// structs
namespace
{
    struct Vertex
    {
        math::vec2 pos;
        math::vec3 clr;

        static vk::VertexInputBindingDescription getBindingDescription()
        {
            return {
                .binding = 0,
                .stride = sizeof(Vertex),
                .inputRate = vk::VertexInputRate::eVertex
            };
        }

        static std::array<vk::VertexInputAttributeDescription, 2> getAttributeDescriptions()
        {
            std::array<vk::VertexInputAttributeDescription, 2> res{{
                {
                    .location = 0,
                    .binding = 0,
                    .format = vk::Format::eR32G32Sfloat,
                    .offset = offsetof(Vertex, pos)
                },
                {
                    .location = 1,
                    .binding = 0,
                    .format = vk::Format::eR32G32B32Sfloat,
                    .offset = offsetof(Vertex, pos)
                }
            }};
            return res;
        }
    };

    struct UniformBufferObject {
        alignas(4) float uTime;
        alignas(16) math::vec3 uClr;
    };

    struct QueueFamilyIndices {
        std::optional<uint32_t> graphicsFamily;
        std::optional<uint32_t> presentFamily;
        
        bool isComplete()
        {
            return graphicsFamily.has_value() and presentFamily.has_value();
        }
    };
    
    struct SwapChainSupportDetails {
        vk::SurfaceCapabilitiesKHR capabilities;
        std::vector<vk::SurfaceFormatKHR> formats;
        std::vector<vk::PresentModeKHR> presentModes;
        
        bool isComplete()
        {
            return not formats.empty() and not presentModes.empty();
        }
    };
};

// attributs
namespace
{
    std::shared_ptr<sk::Window> window;
    vk::Instance instance;
    vk::PhysicalDevice physicalDevice = VK_NULL_HANDLE;
    vk::UniqueDevice logicalDevice;
    vk::Queue graphicsQueue; // handle : interface avec la queue "graphics" de la familyQueue
    vk::Queue presentQueue;  // handle : idem pour present (queue qui s'occupe de donner le rendu à l'écran)
    vk::SurfaceKHR surface;  // "fenêtre" du point de vue de Vulkan
    vk::SwapchainKHR swapChain = VK_NULL_HANDLE; // file d'images attendant d'être rendues
    vk::Format swapChainImageFormat;
    vk::Extent2D swapChainExtent;
    std::vector<vk::Image> swapChainImages;
    std::vector<vk::ImageView> swapChainImageViews;
    vk::RenderPass renderPass; // lien entre résultat du fragment shader et image (color buff) du swapchain
    vk::DescriptorSetLayout descriptorSetLayout; // description de comment lier l'UBO du CPU avec celui du GPU
    vk::PipelineLayout pipelineLayout; // envoi d'uniform dans les shaders
    vk::Pipeline graphicsPipeline;
    std::vector<vk::Framebuffer> swapChainFrameBuffers; // à chaque imageview de la swapchain son buffer de rendu
    vk::CommandPool commandPool;
    vk::UniqueDescriptorPool descriptorPool;
    std::vector<vk::DescriptorSet> descriptorSets;
    int currentFrame;
    bool windowResized;

    const std::vector<Vertex> vertices {
        { { -1.,-1. },{ 1.,0.,0. } },
        { { -1., 1. },{ 0.,1.,0. } },
        { {  1., 1. },{ 0.,0.,1. } },
        { {  1.,-1. },{ 1.,0.,1. } }
    };
    
    const std::vector<uint16_t> indices {
        0, 1, 3,
        3, 1, 2
    };
    
    // pour chaque frame in flight
    std::vector<vk::CommandBuffer, std::allocator<vk::CommandBuffer>> commandBuffers;
    std::vector<vk::Semaphore> imageAvailableSemaphores;
    std::vector<vk::Semaphore> readyForPresentationSemaphores;
    std::vector<vk::Fence> readyForNextFrameFences;
    
    vk::UniqueBuffer vertexBuffer;
    vk::UniqueDeviceMemory vertexBufferMemory;
    vk::UniqueBuffer indexBuffer;
    vk::UniqueDeviceMemory indexBufferMemory;
    
    std::vector<vk::UniqueBuffer> uniformBuffers;
    std::vector<vk::UniqueDeviceMemory> uniformBuffersMemory;
    std::vector<void*> uniformBuffersMapped;
    
    const std::vector<const char*> deviceRequiredExtensions = {
#ifdef __APPLE__
        "VK_KHR_portability_subset",
#endif
        VK_KHR_SWAPCHAIN_EXTENSION_NAME
        // Mettre ici toutes les extensions Vulkan requises
    };
    
    const int NB_FRAMES_IN_FLIGHT = 2;
    
#ifdef DEBUG
    // Validation layer : couche custom de debug pour pas que ça plante sans savoir pk
    std::vector<const char*> validationLayers{ "VK_LAYER_KHRONOS_validation" };
    vk::DebugUtilsMessengerEXT debugMessenger;
#endif
};

// methodes
namespace
{
    void resizeCallback()
    {
        windowResized = true;
    }
    
    void createInstance()
    {
        std::vector<const char*> requiredExtensions = window->getRequiredExtensions();

        vk::ApplicationInfo appInfo{
            .pApplicationName   = "Hello triangle",
            .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
            .pEngineName        = "My Engine",
            .engineVersion      = VK_MAKE_VERSION(1, 0, 0),
            .apiVersion         = VK_API_VERSION_1_3
        };
        
        vk::InstanceCreateInfo createInfo{ .pApplicationInfo = &appInfo };
        
#if DEBUG
        assertValidationLayerSupport();
        createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
        createInfo.ppEnabledLayerNames = validationLayers.data();
        
        // Sert pour debug vkCreateInstance
        auto debugCreateInfo = createDebugMessengerCreateInfo();
        createInfo.pNext = (VkDebugUtilsMessengerCreateInfoEXT*)&debugCreateInfo;
#else
        createInfo.enabledLayerCount = 0;
        createInfo.pNext = nullptr;
#endif

#if __APPLE__
        requiredExtensions.emplace_back(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
        createInfo.flags |= vk::InstanceCreateFlags(VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR);
#endif
        
        createInfo.enabledExtensionCount = static_cast<uint32_t>(requiredExtensions.size());
        createInfo.ppEnabledExtensionNames = requiredExtensions.data();
        
        instance = vk::createInstance(createInfo, nullptr);
    }

#ifdef DEBUG
    void assertValidationLayerSupport()
    {
        std::vector<vk::LayerProperties> availableLayers = vk::enumerateInstanceLayerProperties();
        
        // On veut savoir is les validationLayers qu'on souhaite sont bel et bien supportés par vulkan
        for(const char* vLayer : validationLayers)
        {
            std::string svLayer(vLayer);
            auto str_eq = [&](vk::LayerProperties l){ return svLayer.compare(std::string(l.layerName)); };
            if(std::find_if_not(availableLayers.begin(), availableLayers.end(), str_eq) == availableLayers.end())
            {
                throw std::runtime_error("Unequested validation layer is unavailable : " + svLayer);
            }
        }
        // Ici tout va bien
    }
    
    static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
                                                        VkDebugUtilsMessageTypeFlagsEXT messageType,
                                                        const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
                                                        void* pUserData)
    {
        // Cette fonction de debug custom sera appelée par Vulkan pour communiquer l'erreur
        if(messageSeverity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
        {
            std::cerr   << "!! Cought error through custom validation layer : "
                        << pCallbackData->pMessage
                        << std::endl;
            
            for(int i = 0; i < pCallbackData->objectCount; i++)
            {
                std::cerr   << "Involved vulkan object "
                            << i + 1
                            << " :\n\t"
                            << (pCallbackData->pObjects[i]).pObjectName
                            << std::endl;
            }
        }
        return VK_FALSE;
    }
    
    // Proxy pour vkCreateDebugUtilsMessengerEXT (sert à créer VkDebugUtilsMessengerEXT) qui n'est pas chargé automatiquement
    static void createDebugUtilsMessengerEXT(vk::Instance instance,
                                      const vk::DebugUtilsMessengerCreateInfoEXT* pCreateInfo,
                                      const vk::AllocationCallbacks* pAllocator,
                                      vk::DebugUtilsMessengerEXT* pDebugMessenger)
    {
        auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
        if (func != nullptr)
        {
            if(func(instance,
                    (VkDebugUtilsMessengerCreateInfoEXT*)pCreateInfo,
                    (VkAllocationCallbacks*)pAllocator,
                    (VkDebugUtilsMessengerEXT *)pDebugMessenger)
               != VK_SUCCESS)
            {
                throw std::runtime_error("vkCreateDebugUtilsMessengerEXT : failed to set up debug messenger !");
            }
            return;
        }
        throw std::runtime_error("vkCreateDebugUtilsMessengerEXT not found (extension not present)");
    }
    
    // Proxy pour vkDestroyDebugUtilsMessengerEXT (sert à free le debugMessenger)
    static void destroyDebugUtilsMessengerEXT(vk::Instance instance,
                                       vk::DebugUtilsMessengerEXT debugMessenger,
                                       const vk::AllocationCallbacks* pAllocator)
    {
        auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
        if(func != nullptr)
        {
            func(instance, debugMessenger, (VkAllocationCallbacks*)pAllocator);
        }
    }
    
    void setupDebugMessenger()
    {
        vk::DebugUtilsMessengerCreateInfoEXT createInfo = createDebugMessengerCreateInfo();
        createDebugUtilsMessengerEXT(instance, &createInfo, nullptr, &debugMessenger);
    }
    
    // cf populateDebugMessengerCreateInfo dans le pdf
    vk::DebugUtilsMessengerCreateInfoEXT createDebugMessengerCreateInfo()
    {
        vk::DebugUtilsMessengerCreateInfoEXT createInfo {
            .messageSeverity = vk::DebugUtilsMessageSeverityFlagBitsEXT::eError
            |   vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning
            |   vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose,
        
            .messageType = vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral
            |   vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance
            |   vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation,
        
            .pfnUserCallback = debugCallback,
            .pUserData = nullptr    // ici on peut donner un argument à debugCallback
        };
        
        return createInfo;
    }
#endif
    
    SwapChainSupportDetails querySwapChainSupport(const vk::PhysicalDevice& gpu)
    {
        return {
            .capabilities = gpu.getSurfaceCapabilitiesKHR(surface),
            .formats = gpu.getSurfaceFormatsKHR(surface),
            .presentModes = gpu.getSurfacePresentModesKHR(surface)
        };
    }
    
    // Une queue family c'est un certain type de queues (elles sont catégorisées selon ce à quoi elles servent)
    QueueFamilyIndices findQueueFamilies(vk::PhysicalDevice device)
    {
        QueueFamilyIndices indices;
        
        std::vector<vk::QueueFamilyProperties> qProperties = device.getQueueFamilyProperties();
        
        int i = 0;
        for(const auto& property : qProperties)
        {
            if(property.queueCount > 0 and property.queueFlags & vk::QueueFlagBits::eGraphics)
            {
                indices.graphicsFamily = i;
            }
            
            if(property.queueCount > 0 and device.getSurfaceSupportKHR(i, surface))
            {
                indices.presentFamily = i;
            }
            
            if(indices.isComplete())
            {
                break;
            }
            
            i++;
        }
        
        return indices;
    }
    
    bool deviceSupportsExtensions(vk::PhysicalDevice gpu)
    {
        // s'assure que ce gpu supporte bien les device extensions dont on a besoin
        std::vector<vk::ExtensionProperties> availableExtensions = gpu.enumerateDeviceExtensionProperties();
        
        for(const std::string& extension : deviceRequiredExtensions)
        {
            auto str_eq = [&](vk::ExtensionProperties l){ return extension.compare(std::string(l.extensionName)); };
            
            if(std::find_if_not(availableExtensions.begin(), availableExtensions.end(), str_eq) == availableExtensions.end())
            {
                // On a trouvé une extension requise non supportée par ce gpu
                return false;
            }
        }
        return true;
    }
    
    vk::PresentModeKHR chooseSwapPresentMode(const std::vector<vk::PresentModeKHR>& availableModes)
    {
        for (const auto& availableMode : availableModes)
        {
            if(availableMode == vk::PresentModeKHR::eMailbox)
            {
                return availableMode;
            }
        }
#ifdef DEBUG
        std::cout << "MAILBOX swapchain present mode unavailable, choosing FIFO.." << std::endl;
#endif
        return vk::PresentModeKHR::eFifo;
    }
    
    vk::SurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<vk::SurfaceFormatKHR>& availableFormats)
    {
        for (const auto& availableFormat : availableFormats)
        {
            if(availableFormat.format == vk::Format::eB8G8R8A8Unorm
               and availableFormat.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear)
            {
                return availableFormat;
            }
        }
#ifdef DEBUG
        std::cout << "8bit-SRGB swapchain surface format unavailable, choosing the first available one.." << std::endl;
#endif
        return availableFormats[0];
    }
    
    vk::Extent2D chooseSwapExtent(const vk::SurfaceCapabilitiesKHR& capabilities)
    {
        // Swap extent : resolution des images dans la swap chain
        // capabilities contient les résolutions acceptées
        if(capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max())
        {
            return capabilities.currentExtent;
        }
        else
        {
            int width, height;
            window->getSize(width, height);
            
            vk::Extent2D actualExtent{
                static_cast<uint32_t>(CLAMP(width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width)),
                
                static_cast<uint32_t>(CLAMP(height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height))
            };

            return actualExtent;
        }
    }
    
    int rateGPU(vk::PhysicalDevice gpu)
    {
        int score = 0;
        vk::PhysicalDeviceProperties gpuProperties;
        vk::PhysicalDeviceFeatures gpuFeatures;
        gpu.getProperties(&gpuProperties);
        gpu.getFeatures(&gpuFeatures);
        
        if(gpuProperties.deviceType == vk::PhysicalDeviceType::eDiscreteGpu)
        {
            // askip les discrete GPU c'est mieux que tout le reste
            score += 10000;
        }
        
        // Un meilleur GPU supporte des textures plus grandes
        score += gpuProperties.limits.maxImageDimension2D;
        
        QueueFamilyIndices qIndices = findQueueFamilies(gpu);
        if(not qIndices.isComplete())
        {
            // On a besoin que le gpu puisse "comprendre" les commandes
            return 0;
        }
        
        if(not deviceSupportsExtensions(gpu))
        {
            // On a besoin que le gpu supporte toutes les extensions requises
            return 0;
        }
        
        if(not querySwapChainSupport(gpu).isComplete())
        {
            // On a besoin que le gpu supporte le swap chain
            return 0;
        }
        
        // imposer ici que le GPU doive supporter certaines fonctionnalités
        // ex : if(not gpuFeatures.geometryShader) return 0;
        
        return score;
    }
    
    void pickPhysicalDevice()
    {
        std::vector<vk::PhysicalDevice> gpus = instance.enumeratePhysicalDevices();
        if (gpus.size() == 0)
        {
            throw std::runtime_error("No GPU ??");
        }
        
        // On itère sur tous les GPUs disponibles pour choisir "le meilleur" (cf méthode rateGPU)
        int bestScore = 0;
        int tmpScore = 0;
        for(const auto& gpu : gpus)
        {
            tmpScore = rateGPU(gpu);
            if(tmpScore > bestScore)
            {
                bestScore = tmpScore;
                physicalDevice = gpu;
            }
        }
        
        if(bestScore == 0 || physicalDevice == VK_NULL_HANDLE)
        {
            throw std::runtime_error("Found some physical devices, but all are unsuitable.");
        }
    }
    
    void createLogicalDevice()
    {
        QueueFamilyIndices indices = findQueueFamilies(physicalDevice);
        // si on soumet des instructions au GPU en même temps via 2 queues de la même queue family,
        // le gpu va traiter en priorité les commandes de la queue avec la plus grande priorité
        float qPriority = 1.0f; // On va passer a pQueuePriorities un tableau de priorités, ici juste &qPriority
        std::vector<vk::DeviceQueueCreateInfo> allQueuesCreateInfo;
        
        vk::DeviceQueueCreateInfo queueCreateInfo{
            .queueFamilyIndex = indices.graphicsFamily.value(),
            .queueCount = 1,
            .pQueuePriorities = &qPriority
        };
        allQueuesCreateInfo.push_back(queueCreateInfo);
        uint32_t nbNeededQueues = 1;
        
        if(indices.presentFamily.value() != indices.graphicsFamily.value())
        {
            // Pas la peine de push dans allQueuesCreateInfo une 2eme fois la même queue...
            // Si jamais on est pas dans la même queue pour graphics et present, on push la 2eme aussi
            vk::DeviceQueueCreateInfo queueCreateInfo2{
                .queueFamilyIndex = indices.presentFamily.value(),
                .queueCount = 1,
                .pQueuePriorities = &qPriority
            };
            allQueuesCreateInfo.push_back(queueCreateInfo2);
            nbNeededQueues = 2;
        }
        
        vk::PhysicalDeviceFeatures deviceFeatures = vk::PhysicalDeviceFeatures();
        
        // Idem que createInfo pour vk::Instance
        vk::DeviceCreateInfo createInfo{
            .flags = vk::DeviceCreateFlags(),
            .queueCreateInfoCount = nbNeededQueues,
            .pQueueCreateInfos = allQueuesCreateInfo.data(),
            .enabledExtensionCount = static_cast<uint32_t>(deviceRequiredExtensions.size()),
            .ppEnabledExtensionNames = deviceRequiredExtensions.data(),
            .pEnabledFeatures = &deviceFeatures
        };
        
#ifdef DEBUG
        createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
        createInfo.ppEnabledLayerNames = validationLayers.data();
#else
        createInfo.enabledLayerCount = 0;
#endif
        logicalDevice = physicalDevice.createDeviceUnique(createInfo);
        
        graphicsQueue = logicalDevice->getQueue(indices.graphicsFamily.value(), 0);
        presentQueue = logicalDevice->getQueue(indices.presentFamily.value(), 0);
    }
    
    void createSurface()
    {
        surface = window->createSurface(instance);
    }
    
    void cleanupSwapChain()
    {
        for(auto& frameBuffer : swapChainFrameBuffers)
        {
            logicalDevice->destroyFramebuffer(frameBuffer);
        }
        swapChainFrameBuffers.clear();
        
        for (auto imageView : swapChainImageViews)
        {
            logicalDevice->destroyImageView(imageView);
        }
        swapChainImageViews.clear();
        
        logicalDevice->destroySwapchainKHR(swapChain);
    }


    void createSwapChain()
    {
        SwapChainSupportDetails swapChainSupport = querySwapChainSupport(physicalDevice);
        
        vk::SurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(swapChainSupport.formats);
        vk::PresentModeKHR presentMode = chooseSwapPresentMode(swapChainSupport.presentModes);
        vk::Extent2D extent = chooseSwapExtent(swapChainSupport.capabilities);
        
        // Combien d'images on laisse dans le swap chain ? 1 + le minimum possible pour que ça fonctionne
        uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1;
        const uint32_t maxImgCount = swapChainSupport.capabilities.maxImageCount;
        
        // Si le gpu impose un maximum d'images dans la swap chain (0 := pas de limite) on limite imageCount
        if(maxImgCount > 0 && imageCount > maxImgCount)
        {
            imageCount = maxImgCount;
        }
        
        vk::SwapchainCreateInfoKHR scCreateInfo{
            .flags = vk::SwapchainCreateFlagsKHR(),
            .surface = surface,
            .minImageCount = imageCount,
            .imageFormat = surfaceFormat.format,
            .imageColorSpace = surfaceFormat.colorSpace,
            .imageExtent = extent,
            .imageArrayLayers = 1, // > 1 pour stereoscopic 3D application (VR)
            .imageUsage = vk::ImageUsageFlagBits::eColorAttachment,
            
            // on applique pas de transformation finale (rotation, flip..)
            .preTransform = swapChainSupport.capabilities.currentTransform,
            // on pourrait utiliser le canal alpha pour blend avec d'autres fenêtres
            .compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque,
            
            .presentMode = presentMode,
            .clipped = vk::True, // on fait rien avec les pixels cachés par d'autres fenêtre
            .oldSwapchain = vk::SwapchainKHR(VK_NULL_HANDLE) // pour les fenêtres redimensionnables, recréer une swap chain.
        };
        
        /*  dans imageUsage on dit qu'on prend les images de la swap chain pour directement
            les envoyer au rendu. On pourrait plutôt les envoyer ailleurs (ex : pour faire du post-processing?)
            avec VK_IMAGE_USAGE_TRANSFER_DST_BIT */
        
        QueueFamilyIndices indices = findQueueFamilies(physicalDevice);
        uint32_t queueFamilyIndices[2] = {indices.graphicsFamily.value(), indices.presentFamily.value()};
        if(indices.graphicsFamily != indices.presentFamily)
        {
            // Si on utilise 2 queue familles différentes, les deux veulent accéder aux mêmes images
            // dans la swapChain. Elles se le partagent avec le mode concurrent
            scCreateInfo.imageSharingMode = vk::SharingMode::eConcurrent;
            scCreateInfo.queueFamilyIndexCount = 2;
            scCreateInfo.pQueueFamilyIndices = queueFamilyIndices;
        }
        else
        {
            // Ce mode de partage transfère l'"ownership" des images. Ici on a une seule famille donc
            // on doit choisir ce mode, car l'autre requiert au moins 2 familles
            scCreateInfo.imageSharingMode = vk::SharingMode::eExclusive;
            scCreateInfo.queueFamilyIndexCount = 0; // Optionnel
            scCreateInfo.pQueueFamilyIndices = nullptr; // Optionnel
        }
        
        try {
            swapChain = logicalDevice->createSwapchainKHR(scCreateInfo);
        } catch (vk::SystemError err) {
            throw std::runtime_error("Failed to create swap chain.");
        }
        
        swapChainImages = logicalDevice->getSwapchainImagesKHR(swapChain);

        swapChainImageFormat = surfaceFormat.format;
        swapChainExtent = extent;
    }
    
    void createImageViews()
    {
        for(const auto& image : swapChainImages)
        {
            vk::ImageViewCreateInfo ivCreateInfo{
                .image = image,
                .viewType = vk::ImageViewType::e2D,
                .format = swapChainImageFormat,
            };
            
            ivCreateInfo.components.r = vk::ComponentSwizzle::eIdentity;
            ivCreateInfo.components.g = vk::ComponentSwizzle::eIdentity;
            ivCreateInfo.components.b = vk::ComponentSwizzle::eIdentity;
            ivCreateInfo.components.a = vk::ComponentSwizzle::eIdentity;
            
            ivCreateInfo.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
            ivCreateInfo.subresourceRange.baseMipLevel = 0;
            ivCreateInfo.subresourceRange.levelCount = 1;
            ivCreateInfo.subresourceRange.baseArrayLayer = 0;
            ivCreateInfo.subresourceRange.layerCount = 1;
            
            // pour un jeu en 3D stéréographique, faire 2 imageViews pour chaque image, avec 2 layers par img
            try {
                swapChainImageViews.push_back(logicalDevice->createImageView(ivCreateInfo));
            }
            catch (vk::SystemError err) {
                throw std::runtime_error("Failed to create image views from swap chain image.");
            }
        }
    }
    
    static std::vector<char> readFile(const std::string& filename)
    {
        std::ifstream file(filename, std::ios::ate | std::ios::binary);
        
        if(not file.is_open())
        {
            throw std::runtime_error("File not found : " + filename);
        }
        
        // On a ouvert le fichier à la fin pour savoir sa taille
        size_t fileSize = static_cast<size_t>(file.tellg());
        std::vector<char> buffer(fileSize);
        
        // On revient au début du fichier pour lire fileSize octets
        file.seekg(0);
        file.read(buffer.data(), fileSize);
        
        file.close();
        
        return buffer;
    }
    
    void createFrameBuffers()
    {
        for(const auto& imageView : swapChainImageViews)
        {
            vk::ImageView attachments[] = { imageView };
            
            vk::FramebufferCreateInfo fbCreateInfo{
                .flags = vk::FramebufferCreateFlags(),
                .renderPass = renderPass,
                .attachmentCount = 1,
                .pAttachments = attachments,
                .width = swapChainExtent.width,
                .height = swapChainExtent.height,
                .layers = 1
            };
            
            try {
                swapChainFrameBuffers.push_back(logicalDevice->createFramebuffer(fbCreateInfo));
            } catch (vk::SystemError err) {
                throw std::runtime_error("Failed to create frame buffer.");
            }
        }
    }
    
    void recreateSwapChain()
    {
        int width = 0, height = 0;
        window->getSize(width, height);
        
        while(width == 0 or height == 0)
        {
            window->getSize(width, height);
            window->waitForEvent();
        }
        
        logicalDevice->waitIdle();
        cleanupSwapChain();
        
        createSwapChain();
        createImageViews();
        createFrameBuffers();
    }
    
    // Objet vulkan contenant le bytecode d'un shader en SPIR-V
    vk::UniqueShaderModule createShaderModule(const std::vector<char>& code)
    {
        vk::ShaderModuleCreateInfo smCreateInfo{
            .flags = vk::ShaderModuleCreateFlags(),
            .codeSize = code.size(),
            .pCode = reinterpret_cast<const uint32_t*>(code.data()),
        };
        
        try {
            return logicalDevice->createShaderModuleUnique(smCreateInfo);
        } catch (vk::SystemError) {
            throw std::runtime_error("Failed to create shader module.");
        }
    }
    
    void createRenderPass()
    {
        // un seul color buffer, une des images du swapchain
        vk::AttachmentDescription colorAttachment{
            .format = swapChainImageFormat,
            .samples = vk::SampleCountFlagBits::e1, // pas de multisampling pour l'instant
            .loadOp = vk::AttachmentLoadOp::eClear, // clear cette image avant de refaire un rendu
            .storeOp = vk::AttachmentStoreOp::eStore, // pour qu'on puisse voir le rendu quand même
            .stencilLoadOp = vk::AttachmentLoadOp::eDontCare, // pas de stencil buffer pour l'instant
            .stencilStoreOp = vk::AttachmentStoreOp::eDontCare,
            .initialLayout = vk::ImageLayout::eUndefined, // peu importe dans quel layout était avant ce pass
            .finalLayout = vk::ImageLayout::ePresentSrcKHR // on compte présenter cette image au swap chain
        };
        
        // un render pass est composé d'un ou plusieurs subpasses (ex : plusieurs couches de post-processing)
        vk::AttachmentReference colorAttachmentRef{
            .attachment = 0, // l'index de colorAttachment est 0 (c'est le seul...)
            .layout = vk::ImageLayout::eColorAttachmentOptimal // on précise que c'est un color buffer
        };
        
        // quand on écrit layout(location = 0) out vec4 outColor dans le fragment shader,
        // le résultat de outColor sera directement écrit dans subpass.pColorAttachments[0] !
        vk::SubpassDescription subpass{
            .pipelineBindPoint = vk::PipelineBindPoint::eGraphics, // c'est un graphics subpass, pas compute
            .colorAttachmentCount = 1,
            .pColorAttachments = &colorAttachmentRef
        };
        
        // Attendre le stage "color attachment" (entre vert et frag) pour faire la transition d'image layout
        // Sinon le ferait "au début de la pipeline", alors qu'on a pas encore d'image techniquement
        // En gros faut attendre que la swap chain finisse de lire depuis l'image avant qu'on y écrive
        vk::SubpassDependency dependency {
            .srcSubpass = VK_SUBPASS_EXTERNAL,
            .dstSubpass = 0,
            .srcStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput,
            .dstStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput,
            .dstAccessMask = vk::AccessFlagBits::eColorAttachmentRead | vk::AccessFlagBits::eColorAttachmentWrite
        };
        
        vk::RenderPassCreateInfo rpCreateInfo{
            .attachmentCount = 1,
            .pAttachments = &colorAttachment,
            .subpassCount = 1,
            .pSubpasses = &subpass,
            .dependencyCount = 1,
            .pDependencies = &dependency
        };
        
        try {
            renderPass = logicalDevice->createRenderPass(rpCreateInfo);
        } catch (vk::SystemError err) {
            throw std::runtime_error("Failed to create render pass.");
        }
    }
    
    void createGraphicsPipeline()
    {
        auto vertCode = readFile("../shaders/out/triangle.vert.spv");
        auto fragCode = readFile("../shaders/out/triangle.frag.spv");
        
        auto vertModule = createShaderModule(vertCode);
        auto fragModule = createShaderModule(fragCode);
        
        vk::PipelineShaderStageCreateInfo vertpssCreateInfo{
            .flags  = vk::PipelineShaderStageCreateFlags(),
            .stage  = vk::ShaderStageFlagBits::eVertex,
            .module = *vertModule,
            .pName  = "main"         // entry point
            // .pSpecializationInfo : pour donner des valeurs à des constantes depuis ici
        };
        
        vk::PipelineShaderStageCreateInfo fragpssCreateInfo{
            .flags  = vk::PipelineShaderStageCreateFlags(),
            .stage  = vk::ShaderStageFlagBits::eFragment,
            .module = *fragModule,
            .pName  = "main"
        };
        
        vk::PipelineShaderStageCreateInfo stages[2] = {vertpssCreateInfo, fragpssCreateInfo};
        
        // Paramètres de la pipeline qu'on préfère spécifier à chaque draw call
        std::vector<vk::DynamicState> dynamicStates = {
            vk::DynamicState::eViewport,
            vk::DynamicState::eScissor
        };
        
        vk::PipelineDynamicStateCreateInfo pdsCreateInfo{
            .flags = vk::PipelineDynamicStateCreateFlags(),
            .dynamicStateCount = static_cast<uint32_t>(dynamicStates.size()),
            .pDynamicStates = dynamicStates.data()
        };
        
        auto bindingDescription = Vertex::getBindingDescription();
        auto attributeDescriptions = Vertex::getAttributeDescriptions();
        
        // On met les vertex dans la pipeline (pas encore le VBO)
        vk::PipelineVertexInputStateCreateInfo pvisCreateInfo{
            .flags                              = vk::PipelineVertexInputStateCreateFlags(),
            .vertexBindingDescriptionCount      = 1,
            .pVertexBindingDescriptions         = &bindingDescription,
            .vertexAttributeDescriptionCount    = static_cast<uint32_t>(attributeDescriptions.size()),
            .pVertexAttributeDescriptions       = attributeDescriptions.data()
        };
        
        // Input assembly : comment interpréter les vertex pour en faire des primitives ?
        // Ici on veut les interpréter comme des triangles sans réutiliser de vertex
        vk::PipelineInputAssemblyStateCreateInfo piasCreateInfo{
            .flags                  = vk::PipelineInputAssemblyStateCreateFlags(),
            .topology               = vk::PrimitiveTopology::eTriangleList,
            .primitiveRestartEnable = vk::False // réutiliser les 2 dernier vertex du triangle
        };
        
        vk::Viewport viewport{
            .x          = 0.0f,
            .y          = 0.0f,
            .width      = static_cast<float>(swapChainExtent.width),
            .height     = static_cast<float>(swapChainExtent.height),
            .minDepth   = 0.0f,
            .maxDepth   = 1.0f
        };

        vk::Rect2D scissor {
            .offset = { 0,0 },
            .extent = swapChainExtent
        };
        
        // Ici on enverrait le viewport et scissor s'ils étaient statiques
        vk::PipelineViewportStateCreateInfo pvsCreateInfo{
            .flags          = vk::PipelineViewportStateCreateFlags(),
            .viewportCount  = 1,
            .pViewports     = &viewport,
            .scissorCount   = 1,
            .pScissors      = &scissor
        };
        
        vk::PipelineRasterizationStateCreateInfo prsCreateInfo{
            .depthClampEnable           = vk::False, // on discard les fragments en dehors de near et far
            .rasterizerDiscardEnable    = vk::False, // désactiverait le rasterizer
            .polygonMode                = vk::PolygonMode::eFill, // eLine : wireframe, ePoint : vertices
            .cullMode                   = vk::CullModeFlagBits::eBack, // on discard les fragments de dos
            .frontFace                  = vk::FrontFace::eCounterClockwise, // sens trigo <=> de face
            .depthBiasEnable            = vk::False, // on pourrait appliquer une transformation au depth
            .lineWidth                  = 1.0f
        };

        // Multisampling : sert pour faire de l'anti-aliasing (ici désactivé)
        vk::PipelineMultisampleStateCreateInfo pmsCreateInfo{
            .rasterizationSamples   = vk::SampleCountFlagBits::e1,
            .sampleShadingEnable    = vk::False
        };
        
        // utiliser vk::PipelineDepthStencilStateCreateInfo ici pour depth et/ou stencil buffer
        
        // Avant de colorier un pixel, que faire de la couleur qu'on y trouve ?
        // Nous on va faire un lerp entre nouvelle et ancienne couleur selon alpha
        vk::PipelineColorBlendAttachmentState colorBlendAttachment{
            .blendEnable            = vk::True,
            .srcColorBlendFactor    = vk::BlendFactor::eSrcAlpha,
            .dstColorBlendFactor    = vk::BlendFactor::eOneMinusSrcAlpha,
            .colorBlendOp           = vk::BlendOp::eAdd,
            .srcAlphaBlendFactor    = vk::BlendFactor::eOne,
            .dstAlphaBlendFactor    = vk::BlendFactor::eZero,
            .alphaBlendOp           = vk::BlendOp::eAdd,
            .colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA
        };
        
        vk::PipelineColorBlendStateCreateInfo pcbsCreateInfo{
            .logicOpEnable      = vk::False, // op arithmétiques, pas bitwise
            .logicOp            = vk::LogicOp::eCopy,
            .attachmentCount    = 1,
            .pAttachments       = &colorBlendAttachment,
            // on pourrait utiliser des constantes pour les calculs de attachment via .blendConstants[i]
        };
        pcbsCreateInfo.blendConstants[0] = 0.0f;
        pcbsCreateInfo.blendConstants[1] = 0.0f;
        pcbsCreateInfo.blendConstants[2] = 0.0f;
        pcbsCreateInfo.blendConstants[3] = 0.0f;
        
        // Ici on envoie aux shaders des valeurs pour les uniform
        vk::PipelineLayoutCreateInfo plCreateInfo{
            .setLayoutCount         = 1,
            .pSetLayouts            = &descriptorSetLayout,
        };
        
        pipelineLayout = logicalDevice->createPipelineLayout(plCreateInfo);
        
        vk::GraphicsPipelineCreateInfo pipelineCreateInfo{
            .stageCount             = 2,
            .pStages                = stages, // les 2 shaders
            .pVertexInputState      = &pvisCreateInfo,
            .pInputAssemblyState    = &piasCreateInfo,
            .pViewportState         = &pvsCreateInfo,
            .pRasterizationState    = &prsCreateInfo,
            .pMultisampleState      = &pmsCreateInfo,
            .pDepthStencilState     = nullptr,  // pas de depth/stencil buffer
            .pColorBlendState       = &pcbsCreateInfo,
            .pDynamicState          = &pdsCreateInfo,
            .layout                 = pipelineLayout,
            .renderPass             = renderPass,
            .subpass                = 0,
            .basePipelineHandle     = nullptr
            // cf dérivée d'une graphics pipeline pour créer une pipeline à partir d'une autre
        };
        
        graphicsPipeline = logicalDevice->createGraphicsPipeline(nullptr, pipelineCreateInfo).value;
    }
    
    void createCommandPool()
    {
        QueueFamilyIndices queueFamilyIndices = findQueueFamilies(physicalDevice);
        
        vk::CommandPoolCreateInfo cpCreateInfo {
            .flags = vk::CommandPoolCreateFlags(vk::CommandPoolCreateFlagBits::eResetCommandBuffer),
            .queueFamilyIndex = queueFamilyIndices.graphicsFamily.value()
        };
        
        try {
            commandPool = logicalDevice->createCommandPool(cpCreateInfo);
        } catch (vk::SystemError err) {
            throw std::runtime_error("Failed to create command pool.");
        }
    }
    
    // Permet d'écrire les commandes qu'on souhaite dans un command buffer
    // Cette commande s'adresse au rendu sur une image de la swapChain, d'indice imageIndex
    void recordCommandBuffer(vk::CommandBuffer& commandBuffer, uint32_t imageIndex)
    {
        vk::CommandBufferBeginInfo beginInfo {};
        // cf vk::CommandBufferUsageFlagBits::eSimultaneousUse / eRenderPassContinue / eOneTimeSubmit
        
        // Commencer à ajouter des commandes à ce command buffer
        try {
            commandBuffer.begin(beginInfo);
        } catch (vk::SystemError err) {
            throw std::runtime_error("Failed to start recording commands in command buffer.");
        }
        
        vk::ClearValue clearColor = { std::array<float, 4>{ 0.1f, 0.1f, 0.15f, 0.0f } };
        
        vk::RenderPassBeginInfo renderPassInfo {
            .renderPass = renderPass,
            .framebuffer = swapChainFrameBuffers[imageIndex],
            .renderArea = {
                .offset = { 0,0 },
                .extent = swapChainExtent
            },
            .clearValueCount = 1,
            .pClearValues = &clearColor
        };
        
        // C'est parti pour le renderPass
        commandBuffer.beginRenderPass(renderPassInfo, vk::SubpassContents::eInline);
        
        commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, graphicsPipeline);
        
        vk::Viewport viewport{
            .x          = 0.0f,
            .y          = 0.0f,
            .width      = static_cast<float>(swapChainExtent.width),
            .height     = static_cast<float>(swapChainExtent.height),
            .minDepth   = 0.0f,
            .maxDepth   = 1.0f
        };
        commandBuffer.setViewport(0, 1, &viewport);
        
        vk::Rect2D scissor {
            .offset = { 0,0 },
            .extent = swapChainExtent
        };
        commandBuffer.setScissor(0, 1, &scissor);
        
        vk::Buffer allVertexBuffers[]{ *vertexBuffer };
        vk::DeviceSize offsets[]{ 0 };
        commandBuffer.bindVertexBuffers(0, 1, allVertexBuffers, offsets);
        commandBuffer.bindIndexBuffer(*indexBuffer, 0, vk::IndexType::eUint16);
        
        commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, pipelineLayout, 0, 1, &descriptorSets[currentFrame], 0, nullptr);
        
        // nb de vertex, nb d'instances (cf instanced rendering ?), offset pour vertex et instance
        commandBuffer.drawIndexed(static_cast<uint32_t>(indices.size()), 1, 0, 0, 0);
        
        commandBuffer.endRenderPass();
        
        // On a fini d'enregistrer ce qu'on veut que commandBuffer fasse
        commandBuffer.end();
    }
    
    void createCommandBuffers()
    {
        commandBuffers.resize(NB_FRAMES_IN_FLIGHT);
        
        vk::CommandBufferAllocateInfo allocInfo {
            .commandPool = commandPool,
            .level = vk::CommandBufferLevel::ePrimary, // secondary : pourrait être appelé depuis des primary
            .commandBufferCount = (uint32_t)commandBuffers.size()
        };
        
        try {
            // allocateCommandBuffers crée ici un std::vector d'un seul élément
            commandBuffers = logicalDevice->allocateCommandBuffers(allocInfo);
        } catch (vk::SystemError err) {
            throw std::runtime_error("Failed to create command buffer.");
        }
        
        for(size_t i = 0; i < commandBuffers.size(); i++)
        {
            recordCommandBuffer(commandBuffers[i], (uint32_t)i);
        }
        
    }
    
    void createSyncObjects()
    {
        // 2 sémaphores : le GPU attend qu'une image de la swap chain ait été trouvée
        // puis que le rendu ait été fini dessus pour la présenter à l'écran
        // 1 fence (= sémaphore pour le CPU) : l'exécution attend la fin de la frame
        // pour pas faire le rendu de 2 frames en même temps
        // et on fait ça pour chaque frame in flight
        
        imageAvailableSemaphores.resize(NB_FRAMES_IN_FLIGHT);
        readyForPresentationSemaphores.resize(NB_FRAMES_IN_FLIGHT);
        readyForNextFrameFences.resize(NB_FRAMES_IN_FLIGHT);
        
        vk::FenceCreateInfo fenceInfo {
            // On crée le fence en état "signalé" pour pas que le premier draw call attende indéfiniment
            .flags = vk::FenceCreateFlags(vk::FenceCreateFlagBits::eSignaled)
        };
        
        try {
            for(int i = 0; i < NB_FRAMES_IN_FLIGHT; i++)
            {
                imageAvailableSemaphores[i] = logicalDevice->createSemaphore({});
                readyForPresentationSemaphores[i] = logicalDevice->createSemaphore({});
                readyForNextFrameFences[i] = logicalDevice->createFence(fenceInfo);
            }
        } catch (vk::SystemError err) {
            throw std::runtime_error("Failed to create semaphores / fence.");
        }
    }
    
    uint32_t findMemoryType(uint32_t typeFilter, vk::MemoryPropertyFlags properties)
    {
        vk::PhysicalDeviceMemoryProperties memProperties = physicalDevice.getMemoryProperties();

        for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
            if ((typeFilter & (1 << i)) and (memProperties.memoryTypes[i].propertyFlags & properties) == properties)
                return i;
        }

        throw std::runtime_error("Found no compatible memory type.");
    }
    
    void createBuffer(vk::DeviceSize size,
                      vk::BufferUsageFlags usage,
                      vk::MemoryPropertyFlags properties,
                      vk::UniqueBuffer& buffer,
                      vk::UniqueDeviceMemory& bufferMemory)
    {
        vk::BufferCreateInfo bufferInfo{
            .size = size,
            .usage = usage,
            .sharingMode = vk::SharingMode::eExclusive  // car utilisé par 1 seule family queue
        };
        
        buffer = logicalDevice->createBufferUnique(bufferInfo);
        

        vk::MemoryRequirements memReq = logicalDevice->getBufferMemoryRequirements(*buffer);
    
        vk::MemoryAllocateInfo allocInfo{
            .allocationSize = memReq.size,
            .memoryTypeIndex = findMemoryType(memReq.memoryTypeBits, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent)
        };
        
        bufferMemory = logicalDevice->allocateMemoryUnique(allocInfo);

        logicalDevice->bindBufferMemory(*buffer, *bufferMemory, 0);
    }
    
    void copyBuffer(vk::Buffer srcBuf, vk::Buffer dstBuf, vk::DeviceSize size)
    {
        vk::CommandBufferAllocateInfo allocInfo {
            .commandPool = commandPool,
            .level = vk::CommandBufferLevel::ePrimary,
            .commandBufferCount = 1
        };
        
        vk::UniqueCommandBuffer commandBuffer = std::move(logicalDevice->allocateCommandBuffersUnique(allocInfo)[0]);
        
        vk::CommandBufferBeginInfo beginInfo {
            .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit
        };
        
        commandBuffer->begin(beginInfo);
        
        vk::BufferCopy copyRegion {
            .size = size
        };
        
        commandBuffer->copyBuffer(srcBuf, dstBuf, 1, &copyRegion);
        commandBuffer->end();
        
        vk::SubmitInfo submitInfo {
            .commandBufferCount = 1,
            .pCommandBuffers = &*commandBuffer
        };
        
        graphicsQueue.submit(submitInfo);
        graphicsQueue.waitIdle();
    }
    
    void createVertexBuffer()
    {
        vk::DeviceSize bufSize = vertices.size() * sizeof(vertices[0]);
        
        // Un staging buffer sert à pouvoir avoir un vbuf avec VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
        // mais dcp elle est pas mappable sur cpu donc il faut passer par un intermédiaire (staging buf)
        vk::UniqueBuffer stagingBuffer;
        vk::UniqueDeviceMemory stagingBufferMemory;
        
        createBuffer(bufSize, vk::BufferUsageFlagBits::eTransferSrc,
                     vk::MemoryPropertyFlagBits::eHostCoherent | vk::MemoryPropertyFlagBits::eHostVisible,
                     stagingBuffer, stagingBufferMemory);
        
        // on met vertices dans le staging buffer
        void* data = logicalDevice->mapMemory(*stagingBufferMemory, 0, bufSize);
        memcpy(data, vertices.data(), (size_t)bufSize);
        logicalDevice->unmapMemory(*stagingBufferMemory);
        
        // on met le staging buffer dans le vrai vertex buffer
        createBuffer(bufSize, vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eVertexBuffer, vk::MemoryPropertyFlagBits::eDeviceLocal, vertexBuffer, vertexBufferMemory);
        
        copyBuffer(*stagingBuffer, *vertexBuffer, bufSize);
    }
    
    void createIndexBuffer()
    {
        vk::DeviceSize bufSize = indices.size() * sizeof(indices[0]);
        
        // Un staging buffer sert à pouvoir avoir un vbuf avec VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
        // mais dcp elle est pas mappable sur cpu donc il faut passer par un intermédiaire (staging buf)
        vk::UniqueBuffer stagingBuffer;
        vk::UniqueDeviceMemory stagingBufferMemory;
        
        createBuffer(bufSize, vk::BufferUsageFlagBits::eTransferSrc,
                     vk::MemoryPropertyFlagBits::eHostCoherent | vk::MemoryPropertyFlagBits::eHostVisible,
                     stagingBuffer, stagingBufferMemory);
        
        // on met les index dans le staging buffer
        void* data = logicalDevice->mapMemory(*stagingBufferMemory, 0, bufSize);
        memcpy(data, indices.data(), (size_t)bufSize);
        logicalDevice->unmapMemory(*stagingBufferMemory);
        
        // on met le staging buffer dans le vrai index buffer
        createBuffer(bufSize, vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eIndexBuffer, vk::MemoryPropertyFlagBits::eDeviceLocal, indexBuffer, indexBufferMemory);
        
        copyBuffer(*stagingBuffer, *indexBuffer, bufSize);
    }
    
    void createUniformBuffers()
    {
        vk::DeviceSize bufSize = sizeof(UniformBufferObject);
        
        uniformBuffers.resize(NB_FRAMES_IN_FLIGHT);
        uniformBuffersMemory.resize(NB_FRAMES_IN_FLIGHT);
        uniformBuffersMapped.resize(NB_FRAMES_IN_FLIGHT);
        
        for (size_t i = 0; i < NB_FRAMES_IN_FLIGHT; i++)
        {
            createBuffer(bufSize,
                         vk::BufferUsageFlagBits::eUniformBuffer,
                         vk::MemoryPropertyFlagBits::eHostCoherent | vk::MemoryPropertyFlagBits::eHostVisible,
                         uniformBuffers[i],
                         uniformBuffersMemory[i]);
            
            uniformBuffersMapped[i] = logicalDevice->mapMemory(*(uniformBuffersMemory[i]), 0, bufSize);
        }
    }
    
    void createDescriptorSetLayout()
    {
        vk::DescriptorSetLayoutBinding uboLayoutBinding {
            .binding            = 0,
            .descriptorType     = vk::DescriptorType::eUniformBuffer,
            .descriptorCount    = 1,
            .stageFlags         = vk::ShaderStageFlagBits::eVertex,
            .pImmutableSamplers = nullptr
        };
        
        vk::DescriptorSetLayoutCreateInfo layoutInfo {
            .bindingCount   = 1,
            .pBindings      = &uboLayoutBinding
        };
        
        descriptorSetLayout = logicalDevice->createDescriptorSetLayout(layoutInfo);
    }
    
    void createDescriptorPool()
    {
        vk::DescriptorPoolSize poolSize {
            .type            = vk::DescriptorType::eUniformBuffer,  // ne pas oublier cette ligne haha
            .descriptorCount = static_cast<uint32_t>(NB_FRAMES_IN_FLIGHT)
        };
        
        vk::DescriptorPoolCreateInfo poolInfo {
            .maxSets        = static_cast<uint32_t>(NB_FRAMES_IN_FLIGHT),
            .poolSizeCount  = 1,
            .pPoolSizes     = &poolSize
        };
        
        descriptorPool = logicalDevice->createDescriptorPoolUnique(poolInfo);
    }
    
    void createDescriptorSets()
    {
        std::vector<vk::DescriptorSetLayout> layouts(NB_FRAMES_IN_FLIGHT, descriptorSetLayout);
        
        vk::DescriptorSetAllocateInfo allocInfo {
            .descriptorPool     = *descriptorPool,
            .descriptorSetCount = static_cast<uint32_t>(NB_FRAMES_IN_FLIGHT),
            .pSetLayouts        = layouts.data()
        };
        
        descriptorSets = logicalDevice->allocateDescriptorSets(allocInfo);
        
        for (size_t i = 0; i < NB_FRAMES_IN_FLIGHT; i++)
        {
            // Ici on met le contenu du UBO dans chaque descriptor set (un par frame-in-flight)
            vk::DescriptorBufferInfo bufInfo {
                .buffer = *uniformBuffers[i],
                .offset = 0,
                .range  = sizeof(UniformBufferObject)
            };
            
            vk::WriteDescriptorSet descriptorWrite {
                .dstSet             = descriptorSets[i],
                .dstBinding         = 0,    // quand on écrit layout(binding = 0) en GLSL on y accède ici à ce 0
                .dstArrayElement    = 0,
                .descriptorCount    = 1,
                .descriptorType     = vk::DescriptorType::eUniformBuffer,
                .pBufferInfo        = &bufInfo
            };
            
            logicalDevice->updateDescriptorSets(1, &descriptorWrite, 0, nullptr);
        }
    }

    void initVulkan()
    {
        std::vector<vk::ExtensionProperties> extensions = vk::enumerateInstanceExtensionProperties(nullptr);
        
#ifdef DEBUG
        // On affiche les extensions vulkan supportées avant de créer l'instance
        std::cout << "Available extensions : " << std::endl;
        for(const auto& e : extensions)
        {
            std::cout << '\t' << e.extensionName << std::endl;
        }
#endif
        createInstance();
        
#ifdef DEBUG
        setupDebugMessenger();
#endif
        createSurface();
        
        // Choisir le GPU
        pickPhysicalDevice();
        
        // Instancier l'interface avec le GPU
        createLogicalDevice();
        
        // File d'images qui attendent d'être affichées
        createSwapChain();
        
        // Pour chaque image de la swap chain, créer son imageView (comment l'interpréter)
        createImageViews();

        // Spécifier à Vulkan des infos sur les color/depth buffer
        // À quoi s'attendre en terme de framebuffer
        createRenderPass();
        
        // Description de comment associer des uniform GLSL à chaque morceau de l'Uniform Buffer Object
        // En fait décrit à quoi va ressembler le descriptor set qui lui contiendra les uniform
        createDescriptorSetLayout();
        
        // Toutes les infos dont vulkan a besoin pour faire le rendu
        // Il faudra instancier une pipeline graphique à chaque fois qu'on veut modifier les shaders
        createGraphicsPipeline();
        
        // Pour chaque imageView de la swap chain, créer son framebuffer associé
        // le lien entre résultat du frag shader et les imageViews se fait sur des framebuffer via renderpass
        createFrameBuffers();
        
        // Tableau de command buffers. Mais on en a un seul
        createCommandPool();
        
        // Créer le VBO qui contient les vertex avec leurs attributs et tout
        createVertexBuffer();
        
        // Index buffer pour éviter les doublons de vertex
        createIndexBuffer();
        
        // Contient les valeurs des uniform pour chaque frame in flight
        createUniformBuffers();
        
        // Équivalent de commandPool mais pour uniform buffer
        createDescriptorPool();
        
        // Ce qu'on envoie au GPU (contient les uniform et est décrit par son descriptorSetLayout)
        createDescriptorSets();
        
        // Enregistrement des commandes qu'on veut faire pour le draw call
        createCommandBuffers();
        
        // Créer les 2 sémaphores et le fence
        createSyncObjects();
    }
};


std::shared_ptr<sk::Window> sk::begin()
{
    currentFrame = 0;
    windowResized = false;
    window = std::make_shared<sk::Window>(1366, 768);

    initVulkan();

    return window;
}

void sk::end()
{
    for(int i = 0; i < NB_FRAMES_IN_FLIGHT; i++)
    {
        logicalDevice->destroySemaphore(imageAvailableSemaphores[i]);
        logicalDevice->destroySemaphore(readyForPresentationSemaphores[i]);
        logicalDevice->destroyFence(readyForNextFrameFences[i]);
    }
    
    cleanupSwapChain();
    
    logicalDevice->destroyCommandPool(commandPool);
    logicalDevice->destroyDescriptorSetLayout(descriptorSetLayout);
    
    logicalDevice->destroyPipeline(graphicsPipeline);
    logicalDevice->destroyPipelineLayout(pipelineLayout);
    logicalDevice->destroyRenderPass(renderPass);
    
#ifdef DEBUG
    destroyDebugUtilsMessengerEXT(instance, debugMessenger, nullptr);
#endif
    
    instance.destroySurfaceKHR(surface);
}

void sk::setUniforms(float uTime, math::vec3 uClr)
{
    UniformBufferObject ubo = {
        .uTime  = uTime,
        .uClr   = uClr
    };
        
    memcpy(uniformBuffersMapped[currentFrame], &ubo, sizeof(ubo));
}

void sk::draw()
{
    // On commence par attendre que la frame précédente soit finie
    if(logicalDevice->waitForFences(1, &readyForNextFrameFences[currentFrame], vk::True, UINT64_MAX) != vk::Result::eSuccess)
    {
        throw std::runtime_error("Timeout or error during waitForFences !");
    }
    
    // Obtient la prochaine image dispo de la swap chain, et puis fait un post dans imageAvailable
    uint32_t imgId;
    try {
        imgId = logicalDevice->acquireNextImageKHR(swapChain, UINT64_MAX, imageAvailableSemaphores[currentFrame], nullptr).value;
    } catch (vk::OutOfDateKHRError(const std::string &msg)) {
        recreateSwapChain();
        return;
    }
    
    // On reset le fence uniquement si on doit pas recréer la swap chain (évite une famine)
    logicalDevice->resetFences(readyForNextFrameFences[currentFrame]);
    
    // Ensuite il faut record ce qu'on veut faire dans commandBuffer, pour l'image d'indice imgId
    commandBuffers[currentFrame].reset();
    recordCommandBuffer(commandBuffers[currentFrame], (uint32_t)imgId);
    
    // On voudra attendre le sémaphore imageAvailable au moment du color attachment (donc entre vert et frag)
    vk::PipelineStageFlags waitStages[] = { vk::PipelineStageFlagBits::eColorAttachmentOutput };
    
    // Ensuite on peut submit le command buffer
    vk::SubmitInfo submitInfo {
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &imageAvailableSemaphores[currentFrame],
        .pWaitDstStageMask = waitStages,
        .commandBufferCount = 1,
        .pCommandBuffers = &commandBuffers[currentFrame],
        .signalSemaphoreCount = 1,
        .pSignalSemaphores = &readyForPresentationSemaphores[currentFrame]
    };

    graphicsQueue.submit(submitInfo, readyForNextFrameFences[currentFrame]);
    
    // Reste plus qu'à envoyer le résultat du rendu à la swap chain pour qu'on puisse le voir
    vk::PresentInfoKHR presentInfo {
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &readyForPresentationSemaphores[currentFrame],
        .swapchainCount = 1,
        .pSwapchains = &swapChain,
        .pImageIndices = &imgId,
        .pResults = nullptr
    };
    
    vk::Result presentRes;
    try {
        presentRes = presentQueue.presentKHR(presentInfo);
    } catch (vk::OutOfDateKHRError(const std::string &msg)) {
        recreateSwapChain();
        return;
    }
    
    if(presentRes == vk::Result::eSuboptimalKHR or windowResized)
    {
        windowResized = false;
        recreateSwapChain();
    }

    currentFrame = (1 + currentFrame) % NB_FRAMES_IN_FLIGHT;
}
