// Hashing and random number generation based on PCG
// From "Hash Functions for GPU Rendering" by Jarzynski & Olano
// https://jcgt.org/published/0009/03/02/
// https://www.reedbeta.com/blog/hash-functions-for-gpu-rendering/

uint Hash(uint x)
{
    uint state = x * 747796405u + 2891336453u;
    uint word = ((state >> ((state >> 28u) + 4u)) ^ state) * 277803737u;
    return (word >> 22u) ^ word;
}

uint Hash(uint2 v) { return Hash(Hash(v.x) + v.y); }
uint Hash(uint3 v) { return Hash(Hash(Hash(v.x) + v.y) + v.z); }
uint Hash(uint4 v) { return Hash(Hash(Hash(Hash(v.x) + v.y) + v.z) + v.w); }
uint Hash(uint x, uint y) { return Hash(uint2(x, y)); }
uint Hash(uint x, uint y, uint z) { return Hash(uint3(x, y, z)); }
uint Hash(uint x, uint y, uint z, uint w) { return Hash(uint4(x, y, z, w)); }

struct Random
{
    uint State;

    void Init(uint seed)
    {
        State = seed;
    }

    uint NextUint()
    {
        uint state = State;
        State = state * 747796405u + 2891336453u;
        uint word = ((state >> ((state >> 28u) + 4u)) ^ state) * 277803737u;
        return (word >> 22u) ^ word;
    }

    // Returns a value [min, max)
    uint NextUint(uint min, uint max)
    {
        return min + (uint)(NextFloat() * (max - min));
    }

    // Returns a value [0, 1)
    float NextFloat()
    {
        return asfloat(0x3F800000 | (NextUint() >> 9)) - 1.f;
    }

    // Returns a value [min, max)
    float NextFloat(float min, float max)
    {
        return min + NextFloat() * (max - min);
    }

    float3 NextFloat3()
    {
        return float3(NextFloat(), NextFloat(), NextFloat());
    }

    // Returns a vector where each component is [min.x, max.x)
    float3 NextFloat3(float3 min, float3 max)
    {
        return float3(NextFloat(min.x, max.x), NextFloat(min.y, max.y), NextFloat(min.z, max.z));
    }
};
