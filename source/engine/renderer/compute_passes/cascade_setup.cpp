#include "cascade_setup.h"
#include "../renderer.h"
using namespace engine;

#define WORK_TILE_SIZE 16

static AutoCVarInt32 cVarFixCascade(
	"r.Shadow.FixCascade",
	"Enable fix cascade. 0 is off, others are on.",
	"Shadow",
	1,
	CVarFlags::ReadAndWrite
);

struct GpuCascadeSetupPushConstant
{
	uint32 reverseZ;
	float shadowMapSize;
	uint32 bFixCascade;
};

void engine::GpuCascadeSetupPass::initInner()
{
	bInitPipeline = false;
	createPipeline();
	m_deletionQueue.push([&]()
	{
		destroyPipeline();
	});
}

void engine::GpuCascadeSetupPass::beforeSceneTextureRecreate()
{
	destroyPipeline();
}

void engine::GpuCascadeSetupPass::afterSceneTextureRecreate()
{
	createPipeline();
}

void engine::GpuCascadeSetupPass::record(VkCommandBuffer& cmd,uint32 backBufferIndex)
{
	if(m_renderScene->isSceneEmpty())
	{
		return;
	}

	GpuCascadeSetupPushConstant gpuPushConstant = {}; 
	static auto* cVarShadowMapSize = CVarSystem::get()->getInt32CVar("r.Shadow.ShadowMapSize");
	CHECK(cVarShadowMapSize);
	gpuPushConstant.reverseZ = reverseZOpen();
	gpuPushConstant.bFixCascade = cVarFixCascade.get() != 0;
	gpuPushConstant.shadowMapSize = float(*cVarShadowMapSize);

	std::array<VkBufferMemoryBarrier,2> bufferBarriers {};
	bufferBarriers[0].sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
	bufferBarriers[1].sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;

	VkDeviceSize asyncRange0 =  m_renderScene->m_evaluateDepthMinMax.size;
	bufferBarriers[0].buffer = m_renderScene->m_evaluateDepthMinMax.buffer->GetVkBuffer();
	bufferBarriers[0].size = asyncRange0;
	bufferBarriers[0].srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
	bufferBarriers[0].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

	VkDeviceSize asyncRange1 = m_renderScene->m_cascadeSetupBuffer.size;
	bufferBarriers[1].buffer = m_renderScene->m_cascadeSetupBuffer.buffer->GetVkBuffer();
	bufferBarriers[1].size = asyncRange1;
	bufferBarriers[1].srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
	bufferBarriers[1].dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;

	vkCmdPipelineBarrier(
		cmd,
		VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
		VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
		0,
		0,nullptr,
		(uint32)bufferBarriers.size(),bufferBarriers.data(),
		0,nullptr);

	vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, m_pipelines[backBufferIndex]);

	vkCmdPushConstants(cmd, m_pipelineLayouts[backBufferIndex], 
		VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(GpuCascadeSetupPushConstant), &gpuPushConstant);

	std::vector<VkDescriptorSet> compPassSets = {
		  m_renderer->getFrameData().m_frameDataDescriptorSets[backBufferIndex].set
		, m_renderer->getRenderScene().m_evaluateDepthMinMax.descriptorSets.set
		, m_renderer->getRenderScene().m_cascadeSetupBuffer.descriptorSets.set
	};

	vkCmdBindDescriptorSets(
		cmd,
		VK_PIPELINE_BIND_POINT_COMPUTE, 
		m_pipelineLayouts[backBufferIndex], 
		0, 
		(uint32)compPassSets.size(), 
		compPassSets.data(), 
		0, 
		nullptr
	);

	vkCmdDispatch(cmd, 1, 1, 1);

	bufferBarriers[0].srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
	bufferBarriers[0].dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
	bufferBarriers[0].buffer = m_renderScene->m_evaluateDepthMinMax.buffer->GetVkBuffer();
	bufferBarriers[0].size = asyncRange0;

	bufferBarriers[1].buffer = m_renderScene->m_cascadeSetupBuffer.buffer->GetVkBuffer();
	bufferBarriers[1].size = asyncRange1;
	bufferBarriers[1].srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
	bufferBarriers[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

	vkCmdPipelineBarrier(
		cmd,
		VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
		VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
		0,
		0, nullptr,
		(uint32)bufferBarriers.size(),bufferBarriers.data(),
		0, nullptr);
}

void engine::GpuCascadeSetupPass::barrierUseStart()
{
}

void engine::GpuCascadeSetupPass::barrierUseEnd()
{
}

void engine::GpuCascadeSetupPass::createPipeline()
{
	if(bInitPipeline) return;

	uint32 backBufferCount = (uint32)VulkanRHI::get()->getSwapchainImageViews().size();

	m_pipelines.resize(backBufferCount);
	m_pipelineLayouts.resize(backBufferCount);
	for(uint32 index = 0; index<backBufferCount; index++)
	{
		VkPipelineLayoutCreateInfo plci = vkPipelineLayoutCreateInfo();

		VkPushConstantRange push_constant{};
		push_constant.offset = 0;
		push_constant.size = sizeof(GpuCascadeSetupPushConstant);
		push_constant.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

		plci.pPushConstantRanges = &push_constant;
		plci.pushConstantRangeCount = 1; // NOTE: 我们有一个PushConstant

		std::vector<VkDescriptorSetLayout> setLayouts = {
			  m_renderer->getFrameData().m_frameDataDescriptorSetLayouts[index].layout
			, m_renderer->getRenderScene().m_evaluateDepthMinMax.descriptorSetLayout.layout
			, m_renderer->getRenderScene().m_cascadeSetupBuffer.descriptorSetLayout.layout
		};

		plci.setLayoutCount = (uint32)setLayouts.size();
		plci.pSetLayouts = setLayouts.data();

		m_pipelineLayouts[index] = VulkanRHI::get()->createPipelineLayout(plci);

		auto* shaderModule = VulkanRHI::get()->getShader("media/shader/fallback/bin/cascadeShadowSetup.comp.spv");
		VkPipelineShaderStageCreateInfo shaderStageCI{};
		shaderStageCI.module = shaderModule->GetModule();
		shaderStageCI.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		shaderStageCI.stage = VK_SHADER_STAGE_COMPUTE_BIT;
		shaderStageCI.pName = "main";

		VkComputePipelineCreateInfo computePipelineCreateInfo{};
		computePipelineCreateInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
		computePipelineCreateInfo.layout = m_pipelineLayouts[index];
		computePipelineCreateInfo.flags = 0;
		computePipelineCreateInfo.stage = shaderStageCI;
		vkCheck(vkCreateComputePipelines(VulkanRHI::get()->getDevice(),nullptr,1,&computePipelineCreateInfo,nullptr,&m_pipelines[index]));
	}

	bInitPipeline = true;
}

void engine::GpuCascadeSetupPass::destroyPipeline()
{
	if(!bInitPipeline) return;

	for(uint32 index = 0; index<m_pipelines.size(); index++)
	{
		vkDestroyPipeline(VulkanRHI::get()->getDevice(),m_pipelines[index],nullptr);
		vkDestroyPipelineLayout(VulkanRHI::get()->getDevice(),m_pipelineLayouts[index],nullptr);
	}
	m_pipelines.resize(0);
	m_pipelineLayouts.resize(0);

	bInitPipeline = false;
}
