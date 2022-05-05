#include <glfw3.h>
#include <glfw3native.h>
#include <volk.h>

#include <iostream>
#include <stdexcept>
#include <cstdlib>

#include <vector>
#include <array>
#include <algorithm>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE

#include <glm/glm.hpp>
#include <glm/gtx/transform.hpp>

#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>

#if 0
#include <objparser.h>
#endif // 0

#include <meshoptimizer.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#define VK_CHECK(call) \
  do { \
    VkResult result = call; \
    if(result != VK_SUCCESS) \
      throw std::runtime_error("Vulkan error!"); \
  } while (0)

VkBool32 debugReportCallback(VkDebugReportFlagsEXT flags, 
    VkDebugReportObjectTypeEXT objectType, 
    uint64_t object,
    size_t location,
    int32_t messageCode,
    const char* pLayerPrefix,
    const char* pMessage,
    void* pUserData)
{
    switch (flags)
    {
    case VK_DEBUG_REPORT_WARNING_BIT_EXT:
    case VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT:
    {
        std::cout << "WARNING: " << pMessage << std::endl;
        break;
    }
    case VK_DEBUG_REPORT_INFORMATION_BIT_EXT:
    case VK_DEBUG_REPORT_DEBUG_BIT_EXT: {
        std::cout << "INFO: " << pMessage << std::endl;
        break;
    }
    case VK_DEBUG_REPORT_ERROR_BIT_EXT: {
        std::cout << "ERROR: " << pMessage << std::endl;
        break;
    }
    }

    if (flags & VK_DEBUG_REPORT_ERROR_BIT_EXT)
        throw std::runtime_error("Validation Error Encountered!");

    return VK_FALSE;
}

VkDebugReportCallbackEXT registerDebugCallback(VkInstance instance)
{
    VkDebugReportCallbackCreateInfoEXT createInfo = { VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT };
    createInfo.flags = VK_DEBUG_REPORT_WARNING_BIT_EXT | VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT | VK_DEBUG_REPORT_ERROR_BIT_EXT;
    createInfo.pfnCallback = debugReportCallback;

    VkDebugReportCallbackEXT callback = 0;
    VK_CHECK(vkCreateDebugReportCallbackEXT(instance, &createInfo, 0, &callback));

    return callback;
}

void destroyDebugCallback(VkInstance instance, VkDebugReportCallbackEXT callback)
{
    vkDestroyDebugReportCallbackEXT(instance, callback, 0);
}

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

        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API); // tell glfw to not create opengl context by default
        glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

        window = glfwCreateWindow(1024, 768, "Vulkan", nullptr, nullptr);

        if (!window)
            throw std::runtime_error("GLFW couldn't create window");
    }

    void InitVulkan()
    {
        VK_CHECK(volkInitialize());

        CreateInstance();
        //DebugExtensionSupport();
        
        volkLoadInstance(instance);

        // Set Debug Callback for Validation Errors
        debugMessenger = registerDebugCallback(instance);

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
        appInfo.apiVersion = VK_API_VERSION_1_3;

        VkInstanceCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        createInfo.pApplicationInfo = &appInfo;


        // get required extensions from GLFW
        uint32_t glfwExtensionCount = 0;
        const char** glfwExtensions;

        glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

        std::vector<const char*> extensionNames(glfwExtensions, glfwExtensions + glfwExtensionCount);

        extensionNames.push_back("VK_EXT_debug_report");

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

        std::vector<const char*> extensions =
        {
          VK_KHR_SWAPCHAIN_EXTENSION_NAME,
          VK_KHR_PUSH_DESCRIPTOR_EXTENSION_NAME,
          VK_KHR_16BIT_STORAGE_EXTENSION_NAME,
          VK_KHR_8BIT_STORAGE_EXTENSION_NAME,
        };

        VkPhysicalDeviceFeatures deviceFeatures = {};
        deviceFeatures.samplerAnisotropy = VK_TRUE;

        //VkPhysicalDevice16BitStorageFeatures features16 = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_16BIT_STORAGE_FEATURES };
        //features16.storageBuffer16BitAccess = true;

        //VkPhysicalDevice8BitStorageFeatures features8 = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_8BIT_STORAGE_FEATURES };
        //features8.storageBuffer8BitAccess = true;

        VkPhysicalDeviceVulkan12Features features = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES };
        features.shaderInt8 = true;
        features.uniformAndStorageBuffer8BitAccess = true;

        VkDeviceCreateInfo createInfo = { VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO };
        createInfo.queueCreateInfoCount = 1;
        createInfo.pQueueCreateInfos = &queueInfo;
        createInfo.ppEnabledExtensionNames = extensions.data();
        createInfo.enabledExtensionCount = uint32_t(extensions.size());
        createInfo.pEnabledFeatures = &deviceFeatures;
        createInfo.pNext = &features;//&features16;
        //features16.pNext = &features8;

        // might need to enable feature for read-write buffers in shaders (vertexPipelineStoresAndAtomics) 

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
            swapchainImageViews[i] = CreateImageView(swapchainImages[i], swapchainFormat, VK_IMAGE_ASPECT_COLOR_BIT);

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
        VkAttachmentDescription attachments[2] = {};
        attachments[0].format = swapchainFormat;
        attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
        attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attachments[0].initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        attachments[0].finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        attachments[1].format = VK_FORMAT_D32_SFLOAT;
        attachments[1].samples = VK_SAMPLE_COUNT_1_BIT;
        attachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        attachments[1].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attachments[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        attachments[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attachments[1].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        attachments[1].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        VkAttachmentReference colorAttachments = { 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };
        VkAttachmentReference depthAttachment = { 1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL };

        VkSubpassDescription subpass = {};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments = &colorAttachments;
        subpass.pDepthStencilAttachment = &depthAttachment;

        VkRenderPassCreateInfo createInfo = { VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO };
        createInfo.attachmentCount = sizeof(attachments) / sizeof(attachments[0]);
        createInfo.pAttachments = attachments;
        createInfo.subpassCount = 1;
        createInfo.pSubpasses = &subpass;

        VK_CHECK(vkCreateRenderPass(device, &createInfo, 0, &renderPass));
    }

    VkFramebuffer CreateFramebuffer(VkImageView imageView, uint32_t width, uint32_t height)
    {
        std::array<VkImageView, 2> attachments = { imageView, depthImageView };

        VkFramebufferCreateInfo createInfo = { VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO };
        createInfo.renderPass = renderPass;
        createInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
        createInfo.pAttachments = attachments.data();
        createInfo.width = width;
        createInfo.height = height;
        createInfo.layers = 1;

        VkFramebuffer framebuffer = 0;
        VK_CHECK(vkCreateFramebuffer(device, &createInfo, 0, &framebuffer));

        return framebuffer;
    }

    VkImageView CreateImageView(VkImage swapchainImage, VkFormat swapchainFormat, VkImageAspectFlags aspectFlags)
    {
        VkImageViewCreateInfo createInfo = { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
        createInfo.image = swapchainImage;
        createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        createInfo.format = swapchainFormat;
        createInfo.subresourceRange.aspectMask = aspectFlags;
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
        std::vector<VkDescriptorSetLayoutBinding> setBindings(2);
        setBindings[0].binding = 0;
        setBindings[0].descriptorCount = 1;
        setBindings[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        setBindings[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

        setBindings[1].binding = 1;
        setBindings[1].descriptorCount = 1;
        setBindings[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        setBindings[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;


        VkDescriptorSetLayoutCreateInfo descriptorCreateInfo = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
        descriptorCreateInfo.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR;
        descriptorCreateInfo.bindingCount = static_cast<uint32_t>(setBindings.size());
        descriptorCreateInfo.pBindings = setBindings.data();

        VK_CHECK(vkCreateDescriptorSetLayout(device, &descriptorCreateInfo, 0, &descriptorSetLayout));

        VkPushConstantRange pushConstantRange = {};
        pushConstantRange.offset = 0;
        pushConstantRange.size = sizeof(MeshPushConstants);
        pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

        VkPipelineLayoutCreateInfo createInfo = { VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
        createInfo.setLayoutCount = 1;
        createInfo.pSetLayouts = &descriptorSetLayout;
        createInfo.pushConstantRangeCount = 1;
        createInfo.pPushConstantRanges = &pushConstantRange;

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
        rasterizationState.cullMode = VK_CULL_MODE_BACK_BIT;
        rasterizationState.lineWidth = 1.0;
        createInfo.pRasterizationState = &rasterizationState;

        VkPipelineMultisampleStateCreateInfo multisampleState = { VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO };
        multisampleState.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
        createInfo.pMultisampleState = &multisampleState;

        VkPipelineDepthStencilStateCreateInfo depthStencilState = { VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO };
        depthStencilState.depthTestEnable = VK_TRUE;
        depthStencilState.depthWriteEnable= VK_TRUE;
        depthStencilState.depthCompareOp = VK_COMPARE_OP_LESS;
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

    struct Vertex
    {
        float vx, vy, vz;
        uint8_t nx, ny, nz, nw;
        float tu, tv;
    };

    struct Mesh
    {
        std::vector<Vertex> vertices;
        std::vector<uint32_t> indices;
    };

    struct Texture
    {
        stbi_uc* pixels;
        uint32_t imageWidth;
        uint32_t imageHeight;
        uint32_t imageSize;
    };

    bool LoadMesh(Mesh& result, const char* path)
    {
        // Load Model using tinyobjloader
        tinyobj::ObjReaderConfig reader_config;
        reader_config.mtl_search_path = "./"; // Path to material files
        reader_config.triangulate = false;

        tinyobj::ObjReader reader;

        if (!reader.ParseFromFile(path, reader_config)) {
            if (!reader.Error().empty()) {
                std::cerr << "TinyObjReader: " << reader.Error();
            }
        }

        if (!reader.Warning().empty()) {
            std::cout << "TinyObjReader: " << reader.Warning();
        }

        auto& attrib = reader.GetAttrib();
        auto& shapes = reader.GetShapes();
        auto& materials = reader.GetMaterials();

        uint32_t indexCount = 0;

        for (int i = 0; i < shapes.size(); ++i)
        {
            indexCount += uint32_t(shapes[i].mesh.indices.size());
        }

        std::vector<Vertex> vertices(indexCount);
        std::vector<uint32_t> indices(indexCount);

        // Loop over shapes
        for (size_t s = 0; s < shapes.size(); s++) {
            // Loop over faces(polygon)
            size_t index_offset = 0;
            for (size_t f = 0; f < shapes[s].mesh.num_face_vertices.size(); f++) {
                size_t fv = size_t(shapes[s].mesh.num_face_vertices[f]);


                // Loop over vertices in the face.
                for (size_t v = 0; v < fv; v++) {
                    Vertex& vert = vertices[index_offset + v];

                    // access to vertex
                    tinyobj::index_t idx = shapes[s].mesh.indices[index_offset + v];
                    vert.vx = attrib.vertices[3 * size_t(idx.vertex_index) + 0];
                    vert.vy = attrib.vertices[3 * size_t(idx.vertex_index) + 1];
                    vert.vz = attrib.vertices[3 * size_t(idx.vertex_index) + 2];

                    // Check if `normal_index` is zero or positive. negative = no normal data
                    if (idx.normal_index >= 0) {
                        vert.nx = uint8_t(attrib.normals[3 * size_t(idx.normal_index) + 0] * 127.0f + 127.0f);
                        vert.ny = uint8_t(attrib.normals[3 * size_t(idx.normal_index) + 1] * 127.0f + 127.0f);
                        vert.nz = uint8_t(attrib.normals[3 * size_t(idx.normal_index) + 2] * 127.0f + 127.0f);
                    }

                    // Check if `texcoord_index` is zero or positive. negative = no texcoord data
                    if (idx.texcoord_index >= 0) {
                        vert.tu = attrib.texcoords[2 * size_t(idx.texcoord_index) + 0];
                        vert.tv = 1.0 - attrib.texcoords[2 * size_t(idx.texcoord_index) + 1];
                    }

                    // Optional: vertex colors
                    // tinyobj::real_t red   = attrib.colors[3*size_t(idx.vertex_index)+0];
                    // tinyobj::real_t green = attrib.colors[3*size_t(idx.vertex_index)+1];
                    // tinyobj::real_t blue  = attrib.colors[3*size_t(idx.vertex_index)+2];
                    
                    indices[index_offset + v] = idx.vertex_index;
                }
                index_offset += fv;


                // per-face material
                //shapes[s].mesh.material_ids[f];
            }
        }

        // Use meshoptimizer to get an optimized mesh
        std::vector<uint32_t> remap(indexCount);
        size_t vertexCount = meshopt_generateVertexRemap(remap.data(), 0, indexCount, vertices.data(), indexCount, sizeof(Vertex));

        result.vertices.resize(vertexCount);
        result.indices.resize(indexCount);

        meshopt_remapVertexBuffer(result.vertices.data(), vertices.data(), indexCount, sizeof(Vertex), remap.data());
        meshopt_remapIndexBuffer(result.indices.data(), 0, indexCount, remap.data());

        return true;
    }

    void LoadTexture(Texture& tex, const char* path) 
    {
        int texWidth, texHeight, texChannels;
        stbi_uc* pixels = stbi_load(path, &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
        uint32_t imageSize = texWidth * texHeight * 4;

        if (!pixels) {
            throw std::runtime_error("failed to load texture image!");
        }

        tex.pixels = pixels;
        tex.imageWidth = texWidth;
        tex.imageHeight = texHeight;
        tex.imageSize = imageSize;
    }

    struct Buffer
    {
        VkBuffer buffer;
        VkDeviceMemory memory;
        void* data;
        size_t size;
    };

    struct MeshPushConstants
    {
        glm::vec4 data;
        glm::mat4 transformationMatrix;
    };

    struct Image
    {
        VkImage image;
        VkDeviceMemory memory;

    };

    uint32_t SelectMemoryType(const VkPhysicalDeviceMemoryProperties& memProps, uint32_t memoryTypeBits, VkMemoryPropertyFlags flags)
    {
        for (uint32_t i = 0; i < memProps.memoryTypeCount; ++i)
        {
            if ((memoryTypeBits & (1 << i)) != 0 && (memProps.memoryTypes[i].propertyFlags & flags) == flags)
                return i;
        }

        throw std::runtime_error("No compatible memory type found");
        return ~0u;
    }

    void CreateBuffer(Buffer& result, const VkPhysicalDeviceMemoryProperties& memProps, size_t size, VkBufferUsageFlags usage)
    {

        VkBufferCreateInfo createInfo = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
        createInfo.size = size;
        createInfo.usage = usage;
        
        VkBuffer buffer = 0;
        VK_CHECK(vkCreateBuffer(device, &createInfo, 0, &buffer));

        VkMemoryRequirements memoryRequirements;
        vkGetBufferMemoryRequirements(device, buffer, &memoryRequirements);

        uint32_t memoryTypeIndex = SelectMemoryType(memProps, memoryRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

        VkMemoryAllocateInfo allocateInfo = { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
        allocateInfo.allocationSize = memoryRequirements.size;
        allocateInfo.memoryTypeIndex = memoryTypeIndex;

        VkDeviceMemory memory = 0;
        VK_CHECK(vkAllocateMemory(device, &allocateInfo, 0, &memory));

        VK_CHECK(vkBindBufferMemory(device, buffer, memory, 0));

        void* data = 0;
        VK_CHECK(vkMapMemory(device, memory, 0, size, 0, &data));

        result.buffer = buffer;
        result.memory = memory;
        result.data = data;
        result.size = size;
    }

    void CopyStagingBufferToGPU(Buffer& vb, Buffer& stagingBuffer, const VkPhysicalDeviceMemoryProperties& memProps, size_t size, VkBufferUsageFlags usage, VkQueue queue)
    {
        VkBufferCreateInfo createInfo = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
        createInfo.size = size;
        createInfo.usage = usage;

        VkBuffer buffer = 0;
        VK_CHECK(vkCreateBuffer(device, &createInfo, 0, &buffer));

        VkMemoryRequirements memoryRequirements;
        vkGetBufferMemoryRequirements(device, buffer, &memoryRequirements);

        uint32_t memoryTypeIndex = SelectMemoryType(memProps, memoryRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        VkMemoryAllocateInfo allocateInfo = { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
        allocateInfo.allocationSize = memoryRequirements.size;
        allocateInfo.memoryTypeIndex = memoryTypeIndex;

        VkDeviceMemory memory = 0;
        VK_CHECK(vkAllocateMemory(device, &allocateInfo, 0, &memory));

        VK_CHECK(vkBindBufferMemory(device, buffer, memory, 0));

        // submit to queue a copy buffer command
        VkCommandBufferAllocateInfo cbAllocateInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
        cbAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        cbAllocateInfo.commandPool = commandPool;
        cbAllocateInfo.commandBufferCount = 1;

        VkCommandBuffer commandBuffer = 0;
        VK_CHECK(vkAllocateCommandBuffers(device, &cbAllocateInfo, &commandBuffer));

        VkCommandBufferBeginInfo beginInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

        VK_CHECK(vkBeginCommandBuffer(commandBuffer, &beginInfo));

            VkBufferCopy copyRegion = {};
            copyRegion.size = size;
            
            vkCmdCopyBuffer(commandBuffer, stagingBuffer.buffer, buffer, 1, &copyRegion);

        vkEndCommandBuffer(commandBuffer);

        VkSubmitInfo submitInfo = { VK_STRUCTURE_TYPE_SUBMIT_INFO };
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &commandBuffer;

        VK_CHECK(vkQueueSubmit(queue, 1, &submitInfo, 0));
        vkQueueWaitIdle(queue);

        vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);

        vb.buffer = buffer;
        vb.data = stagingBuffer.data;
        vb.size = size;
        vb.memory = memory;
    }

    void DestroyBuffer(const Buffer& buffer)
    {
        vkFreeMemory(device, buffer.memory, 0);
        vkDestroyBuffer(device, buffer.buffer, 0);
    }

    void DestroyImage(const Image& image)
    {
        vkFreeMemory(device, image.memory, 0);
        vkDestroyImage(device, image.image, 0);
    }

    void CreateImage(Image& result, const VkPhysicalDeviceMemoryProperties& memProps, VkFormat format, uint32_t width, uint32_t height, VkImageUsageFlags usage)
    {
        VkImageCreateInfo createInfo = { VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
        createInfo.imageType = VK_IMAGE_TYPE_2D;
        createInfo.format = format;
        createInfo.mipLevels = 1;
        createInfo.arrayLayers = 1;
        createInfo.extent.width = width;
        createInfo.extent.height = height;
        createInfo.extent.depth = 1;
        createInfo.usage = usage;
        createInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        createInfo.tiling = VK_IMAGE_TILING_OPTIMAL;

        VkImage image = 0;
        VK_CHECK(vkCreateImage(device, &createInfo, 0, &image));

        VkMemoryRequirements memoryRequirements;
        vkGetImageMemoryRequirements(device, image, &memoryRequirements);

        uint32_t memoryTypeIndex = SelectMemoryType(memProps, memoryRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        VkMemoryAllocateInfo allocateInfo = { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
        allocateInfo.allocationSize = memoryRequirements.size;
        allocateInfo.memoryTypeIndex = memoryTypeIndex;

        VkDeviceMemory memory = 0;
        VK_CHECK(vkAllocateMemory(device, &allocateInfo, 0, &memory));

        VK_CHECK(vkBindImageMemory(device, image, memory, 0));
   
        result.image = image;
        result.memory = memory;
    }

    void TransitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout, VkQueue queue)
    {
        // submit to queue a copy buffer command
        VkCommandBufferAllocateInfo cbAllocateInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
        cbAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        cbAllocateInfo.commandPool = commandPool;
        cbAllocateInfo.commandBufferCount = 1;

        VkCommandBuffer commandBuffer = 0;
        VK_CHECK(vkAllocateCommandBuffers(device, &cbAllocateInfo, &commandBuffer));

        VkCommandBufferBeginInfo beginInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

        VK_CHECK(vkBeginCommandBuffer(commandBuffer, &beginInfo));

            VkPipelineStageFlags srcStage;
            VkPipelineStageFlags dstStage;

            if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
            {
                srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
                dstStage = VK_PIPELINE_STAGE_TRANSFER_BIT;

                VkImageMemoryBarrier memBarrier = ImageBarrier(image, 0, VK_ACCESS_TRANSFER_WRITE_BIT, oldLayout, newLayout);
                vkCmdPipelineBarrier(commandBuffer, srcStage, dstStage, 0, 0, nullptr, 0, nullptr, 1, &memBarrier);

            }
            else if(oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
            {
                srcStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
                dstStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;

                VkImageMemoryBarrier memBarrier = ImageBarrier(image, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT, oldLayout, newLayout);
                vkCmdPipelineBarrier(commandBuffer, srcStage, dstStage, 0, 0, nullptr, 0, nullptr, 1, &memBarrier);
            }

        vkEndCommandBuffer(commandBuffer);

        VkSubmitInfo submitInfo = { VK_STRUCTURE_TYPE_SUBMIT_INFO };
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &commandBuffer;

        VK_CHECK(vkQueueSubmit(queue, 1, &submitInfo, 0));
        vkQueueWaitIdle(queue);

        vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
    }

    void CopyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height, VkQueue queue)
    {
        // submit to queue a copy buffer command
        VkCommandBufferAllocateInfo cbAllocateInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
        cbAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        cbAllocateInfo.commandPool = commandPool;
        cbAllocateInfo.commandBufferCount = 1;

        VkCommandBuffer commandBuffer = 0;
        VK_CHECK(vkAllocateCommandBuffers(device, &cbAllocateInfo, &commandBuffer));

        VkCommandBufferBeginInfo beginInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

        VK_CHECK(vkBeginCommandBuffer(commandBuffer, &beginInfo));

            VkBufferImageCopy copyRegion = {};
            copyRegion.bufferOffset = 0;
            copyRegion.bufferRowLength = 0;
            copyRegion.bufferImageHeight = 0;

            copyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            copyRegion.imageSubresource.mipLevel = 0;
            copyRegion.imageSubresource.baseArrayLayer = 0;
            copyRegion.imageSubresource.layerCount = 1;

            copyRegion.imageOffset = { 0,0,0 };
            copyRegion.imageExtent = { width, height, 1 };

            vkCmdCopyBufferToImage(commandBuffer, buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);

        vkEndCommandBuffer(commandBuffer);

        VkSubmitInfo submitInfo = { VK_STRUCTURE_TYPE_SUBMIT_INFO };
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &commandBuffer;

        VK_CHECK(vkQueueSubmit(queue, 1, &submitInfo, 0));
        vkQueueWaitIdle(queue);

        vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
    }

    VkSampler CreateTextureSampler()
    {
        VkSamplerCreateInfo samplerInfo = { VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO };
        samplerInfo.magFilter = VK_FILTER_LINEAR;
        samplerInfo.minFilter = VK_FILTER_LINEAR;
        samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.mipLodBias = 0.0f;
        samplerInfo.anisotropyEnable = VK_TRUE;
        samplerInfo.maxAnisotropy = 1.0f;
        samplerInfo.minLod = 0.0f;
        samplerInfo.maxLod = 0.0f;
        samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
        samplerInfo.unnormalizedCoordinates = VK_FALSE;

        VkSampler sampler = 0;
        VK_CHECK(vkCreateSampler(device, &samplerInfo, 0, &sampler));

        return sampler;
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

        int windowWidth;
        int windowHeight;
        glfwGetWindowSize(window, &windowWidth, &windowHeight);

        VkPhysicalDeviceMemoryProperties memoryProperties;
        vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memoryProperties);
        
        // depth stuff
        CreateImage(depthImage, memoryProperties, VK_FORMAT_D32_SFLOAT, windowWidth, windowHeight, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT);
        depthImageView = CreateImageView(depthImage.image, VK_FORMAT_D32_SFLOAT, VK_IMAGE_ASPECT_DEPTH_BIT);

        if (!CreateSwapchain(swapchain, windowWidth, windowHeight, 0))
            throw std::runtime_error("Cannot make a swapchain");

        triangleVS = CreateShader("shaders/mesh.vert.spv");
        triangleFS = CreateShader("shaders/triangle.frag.spv");

        pipelineCache = CreatePipelineCache();
        pipelineLayout = CreatePipelineLayout();
        trianglePipeline = CreateGraphicsPipeline(pipelineCache, triangleVS, triangleFS);
        

        // Buffers
        Buffer stagingVertexbuffer;
        CreateBuffer(stagingVertexbuffer, memoryProperties, 128 * 1024 * 1024, VK_BUFFER_USAGE_TRANSFER_SRC_BIT);

        Buffer ib;
        CreateBuffer(ib, memoryProperties, 128 * 1024 * 1024, VK_BUFFER_USAGE_INDEX_BUFFER_BIT);
        
        Buffer stagingTexture; // staging buffer for a texture image
        CreateBuffer(stagingTexture, memoryProperties, 128 * 1024 * 1024, VK_BUFFER_USAGE_TRANSFER_SRC_BIT);

        
        glm::mat4 view = glm::lookAt(glm::vec3(2.0f, 2.0f, 2.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f));
        glm::mat4 proj = glm::perspective(glm::radians(45.0f), windowWidth / (float)windowHeight, 0.1f, 10.0f);

        MeshPushConstants constants;

        Mesh bunny;
        LoadMesh(bunny, "mesh/viking_room.obj");

        Texture tex;
        LoadTexture(tex, "mesh/viking_room.png");

        memcpy(stagingVertexbuffer.data, bunny.vertices.data(), bunny.vertices.size() * sizeof(Vertex));
        memcpy(ib.data, bunny.indices.data(), bunny.indices.size() * sizeof(uint32_t));
        memcpy(stagingTexture.data, tex.pixels, tex.imageSize);

        Buffer vb;
        CopyStagingBufferToGPU(vb, stagingVertexbuffer, memoryProperties, 128 * 1024 * 1024, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, queue);

        Image t;
        CreateImage(t, memoryProperties, VK_FORMAT_R8G8B8A8_UNORM, tex.imageWidth, tex.imageHeight, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT);
        TransitionImageLayout(t.image, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, queue);
        CopyBufferToImage(stagingTexture.buffer, t.image, tex.imageWidth, tex.imageHeight, queue);
        TransitionImageLayout(t.image, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, queue);
        VkImageView textureImageView = CreateImageView(t.image, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_ASPECT_COLOR_BIT);
        VkSampler textureSampler = CreateTextureSampler();
        
        
        float angle = 0.0f;

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

            angle += 0.1f;
            if (angle > 360.0f) angle -= 360.0f;

            glm::mat4 model = glm::rotate(glm::mat4(1.0f), glm::radians(angle), glm::vec3(0.0f, 0.0f, 1.0f));
            constants.transformationMatrix = proj * view * model;

            //upload the matrix to the GPU via push constants
            vkCmdPushConstants(commandBuffer, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(MeshPushConstants), &constants);

            VkImageMemoryBarrier renderBeginBarrier = ImageBarrier(swapchain.images[imageIndex], 0, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
            vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_DEPENDENCY_BY_REGION_BIT, 0, 0, 0, 0, 1, &renderBeginBarrier);


            VkClearColorValue color = { 48.f / 256.f, 10.f / 256.f, 36.f / 256.f, 1.0f };
            VkClearValue clearColor = { color };
            VkClearValue depthClear = { 1.0f, 0 };

            std::array<VkClearValue, 2> clearValues = { clearColor, depthClear };

            VkRenderPassBeginInfo passBeginInfo = { VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO };
            passBeginInfo.renderPass = renderPass;
            passBeginInfo.framebuffer = swapchain.framebuffers[imageIndex];
            passBeginInfo.renderArea.extent.width = swapchain.width;
            passBeginInfo.renderArea.extent.height = swapchain.height;
            passBeginInfo.clearValueCount = static_cast<uint32_t>( clearValues.size());
            passBeginInfo.pClearValues = clearValues.data();

            vkCmdBeginRenderPass(commandBuffer, &passBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

            // -height flips the viewport because vulkan has a weird coordinate system
            VkViewport viewport = { 0, float(swapchain.height), float(swapchain.width), -float(swapchain.height), 0, 1 };
            VkRect2D scissor = { {0, 0}, {swapchain.width, swapchain.height} };

            vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
            vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

            vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, trianglePipeline);

            VkDescriptorBufferInfo vBufferInfo;
            vBufferInfo.buffer = vb.buffer;
            vBufferInfo.offset = 0;
            vBufferInfo.range = vb.size;

            VkDescriptorImageInfo texInfo;
            texInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            texInfo.imageView = textureImageView;
            texInfo.sampler = textureSampler;

            std::vector<VkWriteDescriptorSet> writeDescriptors(2);
            writeDescriptors[0].sType = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
            writeDescriptors[0].dstBinding = 0;
            writeDescriptors[0].descriptorCount = 1;
            writeDescriptors[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            writeDescriptors[0].pBufferInfo = &vBufferInfo;

            writeDescriptors[1].sType = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
            writeDescriptors[1].dstBinding = 1;
            writeDescriptors[1].descriptorCount = 1;
            writeDescriptors[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            writeDescriptors[1].pImageInfo = &texInfo;

            vkCmdPushDescriptorSetKHR(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, uint32_t(writeDescriptors.size()), writeDescriptors.data());
#if 0
            VkBuffer vertexBuffers[] = { vb.buffer };
            VkDeviceSize offsets[] = { 0 };

            vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);
#endif
            vkCmdBindIndexBuffer(commandBuffer, ib.buffer, 0, VK_INDEX_TYPE_UINT32);
            vkCmdDrawIndexed(commandBuffer, uint32_t(bunny.indices.size()), 1, 0, 0, 0);

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

        vkDestroyImageView(device, textureImageView, 0);
        vkDestroySampler(device, textureSampler, 0);

        DestroyImage(t);
        DestroyImage(depthImage);
        vkDestroyImageView(device, depthImageView, 0);

        DestroyBuffer(vb);
        DestroyBuffer(ib);
        DestroyBuffer(stagingTexture);
        DestroyBuffer(stagingVertexbuffer);

        stbi_image_free(tex.pixels);
    }

    void Cleanup()
    {

        vkDestroyPipeline(device, trianglePipeline, 0);

        vkDestroyPipelineCache(device, pipelineCache, 0);
        vkDestroyDescriptorSetLayout(device, descriptorSetLayout, 0);
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
    VkDescriptorSetLayout descriptorSetLayout;
    VkPipeline trianglePipeline;
    VkFormat swapchainFormat;
    VkDebugReportCallbackEXT debugMessenger;

    uint32_t queueFamilyIndex;

    Image depthImage;
    VkImageView depthImageView;
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