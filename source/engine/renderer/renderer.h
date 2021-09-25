#pragma once
#include "../core/runtime_module.h"
#include "imgui_pass.h"
#include "render_scene.h"
#include "../shader_compiler/shader_compiler.h"
#include "frame_data.h"


namespace engine{

class GraphicsPass;
class MaterialLibrary;
class Renderer : public IRuntimeModule
{
public:
	Renderer(Ref<ModuleManager>);
	
public:
	typedef void (RegisterImguiFunc)(size_t);

	virtual bool init() override;
	virtual void tick(float dt) override;
	virtual void release() override;

	void addImguiFunction(std::string name,std::function<RegisterImguiFunc>&& func)
	{
		this->m_uiFunctions[name] = func; 
	}
	void removeImguiFunction(std::string name)
	{
		this->m_uiFunctions.erase(name);
	}

	void UpdateScreenSize(uint32 width,uint32 height);
	RenderScene& getRenderScene() { return *m_renderScene; }
	PerFrameData& getFrameData() { return m_frameData; }

private:
	ImguiPass* m_uiPass;
	RenderScene* m_renderScene;
	PerFrameData m_frameData { };
	std::vector<GraphicsPass*> m_renderpasses {};

	// 动态描述符申请，每次ScreenSize改变时重置。
	std::vector<VulkanDescriptorAllocator*> m_dynamicDescriptorAllocator{};

private:
	void uiRecord(size_t i);

	std::unordered_map<std::string,std::function<RegisterImguiFunc>> m_uiFunctions{};
	bool show_demo_window = true;
	bool show_another_window = false;

	uint32 m_screenViewportWidth = ScreenTextureInitSize;
	uint32 m_screenViewportHeight = ScreenTextureInitSize;

	GPUFrameData m_gpuFrameData { };
	GPUViewData m_gpuViewData { };

	void updateGPUData();

	MaterialLibrary* m_materialLibrary;

public:
	
	MaterialLibrary* getMaterialLibrary() { return m_materialLibrary; }
	VulkanDescriptorAllocator& getDynamicDescriptorAllocator(uint32 i) { return *m_dynamicDescriptorAllocator[i]; }
	VulkanDescriptorFactory vkDynamicDescriptorFactoryBegin(uint32 i) 
	{ 
		return VulkanDescriptorFactory::begin(&VulkanRHI::get()->getDescriptorLayoutCache(), m_dynamicDescriptorAllocator[i]); 
	}
};

}