#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

#include <iostream>
#include <stdexcept>
#include <cstdlib>

#include <vector>
#include <algorithm>

#define VK_CHECK(call) \
  do { \
    VkResult result = call; \
    if(result != VK_SUCCESS) \
      throw std::runtime_error("Vulkan error!"); \
  } while (0)

static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT           messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT                  messageTypes,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
    void* pUserData)
{
    switch (messageSeverity)
    {
    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT: {
        std::cout << "WARNING: " << pCallbackData->pMessage << std::endl;
        break;
    }
    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT:
    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT: {
        std::cout << "INFO: " << pCallbackData->pMessage << std::endl;
        break;
    }
    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT: {
        std::cout << "ERROR: " << pCallbackData->pMessage << std::endl;
        break;
    }
    }

    if (messageSeverity > VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
        throw std::runtime_error("Validation Error Encountered!");

    return VK_FALSE;
}

VkDebugUtilsMessengerEXT registerDebugCallback(VkInstance instance)
{
    // defines what debug info gets printed
    // doesn't print general INFO stuff because that clutters console window
    VkDebugUtilsMessengerCreateInfoEXT createInfo = { VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT };
    createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT; // | VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT;

    createInfo.pfnUserCallback = debugCallback;

    PFN_vkCreateDebugUtilsMessengerEXT vkCreateDebugUtilsMessengerEXT = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");

    VkDebugUtilsMessengerEXT callback = 0;
    if (vkCreateDebugUtilsMessengerEXT != nullptr) {
        VK_CHECK(vkCreateDebugUtilsMessengerEXT(instance, &createInfo, 0, &callback));
    }

    return callback;
}

void destroyDebugCallback(VkInstance instance, VkDebugUtilsMessengerEXT callback)
{
    PFN_vkDestroyDebugUtilsMessengerEXT vkDestroyDebugUtilsMessengerEXT = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");

    vkDestroyDebugUtilsMessengerEXT(instance, callback, 0);
}

const uint32_t WIDTH = 1280;
const uint32_t HEIGHT = 720;

// Write code first and as it becomes painful to deal with make it less painful to deal with
// Semantic Compression by Casey Muratori

class HelloTriangleApplication
{
public:
    void Run()
    {
        InitWindow();
        InitVulkan();
        MainLoop();
        Cleanup();
    }

private:
    void InitWindow()
    {
        glfwInit();

        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

        window = glfwCreateWindow(WIDTH, HEIGHT, "Vulkan", nullptr, nullptr);

        if (!window)
            throw std::runtime_error("GLFW couldn't create window");
    }

    void InitVulkan()
    {
        CreateInstance();
        //DebugExtensionSupport();

        CreatePhysicalAndLogicalDevice();
        CreateSurface();
        CreateVulkanSemaphores();
        CreateCommandPool();
    }

    VkPhysicalDevice PickPhysicalDevice(std::vector<VkPhysicalDevice>& pd, uint32_t pdc)
    {
        for (uint32_t i = 0; i < pdc; ++i)
        {
            VkPhysicalDeviceProperties props;
            vkGetPhysicalDeviceProperties(pd[i], &props);

            queueFamilyIndex = GetGraphicsQueueFamily(pd[i]);

            if (queueFamilyIndex == VK_QUEUE_FAMILY_IGNORED)
                continue;

            // check for presentation support on the device
            if (!vkGetPhysicalDeviceWin32PresentationSupportKHR(pd[i], queueFamilyIndex))
                continue;

            if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
            {
                std::cout << "Picking discrete GPU " << props.deviceName << std::endl;
                return pd[i];
            }
        }

        // return the first device if no discrete gpu found
        if (pdc > 0 && queueFamilyIndex != VK_QUEUE_FAMILY_IGNORED)
        {
            VkPhysicalDeviceProperties props;
            vkGetPhysicalDeviceProperties(pd[0], &props);

            std::cout << "Picking fallback GPU " << props.deviceName << std::endl;
            return pd[0];
        }

        std::cout << "No physical devices available!" << std::endl;
        return VK_NULL_HANDLE;
    }


    void CreateInstance()
    {
        // for portability should check if the used api version is available first
        VkApplicationInfo appInfo{};
        appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        appInfo.pApplicationName = "MyFirstVulkanTriangle";
        appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.pEngineName = "No Engine";
        appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.apiVersion = VK_API_VERSION_1_2;

        VkInstanceCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        createInfo.pApplicationInfo = &appInfo;


        // get required extensions from GLFW
        uint32_t glfwExtensionCount = 0;
        const char** glfwExtensions;

        glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

        std::vector<const char*> extensionNames(glfwExtensions, glfwExtensions + glfwExtensionCount);

        extensionNames.push_back("VK_EXT_debug_utils");

        createInfo.enabledExtensionCount = static_cast<uint32_t>(extensionNames.size());
        createInfo.ppEnabledExtensionNames = extensionNames.data();

#ifdef _DEBUG
        const char* debugLayers[] =
        {
          "VK_LAYER_KHRONOS_validation"
        };

        createInfo.ppEnabledLayerNames = debugLayers;
        createInfo.enabledLayerCount = 1;
#endif

        VK_CHECK(vkCreateInstance(&createInfo, 0, &instance));

        // Set Debug Callback for Validation Errors
        debugMessenger = registerDebugCallback(instance);
    }

    uint32_t GetGraphicsQueueFamily(VkPhysicalDevice pd)
    {
        // Find a QueueFamily that supports graphics
        uint32_t queueFamilyCount;
        vkGetPhysicalDeviceQueueFamilyProperties(pd, &queueFamilyCount, 0);
        std::vector<VkQueueFamilyProperties> queueFamilyProperties(queueFamilyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(pd, &queueFamilyCount, queueFamilyProperties.data());

        for (uint32_t i = 0; i < queueFamilyCount; ++i)
        {
            if (queueFamilyProperties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
                return i;
        }

        // throw std::runtime_error("No queue families support graphics. Is this a compute-only device?");

        return VK_QUEUE_FAMILY_IGNORED;
    }

    void CreatePhysicalAndLogicalDevice()
    {
        uint32_t deviceCount;
        VK_CHECK(vkEnumeratePhysicalDevices(instance, &deviceCount, 0));
        std::vector<VkPhysicalDevice> physicalDevices(deviceCount);
        VK_CHECK(vkEnumeratePhysicalDevices(instance, &deviceCount, physicalDevices.data()));

        physicalDevice = PickPhysicalDevice(physicalDevices, deviceCount);
        if (physicalDevice == VK_NULL_HANDLE)
            throw std::runtime_error("Abort! No Vulkan device found.");

        float queuePriorities[] = { 1.0f };

        VkDeviceQueueCreateInfo queueInfo = { VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO };
        queueInfo.queueFamilyIndex = queueFamilyIndex;
        queueInfo.queueCount = 1;
        queueInfo.pQueuePriorities = queuePriorities;

        const char* extensions[] =
        {
          VK_KHR_SWAPCHAIN_EXTENSION_NAME
        };

        VkDeviceCreateInfo createInfo = { VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO };
        createInfo.queueCreateInfoCount = 1;
        createInfo.pQueueCreateInfos = &queueInfo;
        createInfo.ppEnabledExtensionNames = extensions;
        createInfo.enabledExtensionCount = sizeof(extensions) / sizeof(extensions[0]);

        VK_CHECK(vkCreateDevice(physicalDevice, &createInfo, nullptr, &device));
    }

    void CreateSurface()
    {
#ifdef VK_USE_PLATFORM_WIN32_KHR
        VkWin32SurfaceCreateInfoKHR createInfo = { VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR };
        createInfo.hinstance = GetModuleHandle(0);
        createInfo.hwnd = glfwGetWin32Window(window);

        VK_CHECK(vkCreateWin32SurfaceKHR(instance, &createInfo, 0, &surface));
#else
#error Unsupported Platforms
#endif
    }

    void GetSwapchainFormat()
    {
        uint32_t formatCount = 0;
        VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, 0));
        std::vector<VkSurfaceFormatKHR> formats(formatCount);
        VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, formats.data()));

        // special case where if the only format supported is undefined that means any format is supported
        if (formatCount == 1 && formats[0].format == VK_FORMAT_UNDEFINED)
        {
            swapchainFormat = VK_FORMAT_R8G8B8A8_UNORM;
            return;
        }

        // give preference to 32 bit formats
        for (uint32_t i = 0; i < formatCount; ++i)
        {
            if (formats[i].format == VK_FORMAT_R8G8B8A8_UNORM || formats[i].format == VK_FORMAT_B8G8R8A8_UNORM)
            {
                swapchainFormat = formats[i].format;
                return;
            }
        }

        swapchainFormat = formats[0].format;
    }

    VkSwapchainKHR CreateSwapchain(uint32_t width, uint32_t height, VkSwapchainKHR oldSwapchain = 0)
    {
        // get surface capabilities before creating swapchain
        VkSurfaceCapabilitiesKHR surfaceCap;
        VK_CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &surfaceCap));

        // Not all alpha composites are supported on all platforms so it is important to read from the surface caps
        VkCompositeAlphaFlagBitsKHR surfaceComposite = (surfaceCap.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR)
            ? VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR
            : (surfaceCap.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR)
            ? VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR
            : (surfaceCap.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR)
            ? VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR
            : VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR;

        VkBool32 isSupported = 0;
        VK_CHECK(vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, queueFamilyIndex, surface, &isSupported));

        if (isSupported == VK_FALSE)
            throw std::runtime_error("Surface does not support presentation");

        VkSwapchainCreateInfoKHR createInfo = { VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR };
        createInfo.surface = surface;
        createInfo.minImageCount = std::max(2u, surfaceCap.minImageCount); // double buffered at min
        createInfo.imageFormat = swapchainFormat;
        createInfo.imageColorSpace = VK_COLORSPACE_SRGB_NONLINEAR_KHR;
        createInfo.imageExtent.width = width;
        createInfo.imageExtent.height = height;
        createInfo.imageArrayLayers = 1;
        createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        createInfo.queueFamilyIndexCount = 1;
        createInfo.pQueueFamilyIndices = &queueFamilyIndex;
        createInfo.presentMode = VK_PRESENT_MODE_FIFO_KHR;
        createInfo.preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
        createInfo.compositeAlpha = surfaceComposite;
        createInfo.presentMode = VK_PRESENT_MODE_FIFO_KHR;
        createInfo.oldSwapchain = oldSwapchain;

        VkSwapchainKHR swapchain = 0;
        VK_CHECK(vkCreateSwapchainKHR(device, &createInfo, 0, &swapchain));

        return swapchain;
    }

    struct Swapchain
    {
        VkSwapchainKHR swapchain;

        std::vector<VkImage> images;
        std::vector<VkImageView> imageViews;
        std::vector<VkFramebuffer> framebuffers;

        uint32_t width, height;
        uint32_t imageCount;
    };

    bool CreateSwapchain(Swapchain& result, uint32_t width, uint32_t height, VkSwapchainKHR old = 0)
    {
        VkSwapchainKHR swapchain = CreateSwapchain(width, height, old);

        if (!swapchain)
            return false;

        uint32_t imageCount = 0;

        VK_CHECK(vkGetSwapchainImagesKHR(device, swapchain, &imageCount, 0));
        std::vector<VkImage> swapchainImages(imageCount);
        VK_CHECK(vkGetSwapchainImagesKHR(device, swapchain, &imageCount, swapchainImages.data()));

        std::vector<VkImageView> swapchainImageViews(imageCount);
        for (uint32_t i = 0; i < imageCount; ++i)
            swapchainImageViews[i] = CreateImageView(swapchainImages[i]);

        std::vector<VkFramebuffer> swapchainFramebuffers(imageCount);
        for (uint32_t i = 0; i < imageCount; ++i)
            swapchainFramebuffers[i] = CreateFramebuffer(swapchainImageViews[i], width, height);

        result.swapchain = swapchain;
        result.imageCount = imageCount;
        result.width = width;
        result.height = height;
        result.images = swapchainImages;
        result.imageViews = swapchainImageViews;
        result.framebuffers = swapchainFramebuffers;

        return true;
    }

    void ResizeSwapchain(Swapchain& swapchain, uint32_t width, uint32_t height)
    {
        Swapchain oldSwapchain = swapchain;

        CreateSwapchain(this->swapchain, width, height, oldSwapchain.swapchain);

        VK_CHECK(vkDeviceWaitIdle(device));

        // TODO: this is a bit weird... fix it
        DestroySwapchain(oldSwapchain);
    }

    void DestroySwapchain(Swapchain& swapchain)
    {
        for (uint32_t i = 0; i < swapchain.imageCount; ++i)
            vkDestroyFramebuffer(device, swapchain.framebuffers[i], 0);

        for (uint32_t i = 0; i < swapchain.imageCount; ++i)
            vkDestroyImageView(device, swapchain.imageViews[i], 0);

        vkDestroySwapchainKHR(device, swapchain.swapchain, 0);
    }

    void DebugExtensionSupport()
    {
        // Details of extensions supported
        uint32_t extensionCount = 0;

        vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);
        std::vector<VkExtensionProperties> extensions(extensionCount);
        vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, extensions.data());

        std::cout << "available extensions:\n";

        for (const auto& extension : extensions) {
            std::cout << '\t' << extension.extensionName << '\n';
        }
    }

    void CreateVulkanSemaphores()
    {
        VkSemaphoreCreateInfo createInfo = { VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };

        VK_CHECK(vkCreateSemaphore(device, &createInfo, 0, &acquireSemaphore));
        VK_CHECK(vkCreateSemaphore(device, &createInfo, 0, &releaseSemaphore));
    }

    void CreateCommandPool()
    {
        VkCommandPoolCreateInfo createInfo = { VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };
        createInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
        createInfo.queueFamilyIndex = queueFamilyIndex;

        VK_CHECK(vkCreateCommandPool(device, &createInfo, 0, &commandPool));
    }

    void CreateRenderPass()
    {
        VkAttachmentDescription attachments[1] = {};
        attachments[0].format = swapchainFormat;
        attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
        attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attachments[0].initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        attachments[0].finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        VkAttachmentReference colorAttachments = { 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };

        VkSubpassDescription subpass = {};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments = &colorAttachments;

        VkRenderPassCreateInfo createInfo = { VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO };
        createInfo.attachmentCount = sizeof(attachments) / sizeof(attachments[0]);
        createInfo.pAttachments = attachments;
        createInfo.subpassCount = 1;
        createInfo.pSubpasses = &subpass;

        VK_CHECK(vkCreateRenderPass(device, &createInfo, 0, &renderPass));
    }

    VkFramebuffer CreateFramebuffer(VkImageView imageView, uint32_t width, uint32_t height)
    {
        VkFramebufferCreateInfo createInfo = { VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO };
        createInfo.renderPass = renderPass;
        createInfo.attachmentCount = 1;
        createInfo.pAttachments = &imageView;
        createInfo.width = width;
        createInfo.height = height;
        createInfo.layers = 1;

        VkFramebuffer framebuffer = 0;
        VK_CHECK(vkCreateFramebuffer(device, &createInfo, 0, &framebuffer));

        return framebuffer;
    }

    VkImageView CreateImageView(VkImage swapchainImage)
    {
        VkImageViewCreateInfo createInfo = { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
        createInfo.image = swapchainImage;
        createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        createInfo.format = swapchainFormat;
        createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        createInfo.subresourceRange.levelCount = 1;
        createInfo.subresourceRange.layerCount = 1;

        VkImageView imageView = 0;
        VK_CHECK(vkCreateImageView(device, &createInfo, 0, &imageView));

        return imageView;
    }

    VkShaderModule CreateShader(const char* path)
    {
        // Read file contents from path
        FILE* file = fopen(path, "rb");

        fseek(file, 0, SEEK_END);
        long length = ftell(file);
        fseek(file, 0, SEEK_SET);

        char* buffer = new char[length];
        size_t rc = fread(buffer, 1, length, file);
        fclose(file);

        // Fill Create info struct
        VkShaderModuleCreateInfo createInfo = { VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO };
        createInfo.codeSize = length;
        createInfo.pCode = reinterpret_cast<const uint32_t*>(buffer);

        // Create Shader module
        VkShaderModule shaderModule = 0;
        VK_CHECK(vkCreateShaderModule(device, &createInfo, 0, &shaderModule));

        return shaderModule;
    }

    VkPipelineCache CreatePipelineCache()
    {
        VkPipelineCacheCreateInfo createInfo = { VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO };
        createInfo.initialDataSize = 0;
        createInfo.pInitialData = nullptr;

        VkPipelineCache pipelineCache = 0;
        VK_CHECK(vkCreatePipelineCache(device, &createInfo, 0, &pipelineCache));

        return pipelineCache;
    }

    VkPipelineLayout CreatePipelineLayout()
    {
        VkPipelineLayoutCreateInfo createInfo = { VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };

        VkPipelineLayout pipelineLayout = 0;
        VK_CHECK(vkCreatePipelineLayout(device, &createInfo, 0, &pipelineLayout));

        return pipelineLayout;
    }

    VkPipeline CreateGraphicsPipeline(VkPipelineCache cache, VkShaderModule vs, VkShaderModule fs)
    {
        VkGraphicsPipelineCreateInfo createInfo = { VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO };

        VkPipelineShaderStageCreateInfo stages[2] = {};
        stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
        stages[0].module = vs;
        stages[0].pName = "main";
        stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        stages[1].module = fs;
        stages[1].pName = "main";

        createInfo.stageCount = sizeof(stages) / sizeof(stages[0]);
        createInfo.pStages = stages;

        // everything is left to 0 because our vertex data is in the shader itself
        VkPipelineVertexInputStateCreateInfo vertexInput = { VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO };
        createInfo.pVertexInputState = &vertexInput;

        // if using the same shader for drawing triangles and lines, then would need to change topology
        VkPipelineInputAssemblyStateCreateInfo inputAssembly = { VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO };
        inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        createInfo.pInputAssemblyState = &inputAssembly;

        // viewport should be set dynamically
        VkPipelineViewportStateCreateInfo viewportState = { VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO };
        viewportState.viewportCount = 1;
        viewportState.scissorCount = 1;
        createInfo.pViewportState = &viewportState;

        VkPipelineRasterizationStateCreateInfo rasterizationState = { VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO };
        rasterizationState.polygonMode = VK_POLYGON_MODE_FILL;
        rasterizationState.lineWidth = 1.0;
        createInfo.pRasterizationState = &rasterizationState;

        VkPipelineMultisampleStateCreateInfo multisampleState = { VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO };
        multisampleState.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
        createInfo.pMultisampleState = &multisampleState;

        VkPipelineDepthStencilStateCreateInfo depthStencilState = { VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO };
        createInfo.pDepthStencilState = &depthStencilState;

        VkPipelineColorBlendAttachmentState colorAttachmentState = {};
        colorAttachmentState.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

        VkPipelineColorBlendStateCreateInfo colorBlendState = { VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO };
        colorBlendState.attachmentCount = 1;
        colorBlendState.pAttachments = &colorAttachmentState;
        createInfo.pColorBlendState = &colorBlendState;

        VkDynamicState dynamicStates[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };

        VkPipelineDynamicStateCreateInfo dynamicState = { VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO };
        dynamicState.dynamicStateCount = sizeof(dynamicStates) / sizeof(dynamicStates[0]);
        dynamicState.pDynamicStates = dynamicStates;
        createInfo.pDynamicState = &dynamicState;

        createInfo.layout = pipelineLayout;
        createInfo.renderPass = renderPass;

        VkPipeline pipeline = 0;
        VK_CHECK(vkCreateGraphicsPipelines(device, cache, 1, &createInfo, 0, &pipeline));

        return pipeline;
    }



    VkImageMemoryBarrier ImageBarrier(VkImage image,
        VkAccessFlags srcAccessMask, VkAccessFlags dstAccessMask, VkImageLayout oldLayout, VkImageLayout newLayout)
    {
        VkImageMemoryBarrier result = { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };

        result.srcAccessMask = srcAccessMask;
        result.dstAccessMask = dstAccessMask;
        result.oldLayout = oldLayout;
        result.newLayout = newLayout;
        result.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        result.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        result.image = image;
        result.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        result.subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
        result.subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;

        return result;
    }

    void MainLoop()
    {

        VkQueue queue = 0;
        vkGetDeviceQueue(device, queueFamilyIndex, 0, &queue);

        VkCommandBufferAllocateInfo allocateInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
        allocateInfo.commandPool = commandPool;
        allocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocateInfo.commandBufferCount = 1;

        VkCommandBuffer commandBuffer = 0;
        VK_CHECK(vkAllocateCommandBuffers(device, &allocateInfo, &commandBuffer));

        GetSwapchainFormat();

        CreateRenderPass();

        if (!CreateSwapchain(swapchain, WIDTH, HEIGHT, 0))
            throw std::runtime_error("Cannot make a swapchain");

        triangleVS = CreateShader("shaders/triangle.vert.spv");
        triangleFS = CreateShader("shaders/triangle.frag.spv");

        pipelineCache = CreatePipelineCache();
        pipelineLayout = CreatePipelineLayout();
        trianglePipeline = CreateGraphicsPipeline(pipelineCache, triangleVS, triangleFS);

        while (!glfwWindowShouldClose(window)) {
            glfwPollEvents();

            // check if swapchain needs to be resized
            int newWidth = 0, newHeight = 0;
            glfwGetWindowSize(window, &newWidth, &newHeight);

            if (swapchain.width != newWidth || swapchain.height != newHeight)
            {
                ResizeSwapchain(swapchain, newWidth, newHeight);
            }

            uint32_t imageIndex = 0;
            VK_CHECK(vkAcquireNextImageKHR(device, swapchain.swapchain, ~0ull, acquireSemaphore, VK_NULL_HANDLE, &imageIndex));

            VK_CHECK(vkResetCommandPool(device, commandPool, 0));

            VkCommandBufferBeginInfo beginInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
            beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

            VK_CHECK(vkBeginCommandBuffer(commandBuffer, &beginInfo));

            VkImageMemoryBarrier renderBeginBarrier = ImageBarrier(swapchain.images[imageIndex], 0, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
            vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_DEPENDENCY_BY_REGION_BIT, 0, 0, 0, 0, 1, &renderBeginBarrier);


            VkClearColorValue color = { 48.f / 256.f, 10.f / 256.f, 36.f / 256.f, 1.0f };
            VkClearValue clearColor = { color };

            VkRenderPassBeginInfo passBeginInfo = { VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO };
            passBeginInfo.renderPass = renderPass;
            passBeginInfo.framebuffer = swapchain.framebuffers[imageIndex];
            passBeginInfo.renderArea.extent.width = swapchain.width;
            passBeginInfo.renderArea.extent.height = swapchain.height;
            passBeginInfo.clearValueCount = 1;
            passBeginInfo.pClearValues = &clearColor;

            vkCmdBeginRenderPass(commandBuffer, &passBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

            // -height flips the viewport because vulkan has a weird coordinate system
            VkViewport viewport = { 0, float(swapchain.height), float(swapchain.width), -float(swapchain.height), 0, 1 };
            VkRect2D scissor = { {0, 0}, {swapchain.width, swapchain.height} };

            vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
            vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

            vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, trianglePipeline);
            vkCmdDraw(commandBuffer, 3, 1, 0, 0);

            vkCmdEndRenderPass(commandBuffer);


            VkImageMemoryBarrier renderEndBarrier = ImageBarrier(swapchain.images[imageIndex], VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
            vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_DEPENDENCY_BY_REGION_BIT, 0, 0, 0, 0, 1, &renderEndBarrier);

            VK_CHECK(vkEndCommandBuffer(commandBuffer));

            VkPipelineStageFlags submitStageFlags = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

            VkSubmitInfo submitInfo = { VK_STRUCTURE_TYPE_SUBMIT_INFO };
            submitInfo.waitSemaphoreCount = 1;
            submitInfo.pWaitSemaphores = &acquireSemaphore;
            submitInfo.pWaitDstStageMask = &submitStageFlags;
            submitInfo.commandBufferCount = 1;
            submitInfo.pCommandBuffers = &commandBuffer;
            submitInfo.signalSemaphoreCount = 1;
            submitInfo.pSignalSemaphores = &releaseSemaphore;

            VK_CHECK(vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE));

            VkPresentInfoKHR presentInfo = { VK_STRUCTURE_TYPE_PRESENT_INFO_KHR };
            presentInfo.swapchainCount = 1;
            presentInfo.pSwapchains = &swapchain.swapchain;
            presentInfo.pImageIndices = &imageIndex;
            presentInfo.waitSemaphoreCount = 1;
            presentInfo.pWaitSemaphores = &releaseSemaphore;

            VK_CHECK(vkQueuePresentKHR(queue, &presentInfo));

            VK_CHECK(vkDeviceWaitIdle(device));
        }

    }

    void Cleanup()
    {

        vkDestroyPipeline(device, trianglePipeline, 0);

        vkDestroyPipelineCache(device, pipelineCache, 0);
        vkDestroyPipelineLayout(device, pipelineLayout, 0);

        vkDestroyShaderModule(device, triangleFS, 0);
        vkDestroyShaderModule(device, triangleVS, 0);

        vkDestroyCommandPool(device, commandPool, 0);
        vkDestroyRenderPass(device, renderPass, 0);

        vkDestroySemaphore(device, acquireSemaphore, 0);
        vkDestroySemaphore(device, releaseSemaphore, 0);

        DestroySwapchain(swapchain);
        vkDestroySurfaceKHR(instance, surface, 0);

        vkDestroyDevice(device, nullptr);
        destroyDebugCallback(instance, debugMessenger);
        vkDestroyInstance(instance, nullptr);

        glfwDestroyWindow(window);
        glfwTerminate();
    }

private:
    GLFWwindow* window;
    VkInstance instance;
    VkPhysicalDevice physicalDevice;
    VkDevice device;
    VkSurfaceKHR surface;
    Swapchain swapchain;
    VkSemaphore acquireSemaphore;
    VkSemaphore releaseSemaphore;
    VkCommandPool commandPool;
    VkRenderPass renderPass;
    VkShaderModule triangleVS;
    VkShaderModule triangleFS;
    VkPipelineCache pipelineCache;
    VkPipelineLayout pipelineLayout;
    VkPipeline trianglePipeline;
    VkFormat swapchainFormat;
    VkDebugUtilsMessengerEXT debugMessenger;

    uint32_t queueFamilyIndex;
};

int main()
{
    HelloTriangleApplication app;

    try
    {
        app.Run();
    }
    catch (const std::exception& e)
    {
        std::cerr << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}