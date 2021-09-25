#pragma once
#include "../../vk/vk_rhi.h"
#include "../render_scene.h"
#include "../../shader_compiler/shader_compiler.h"

namespace engine{
class Renderer;
class GraphicsPass
{
public:
	GraphicsPass() = delete;
	GraphicsPass(Ref<Renderer> renderer,Ref<RenderScene> scene,Ref<shaderCompiler::ShaderCompiler> sc,const std::string& name,shaderCompiler::EShaderPass passType)
	: m_renderer(renderer), m_renderScene(scene),m_passName(name),m_passType(passType),m_shaderCompiler(sc) { }

	virtual ~GraphicsPass(){ release(); }

	void init();
	void release();
	

	VkRenderPass getRenderpass() { return m_renderpass; }
	virtual void dynamicRecord(VkCommandBuffer& cmd,uint32 backBufferIndex) = 0;

private:
	std::string m_passName;
	shaderCompiler::EShaderPass m_passType;

protected:
	VkRenderPass m_renderpass;

	Ref<shaderCompiler::ShaderCompiler> m_shaderCompiler;
	Ref<Renderer> m_renderer;
	Ref<RenderScene> m_renderScene;
	DeletionQueue m_deletionQueue = {};
	
	// 在交换链重建前需要清理renderpass资源。
	virtual void beforeSceneTextureRecreate() = 0;

	// 在交换链重建后需要创建renderpass资源。
	virtual void afterSceneTextureRecreate() = 0;

	virtual void initInner() = 0;
public:
	bool isPassType(shaderCompiler::EShaderPass type) { return m_passType == type; }
};

}