#include "pch.h"
#include "GltfAccessorView.h"

#include "Math.h"

static int GetComponentTypeSizeBytes(int componentType)
{
    switch (componentType)
    {
        case TINYGLTF_COMPONENT_TYPE_BYTE:
        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
            return 1;
        case TINYGLTF_COMPONENT_TYPE_SHORT:
        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
            return 2;
        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
        case TINYGLTF_COMPONENT_TYPE_FLOAT:
            return 4;
        default:
            return 0;
    }
}

static int GetComponentCount(int type)
{
    switch (type)
    {
        case TINYGLTF_TYPE_SCALAR: return 1;
        case TINYGLTF_TYPE_VEC2: return 2;
        case TINYGLTF_TYPE_VEC3: return 3;
        case TINYGLTF_TYPE_VEC4: return 4;
        case TINYGLTF_TYPE_MAT2: return 4;
        case TINYGLTF_TYPE_MAT3: return 9;
        case TINYGLTF_TYPE_MAT4: return 16;
        default: return 0;
    }
}

//! Returns the natural component size of an accessor, or -1 if this accessor has an invalid type.
static int GetElementSize(const tinygltf::Accessor& accessor)
{
    int elementSize = GetComponentTypeSizeBytes(accessor.componentType);
    int componentCount = GetComponentCount(accessor.type);
    return elementSize == 0 || componentCount == 0 ? -1 : elementSize * componentCount;
}

static std::span<const uint8_t> LoadBufferView(const tinygltf::Model& model, int bufferViewIndex, int* outByteStride)
{
    const tinygltf::BufferView& bufferView = model.bufferViews[bufferViewIndex];
    *outByteStride = (int)bufferView.byteStride;
    std::span<const uint8_t> result = model.buffers[bufferView.buffer].data;
    return result.subspan(bufferView.byteOffset, bufferView.byteLength);
}

template<typename TElement>
class GltfAccessorViewFromBufferView : public GltfAccessorView<TElement>
{
protected:
    std::span<const uint8_t> m_RawView;
    int m_ElementSize;
    int m_ByteStride;

    GltfAccessorViewFromBufferView(const tinygltf::Model& model, const tinygltf::Accessor& accessor)
        : GltfAccessorView<TElement>(model, accessor)
    {
        Assert(accessor.bufferView >= 0);

        m_RawView = LoadBufferView(model, accessor.bufferView, &m_ByteStride);
        m_RawView = m_RawView.subspan(accessor.byteOffset);
        m_ElementSize = GetElementSize(accessor);

        // When the byte stride is undefined, the data will be densely packed
        // https://github.com/KhronosGroup/glTF/tree/5de957b8b0a13c147c90d4ff569250440931872f/specification/2.0#bufferviewbytestride
        if (m_ByteStride == 0)
        {
            m_ByteStride = m_ElementSize;
        }

        // Truncate the view if the data is densely packed
        // For some reason some models have excess data at the end of some buffers (maybe multiple accessors are pointing to a subset of the same buffer view?)
        // (The Buggy model is known to trigger this issue: https://github.com/KhronosGroup/glTF-Sample-Models/tree/master/2.0/Buggy )
        if (m_ElementSize == m_ByteStride)
        {
            m_RawView = m_RawView.subspan(0, m_ElementSize * this->ElementCount());
        }

        Assert(m_ByteStride >= m_ElementSize);
        Assert(m_RawView.size() >= ((m_ByteStride * (this->ElementCount() - 1)) + m_ElementSize)); // Extra math is because the final element does not need to consider the stride
    }
};

template<typename TElement>
class GltfAccessorViewDense : public GltfAccessorViewFromBufferView<TElement>
{
private:
    std::span<const TElement> SpanT() const
    {
        return SpanCast<const uint8_t, const TElement>(this->m_RawView);
    }

public:
    GltfAccessorViewDense(const tinygltf::Model& model, const tinygltf::Accessor& accessor)
        : GltfAccessorViewFromBufferView<TElement>(model, accessor)
    {
        Assert(this->m_ElementSize == this->m_ByteStride);
        Assert(this->m_ElementSize == sizeof(TElement));

        // We assume the raw view and the resulting span will be exactly the correct length
        // You can't assume this is true by default since you're only supposed to read up to ElementCount elements
        // (The Buggy model is known to trigger this issue: https://github.com/KhronosGroup/glTF-Sample-Models/tree/master/2.0/Buggy )
        Assert(this->m_RawView.size() == sizeof(TElement) * this->ElementCount());
        Assert(SpanT().size() == this->ElementCount());
    }

    const TElement& operator[](size_t index) const override
    {
        return SpanT()[index];
    }

    std::span<const TElement> AsDenseSpanMaybeAllocate() override
    {
        return SpanT();
    }
};

template<typename TElement>
class GltfAccessorViewInterleaved : public GltfAccessorViewFromBufferView<TElement>
{
private:
    std::optional<std::vector<TElement>> m_CachedDenseArray;

public:
    GltfAccessorViewInterleaved(const tinygltf::Model& model, const tinygltf::Accessor& accessor)
        : GltfAccessorViewFromBufferView<TElement>(model, accessor)
    {
        Assert(this->m_ElementSize < this->m_ByteStride && "Should've used GltfAccessorViewDense instead!");
        Assert(this->m_ElementSize == sizeof(TElement));
    }

    const TElement& operator[](size_t index) const override
    {
        Assert(index >= 0 && index < this->ElementCount());

        const uint8_t* rawRef = &this->m_RawView[index * this->m_ByteStride];
        return *reinterpret_cast<const TElement*>(rawRef);
    }

    std::span<const TElement> AsDenseSpanMaybeAllocate() override
    {
        if (!m_CachedDenseArray.has_value())
        {
            std::vector<TElement> denseArray;
            denseArray.reserve(this->ElementCount());

            for (size_t i = 0; i < this->ElementCount(); i++)
            {
                denseArray.push_back((*this)[i]);
            }

            m_CachedDenseArray = denseArray;
        }

        return m_CachedDenseArray.value();
    }
};

template<typename TElement>
GltfAccessorViewBase* CreateImpl(const tinygltf::Model& model, const tinygltf::Accessor& accessor)
{
    int elementSize = GetElementSize(accessor);
    Assert(elementSize == sizeof(TElement));

    // None of the models we care about use normalized accessors, skipping this for simplicity
    Assert(!accessor.normalized && "Support for normalized glTF accessors is not implemented.");
    Assert(!accessor.sparse.isSparse && "Support for sparse glTF accessors is not implemented.");
    Assert(accessor.bufferView >= 0 && "Expected a non-sparse accessor to have a buffer view.");

    int byteStride = (int)model.bufferViews[accessor.bufferView].byteStride;

    if (byteStride == 0 || elementSize == byteStride)
    {
        return new GltfAccessorViewDense<TElement>(model, accessor);
    }
    else
    {
        return new GltfAccessorViewInterleaved<TElement>(model, accessor);
    }
}

GltfAccessorViewBase* GltfAccessorViewBase::Create(const tinygltf::Model& model, const tinygltf::Accessor& accessor)
{
    //TODO: Most of these are missing due to unported math types
    switch (accessor.componentType)
    {
        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
            switch (accessor.type)
            {
                case TINYGLTF_TYPE_SCALAR: return CreateImpl<uint8_t>(model, accessor);
            }
            break;
        case TINYGLTF_COMPONENT_TYPE_BYTE:
            switch (accessor.type)
            {
                case TINYGLTF_TYPE_SCALAR: return CreateImpl<int8_t>(model, accessor);
            }
            break;
        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
            switch (accessor.type)
            {
                case TINYGLTF_TYPE_SCALAR: return CreateImpl<uint16_t>(model, accessor);
            }
            break;
        case TINYGLTF_COMPONENT_TYPE_SHORT:
            switch (accessor.type)
            {
                case TINYGLTF_TYPE_SCALAR: return CreateImpl<int16_t>(model, accessor);
            }
            break;
        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
            switch (accessor.type)
            {
                case TINYGLTF_TYPE_SCALAR: return CreateImpl<uint32_t>(model, accessor);
                case TINYGLTF_TYPE_VEC2: return CreateImpl<uint2>(model, accessor);
            }
            break;
        case TINYGLTF_COMPONENT_TYPE_FLOAT:
            switch (accessor.type)
            {
                case TINYGLTF_TYPE_SCALAR: return CreateImpl<float>(model, accessor);
                case TINYGLTF_TYPE_VEC2: return CreateImpl<float2>(model, accessor);
                case TINYGLTF_TYPE_VEC3: return CreateImpl<float3>(model, accessor);
                case TINYGLTF_TYPE_VEC4: return CreateImpl<float4>(model, accessor);
                case TINYGLTF_TYPE_MAT4: return CreateImpl<float4x4>(model, accessor);
            }
            break;
    }
    
    Assert(false && "glTF accessor type/component type combination not supported.");
    return nullptr;
}
