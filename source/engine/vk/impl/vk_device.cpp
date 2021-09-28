#include "vk_device.h"
#include <set>

namespace engine{

void VulkanDevice::init(
	VkInstance instance,
	VkSurfaceKHR surface,
	VkPhysicalDeviceFeatures features,
	const std::vector<const char*>& device_request_extens)
{
	this->m_instance = instance;
	this->m_surface = surface;

	// 1. ѡ����ʵ�Gpu
	pickupSuitableGpu(instance,device_request_extens);

	// 2. �����߼��豸
	this->m_deviceExtensions = device_request_extens;
	this->m_openFeatures = features;
	createLogicDevice();
}

void VulkanDevice::destroy()
{
	if (device != VK_NULL_HANDLE)
	{
		vkDestroyDevice(device, nullptr);
	}
}

void VulkanDevice::createLogicDevice()
{
	// 1. ָ��Ҫ�����Ķ���
	auto indices = findQueueFamilies();

	std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
	std::set<uint32_t> uniqueQueueFamilies = { indices.graphicsFamily, indices.presentFamily,indices.computeFaimly };

	// ����graphicsFamily presentFamily computeFaimly��Ӧ���� 
	float queuePriority = 1.0f;
	for (uint32_t queueFamily : uniqueQueueFamilies) 
	{
		VkDeviceQueueCreateInfo queueCreateInfo{};
		queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		queueCreateInfo.queueFamilyIndex = queueFamily;
		queueCreateInfo.queueCount = 1;
		queueCreateInfo.pQueuePriorities = &queuePriority;
		queueCreateInfos.push_back(queueCreateInfo);
	}

	// 2. ��ʼ��д�����ṹ����Ϣ
	VkDeviceCreateInfo createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	createInfo.pQueueCreateInfos = queueCreateInfos.data(); // �����������
	createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
	createInfo.pEnabledFeatures = &m_openFeatures;

	// 3. �����豸��Ҫ����չ
	createInfo.enabledExtensionCount = static_cast<uint32_t>(m_deviceExtensions.size());
	createInfo.ppEnabledExtensionNames = m_deviceExtensions.data();

	// û���ض����豸�Ĳ㣬���еĲ㶼������ʵ������
	createInfo.ppEnabledLayerNames = NULL;
	createInfo.enabledLayerCount = 0;

	if (vkCreateDevice(physicalDevice, &createInfo, nullptr, &device) != VK_SUCCESS) 
	{
		LOG_GRAPHICS_FATAL("Create vulkan logic device.");
	}

	// ��ȡ����
	vkGetDeviceQueue(device,indices.graphicsFamily,0,&graphicsQueue);
	vkGetDeviceQueue(device,indices.presentFamily, 0,&presentQueue);
	vkGetDeviceQueue(device,indices.computeFaimly, 0,&computeQueue);
}

VkFormat VulkanDevice::findSupportedFormat(const std::vector<VkFormat>& candidates,VkImageTiling tiling,VkFormatFeatureFlags features)
{
	for (VkFormat format : candidates) 
	{
		VkFormatProperties props;
		vkGetPhysicalDeviceFormatProperties(physicalDevice, format, &props);

		if (tiling == VK_IMAGE_TILING_LINEAR && (props.linearTilingFeatures & features) == features) 
		{
			return format;
		} 
		else if (tiling == VK_IMAGE_TILING_OPTIMAL && (props.optimalTilingFeatures & features) == features) 
		{
			return format;
		}
	}

	LOG_GRAPHICS_FATAL("Can't find supported format.");
}

VkFormat VulkanDevice::findDepthStencilFormat()
{
	return findSupportedFormat
	(
		{ VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT },
		VK_IMAGE_TILING_OPTIMAL,
		VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT
	);
}

VkFormat VulkanDevice::findDepthOnlyFormat()
{
	return findSupportedFormat
	(
		{ VK_FORMAT_D32_SFLOAT },
		VK_IMAGE_TILING_OPTIMAL,
		VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT
	);
}

void VulkanDevice::pickupSuitableGpu(VkInstance& instance,const std::vector<const char*>& device_request_extens)
{
	// 1. ��ѯ���е�Gpu
	uint32_t physical_device_count{0};
	vkCheck(vkEnumeratePhysicalDevices(instance, &physical_device_count, nullptr));
	if (physical_device_count < 1)
	{
		LOG_GRAPHICS_FATAL("No gpu support vulkan on your computer.");
	}
	std::vector<VkPhysicalDevice> physical_devices;
	physical_devices.resize(physical_device_count);
	vkCheck(vkEnumeratePhysicalDevices(instance, &physical_device_count, physical_devices.data()));

	// 2. ѡ���һ������Gpu
	ASSERT(!physical_devices.empty(),"No gpu on your computer.");

	// �ҵ�һ������Gpu
	for (auto &gpu : physical_devices)
	{
		VkPhysicalDeviceProperties device_properties;
		vkGetPhysicalDeviceProperties(gpu, &device_properties);
		if (device_properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
		{
			this->physicalDevice = gpu;
			if (isPhysicalDeviceSuitable(device_request_extens)) 
			{
				LOG_GRAPHICS_INFO("Using discrete gpu: {0}.",vkToString(device_properties.deviceName));
				physicalDeviceProperties = device_properties;
				return;
			}
		}
	}

	// ����ֱ�ӷ��ص�һ��gpu
	LOG_GRAPHICS_WARN("No discrete gpu found, using default gpu.");

	this->physicalDevice = physical_devices.at(0);
	if (isPhysicalDeviceSuitable(device_request_extens)) 
	{
		this->physicalDevice = physical_devices.at(0);
		VkPhysicalDeviceProperties device_properties;
		vkGetPhysicalDeviceProperties(this->physicalDevice,&device_properties);
		LOG_GRAPHICS_INFO("Choose default gpu: {0}.",vkToString(device_properties.deviceName));
		physicalDeviceProperties = device_properties;
		return;
	}
	else
	{
		LOG_GRAPHICS_FATAL("No suitable gpu can use.");
	}
}

bool VulkanDevice::isPhysicalDeviceSuitable(const std::vector<const char*>& device_request_extens)
{
	// �������еĶ�����
	VulkanQueueFamilyIndices indices = findQueueFamilies();

	// ֧�����е��豸���
	bool extensionsSupported = checkDeviceExtensionSupport(device_request_extens);

	// ֧�ֽ�������ʽ��Ϊ��
	bool swapChainAdequate = false;
	if (extensionsSupported) 
	{
		auto swapChainSupport = querySwapchainSupportDetail();
		swapChainAdequate = !swapChainSupport.formats.empty() && !swapChainSupport.presentModes.empty();
	}

	return indices.isCompleted() && extensionsSupported && swapChainAdequate;
}

VulkanQueueFamilyIndices VulkanDevice::findQueueFamilies()
{
	VulkanQueueFamilyIndices indices;

	// 1. �ҵ����еĶ�����
	uint32_t queueFamilyCount = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(this->physicalDevice, &queueFamilyCount, nullptr);
	std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
	vkGetPhysicalDeviceQueueFamilyProperties(this->physicalDevice, &queueFamilyCount, queueFamilies.data());

	// 2. �ҵ�֧������Ķ�����
	int i = 0;
	for (const auto& queueFamily : queueFamilies) 
	{
		if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) 
		{
			indices.graphicsFamily = i;
			indices.bGraphicsFamilySet = true;
		}

		if (queueFamily.queueFlags & VK_QUEUE_COMPUTE_BIT) 
		{
			indices.computeFaimly = i;
			indices.bComputeFaimlySet = true;
		}

		VkBool32 presentSupport = false;
		vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, i, m_surface, &presentSupport);
		if (presentSupport) 
		{
			indices.presentFamily = i;
			indices.bPresentFamilySet = true;
		}

		if (indices.isCompleted()) {
			break;
		}

		i++;
	}

	return indices;
}

VulkanSwapchainSupportDetails VulkanDevice::querySwapchainSupportDetail()
{
	// ��ѯ�������湦��
	VulkanSwapchainSupportDetails details;
	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, m_surface, &details.capabilities);

	// ��ѯ�����ʽ
	uint32_t formatCount;
	vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice,m_surface, &formatCount, nullptr);
	if (formatCount != 0) 
	{
		details.formats.resize(formatCount);
		vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, m_surface, &formatCount, details.formats.data());
	}

	// 3. ��ѯչʾ��ʽ
	uint32_t presentModeCount;
	vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, m_surface, &presentModeCount, nullptr);
	if (presentModeCount != 0) 
	{
		details.presentModes.resize(presentModeCount);
		vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, m_surface, &presentModeCount, details.presentModes.data());
	}

	return details;
}

uint32 VulkanDevice::findMemoryType(uint32 typeFilter,VkMemoryPropertyFlags properties)
{
	VkPhysicalDeviceMemoryProperties memProperties;
	vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);

	for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) 
	{
		if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) 
		{
			return i;
		}
	}

	LOG_GRAPHICS_FATAL("No suitable memory type can find.");
}

void VulkanDevice::printAllQueueFamiliesInfo()
{
	// 1. �ҵ����еĶ�����
	uint32_t queueFamilyCount = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, nullptr);
	std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
	vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, queueFamilies.data());

	for(auto &q_family : queueFamilies)
	{
		LOG_GRAPHICS_INFO("queue id: {}." ,  vkToString(q_family.queueCount));
		LOG_GRAPHICS_INFO("queue flag: {}.", vkToString(q_family.queueFlags));
	}
}

bool VulkanDevice::checkDeviceExtensionSupport(const std::vector<const char*>& device_request_extens)
{
	uint32_t extensionCount;
	vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extensionCount, nullptr);

	std::vector<VkExtensionProperties> availableExtensions(extensionCount);
	vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extensionCount, availableExtensions.data());

	std::set<std::string> requiredExtensions(device_request_extens.begin(), device_request_extens.end());

	for (const auto& extension : availableExtensions) 
	{
		requiredExtensions.erase(extension.extensionName);
	}

	return requiredExtensions.empty();
}

}