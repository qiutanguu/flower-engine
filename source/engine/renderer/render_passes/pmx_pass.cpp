#include "pmx_pass.h"
#include "../../launch/launch_engine_loop.h"
#include "../mesh.h"
#include "../material.h"
#include "../renderer.h"
#include "../texture.h"
#include "../pmx_mesh.h"
#include "../../scene/components/pmx_mesh_component.h"

using namespace engine;

void engine::PMXPass::initInner()
{
    createRenderpass();
    createFramebuffers();
    bInitPipeline = false;
    createPipeline();

    m_deletionQueue.push([&]()
    {
        destroyRenderpass();
        destroyFramebuffers();
        destroyPipeline();
    });
}

void engine::PMXPass::beforeSceneTextureRecreate()
{
    destroyFramebuffers();
    destroyPipeline();
}

void engine::PMXPass::afterSceneTextureRecreate()
{
    createFramebuffers();
    createPipeline();
}

void engine::PMXPass::dynamicRecord(uint32 backBufferIndex)
{
    VkCommandBuffer cmd = m_commandbufs[backBufferIndex]->getInstance();
    commandBufBegin(backBufferIndex);

    auto sceneTextureExtent = m_renderScene->getSceneTextures().getHDRSceneColor()->getExtent();
    VkExtent2D sceneTextureExtent2D{};
    sceneTextureExtent2D.width = sceneTextureExtent.width;
    sceneTextureExtent2D.height = sceneTextureExtent.height;

    VkRenderPassBeginInfo rpInfo = vkRenderpassBeginInfo(getRenderpass(),sceneTextureExtent2D,m_framebuffers[backBufferIndex]);
    std::array<VkClearValue,3> clearValues{};

    clearValues[0].color = { { 0.0f, 0.0f, 0.0f, 0.0f } };
    clearValues[1].color = { { 0.0f, 0.0f, 0.0f, 0.0f } };
    clearValues[2].depthStencil = { getEngineClearZFar(), 1 };

    rpInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
    rpInfo.pClearValues = &clearValues[0];

    VkRect2D scissor{};
    scissor.extent = sceneTextureExtent2D;
    scissor.offset = {0,0};

    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = (float)sceneTextureExtent2D.height; // flip y on mesh raster
    viewport.width = (float)sceneTextureExtent2D.width;
    viewport.height = -(float)sceneTextureExtent2D.height; // flip y on mesh raster
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    vkCmdBeginRenderPass(cmd,&rpInfo,VK_SUBPASS_CONTENTS_INLINE);
    vkCmdSetScissor(cmd,0,1,&scissor);
    vkCmdSetViewport(cmd,0,1,&viewport);
    vkCmdSetDepthBias(cmd,0,0,0);

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelines[backBufferIndex]);

    // prepare descriptor set.
    std::vector<VkDescriptorSet> meshPassSets = {
          m_renderer->getFrameData().m_frameDataDescriptorSets[backBufferIndex].set
        , TextureLibrary::get()->getBindlessTextureDescriptorSet()
        , m_renderer->getRenderScene().m_meshObjectSSBO->descriptorSets.set
        , m_renderer->getRenderScene().m_meshMaterialSSBO->descriptorSets.set
        , m_renderer->getRenderScene().m_drawIndirectSSBOGbuffer.descriptorSets.set
    };
    vkCmdBindDescriptorSets(
        cmd,VK_PIPELINE_BIND_POINT_GRAPHICS,
        m_pipelineLayouts[backBufferIndex],
        0,
        (uint32)meshPassSets.size(),
        meshPassSets.data(),
        0,
        nullptr
    );

    // draw pmx mesh.
    for(auto pmxWeakPtr : m_renderScene->m_cachePMXMeshComponents)
    {
        if(auto pmxComp = pmxWeakPtr.lock())
        {
            pmxComp->OnRenderCollect(cmd, m_pipelineLayouts[backBufferIndex]);
        }
    }

    vkCmdEndRenderPass(cmd);

    commandBufEnd(backBufferIndex);
}

void engine::PMXPass::createRenderpass()
{
    const size_t attachmentCount = 3;
    const size_t depthAttachmentIndex = attachmentCount - 1;
    std::array<VkAttachmentDescription,attachmentCount> attachmentDescs{};
    for(size_t i = 0; i < depthAttachmentIndex; i++)
    {
        attachmentDescs[i] = {};
        attachmentDescs[i].samples = VK_SAMPLE_COUNT_1_BIT;

        // Load and store.
        attachmentDescs[i].loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
        attachmentDescs[i].storeOp = VK_ATTACHMENT_STORE_OP_STORE;

        attachmentDescs[i].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        attachmentDescs[i].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;

        attachmentDescs[i].initialLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    }

    attachmentDescs[0].format = SceneTextures::getHDRSceneColorFormat();
    attachmentDescs[1].format = SceneTextures::getVelocityFormat();
    attachmentDescs[depthAttachmentIndex].format = SceneTextures::getDepthStencilFormat();

    attachmentDescs[0].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    attachmentDescs[1].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    // load and store.
    attachmentDescs[depthAttachmentIndex].loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
    attachmentDescs[depthAttachmentIndex].storeOp = VK_ATTACHMENT_STORE_OP_STORE;

    attachmentDescs[depthAttachmentIndex].samples = VK_SAMPLE_COUNT_1_BIT;
    attachmentDescs[depthAttachmentIndex].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachmentDescs[depthAttachmentIndex].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachmentDescs[depthAttachmentIndex].initialLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL; 
    attachmentDescs[depthAttachmentIndex].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    std::vector<VkAttachmentReference> colorReferences;
    colorReferences.push_back({ 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL });
    colorReferences.push_back({ 1, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL });

    VkAttachmentReference depthReference = {};
    depthReference.attachment = depthAttachmentIndex; // depth�������һ��
    depthReference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass = {};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = static_cast<uint32_t>(colorReferences.size());
    subpass.pColorAttachments = colorReferences.data();
    subpass.pDepthStencilAttachment = &depthReference;

    std::array<VkSubpassDependency, 2> dependencies;
    dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
    dependencies[0].dstSubpass = 0;
    dependencies[0].srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    dependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependencies[0].srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
    dependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

    dependencies[1].srcSubpass = 0;
    dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
    dependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependencies[1].dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    dependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    dependencies[1].dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
    dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

    VkRenderPassCreateInfo renderPassInfo = {};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = static_cast<uint32_t>(attachmentDescs.size());
    renderPassInfo.pAttachments = attachmentDescs.data();
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;
    renderPassInfo.dependencyCount = 2;
    renderPassInfo.pDependencies = dependencies.data();

    m_renderpass = VulkanRHI::get()->createRenderpass(renderPassInfo);
}

void engine::PMXPass::createFramebuffers()
{
    auto extent = m_renderScene->getSceneTextures().getHDRSceneColor()->getExtent();

    VkExtent2D extent2D{};
    extent2D.width = extent.width;
    extent2D.height = extent.height;

    VulkanFrameBufferFactory fbf {};
    const uint32 swapchain_imagecount = (uint32)VulkanRHI::get()->getSwapchainImages().size();
    m_framebuffers.resize(swapchain_imagecount);

    for(uint32 i = 0; i < swapchain_imagecount; i++)
    {
        fbf.setRenderpass(m_renderpass)
            .addAttachment(m_renderScene->getSceneTextures().getHDRSceneColor())
            .addAttachment(m_renderScene->getSceneTextures().getVelocity())
            .addAttachment(m_renderScene->getSceneTextures().getDepthStencil());
        m_framebuffers[i] = fbf.create(VulkanRHI::get()->getDevice());
    }
}


void engine::PMXPass::createPipeline()
{
    if(bInitPipeline) return;

    uint32 backBufferCount = (uint32)VulkanRHI::get()->getSwapchainImageViews().size();

    m_pipelines.resize(backBufferCount);
    m_pipelineLayouts.resize(backBufferCount);
    for(uint32 index = 0; index < backBufferCount; index++)
    {
        VkPipelineLayoutCreateInfo plci = vkPipelineLayoutCreateInfo();

        std::array<VkPushConstantRange,1> push_constants{};
        push_constants[0].offset = 0;
        push_constants[0].size = sizeof(PMXGpuPushConstants);
        push_constants[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

        plci.pPushConstantRanges = push_constants.data();
        plci.pushConstantRangeCount = uint32_t(push_constants.size()); // NOTE: ������һ��PushConstant

        std::vector<VkDescriptorSetLayout> setLayouts = {
              m_renderer->getFrameData().m_frameDataDescriptorSetLayouts[index].layout
            , TextureLibrary::get()->getBindlessTextureDescriptorSetLayout()
            , m_renderer->getRenderScene().m_meshObjectSSBO->descriptorSetLayout.layout
            , m_renderer->getRenderScene().m_meshMaterialSSBO->descriptorSetLayout.layout
            , m_renderer->getRenderScene().m_drawIndirectSSBOGbuffer.descriptorSetLayout.layout
        };

        plci.setLayoutCount = (uint32)setLayouts.size();
        plci.pSetLayouts = setLayouts.data();
        m_pipelineLayouts[index] = VulkanRHI::get()->createPipelineLayout(plci);

        VulkanGraphicsPipelineFactory gpf = {};

        auto* vertShader = VulkanRHI::get()->getShader("media/shader/fallback/bin/pmx_pass.vert.spv");
        auto* fragShader = VulkanRHI::get()->getShader("media/shader/fallback/bin/pmx_pass.frag.spv");

        gpf.shaderStages.clear();
        gpf.shaderStages.push_back(vkPipelineShaderStageCreateInfo(VK_SHADER_STAGE_VERTEX_BIT,   *vertShader));
        gpf.shaderStages.push_back(vkPipelineShaderStageCreateInfo(VK_SHADER_STAGE_FRAGMENT_BIT, *fragShader));

        VulkanVertexInputDescription vvid = {};
        vvid.bindings   =  { VulkanVertexBuffer::getInputBinding(getPMXMeshAttributes()) };
        vvid.attributes = VulkanVertexBuffer::getInputAttribute(getPMXMeshAttributes());

        gpf.vertexInputDescription = vvid;
        gpf.inputAssembly = vkInputAssemblyCreateInfo(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
        gpf.rasterizer = vkRasterizationStateCreateInfo(VK_POLYGON_MODE_FILL);
        gpf.rasterizer.cullMode = VK_CULL_MODE_NONE;
        gpf.multisampling = vkMultisamplingStateCreateInfo();

        // two color blend state.
        gpf.colorBlendAttachments.push_back(vkColorBlendAttachmentState());
        gpf.colorBlendAttachments.push_back(vkColorBlendAttachmentState());

        gpf.depthStencil = vkDepthStencilCreateInfo(true,true,getEngineZTestFunc());
        gpf.pipelineLayout =  m_pipelineLayouts[index];

        auto sceneTextureExtent = m_renderScene->getSceneTextures().getHDRSceneColor()->getExtent();
        VkExtent2D sceneTextureExtent2D{};
        sceneTextureExtent2D.width = sceneTextureExtent.width;
        sceneTextureExtent2D.height = sceneTextureExtent.height;
        gpf.viewport.x = 0.0f;
        gpf.viewport.y = (float)sceneTextureExtent2D.height;

        // revert view port like gbuffer.
        gpf.viewport.width =  (float)sceneTextureExtent2D.width;
        gpf.viewport.height = -(float)sceneTextureExtent2D.height;
        gpf.viewport.minDepth = 0.0f;
        gpf.viewport.maxDepth = 1.0f;
        gpf.scissor.offset = { 0, 0 };
        gpf.scissor.extent = sceneTextureExtent2D;

        m_pipelines[index] = gpf.buildMeshDrawPipeline(VulkanRHI::get()->getDevice(),m_renderpass);
    }

    bInitPipeline = true;
}

void engine::PMXPass::destroyRenderpass()
{
    VulkanRHI::get()->destroyRenderpass(m_renderpass);
}

void engine::PMXPass::destroyFramebuffers()
{
    for(uint32 i = 0; i < m_framebuffers.size(); i++)
    {
        VulkanRHI::get()->destroyFramebuffer(m_framebuffers[i]);
    }

    m_framebuffers.resize(0);
}

void engine::PMXPass::destroyPipeline()
{
    if(!bInitPipeline) return;

    for(uint32 index = 0; index < m_pipelines.size(); index++)
    {
        vkDestroyPipeline(VulkanRHI::get()->getDevice(),m_pipelines[index],nullptr);
        vkDestroyPipelineLayout(VulkanRHI::get()->getDevice(),m_pipelineLayouts[index],nullptr);
    }
    m_pipelines.resize(0);
    m_pipelineLayouts.resize(0);

    bInitPipeline = false;
}