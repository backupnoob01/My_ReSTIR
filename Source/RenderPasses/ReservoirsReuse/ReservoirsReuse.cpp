#include "ReservoirsReuse.h"
#include "RenderGraph/RenderPassHelpers.h"
#include "RenderGraph/RenderPassStandardFlags.h"

extern "C" FALCOR_API_EXPORT void registerPlugin(Falcor::PluginRegistry& registry)
{
    registry.registerClass<RenderPass, ReservoirsReuse>();
}
namespace
{
const char kShaderFile[] = "RenderPasses/ReservoirsReuse/ReservoirsReuse.cs.slang";

const ChannelList kInputChannels = {
    // clang-format off
    { "colorin",              "gOutputColor", "Output color (sum of direct and indirect)", false, ResourceFormat::RGBA32Float },
    { "reservoirsin",         "gReservoirsIn",  "ReSTIR reservoirs", false, ResourceFormat::RGBA32Float },
    // clang-format on
};

const ChannelList kOutputChannels = {
    // clang-format off
    { "colorout",             "gOutputColor2", "Output color (sum of direct and indirect)", false, ResourceFormat::RGBA32Float },
    // { "reservoirsout",        "gReservoirsOut",  "ReSTIR reservoirs", true, ResourceFormat::RGBA32Float },
    // clang-format on
};
}

ReservoirsReuse::ReservoirsReuse(ref<Device> pDevice, const Properties& props) : RenderPass(pDevice) {
    mpSampleGenerator = SampleGenerator::create(mpDevice, SAMPLE_GENERATOR_UNIFORM);
    mpState = ComputeState::create(mpDevice);
}

Properties ReservoirsReuse::getProperties() const
{
    return {};
}

RenderPassReflection ReservoirsReuse::reflect(const CompileData& compileData)
{
    // Define the required resources here
    RenderPassReflection reflector;
    // reflector.addOutput("dst");
    // reflector.addInput("src");
    addRenderPassInputs(reflector, kInputChannels);
    addRenderPassOutputs(reflector, kOutputChannels);
    return reflector;
}

void ReservoirsReuse::execute(RenderContext* pRenderContext, const RenderData& renderData)
{
    // renderData holds the requested resources
    // auto& pTexture = renderData.getTexture("src");

    ref<Texture> pDst = renderData.getTexture(kOutputChannels[0].name);
    const uint2 resolution = uint2(pDst->getWidth(), pDst->getHeight());

    if (any(resolution != mFrameDim)) {
        mFrameDim   = resolution;
        mFrameCount = 0;
    }

    auto& dict = renderData.getDictionary();

    if(mOptionsChanged){
        auto flags = dict.getValue(kRenderPassRefreshFlags, RenderPassRefreshFlags::None);
        dict[Falcor::kRenderPassRefreshFlags] = flags | Falcor::RenderPassRefreshFlags::RenderOptionsChanged;

        mOptionsChanged = false;
    }

    if(!mMain.pProgram){
        mMain.pProgram = Program::createCompute(mpDevice, kShaderFile, "main", DefineList(), SlangCompilerFlags::TreatWarningsAsErrors);
        mMain.pProgram->addDefines(getValidResourceDefines(kInputChannels, renderData));
        mMain.pProgram->addDefines(getValidResourceDefines(kOutputChannels, renderData));
        mMain.pProgram->addDefines(mpSampleGenerator->getDefines());
        mMain.pVars = ProgramVars::create(mpDevice, mMain.pProgram->getReflector());
        auto var = mMain.pVars->getRootVar();
        mpSampleGenerator->bindShaderData(var);
    }

    // FALCOR_ASSERT(mMain.pProgram);
    auto var = mMain.pVars->getRootVar();

    var["CB"]["gFrameCount"]   = mFrameCount;
    var["CB"]["gResolution"]   = mFrameDim;
    var["CB"]["kSpaceSamples"] = mSpaceSamples;

    prepareAccumulation(pRenderContext, mFrameDim.x, mFrameDim.y);

    var["gReservoirsOut"] = mpReservoirsOut;

    // Bind I/O buffers. These needs to be done per-frame as the buffers may change anytime.
    auto bind = [&](const ChannelDesc& desc)
    {
        if (!desc.texname.empty())
        {
            var[desc.texname] = renderData.getTexture(desc.name);
        }
    };
    for (auto channel : kInputChannels)
        bind(channel);
    for (auto channel : kOutputChannels)
        bind(channel);

    // Get dimensions of ray dispatch.
    const uint2 targetDim = renderData.getDefaultTextureDims();
    FALCOR_ASSERT(targetDim.x > 0 && targetDim.y > 0);

    mFrameCount++;
    uint3 numGroups = div_round_up(uint3(mFrameDim.x, mFrameDim.y, 1u), mMain.pProgram->getReflector()->getThreadGroupSize());
    mpState->setProgram(mMain.pProgram);
    pRenderContext->dispatch(mpState.get(), mMain.pVars.get(), numGroups);
}

void ReservoirsReuse::renderUI(Gui::Widgets& widget) {
    bool dirty = false;

    dirty |= widget.var("Spatial Reuse Number", mSpaceSamples, 0u, 1u << 16);
    widget.tooltip("The number of spatial reuse sample.", true);

    // If rendering options that modify the output have changed, set flag to indicate that.
    // In execute() we will pass the flag to other passes for reset of temporal data etc.
    if (dirty){
        mOptionsChanged = true;
    }
}

void ReservoirsReuse::prepareAccumulation(RenderContext* pRenderContext, uint32_t width, uint32_t height)
{
    // Allocate/resize/clear buffers for intermedate data. These are different depending on accumulation mode.
    // Buffers that are not used in the current mode are released.
    auto prepareBuffer = [&](ref<Texture>& pBuf, ResourceFormat format)
    {
        if (!pBuf || pBuf->getWidth() != width || pBuf->getHeight() != height)
        {
            pBuf = mpDevice->createTexture2D(
                width, height, format, 1, 1, nullptr, ResourceBindFlags::ShaderResource | ResourceBindFlags::UnorderedAccess
            );
            FALCOR_ASSERT(pBuf);
            mFrameCount = 0;
        }
        // Clear data if accumulation has been reset (either above or somewhere else).
        if (mFrameCount == 0)
            pRenderContext->clearUAV(pBuf->getUAV().get(), float4(0.f));
    };
    prepareBuffer(mpReservoirsOut, ResourceFormat::RGBA32Float);
}

void ReservoirsReuse::setScene(RenderContext* pRenderContext, const ref<Scene>& pScene){
    mMain.pProgram = nullptr;
    mMain.pVars = nullptr;
    mFrameCount = 0;
}