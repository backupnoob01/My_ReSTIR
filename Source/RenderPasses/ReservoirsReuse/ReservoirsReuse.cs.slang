import Utils.Color.ColorHelpers;
import Utils.Sampling.SampleGenerator;
import Utils.Sampling.SampleGeneratorInterface;

RWTexture2D<float4> gOutputColor;
RWTexture2D<float4> gOutputColor2;

RWTexture2D<float4> gReservoirsIn;
RWTexture2D<float4> gReservoirsOut;

cbuffer CB
{
    uint gFrameCount;
    uint2 gResolution;
    uint kSpaceSamples;
};

struct Reservoir {
    float3 current_res;
    float sum_weight;

    [mutating] void update(const float weight, const float3 res, inout SampleGenerator sg) {
        if (weight == 0.f)
            return;
        this.sum_weight += weight;
        if (sampleNext1D(sg) < weight / sum_weight) {
            this.current_res = res;
        }
    }
};

void exportReservoir(const Reservoir r, const uint2 pixel) {
    gReservoirsOut[pixel].xyz = r.current_res;
    gReservoirsOut[pixel].w = r.sum_weight;
}

void importReservoirCurrent(const uint2 pixel, out Reservoir r) {
    r.current_res = gReservoirsIn[pixel].xyz;
    r.sum_weight  = gReservoirsIn[pixel].w;
}

void importReservoirHistory(const uint2 pixel, out Reservoir r) {
    r.current_res = gReservoirsOut[pixel].xyz;
    r.sum_weight = gReservoirsOut[pixel].w;
}

[numthreads(16, 16, 1)]
void main(uint3 dispatchThreadId: SV_DispatchThreadID) {
    if (any(dispatchThreadId.xy >= gResolution))
        return;
    const uint2 pixel = dispatchThreadId.xy;
    gOutputColor2[pixel] = gOutputColor[pixel];
    SampleGenerator sg = SampleGenerator(pixel, gFrameCount);
    // Spatial
    Reservoir r_spatial;
    r_spatial.current_res = float3(0.f);
    r_spatial.sum_weight  = 0.f;
    for (int i = 0; i < kSpaceSamples; i++) {
        float2 nextsample = (sampleNext2D(sg) - 0.5f) * 3;
        const uint2 samplePixel = pixel + int2(nextsample);
        if (any(samplePixel >= gResolution))
            continue;
        // Bias Version
        Reservoir sample;
        importReservoirCurrent(samplePixel, sample);
        if (luminance(sample.current_res) == 0.f)
            continue;
        r_spatial.update(sample.sum_weight * luminance(sample.current_res) / kSpaceSamples, sample.current_res, sg);
    }

    r_spatial.sum_weight /= luminance(r_spatial.current_res);

    // Temporal Bias version
    Reservoir r_final;
    r_final.current_res = float3(0.f);
    r_final.sum_weight = 0.f;

    Reservoir r_before;
    importReservoirHistory(pixel, r_before);

    if (r_before.sum_weight != 0.f && luminance(r_before.current_res) != 0.f) {
        r_final.update(r_before.sum_weight * luminance(r_before.current_res) * 3 / 5, r_before.current_res, sg);
    }
    if (r_spatial.sum_weight != 0.f && luminance(r_spatial.current_res) != 0.f) {
        r_final.update(r_spatial.sum_weight * luminance(r_spatial.current_res) * 2 / 5, r_spatial.current_res, sg);
    }

    // r_spatial.sum_weight *= luminance(r_spatial.current_res);
    if (luminance(r_final.current_res) != 0.f)
        r_final.sum_weight /= luminance(r_final.current_res);

    exportReservoir(r_final, pixel);

    if (luminance(r_final.current_res) == 0.f) {
        r_final.current_res = float3(0.f);
        r_final.sum_weight = 0.f;
        exportReservoir(r_final, pixel);
        return;
    }
        
    // gOutputColor2[pixel].xyz += r.sum_weight * r.current_res / luminance(r.current_res);

    if (r_final.sum_weight == 0.f)
        return;
    
    gOutputColor2[pixel].xyz += r_final.sum_weight * r_final.current_res;
}
