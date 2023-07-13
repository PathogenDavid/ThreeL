struct PsInput
{
    float4 Position : SV_Position;
    float2 Uv : TEXCOORD0;
};

// This vertex shader emits a single triangle which covers the entire screen
// This is more efficient than emitting two triangles to make an actual full-screen quad because it avoids inefficient
// pixel quad efficiency down the screen diagonal and provides better data locality when accessing full-screen textures.
// https://michaldrobot.com/2014/04/01/gcn-execution-patterns-in-full-screen-passes/
// https://wallisc.github.io/rendering/2021/04/18/Fullscreen-Pass.html
PsInput VsMain(uint vertexId : SV_VertexID)
{
    PsInput result;
    result.Uv = float2(uint2(vertexId, vertexId << 1) & 2.xx);
    float2 clipPosition = lerp(float2(-1.f, 1.f), float2(1.f, -1.f), result.Uv);
    result.Position = float4(clipPosition, 0.f, 1.f);
    return result;
}
