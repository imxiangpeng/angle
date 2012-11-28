//
// Copyright (c) 2002-2012 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// Context.cpp: Implements the gl::Context class, managing all GL state and performing
// rendering operations. It is the GLES2 specific implementation of EGLContext.

#include "libGLESv2/Context.h"

#include <algorithm>

#include "libEGL/Display.h"

#include "libGLESv2/main.h"
#include "libGLESv2/mathutil.h"
#include "libGLESv2/utilities.h"
#include "libGLESv2/renderer/renderer9_utils.h" // D3D9_REPLACE
#include "libGLESv2/ResourceManager.h"
#include "libGLESv2/Buffer.h"
#include "libGLESv2/Fence.h"
#include "libGLESv2/Framebuffer.h"
#include "libGLESv2/Program.h"
#include "libGLESv2/ProgramBinary.h"
#include "libGLESv2/Query.h"
#include "libGLESv2/Renderbuffer.h"
#include "libGLESv2/Shader.h"
#include "libGLESv2/Texture.h"
#include "libGLESv2/VertexDataManager.h"
#include "libGLESv2/IndexDataManager.h"

#undef near
#undef far

namespace gl
{
Context::Context(const gl::Context *shareContext, rx::Renderer *renderer, bool notifyResets, bool robustAccess)
{
    ASSERT(robustAccess == false);   // Unimplemented

    ASSERT(dynamic_cast<rx::Renderer9*>(renderer) != NULL); // D3D9_REPLACE
    mRenderer = static_cast<rx::Renderer9*>(renderer);

    mDevice = NULL;

    mFenceHandleAllocator.setBaseHandle(0);

    setClearColor(0.0f, 0.0f, 0.0f, 0.0f);

    mState.depthClearValue = 1.0f;
    mState.stencilClearValue = 0;

    mState.rasterizer.cullFace = false;
    mState.rasterizer.cullMode = GL_BACK;
    mState.rasterizer.frontFace = GL_CCW;
    mState.rasterizer.polygonOffsetFill = false;
    mState.rasterizer.polygonOffsetFactor = 0.0f;
    mState.rasterizer.polygonOffsetUnits = 0.0f;
    mState.rasterizer.scissorTest = false;
    mState.scissor.x = 0;
    mState.scissor.y = 0;
    mState.scissor.width = 0;
    mState.scissor.height = 0;

    mState.blend.blend = false;
    mState.blend.sourceBlendRGB = GL_ONE;
    mState.blend.sourceBlendAlpha = GL_ONE;
    mState.blend.destBlendRGB = GL_ZERO;
    mState.blend.destBlendAlpha = GL_ZERO;
    mState.blend.blendEquationRGB = GL_FUNC_ADD;
    mState.blend.blendEquationAlpha = GL_FUNC_ADD;
    mState.blend.sampleAlphaToCoverage = false;
    mState.blend.dither = true;

    mState.blendColor.red = 0;
    mState.blendColor.green = 0;
    mState.blendColor.blue = 0;
    mState.blendColor.alpha = 0;

    mState.depthStencil.depthTest = false;
    mState.depthStencil.depthFunc = GL_LESS;
    mState.depthStencil.depthMask = true;
    mState.depthStencil.stencilTest = false;
    mState.depthStencil.stencilFunc = GL_ALWAYS;
    mState.depthStencil.stencilMask = -1;
    mState.depthStencil.stencilWritemask = -1;
    mState.depthStencil.stencilBackFunc = GL_ALWAYS;
    mState.depthStencil.stencilBackMask = - 1;
    mState.depthStencil.stencilBackWritemask = -1;
    mState.depthStencil.stencilFail = GL_KEEP;
    mState.depthStencil.stencilPassDepthFail = GL_KEEP;
    mState.depthStencil.stencilPassDepthPass = GL_KEEP;
    mState.depthStencil.stencilBackFail = GL_KEEP;
    mState.depthStencil.stencilBackPassDepthFail = GL_KEEP;
    mState.depthStencil.stencilBackPassDepthPass = GL_KEEP;

    mState.stencilRef = 0;
    mState.stencilBackRef = 0;

    mState.sampleCoverage = false;
    mState.sampleCoverageValue = 1.0f;
    mState.sampleCoverageInvert = false;
    mState.generateMipmapHint = GL_DONT_CARE;
    mState.fragmentShaderDerivativeHint = GL_DONT_CARE;

    mState.lineWidth = 1.0f;

    mState.viewport.x = 0;
    mState.viewport.y = 0;
    mState.viewport.width = 0;
    mState.viewport.height = 0;
    mState.zNear = 0.0f;
    mState.zFar = 1.0f;

    mState.blend.colorMaskRed = true;
    mState.blend.colorMaskGreen = true;
    mState.blend.colorMaskBlue = true;
    mState.blend.colorMaskAlpha = true;

    if (shareContext != NULL)
    {
        mResourceManager = shareContext->mResourceManager;
        mResourceManager->addRef();
    }
    else
    {
        mResourceManager = new ResourceManager(mRenderer);
    }

    // [OpenGL ES 2.0.24] section 3.7 page 83:
    // In the initial state, TEXTURE_2D and TEXTURE_CUBE_MAP have twodimensional
    // and cube map texture state vectors respectively associated with them.
    // In order that access to these initial textures not be lost, they are treated as texture
    // objects all of whose names are 0.

    mTexture2DZero.set(new Texture2D(mRenderer, 0));
    mTextureCubeMapZero.set(new TextureCubeMap(mRenderer, 0));

    mState.activeSampler = 0;
    bindArrayBuffer(0);
    bindElementArrayBuffer(0);
    bindTextureCubeMap(0);
    bindTexture2D(0);
    bindReadFramebuffer(0);
    bindDrawFramebuffer(0);
    bindRenderbuffer(0);

    mState.currentProgram = 0;
    mCurrentProgramBinary.set(NULL);

    mState.packAlignment = 4;
    mState.unpackAlignment = 4;
    mState.packReverseRowOrder = false;

    mVertexDataManager = NULL;
    mIndexDataManager = NULL;
    mLineLoopIB = NULL;

    mInvalidEnum = false;
    mInvalidValue = false;
    mInvalidOperation = false;
    mOutOfMemory = false;
    mInvalidFramebufferOperation = false;

    mHasBeenCurrent = false;
    mContextLost = false;
    mResetStatus = GL_NO_ERROR;
    mResetStrategy = (notifyResets ? GL_LOSE_CONTEXT_ON_RESET_EXT : GL_NO_RESET_NOTIFICATION_EXT);
    mRobustAccess = robustAccess;

    mSupportsDXT1Textures = false;
    mSupportsDXT3Textures = false;
    mSupportsDXT5Textures = false;
    mSupportsEventQueries = false;
    mSupportsOcclusionQueries = false;
    mNumCompressedTextureFormats = 0;
    mMaskedClearSavedState = NULL;
    markAllStateDirty();
}

Context::~Context()
{
    if (mState.currentProgram != 0)
    {
        Program *programObject = mResourceManager->getProgram(mState.currentProgram);
        if (programObject)
        {
            programObject->release();
        }
        mState.currentProgram = 0;
    }
    mCurrentProgramBinary.set(NULL);

    while (!mFramebufferMap.empty())
    {
        deleteFramebuffer(mFramebufferMap.begin()->first);
    }

    while (!mFenceMap.empty())
    {
        deleteFence(mFenceMap.begin()->first);
    }

    while (!mQueryMap.empty())
    {
        deleteQuery(mQueryMap.begin()->first);
    }

    for (int type = 0; type < TEXTURE_TYPE_COUNT; type++)
    {
        for (int sampler = 0; sampler < MAX_COMBINED_TEXTURE_IMAGE_UNITS_VTF; sampler++)
        {
            mState.samplerTexture[type][sampler].set(NULL);
        }
    }

    for (int type = 0; type < TEXTURE_TYPE_COUNT; type++)
    {
        mIncompleteTextures[type].set(NULL);
    }

    for (int i = 0; i < MAX_VERTEX_ATTRIBS; i++)
    {
        mState.vertexAttribute[i].mBoundBuffer.set(NULL);
    }

    for (int i = 0; i < QUERY_TYPE_COUNT; i++)
    {
        mState.activeQuery[i].set(NULL);
    }

    mState.arrayBuffer.set(NULL);
    mState.elementArrayBuffer.set(NULL);
    mState.renderbuffer.set(NULL);

    mTexture2DZero.set(NULL);
    mTextureCubeMapZero.set(NULL);

    delete mVertexDataManager;
    delete mIndexDataManager;
    delete mLineLoopIB;

    if (mMaskedClearSavedState)
    {
        mMaskedClearSavedState->Release();
    }

    mResourceManager->release();
}

void Context::makeCurrent(egl::Surface *surface)
{
    mDevice = mRenderer->getDevice(); // D3D9_REMOVE

    if (!mHasBeenCurrent)
    {
        mVertexDataManager = new VertexDataManager(mRenderer);
        mIndexDataManager = new IndexDataManager(mRenderer);

        mSupportsShaderModel3 = mRenderer->getShaderModel3Support();
        mMaximumPointSize = mRenderer->getMaxPointSize();
        mSupportsVertexTexture = mRenderer->getVertexTextureSupport();
        mSupportsNonPower2Texture = mRenderer->getNonPower2TextureSupport();
        mSupportsInstancing = mRenderer->getInstancingSupport();

        mMaxTextureDimension = std::min(std::min(mRenderer->getMaxTextureWidth(), mRenderer->getMaxTextureHeight()),
                                        (int)gl::IMPLEMENTATION_MAX_TEXTURE_SIZE);
        mMaxCubeTextureDimension = std::min(mMaxTextureDimension, (int)gl::IMPLEMENTATION_MAX_CUBE_MAP_TEXTURE_SIZE);
        mMaxRenderbufferDimension = mMaxTextureDimension;
        mMaxTextureLevel = log2(mMaxTextureDimension) + 1;
        mMaxTextureAnisotropy = mRenderer->getTextureMaxAnisotropy();
        TRACE("MaxTextureDimension=%d, MaxCubeTextureDimension=%d, MaxRenderbufferDimension=%d, MaxTextureLevel=%d, MaxTextureAnisotropy=%f",
              mMaxTextureDimension, mMaxCubeTextureDimension, mMaxRenderbufferDimension, mMaxTextureLevel, mMaxTextureAnisotropy);

        mSupportsEventQueries = mRenderer->getEventQuerySupport();
        mSupportsOcclusionQueries = mRenderer->getOcclusionQuerySupport();
        mSupportsDXT1Textures = mRenderer->getDXT1TextureSupport();
        mSupportsDXT3Textures = mRenderer->getDXT3TextureSupport();
        mSupportsDXT5Textures = mRenderer->getDXT5TextureSupport();
        mSupportsFloat32Textures = mRenderer->getFloat32TextureSupport(&mSupportsFloat32LinearFilter, &mSupportsFloat32RenderableTextures);
        mSupportsFloat16Textures = mRenderer->getFloat16TextureSupport(&mSupportsFloat16LinearFilter, &mSupportsFloat16RenderableTextures);
        mSupportsLuminanceTextures = mRenderer->getLuminanceTextureSupport();
        mSupportsLuminanceAlphaTextures = mRenderer->getLuminanceAlphaTextureSupport();
        mSupportsDepthTextures = mRenderer->getDepthTextureSupport();
        mSupportsTextureFilterAnisotropy = mRenderer->getTextureFilterAnisotropySupport();

        mSupports32bitIndices = mRenderer->get32BitIndexSupport();

        mNumCompressedTextureFormats = 0;
        if (supportsDXT1Textures())
        {
            mNumCompressedTextureFormats += 2;
        }
        if (supportsDXT3Textures())
        {
            mNumCompressedTextureFormats += 1;
        }
        if (supportsDXT5Textures())
        {
            mNumCompressedTextureFormats += 1;
        }

        initExtensionString();
        initRendererString();

        mState.viewport.x = 0;
        mState.viewport.y = 0;
        mState.viewport.width = surface->getWidth();
        mState.viewport.height = surface->getHeight();

        mState.scissor.x = 0;
        mState.scissor.y = 0;
        mState.scissor.width = surface->getWidth();
        mState.scissor.height = surface->getHeight();

        mHasBeenCurrent = true;
    }

    // Wrap the existing swapchain resources into GL objects and assign them to the '0' names
    rx::SwapChain *swapchain = surface->getSwapChain();

    Colorbuffer *colorbufferZero = new Colorbuffer(mRenderer, swapchain);
    DepthStencilbuffer *depthStencilbufferZero = new DepthStencilbuffer(mRenderer, swapchain);
    Framebuffer *framebufferZero = new DefaultFramebuffer(mRenderer, colorbufferZero, depthStencilbufferZero);

    setFramebufferZero(framebufferZero);

    // Reset pixel shader to null to work around a bug that only happens with Intel GPUs.
    // http://crbug.com/110343
    mDevice->SetPixelShader(NULL);
    
    markAllStateDirty();
}

// This function will set all of the state-related dirty flags, so that all state is set during next pre-draw.
void Context::markAllStateDirty()
{
    for (int t = 0; t < MAX_TEXTURE_IMAGE_UNITS; t++)
    {
        mAppliedTextureSerialPS[t] = 0;
    }

    for (int t = 0; t < MAX_VERTEX_TEXTURE_IMAGE_UNITS_VTF; t++)
    {
        mAppliedTextureSerialVS[t] = 0;
    }

    mAppliedProgramBinarySerial = 0;
    mAppliedRenderTargetSerial = 0;
    mAppliedDepthbufferSerial = 0;
    mAppliedStencilbufferSerial = 0;
    mAppliedIBSerial = 0;
    mDepthStencilInitialized = false;
    mViewportInitialized = false;
    mRenderTargetDescInitialized = false;

    mVertexDeclarationCache.markStateDirty();

    mDxUniformsDirty = true;
}

void Context::markDxUniformsDirty()
{
    mDxUniformsDirty = true;
}

// NOTE: this function should not assume that this context is current!
void Context::markContextLost()
{
    if (mResetStrategy == GL_LOSE_CONTEXT_ON_RESET_EXT)
        mResetStatus = GL_UNKNOWN_CONTEXT_RESET_EXT;
    mContextLost = true;
}

bool Context::isContextLost()
{
    return mContextLost;
}

void Context::setClearColor(float red, float green, float blue, float alpha)
{
    mState.colorClearValue.red = red;
    mState.colorClearValue.green = green;
    mState.colorClearValue.blue = blue;
    mState.colorClearValue.alpha = alpha;
}

void Context::setClearDepth(float depth)
{
    mState.depthClearValue = depth;
}

void Context::setClearStencil(int stencil)
{
    mState.stencilClearValue = stencil;
}

void Context::setCullFace(bool enabled)
{
    mState.rasterizer.cullFace = enabled;
}

bool Context::isCullFaceEnabled() const
{
    return mState.rasterizer.cullFace;
}

void Context::setCullMode(GLenum mode)
{
    mState.rasterizer.cullMode = mode;
}

void Context::setFrontFace(GLenum front)
{
    mState.rasterizer.frontFace = front;
}

void Context::setDepthTest(bool enabled)
{
    mState.depthStencil.depthTest = enabled;
}

bool Context::isDepthTestEnabled() const
{
    return mState.depthStencil.depthTest;
}

void Context::setDepthFunc(GLenum depthFunc)
{
     mState.depthStencil.depthFunc = depthFunc;
}

void Context::setDepthRange(float zNear, float zFar)
{
    mState.zNear = zNear;
    mState.zFar = zFar;
}

void Context::setBlend(bool enabled)
{
    mState.blend.blend = enabled;
}

bool Context::isBlendEnabled() const
{
    return mState.blend.blend;
}

void Context::setBlendFactors(GLenum sourceRGB, GLenum destRGB, GLenum sourceAlpha, GLenum destAlpha)
{
    mState.blend.sourceBlendRGB = sourceRGB;
    mState.blend.destBlendRGB = destRGB;
    mState.blend.sourceBlendAlpha = sourceAlpha;
    mState.blend.destBlendAlpha = destAlpha;
}

void Context::setBlendColor(float red, float green, float blue, float alpha)
{
    mState.blendColor.red = red;
    mState.blendColor.green = green;
    mState.blendColor.blue = blue;
    mState.blendColor.alpha = alpha;
}

void Context::setBlendEquation(GLenum rgbEquation, GLenum alphaEquation)
{
    mState.blend.blendEquationRGB = rgbEquation;
    mState.blend.blendEquationAlpha = alphaEquation;
}

void Context::setStencilTest(bool enabled)
{
    mState.depthStencil.stencilTest = enabled;
}

bool Context::isStencilTestEnabled() const
{
    return mState.depthStencil.stencilTest;
}

void Context::setStencilParams(GLenum stencilFunc, GLint stencilRef, GLuint stencilMask)
{
    mState.depthStencil.stencilFunc = stencilFunc;
    mState.stencilRef = (stencilRef > 0) ? stencilRef : 0;
    mState.depthStencil.stencilMask = stencilMask;
}

void Context::setStencilBackParams(GLenum stencilBackFunc, GLint stencilBackRef, GLuint stencilBackMask)
{
    mState.depthStencil.stencilBackFunc = stencilBackFunc;
    mState.stencilBackRef = (stencilBackRef > 0) ? stencilBackRef : 0;
    mState.depthStencil.stencilBackMask = stencilBackMask;
}

void Context::setStencilWritemask(GLuint stencilWritemask)
{
    mState.depthStencil.stencilWritemask = stencilWritemask;
}

void Context::setStencilBackWritemask(GLuint stencilBackWritemask)
{
    mState.depthStencil.stencilBackWritemask = stencilBackWritemask;
}

void Context::setStencilOperations(GLenum stencilFail, GLenum stencilPassDepthFail, GLenum stencilPassDepthPass)
{
    mState.depthStencil.stencilFail = stencilFail;
    mState.depthStencil.stencilPassDepthFail = stencilPassDepthFail;
    mState.depthStencil.stencilPassDepthPass = stencilPassDepthPass;
}

void Context::setStencilBackOperations(GLenum stencilBackFail, GLenum stencilBackPassDepthFail, GLenum stencilBackPassDepthPass)
{
    mState.depthStencil.stencilBackFail = stencilBackFail;
    mState.depthStencil.stencilBackPassDepthFail = stencilBackPassDepthFail;
    mState.depthStencil.stencilBackPassDepthPass = stencilBackPassDepthPass;
}

void Context::setPolygonOffsetFill(bool enabled)
{
     mState.rasterizer.polygonOffsetFill = enabled;
}

bool Context::isPolygonOffsetFillEnabled() const
{
    return mState.rasterizer.polygonOffsetFill;
}

void Context::setPolygonOffsetParams(GLfloat factor, GLfloat units)
{
    mState.rasterizer.polygonOffsetFactor = factor;
    mState.rasterizer.polygonOffsetUnits = units;
}

void Context::setSampleAlphaToCoverage(bool enabled)
{
    mState.blend.sampleAlphaToCoverage = enabled;
}

bool Context::isSampleAlphaToCoverageEnabled() const
{
    return mState.blend.sampleAlphaToCoverage;
}

void Context::setSampleCoverage(bool enabled)
{
    mState.sampleCoverage = enabled;
}

bool Context::isSampleCoverageEnabled() const
{
    return mState.sampleCoverage;
}

void Context::setSampleCoverageParams(GLclampf value, bool invert)
{
    mState.sampleCoverageValue = value;
    mState.sampleCoverageInvert = invert;
}

void Context::setScissorTest(bool enabled)
{
    mState.rasterizer.scissorTest = enabled;
}

bool Context::isScissorTestEnabled() const
{
    return mState.rasterizer.scissorTest;
}

void Context::setDither(bool enabled)
{
    mState.blend.dither = enabled;
}

bool Context::isDitherEnabled() const
{
    return mState.blend.dither;
}

void Context::setLineWidth(GLfloat width)
{
    mState.lineWidth = width;
}

void Context::setGenerateMipmapHint(GLenum hint)
{
    mState.generateMipmapHint = hint;
}

void Context::setFragmentShaderDerivativeHint(GLenum hint)
{
    mState.fragmentShaderDerivativeHint = hint;
    // TODO: Propagate the hint to shader translator so we can write
    // ddx, ddx_coarse, or ddx_fine depending on the hint.
    // Ignore for now. It is valid for implementations to ignore hint.
}

void Context::setViewportParams(GLint x, GLint y, GLsizei width, GLsizei height)
{
    mState.viewport.x = x;
    mState.viewport.y = y;
    mState.viewport.width = width;
    mState.viewport.height = height;
}

void Context::setScissorParams(GLint x, GLint y, GLsizei width, GLsizei height)
{
    mState.scissor.x = x;
    mState.scissor.y = y;
    mState.scissor.width = width;
    mState.scissor.height = height;
}

void Context::setColorMask(bool red, bool green, bool blue, bool alpha)
{
    mState.blend.colorMaskRed = red;
    mState.blend.colorMaskGreen = green;
    mState.blend.colorMaskBlue = blue;
    mState.blend.colorMaskAlpha = alpha;
}

void Context::setDepthMask(bool mask)
{
    mState.depthStencil.depthMask = mask;
}

void Context::setActiveSampler(unsigned int active)
{
    mState.activeSampler = active;
}

GLuint Context::getReadFramebufferHandle() const
{
    return mState.readFramebuffer;
}

GLuint Context::getDrawFramebufferHandle() const
{
    return mState.drawFramebuffer;
}

GLuint Context::getRenderbufferHandle() const
{
    return mState.renderbuffer.id();
}

GLuint Context::getArrayBufferHandle() const
{
    return mState.arrayBuffer.id();
}

GLuint Context::getActiveQuery(GLenum target) const
{
    Query *queryObject = NULL;
    
    switch (target)
    {
      case GL_ANY_SAMPLES_PASSED_EXT:
        queryObject = mState.activeQuery[QUERY_ANY_SAMPLES_PASSED].get();
        break;
      case GL_ANY_SAMPLES_PASSED_CONSERVATIVE_EXT:
        queryObject = mState.activeQuery[QUERY_ANY_SAMPLES_PASSED_CONSERVATIVE].get();
        break;
      default:
        ASSERT(false);
    }

    if (queryObject)
    {
        return queryObject->id();
    }
    else
    {
        return 0;
    }
}

void Context::setEnableVertexAttribArray(unsigned int attribNum, bool enabled)
{
    mState.vertexAttribute[attribNum].mArrayEnabled = enabled;
}

const VertexAttribute &Context::getVertexAttribState(unsigned int attribNum)
{
    return mState.vertexAttribute[attribNum];
}

void Context::setVertexAttribState(unsigned int attribNum, Buffer *boundBuffer, GLint size, GLenum type, bool normalized,
                                   GLsizei stride, const void *pointer)
{
    mState.vertexAttribute[attribNum].mBoundBuffer.set(boundBuffer);
    mState.vertexAttribute[attribNum].mSize = size;
    mState.vertexAttribute[attribNum].mType = type;
    mState.vertexAttribute[attribNum].mNormalized = normalized;
    mState.vertexAttribute[attribNum].mStride = stride;
    mState.vertexAttribute[attribNum].mPointer = pointer;
}

const void *Context::getVertexAttribPointer(unsigned int attribNum) const
{
    return mState.vertexAttribute[attribNum].mPointer;
}

void Context::setPackAlignment(GLint alignment)
{
    mState.packAlignment = alignment;
}

GLint Context::getPackAlignment() const
{
    return mState.packAlignment;
}

void Context::setUnpackAlignment(GLint alignment)
{
    mState.unpackAlignment = alignment;
}

GLint Context::getUnpackAlignment() const
{
    return mState.unpackAlignment;
}

void Context::setPackReverseRowOrder(bool reverseRowOrder)
{
    mState.packReverseRowOrder = reverseRowOrder;
}

bool Context::getPackReverseRowOrder() const
{
    return mState.packReverseRowOrder;
}

GLuint Context::createBuffer()
{
    return mResourceManager->createBuffer();
}

GLuint Context::createProgram()
{
    return mResourceManager->createProgram();
}

GLuint Context::createShader(GLenum type)
{
    return mResourceManager->createShader(type);
}

GLuint Context::createTexture()
{
    return mResourceManager->createTexture();
}

GLuint Context::createRenderbuffer()
{
    return mResourceManager->createRenderbuffer();
}

// Returns an unused framebuffer name
GLuint Context::createFramebuffer()
{
    GLuint handle = mFramebufferHandleAllocator.allocate();

    mFramebufferMap[handle] = NULL;

    return handle;
}

GLuint Context::createFence()
{
    GLuint handle = mFenceHandleAllocator.allocate();

    mFenceMap[handle] = new Fence(mRenderer);

    return handle;
}

// Returns an unused query name
GLuint Context::createQuery()
{
    GLuint handle = mQueryHandleAllocator.allocate();

    mQueryMap[handle] = NULL;

    return handle;
}

void Context::deleteBuffer(GLuint buffer)
{
    if (mResourceManager->getBuffer(buffer))
    {
        detachBuffer(buffer);
    }
    
    mResourceManager->deleteBuffer(buffer);
}

void Context::deleteShader(GLuint shader)
{
    mResourceManager->deleteShader(shader);
}

void Context::deleteProgram(GLuint program)
{
    mResourceManager->deleteProgram(program);
}

void Context::deleteTexture(GLuint texture)
{
    if (mResourceManager->getTexture(texture))
    {
        detachTexture(texture);
    }

    mResourceManager->deleteTexture(texture);
}

void Context::deleteRenderbuffer(GLuint renderbuffer)
{
    if (mResourceManager->getRenderbuffer(renderbuffer))
    {
        detachRenderbuffer(renderbuffer);
    }
    
    mResourceManager->deleteRenderbuffer(renderbuffer);
}

void Context::deleteFramebuffer(GLuint framebuffer)
{
    FramebufferMap::iterator framebufferObject = mFramebufferMap.find(framebuffer);

    if (framebufferObject != mFramebufferMap.end())
    {
        detachFramebuffer(framebuffer);

        mFramebufferHandleAllocator.release(framebufferObject->first);
        delete framebufferObject->second;
        mFramebufferMap.erase(framebufferObject);
    }
}

void Context::deleteFence(GLuint fence)
{
    FenceMap::iterator fenceObject = mFenceMap.find(fence);

    if (fenceObject != mFenceMap.end())
    {
        mFenceHandleAllocator.release(fenceObject->first);
        delete fenceObject->second;
        mFenceMap.erase(fenceObject);
    }
}

void Context::deleteQuery(GLuint query)
{
    QueryMap::iterator queryObject = mQueryMap.find(query);
    if (queryObject != mQueryMap.end())
    {
        mQueryHandleAllocator.release(queryObject->first);
        if (queryObject->second)
        {
            queryObject->second->release();
        }
        mQueryMap.erase(queryObject);
    }
}

Buffer *Context::getBuffer(GLuint handle)
{
    return mResourceManager->getBuffer(handle);
}

Shader *Context::getShader(GLuint handle)
{
    return mResourceManager->getShader(handle);
}

Program *Context::getProgram(GLuint handle)
{
    return mResourceManager->getProgram(handle);
}

Texture *Context::getTexture(GLuint handle)
{
    return mResourceManager->getTexture(handle);
}

Renderbuffer *Context::getRenderbuffer(GLuint handle)
{
    return mResourceManager->getRenderbuffer(handle);
}

Framebuffer *Context::getReadFramebuffer()
{
    return getFramebuffer(mState.readFramebuffer);
}

Framebuffer *Context::getDrawFramebuffer()
{
    return mBoundDrawFramebuffer;
}

void Context::bindArrayBuffer(unsigned int buffer)
{
    mResourceManager->checkBufferAllocation(buffer);

    mState.arrayBuffer.set(getBuffer(buffer));
}

void Context::bindElementArrayBuffer(unsigned int buffer)
{
    mResourceManager->checkBufferAllocation(buffer);

    mState.elementArrayBuffer.set(getBuffer(buffer));
}

void Context::bindTexture2D(GLuint texture)
{
    mResourceManager->checkTextureAllocation(texture, TEXTURE_2D);

    mState.samplerTexture[TEXTURE_2D][mState.activeSampler].set(getTexture(texture));
}

void Context::bindTextureCubeMap(GLuint texture)
{
    mResourceManager->checkTextureAllocation(texture, TEXTURE_CUBE);

    mState.samplerTexture[TEXTURE_CUBE][mState.activeSampler].set(getTexture(texture));
}

void Context::bindReadFramebuffer(GLuint framebuffer)
{
    if (!getFramebuffer(framebuffer))
    {
        mFramebufferMap[framebuffer] = new Framebuffer(mRenderer);
    }

    mState.readFramebuffer = framebuffer;
}

void Context::bindDrawFramebuffer(GLuint framebuffer)
{
    if (!getFramebuffer(framebuffer))
    {
        mFramebufferMap[framebuffer] = new Framebuffer(mRenderer);
    }

    mState.drawFramebuffer = framebuffer;

    mBoundDrawFramebuffer = getFramebuffer(framebuffer);
}

void Context::bindRenderbuffer(GLuint renderbuffer)
{
    mResourceManager->checkRenderbufferAllocation(renderbuffer);

    mState.renderbuffer.set(getRenderbuffer(renderbuffer));
}

void Context::useProgram(GLuint program)
{
    GLuint priorProgram = mState.currentProgram;
    mState.currentProgram = program;               // Must switch before trying to delete, otherwise it only gets flagged.

    if (priorProgram != program)
    {
        Program *newProgram = mResourceManager->getProgram(program);
        Program *oldProgram = mResourceManager->getProgram(priorProgram);
        mCurrentProgramBinary.set(NULL);
        mDxUniformsDirty = true;

        if (newProgram)
        {
            newProgram->addRef();
            mCurrentProgramBinary.set(newProgram->getProgramBinary());
        }
        
        if (oldProgram)
        {
            oldProgram->release();
        }
    }
}

void Context::linkProgram(GLuint program)
{
    Program *programObject = mResourceManager->getProgram(program);

    bool linked = programObject->link();

    // if the current program was relinked successfully we
    // need to install the new executables
    if (linked && program == mState.currentProgram)
    {
        mCurrentProgramBinary.set(programObject->getProgramBinary());
        mDxUniformsDirty = true;
    }
}

void Context::setProgramBinary(GLuint program, const void *binary, GLint length)
{
    Program *programObject = mResourceManager->getProgram(program);

    bool loaded = programObject->setProgramBinary(binary, length);

    // if the current program was reloaded successfully we
    // need to install the new executables
    if (loaded && program == mState.currentProgram)
    {
        mCurrentProgramBinary.set(programObject->getProgramBinary());
        mDxUniformsDirty = true;
    }

}

void Context::beginQuery(GLenum target, GLuint query)
{
    // From EXT_occlusion_query_boolean: If BeginQueryEXT is called with an <id>  
    // of zero, if the active query object name for <target> is non-zero (for the  
    // targets ANY_SAMPLES_PASSED_EXT and ANY_SAMPLES_PASSED_CONSERVATIVE_EXT, if  
    // the active query for either target is non-zero), if <id> is the name of an 
    // existing query object whose type does not match <target>, or if <id> is the
    // active query object name for any query type, the error INVALID_OPERATION is
    // generated.

    // Ensure no other queries are active
    // NOTE: If other queries than occlusion are supported, we will need to check
    // separately that:
    //    a) The query ID passed is not the current active query for any target/type
    //    b) There are no active queries for the requested target (and in the case
    //       of GL_ANY_SAMPLES_PASSED_EXT and GL_ANY_SAMPLES_PASSED_CONSERVATIVE_EXT,
    //       no query may be active for either if glBeginQuery targets either.
    for (int i = 0; i < QUERY_TYPE_COUNT; i++)
    {
        if (mState.activeQuery[i].get() != NULL)
        {
            return error(GL_INVALID_OPERATION);
        }
    }

    QueryType qType;
    switch (target)
    {
      case GL_ANY_SAMPLES_PASSED_EXT: 
        qType = QUERY_ANY_SAMPLES_PASSED; 
        break;
      case GL_ANY_SAMPLES_PASSED_CONSERVATIVE_EXT: 
        qType = QUERY_ANY_SAMPLES_PASSED_CONSERVATIVE; 
        break;
      default: 
        ASSERT(false);
        return;
    }

    Query *queryObject = getQuery(query, true, target);

    // check that name was obtained with glGenQueries
    if (!queryObject)
    {
        return error(GL_INVALID_OPERATION);
    }

    // check for type mismatch
    if (queryObject->getType() != target)
    {
        return error(GL_INVALID_OPERATION);
    }

    // set query as active for specified target
    mState.activeQuery[qType].set(queryObject);

    // begin query
    queryObject->begin();
}

void Context::endQuery(GLenum target)
{
    QueryType qType;

    switch (target)
    {
      case GL_ANY_SAMPLES_PASSED_EXT: 
        qType = QUERY_ANY_SAMPLES_PASSED; 
        break;
      case GL_ANY_SAMPLES_PASSED_CONSERVATIVE_EXT: 
        qType = QUERY_ANY_SAMPLES_PASSED_CONSERVATIVE; 
        break;
      default: 
        ASSERT(false);
        return;
    }

    Query *queryObject = mState.activeQuery[qType].get();

    if (queryObject == NULL)
    {
        return error(GL_INVALID_OPERATION);
    }

    queryObject->end();

    mState.activeQuery[qType].set(NULL);
}

void Context::setFramebufferZero(Framebuffer *buffer)
{
    delete mFramebufferMap[0];
    mFramebufferMap[0] = buffer;
    if (mState.drawFramebuffer == 0)
    {
        mBoundDrawFramebuffer = buffer;
    }
}

void Context::setRenderbufferStorage(GLsizei width, GLsizei height, GLenum internalformat, GLsizei samples)
{
    RenderbufferStorage *renderbuffer = NULL;
    switch (internalformat)
    {
      case GL_DEPTH_COMPONENT16:
        renderbuffer = new gl::Depthbuffer(mRenderer, width, height, samples);
        break;
      case GL_RGBA4:
      case GL_RGB5_A1:
      case GL_RGB565:
      case GL_RGB8_OES:
      case GL_RGBA8_OES:
        renderbuffer = new gl::Colorbuffer(mRenderer,width, height, internalformat, samples);
        break;
      case GL_STENCIL_INDEX8:
        renderbuffer = new gl::Stencilbuffer(mRenderer, width, height, samples);
        break;
      case GL_DEPTH24_STENCIL8_OES:
        renderbuffer = new gl::DepthStencilbuffer(mRenderer, width, height, samples);
        break;
      default:
        UNREACHABLE(); return;
    }

    Renderbuffer *renderbufferObject = mState.renderbuffer.get();
    renderbufferObject->setStorage(renderbuffer);
}

Framebuffer *Context::getFramebuffer(unsigned int handle)
{
    FramebufferMap::iterator framebuffer = mFramebufferMap.find(handle);

    if (framebuffer == mFramebufferMap.end())
    {
        return NULL;
    }
    else
    {
        return framebuffer->second;
    }
}

Fence *Context::getFence(unsigned int handle)
{
    FenceMap::iterator fence = mFenceMap.find(handle);

    if (fence == mFenceMap.end())
    {
        return NULL;
    }
    else
    {
        return fence->second;
    }
}

Query *Context::getQuery(unsigned int handle, bool create, GLenum type)
{
    QueryMap::iterator query = mQueryMap.find(handle);

    if (query == mQueryMap.end())
    {
        return NULL;
    }
    else
    {
        if (!query->second && create)
        {
            query->second = new Query(mRenderer, handle, type);
            query->second->addRef();
        }
        return query->second;
    }
}

Buffer *Context::getArrayBuffer()
{
    return mState.arrayBuffer.get();
}

Buffer *Context::getElementArrayBuffer()
{
    return mState.elementArrayBuffer.get();
}

ProgramBinary *Context::getCurrentProgramBinary()
{
    return mCurrentProgramBinary.get();
}

Texture2D *Context::getTexture2D()
{
    return static_cast<Texture2D*>(getSamplerTexture(mState.activeSampler, TEXTURE_2D));
}

TextureCubeMap *Context::getTextureCubeMap()
{
    return static_cast<TextureCubeMap*>(getSamplerTexture(mState.activeSampler, TEXTURE_CUBE));
}

Texture *Context::getSamplerTexture(unsigned int sampler, TextureType type)
{
    GLuint texid = mState.samplerTexture[type][sampler].id();

    if (texid == 0)   // Special case: 0 refers to different initial textures based on the target
    {
        switch (type)
        {
          default: UNREACHABLE();
          case TEXTURE_2D: return mTexture2DZero.get();
          case TEXTURE_CUBE: return mTextureCubeMapZero.get();
        }
    }

    return mState.samplerTexture[type][sampler].get();
}

bool Context::getBooleanv(GLenum pname, GLboolean *params)
{
    switch (pname)
    {
      case GL_SHADER_COMPILER:           *params = GL_TRUE;                             break;
      case GL_SAMPLE_COVERAGE_INVERT:    *params = mState.sampleCoverageInvert;         break;
      case GL_DEPTH_WRITEMASK:           *params = mState.depthStencil.depthMask;       break;
      case GL_COLOR_WRITEMASK:
        params[0] = mState.blend.colorMaskRed;
        params[1] = mState.blend.colorMaskGreen;
        params[2] = mState.blend.colorMaskBlue;
        params[3] = mState.blend.colorMaskAlpha;
        break;
      case GL_CULL_FACE:                 *params = mState.rasterizer.cullFace;          break;
      case GL_POLYGON_OFFSET_FILL:       *params = mState.rasterizer.polygonOffsetFill; break;
      case GL_SAMPLE_ALPHA_TO_COVERAGE:  *params = mState.blend.sampleAlphaToCoverage;  break;
      case GL_SAMPLE_COVERAGE:           *params = mState.sampleCoverage;               break;
      case GL_SCISSOR_TEST:              *params = mState.rasterizer.scissorTest;       break;
      case GL_STENCIL_TEST:              *params = mState.depthStencil.stencilTest;     break;
      case GL_DEPTH_TEST:                *params = mState.depthStencil.depthTest;       break;
      case GL_BLEND:                     *params = mState.blend.blend;                  break;
      case GL_DITHER:                    *params = mState.blend.dither;                 break;
      case GL_CONTEXT_ROBUST_ACCESS_EXT: *params = mRobustAccess ? GL_TRUE : GL_FALSE;  break;
      default:
        return false;
    }

    return true;
}

bool Context::getFloatv(GLenum pname, GLfloat *params)
{
    // Please note: DEPTH_CLEAR_VALUE is included in our internal getFloatv implementation
    // because it is stored as a float, despite the fact that the GL ES 2.0 spec names
    // GetIntegerv as its native query function. As it would require conversion in any
    // case, this should make no difference to the calling application.
    switch (pname)
    {
      case GL_LINE_WIDTH:               *params = mState.lineWidth;                         break;
      case GL_SAMPLE_COVERAGE_VALUE:    *params = mState.sampleCoverageValue;               break;
      case GL_DEPTH_CLEAR_VALUE:        *params = mState.depthClearValue;                   break;
      case GL_POLYGON_OFFSET_FACTOR:    *params = mState.rasterizer.polygonOffsetFactor;    break;
      case GL_POLYGON_OFFSET_UNITS:     *params = mState.rasterizer.polygonOffsetUnits;     break;
      case GL_ALIASED_LINE_WIDTH_RANGE:
        params[0] = gl::ALIASED_LINE_WIDTH_RANGE_MIN;
        params[1] = gl::ALIASED_LINE_WIDTH_RANGE_MAX;
        break;
      case GL_ALIASED_POINT_SIZE_RANGE:
        params[0] = gl::ALIASED_POINT_SIZE_RANGE_MIN;
        params[1] = getMaximumPointSize();
        break;
      case GL_DEPTH_RANGE:
        params[0] = mState.zNear;
        params[1] = mState.zFar;
        break;
      case GL_COLOR_CLEAR_VALUE:
        params[0] = mState.colorClearValue.red;
        params[1] = mState.colorClearValue.green;
        params[2] = mState.colorClearValue.blue;
        params[3] = mState.colorClearValue.alpha;
        break;
      case GL_BLEND_COLOR:
        params[0] = mState.blendColor.red;
        params[1] = mState.blendColor.green;
        params[2] = mState.blendColor.blue;
        params[3] = mState.blendColor.alpha;
        break;
      case GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT:
        if (!supportsTextureFilterAnisotropy())
        {
            return false;
        }
        *params = mMaxTextureAnisotropy;
        break;
      default:
        return false;
    }

    return true;
}

bool Context::getIntegerv(GLenum pname, GLint *params)
{
    // Please note: DEPTH_CLEAR_VALUE is not included in our internal getIntegerv implementation
    // because it is stored as a float, despite the fact that the GL ES 2.0 spec names
    // GetIntegerv as its native query function. As it would require conversion in any
    // case, this should make no difference to the calling application. You may find it in 
    // Context::getFloatv.
    switch (pname)
    {
      case GL_MAX_VERTEX_ATTRIBS:               *params = gl::MAX_VERTEX_ATTRIBS;               break;
      case GL_MAX_VERTEX_UNIFORM_VECTORS:       *params = gl::MAX_VERTEX_UNIFORM_VECTORS;       break;
      case GL_MAX_VARYING_VECTORS:              *params = getMaximumVaryingVectors();           break;
      case GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS: *params = getMaximumCombinedTextureImageUnits(); break;
      case GL_MAX_VERTEX_TEXTURE_IMAGE_UNITS:   *params = getMaximumVertexTextureImageUnits();  break;
      case GL_MAX_TEXTURE_IMAGE_UNITS:          *params = gl::MAX_TEXTURE_IMAGE_UNITS;          break;
      case GL_MAX_FRAGMENT_UNIFORM_VECTORS:     *params = getMaximumFragmentUniformVectors();   break;
      case GL_MAX_RENDERBUFFER_SIZE:            *params = getMaximumRenderbufferDimension();    break;
      case GL_NUM_SHADER_BINARY_FORMATS:        *params = 0;                                    break;
      case GL_SHADER_BINARY_FORMATS:      /* no shader binary formats are supported */          break;
      case GL_ARRAY_BUFFER_BINDING:             *params = mState.arrayBuffer.id();              break;
      case GL_ELEMENT_ARRAY_BUFFER_BINDING:     *params = mState.elementArrayBuffer.id();       break;
      //case GL_FRAMEBUFFER_BINDING:            // now equivalent to GL_DRAW_FRAMEBUFFER_BINDING_ANGLE
      case GL_DRAW_FRAMEBUFFER_BINDING_ANGLE:   *params = mState.drawFramebuffer;               break;
      case GL_READ_FRAMEBUFFER_BINDING_ANGLE:   *params = mState.readFramebuffer;               break;
      case GL_RENDERBUFFER_BINDING:             *params = mState.renderbuffer.id();             break;
      case GL_CURRENT_PROGRAM:                  *params = mState.currentProgram;                break;
      case GL_PACK_ALIGNMENT:                   *params = mState.packAlignment;                 break;
      case GL_PACK_REVERSE_ROW_ORDER_ANGLE:     *params = mState.packReverseRowOrder;           break;
      case GL_UNPACK_ALIGNMENT:                 *params = mState.unpackAlignment;               break;
      case GL_GENERATE_MIPMAP_HINT:             *params = mState.generateMipmapHint;            break;
      case GL_FRAGMENT_SHADER_DERIVATIVE_HINT_OES: *params = mState.fragmentShaderDerivativeHint; break;
      case GL_ACTIVE_TEXTURE:                   *params = (mState.activeSampler + GL_TEXTURE0); break;
      case GL_STENCIL_FUNC:                     *params = mState.depthStencil.stencilFunc;             break;
      case GL_STENCIL_REF:                      *params = mState.stencilRef;                           break;
      case GL_STENCIL_VALUE_MASK:               *params = mState.depthStencil.stencilMask;             break;
      case GL_STENCIL_BACK_FUNC:                *params = mState.depthStencil.stencilBackFunc;         break;
      case GL_STENCIL_BACK_REF:                 *params = mState.stencilBackRef;                       break;
      case GL_STENCIL_BACK_VALUE_MASK:          *params = mState.depthStencil.stencilBackMask;         break;
      case GL_STENCIL_FAIL:                     *params = mState.depthStencil.stencilFail;             break;
      case GL_STENCIL_PASS_DEPTH_FAIL:          *params = mState.depthStencil.stencilPassDepthFail;    break;
      case GL_STENCIL_PASS_DEPTH_PASS:          *params = mState.depthStencil.stencilPassDepthPass;    break;
      case GL_STENCIL_BACK_FAIL:                *params = mState.depthStencil.stencilBackFail;         break;
      case GL_STENCIL_BACK_PASS_DEPTH_FAIL:     *params = mState.depthStencil.stencilBackPassDepthFail; break;
      case GL_STENCIL_BACK_PASS_DEPTH_PASS:     *params = mState.depthStencil.stencilBackPassDepthPass; break;
      case GL_DEPTH_FUNC:                       *params = mState.depthStencil.depthFunc;               break;
      case GL_BLEND_SRC_RGB:                    *params = mState.blend.sourceBlendRGB;                 break;
      case GL_BLEND_SRC_ALPHA:                  *params = mState.blend.sourceBlendAlpha;               break;
      case GL_BLEND_DST_RGB:                    *params = mState.blend.destBlendRGB;                   break;
      case GL_BLEND_DST_ALPHA:                  *params = mState.blend.destBlendAlpha;                 break;
      case GL_BLEND_EQUATION_RGB:               *params = mState.blend.blendEquationRGB;               break;
      case GL_BLEND_EQUATION_ALPHA:             *params = mState.blend.blendEquationAlpha;             break;
      case GL_STENCIL_WRITEMASK:                *params = mState.depthStencil.stencilWritemask;        break;
      case GL_STENCIL_BACK_WRITEMASK:           *params = mState.depthStencil.stencilBackWritemask;    break;
      case GL_STENCIL_CLEAR_VALUE:              *params = mState.stencilClearValue;             break;
      case GL_SUBPIXEL_BITS:                    *params = 4;                                    break;
      case GL_MAX_TEXTURE_SIZE:                 *params = getMaximumTextureDimension();         break;
      case GL_MAX_CUBE_MAP_TEXTURE_SIZE:        *params = getMaximumCubeTextureDimension();     break;
      case GL_NUM_COMPRESSED_TEXTURE_FORMATS:   
        params[0] = mNumCompressedTextureFormats;
        break;
      case GL_MAX_SAMPLES_ANGLE:
        {
            GLsizei maxSamples = getMaxSupportedSamples();
            if (maxSamples != 0)
            {
                *params = maxSamples;
            }
            else
            {
                return false;
            }

            break;
        }
      case GL_SAMPLE_BUFFERS:                   
      case GL_SAMPLES:
        {
            gl::Framebuffer *framebuffer = getDrawFramebuffer();
            if (framebuffer->completeness() == GL_FRAMEBUFFER_COMPLETE)
            {
                switch (pname)
                {
                  case GL_SAMPLE_BUFFERS:
                    if (framebuffer->getSamples() != 0)
                    {
                        *params = 1;
                    }
                    else
                    {
                        *params = 0;
                    }
                    break;
                  case GL_SAMPLES:
                    *params = framebuffer->getSamples();
                    break;
                }
            }
            else 
            {
                *params = 0;
            }
        }
        break;
      case GL_IMPLEMENTATION_COLOR_READ_TYPE:
      case GL_IMPLEMENTATION_COLOR_READ_FORMAT:
        {
            GLenum format, type;
            if (getCurrentReadFormatType(&format, &type))
            {
                if (pname == GL_IMPLEMENTATION_COLOR_READ_FORMAT)
                    *params = format;
                else
                    *params = type;
            }
        }
        break;
      case GL_MAX_VIEWPORT_DIMS:
        {
            int maxDimension = std::max(getMaximumRenderbufferDimension(), getMaximumTextureDimension());
            params[0] = maxDimension;
            params[1] = maxDimension;
        }
        break;
      case GL_COMPRESSED_TEXTURE_FORMATS:
        {
            if (supportsDXT1Textures())
            {
                *params++ = GL_COMPRESSED_RGB_S3TC_DXT1_EXT;
                *params++ = GL_COMPRESSED_RGBA_S3TC_DXT1_EXT;
            }
            if (supportsDXT3Textures())
            {
                *params++ = GL_COMPRESSED_RGBA_S3TC_DXT3_ANGLE;
            }
            if (supportsDXT5Textures())
            {
                *params++ = GL_COMPRESSED_RGBA_S3TC_DXT5_ANGLE;
            }
        }
        break;
      case GL_VIEWPORT:
        params[0] = mState.viewport.x;
        params[1] = mState.viewport.y;
        params[2] = mState.viewport.width;
        params[3] = mState.viewport.height;
        break;
      case GL_SCISSOR_BOX:
        params[0] = mState.scissor.x;
        params[1] = mState.scissor.y;
        params[2] = mState.scissor.width;
        params[3] = mState.scissor.height;
        break;
      case GL_CULL_FACE_MODE:                   *params = mState.rasterizer.cullMode;   break;
      case GL_FRONT_FACE:                       *params = mState.rasterizer.frontFace;  break;
      case GL_RED_BITS:
      case GL_GREEN_BITS:
      case GL_BLUE_BITS:
      case GL_ALPHA_BITS:
        {
            gl::Framebuffer *framebuffer = getDrawFramebuffer();
            gl::Renderbuffer *colorbuffer = framebuffer->getColorbuffer();

            if (colorbuffer)
            {
                switch (pname)
                {
                  case GL_RED_BITS:   *params = colorbuffer->getRedSize();      break;
                  case GL_GREEN_BITS: *params = colorbuffer->getGreenSize();    break;
                  case GL_BLUE_BITS:  *params = colorbuffer->getBlueSize();     break;
                  case GL_ALPHA_BITS: *params = colorbuffer->getAlphaSize();    break;
                }
            }
            else
            {
                *params = 0;
            }
        }
        break;
      case GL_DEPTH_BITS:
        {
            gl::Framebuffer *framebuffer = getDrawFramebuffer();
            gl::Renderbuffer *depthbuffer = framebuffer->getDepthbuffer();

            if (depthbuffer)
            {
                *params = depthbuffer->getDepthSize();
            }
            else
            {
                *params = 0;
            }
        }
        break;
      case GL_STENCIL_BITS:
        {
            gl::Framebuffer *framebuffer = getDrawFramebuffer();
            gl::Renderbuffer *stencilbuffer = framebuffer->getStencilbuffer();

            if (stencilbuffer)
            {
                *params = stencilbuffer->getStencilSize();
            }
            else
            {
                *params = 0;
            }
        }
        break;
      case GL_TEXTURE_BINDING_2D:
        {
            if (mState.activeSampler < 0 || mState.activeSampler > getMaximumCombinedTextureImageUnits() - 1)
            {
                error(GL_INVALID_OPERATION);
                return false;
            }

            *params = mState.samplerTexture[TEXTURE_2D][mState.activeSampler].id();
        }
        break;
      case GL_TEXTURE_BINDING_CUBE_MAP:
        {
            if (mState.activeSampler < 0 || mState.activeSampler > getMaximumCombinedTextureImageUnits() - 1)
            {
                error(GL_INVALID_OPERATION);
                return false;
            }

            *params = mState.samplerTexture[TEXTURE_CUBE][mState.activeSampler].id();
        }
        break;
      case GL_RESET_NOTIFICATION_STRATEGY_EXT:
        *params = mResetStrategy;
        break;
      case GL_NUM_PROGRAM_BINARY_FORMATS_OES:
        *params = 1;
        break;
      case GL_PROGRAM_BINARY_FORMATS_OES:
        *params = GL_PROGRAM_BINARY_ANGLE;
        break;
      default:
        return false;
    }

    return true;
}

bool Context::getQueryParameterInfo(GLenum pname, GLenum *type, unsigned int *numParams)
{
    // Please note: the query type returned for DEPTH_CLEAR_VALUE in this implementation
    // is FLOAT rather than INT, as would be suggested by the GL ES 2.0 spec. This is due
    // to the fact that it is stored internally as a float, and so would require conversion
    // if returned from Context::getIntegerv. Since this conversion is already implemented 
    // in the case that one calls glGetIntegerv to retrieve a float-typed state variable, we
    // place DEPTH_CLEAR_VALUE with the floats. This should make no difference to the calling
    // application.
    switch (pname)
    {
      case GL_COMPRESSED_TEXTURE_FORMATS:
        {
            *type = GL_INT;
            *numParams = mNumCompressedTextureFormats;
        }
        break;
      case GL_SHADER_BINARY_FORMATS:
        {
            *type = GL_INT;
            *numParams = 0;
        }
        break;
      case GL_MAX_VERTEX_ATTRIBS:
      case GL_MAX_VERTEX_UNIFORM_VECTORS:
      case GL_MAX_VARYING_VECTORS:
      case GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS:
      case GL_MAX_VERTEX_TEXTURE_IMAGE_UNITS:
      case GL_MAX_TEXTURE_IMAGE_UNITS:
      case GL_MAX_FRAGMENT_UNIFORM_VECTORS:
      case GL_MAX_RENDERBUFFER_SIZE:
      case GL_NUM_SHADER_BINARY_FORMATS:
      case GL_NUM_COMPRESSED_TEXTURE_FORMATS:
      case GL_ARRAY_BUFFER_BINDING:
      case GL_FRAMEBUFFER_BINDING:
      case GL_RENDERBUFFER_BINDING:
      case GL_CURRENT_PROGRAM:
      case GL_PACK_ALIGNMENT:
      case GL_PACK_REVERSE_ROW_ORDER_ANGLE:
      case GL_UNPACK_ALIGNMENT:
      case GL_GENERATE_MIPMAP_HINT:
      case GL_FRAGMENT_SHADER_DERIVATIVE_HINT_OES:
      case GL_RED_BITS:
      case GL_GREEN_BITS:
      case GL_BLUE_BITS:
      case GL_ALPHA_BITS:
      case GL_DEPTH_BITS:
      case GL_STENCIL_BITS:
      case GL_ELEMENT_ARRAY_BUFFER_BINDING:
      case GL_CULL_FACE_MODE:
      case GL_FRONT_FACE:
      case GL_ACTIVE_TEXTURE:
      case GL_STENCIL_FUNC:
      case GL_STENCIL_VALUE_MASK:
      case GL_STENCIL_REF:
      case GL_STENCIL_FAIL:
      case GL_STENCIL_PASS_DEPTH_FAIL:
      case GL_STENCIL_PASS_DEPTH_PASS:
      case GL_STENCIL_BACK_FUNC:
      case GL_STENCIL_BACK_VALUE_MASK:
      case GL_STENCIL_BACK_REF:
      case GL_STENCIL_BACK_FAIL:
      case GL_STENCIL_BACK_PASS_DEPTH_FAIL:
      case GL_STENCIL_BACK_PASS_DEPTH_PASS:
      case GL_DEPTH_FUNC:
      case GL_BLEND_SRC_RGB:
      case GL_BLEND_SRC_ALPHA:
      case GL_BLEND_DST_RGB:
      case GL_BLEND_DST_ALPHA:
      case GL_BLEND_EQUATION_RGB:
      case GL_BLEND_EQUATION_ALPHA:
      case GL_STENCIL_WRITEMASK:
      case GL_STENCIL_BACK_WRITEMASK:
      case GL_STENCIL_CLEAR_VALUE:
      case GL_SUBPIXEL_BITS:
      case GL_MAX_TEXTURE_SIZE:
      case GL_MAX_CUBE_MAP_TEXTURE_SIZE:
      case GL_SAMPLE_BUFFERS:
      case GL_SAMPLES:
      case GL_IMPLEMENTATION_COLOR_READ_TYPE:
      case GL_IMPLEMENTATION_COLOR_READ_FORMAT:
      case GL_TEXTURE_BINDING_2D:
      case GL_TEXTURE_BINDING_CUBE_MAP:
      case GL_RESET_NOTIFICATION_STRATEGY_EXT:
      case GL_NUM_PROGRAM_BINARY_FORMATS_OES:
      case GL_PROGRAM_BINARY_FORMATS_OES:
        {
            *type = GL_INT;
            *numParams = 1;
        }
        break;
      case GL_MAX_SAMPLES_ANGLE:
        {
            if (getMaxSupportedSamples() != 0)
            {
                *type = GL_INT;
                *numParams = 1;
            }
            else
            {
                return false;
            }
        }
        break;
      case GL_MAX_VIEWPORT_DIMS:
        {
            *type = GL_INT;
            *numParams = 2;
        }
        break;
      case GL_VIEWPORT:
      case GL_SCISSOR_BOX:
        {
            *type = GL_INT;
            *numParams = 4;
        }
        break;
      case GL_SHADER_COMPILER:
      case GL_SAMPLE_COVERAGE_INVERT:
      case GL_DEPTH_WRITEMASK:
      case GL_CULL_FACE:                // CULL_FACE through DITHER are natural to IsEnabled,
      case GL_POLYGON_OFFSET_FILL:      // but can be retrieved through the Get{Type}v queries.
      case GL_SAMPLE_ALPHA_TO_COVERAGE: // For this purpose, they are treated here as bool-natural
      case GL_SAMPLE_COVERAGE:
      case GL_SCISSOR_TEST:
      case GL_STENCIL_TEST:
      case GL_DEPTH_TEST:
      case GL_BLEND:
      case GL_DITHER:
      case GL_CONTEXT_ROBUST_ACCESS_EXT:
        {
            *type = GL_BOOL;
            *numParams = 1;
        }
        break;
      case GL_COLOR_WRITEMASK:
        {
            *type = GL_BOOL;
            *numParams = 4;
        }
        break;
      case GL_POLYGON_OFFSET_FACTOR:
      case GL_POLYGON_OFFSET_UNITS:
      case GL_SAMPLE_COVERAGE_VALUE:
      case GL_DEPTH_CLEAR_VALUE:
      case GL_LINE_WIDTH:
        {
            *type = GL_FLOAT;
            *numParams = 1;
        }
        break;
      case GL_ALIASED_LINE_WIDTH_RANGE:
      case GL_ALIASED_POINT_SIZE_RANGE:
      case GL_DEPTH_RANGE:
        {
            *type = GL_FLOAT;
            *numParams = 2;
        }
        break;
      case GL_COLOR_CLEAR_VALUE:
      case GL_BLEND_COLOR:
        {
            *type = GL_FLOAT;
            *numParams = 4;
        }
        break;
      case GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT:
        if (!supportsTextureFilterAnisotropy())
        {
            return false;
        }
        *type = GL_FLOAT;
        *numParams = 1;
        break;
      default:
        return false;
    }

    return true;
}

// Applies the render target surface, depth stencil surface, viewport rectangle and
// scissor rectangle to the Direct3D 9 device
bool Context::applyRenderTarget(bool ignoreViewport)
{
    Framebuffer *framebufferObject = getDrawFramebuffer();

    if (!framebufferObject || framebufferObject->completeness() != GL_FRAMEBUFFER_COMPLETE)
    {
        return error(GL_INVALID_FRAMEBUFFER_OPERATION, false);
    }

    mRenderer->applyRenderTarget(framebufferObject);

    // if there is no color attachment we must synthesize a NULL colorattachment
    // to keep the D3D runtime happy.  This should only be possible if depth texturing.
    Renderbuffer *renderbufferObject = NULL;
    if (framebufferObject->getColorbufferType() != GL_NONE)
    {
        renderbufferObject = framebufferObject->getColorbuffer();
    }
    else
    {
        renderbufferObject = framebufferObject->getNullColorbuffer();
    }
    if (!renderbufferObject)
    {
        ERR("unable to locate renderbuffer for FBO.");
        return false;
    }

    bool renderTargetChanged = false;
    unsigned int renderTargetSerial = renderbufferObject->getSerial();
    if (renderTargetSerial != mAppliedRenderTargetSerial)
    {
        if (!mRenderer->setRenderTarget(renderbufferObject))
        {
            return false;   // Context must be lost
        }
        mAppliedRenderTargetSerial = renderTargetSerial;
        renderTargetChanged = true;
    }

    Renderbuffer *depthStencil = NULL;
    unsigned int depthbufferSerial = 0;
    unsigned int stencilbufferSerial = 0;
    if (framebufferObject->getDepthbufferType() != GL_NONE)
    {
        depthStencil = framebufferObject->getDepthbuffer();
        if (!depthStencil)
        {
            ERR("Depth stencil pointer unexpectedly null.");
            return false;
        }
        
        depthbufferSerial = depthStencil->getSerial();
    }
    else if (framebufferObject->getStencilbufferType() != GL_NONE)
    {
        depthStencil = framebufferObject->getStencilbuffer();
        if (!depthStencil)
        {
            ERR("Depth stencil pointer unexpectedly null.");
            return false;
        }
        
        stencilbufferSerial = depthStencil->getSerial();
    }

    if (depthbufferSerial != mAppliedDepthbufferSerial ||
        stencilbufferSerial != mAppliedStencilbufferSerial ||
        !mDepthStencilInitialized)
    {
        mRenderer->setDepthStencil(depthStencil);
        mAppliedDepthbufferSerial = depthbufferSerial;
        mAppliedStencilbufferSerial = stencilbufferSerial;
        mDepthStencilInitialized = true;
    }

    if (!mRenderTargetDescInitialized || renderTargetChanged)
    {
        mRenderTargetDesc.width = renderbufferObject->getWidth();
        mRenderTargetDesc.height = renderbufferObject->getHeight();
        mRenderTargetDesc.format = renderbufferObject->getActualFormat();
        mRenderTargetDescInitialized = true;
    }

    D3DVIEWPORT9 viewport;

    float zNear = clamp01(mState.zNear);
    float zFar = clamp01(mState.zFar);

    if (ignoreViewport)
    {
        viewport.X = 0;
        viewport.Y = 0;
        viewport.Width = mRenderTargetDesc.width;
        viewport.Height = mRenderTargetDesc.height;
        viewport.MinZ = 0.0f;
        viewport.MaxZ = 1.0f;
    }
    else
    {
        viewport.X = clamp(mState.viewport.x, 0L, static_cast<LONG>(mRenderTargetDesc.width));
        viewport.Y = clamp(mState.viewport.y, 0L, static_cast<LONG>(mRenderTargetDesc.height));
        viewport.Width = clamp(mState.viewport.width, 0L, static_cast<LONG>(mRenderTargetDesc.width) - static_cast<LONG>(viewport.X));
        viewport.Height = clamp(mState.viewport.height, 0L, static_cast<LONG>(mRenderTargetDesc.height) - static_cast<LONG>(viewport.Y));
    }

    if (viewport.Width <= 0 || viewport.Height <= 0)
    {
        return false;   // Nothing to render
    }

    if (renderTargetChanged || !mViewportInitialized || memcmp(&viewport, &mSetViewport, sizeof mSetViewport) != 0)
    {
        mDevice->SetViewport(&viewport);
        mSetViewport = viewport;
        mViewportInitialized = true;
        mDxUniformsDirty = true;
    }

    mRenderer->setScissorRectangle(mState.scissor, static_cast<int>(mRenderTargetDesc.width),
                                    static_cast<int>(mRenderTargetDesc.height));

    if (mState.currentProgram && mDxUniformsDirty)
    {
        ProgramBinary *programBinary = getCurrentProgramBinary();

        GLint halfPixelSize = programBinary->getDxHalfPixelSizeLocation();
        GLfloat xy[2] = {1.0f / viewport.Width, -1.0f / viewport.Height};
        programBinary->setUniform2fv(halfPixelSize, 1, xy);

        // These values are used for computing gl_FragCoord in Program::linkVaryings().
        GLint coord = programBinary->getDxCoordLocation();
        GLfloat whxy[4] = {mState.viewport.width / 2.0f, mState.viewport.height / 2.0f,
                          (float)mState.viewport.x + mState.viewport.width / 2.0f,
                          (float)mState.viewport.y + mState.viewport.height / 2.0f};
        programBinary->setUniform4fv(coord, 1, whxy);

        GLint depth = programBinary->getDxDepthLocation();
        GLfloat dz[2] = {(zFar - zNear) / 2.0f, (zNear + zFar) / 2.0f};
        programBinary->setUniform2fv(depth, 1, dz);

        GLint depthRange = programBinary->getDxDepthRangeLocation();
        GLfloat nearFarDiff[3] = {zNear, zFar, zFar - zNear};
        programBinary->setUniform3fv(depthRange, 1, nearFarDiff);
        mDxUniformsDirty = false;
    }

    return true;
}

// Applies the fixed-function state (culling, depth test, alpha blending, stenciling, etc) to the Direct3D 9 device
void Context::applyState(GLenum drawMode)
{
    ProgramBinary *programBinary = getCurrentProgramBinary();

    Framebuffer *framebufferObject = getDrawFramebuffer();

    GLint frontCCW = programBinary->getDxFrontCCWLocation();
    GLint ccw = (mState.rasterizer.frontFace == GL_CCW);
    programBinary->setUniform1iv(frontCCW, 1, &ccw);

    GLint pointsOrLines = programBinary->getDxPointsOrLinesLocation();
    GLint alwaysFront = !isTriangleMode(drawMode);
    programBinary->setUniform1iv(pointsOrLines, 1, &alwaysFront);

    const gl::Renderbuffer *depthbuffer = framebufferObject->getDepthbuffer();
    unsigned int depthSize = depthbuffer ? depthbuffer->getDepthSize() : 0;

    mRenderer->setRasterizerState(mState.rasterizer, depthSize);

    unsigned int mask = 0;
    if (mState.sampleCoverage)
    {
        if (mState.sampleCoverageValue != 0)
        {
            float threshold = 0.5f;

            for (int i = 0; i < framebufferObject->getSamples(); ++i)
            {
                mask <<= 1;

                if ((i + 1) * mState.sampleCoverageValue >= threshold)
                {
                    threshold += 1.0f;
                    mask |= 1;
                }
            }
        }

        if (mState.sampleCoverageInvert)
        {
            mask = ~mask;
        }
    }
    else
    {
        mask = 0xFFFFFFFF;
    }
    mRenderer->setBlendState(mState.blend, mState.blendColor, mask);

    unsigned int stencilSize = framebufferObject->hasStencil() ? framebufferObject->getStencilbuffer()->getStencilSize() : 0;
    mRenderer->setDepthStencilState(mState.depthStencil, mState.stencilRef, mState.stencilBackRef,
                                    mState.rasterizer.frontFace == GL_CCW, stencilSize);
}

GLenum Context::applyVertexBuffer(GLint first, GLsizei count, GLsizei instances, GLsizei *repeatDraw)
{
    TranslatedAttribute attributes[MAX_VERTEX_ATTRIBS];

    ProgramBinary *programBinary = getCurrentProgramBinary();
    GLenum err = mVertexDataManager->prepareVertexData(mState.vertexAttribute, programBinary, first, count, attributes, instances);
    if (err != GL_NO_ERROR)
    {
        return err;
    }
    
    return mVertexDeclarationCache.applyDeclaration(mDevice, attributes, programBinary, instances, repeatDraw);
}

// Applies the indices and element array bindings to the Direct3D 9 device
GLenum Context::applyIndexBuffer(const GLvoid *indices, GLsizei count, GLenum mode, GLenum type, TranslatedIndexData *indexInfo)
{
    GLenum err = mIndexDataManager->prepareIndexData(type, count, mState.elementArrayBuffer.get(), indices, indexInfo);

    if (err == GL_NO_ERROR)
    {
        if (indexInfo->serial != mAppliedIBSerial)
        {
            mDevice->SetIndices(indexInfo->indexBuffer);
            mAppliedIBSerial = indexInfo->serial;
        }
    }

    return err;
}

// Applies the shaders and shader constants to the Direct3D 9 device
void Context::applyShaders()
{
    ProgramBinary *programBinary = getCurrentProgramBinary();

    if (programBinary->getSerial() != mAppliedProgramBinarySerial)
    {
        IDirect3DVertexShader9 *vertexShader = programBinary->getVertexShader();
        IDirect3DPixelShader9 *pixelShader = programBinary->getPixelShader();

        mDevice->SetPixelShader(pixelShader);
        mDevice->SetVertexShader(vertexShader);
        programBinary->dirtyAllUniforms();
        mAppliedProgramBinarySerial = programBinary->getSerial();
    }

    programBinary->applyUniforms();
}

// Applies the textures and sampler states to the Direct3D 9 device
void Context::applyTextures()
{
    applyTextures(SAMPLER_PIXEL);

    if (mSupportsVertexTexture)
    {
        applyTextures(SAMPLER_VERTEX);
    }
}

// For each Direct3D 9 sampler of either the pixel or vertex stage,
// looks up the corresponding OpenGL texture image unit and texture type,
// and sets the texture and its addressing/filtering state (or NULL when inactive).
void Context::applyTextures(SamplerType type)
{
    ProgramBinary *programBinary = getCurrentProgramBinary();

    int samplerCount = (type == SAMPLER_PIXEL) ? MAX_TEXTURE_IMAGE_UNITS : MAX_VERTEX_TEXTURE_IMAGE_UNITS_VTF;   // Range of Direct3D 9 samplers of given sampler type
    unsigned int *appliedTextureSerial = (type == SAMPLER_PIXEL) ? mAppliedTextureSerialPS : mAppliedTextureSerialVS;
    int samplerRange = programBinary->getUsedSamplerRange(type);

    for (int samplerIndex = 0; samplerIndex < samplerRange; samplerIndex++)
    {
        int textureUnit = programBinary->getSamplerMapping(type, samplerIndex);   // OpenGL texture image unit index

        if (textureUnit != -1)
        {
            TextureType textureType = programBinary->getSamplerTextureType(type, samplerIndex);

            Texture *texture = getSamplerTexture(textureUnit, textureType);
            unsigned int texSerial = texture->getTextureSerial();

            if (appliedTextureSerial[samplerIndex] != texSerial || texture->hasDirtyParameters() || texture->hasDirtyImages())
            {
                if (texture->isSamplerComplete())
                {
                    if (appliedTextureSerial[samplerIndex] != texSerial || texture->hasDirtyParameters())
                    {
                        SamplerState samplerState;
                        texture->getSamplerState(&samplerState);

                        mRenderer->setSamplerState(type, samplerIndex, samplerState);
                    }

                    if (appliedTextureSerial[samplerIndex] != texSerial || texture->hasDirtyImages())
                    {
                        mRenderer->setTexture(type, samplerIndex, texture);
                    }
                }
                else
                {
                    mRenderer->setTexture(type, samplerIndex, getIncompleteTexture(textureType));
                }

                appliedTextureSerial[samplerIndex] = texSerial;
                texture->resetDirty();
            }
        }
        else
        {
            if (appliedTextureSerial[samplerIndex] != 0)
            {
                mRenderer->setTexture(type, samplerIndex, NULL);
                appliedTextureSerial[samplerIndex] = 0;
            }
        }
    }

    for (int samplerIndex = samplerRange; samplerIndex < samplerCount; samplerIndex++)
    {
        if (appliedTextureSerial[samplerIndex] != 0)
        {
            mRenderer->setTexture(type, samplerIndex, NULL);
            appliedTextureSerial[samplerIndex] = 0;
        }
    }
}

void Context::readPixels(GLint x, GLint y, GLsizei width, GLsizei height,
                         GLenum format, GLenum type, GLsizei *bufSize, void* pixels)
{
    Framebuffer *framebuffer = getReadFramebuffer();

    if (framebuffer->completeness() != GL_FRAMEBUFFER_COMPLETE)
    {
        return error(GL_INVALID_FRAMEBUFFER_OPERATION);
    }

    if (getReadFramebufferHandle() != 0 && framebuffer->getSamples() != 0)
    {
        return error(GL_INVALID_OPERATION);
    }

    GLsizei outputPitch = ComputePitch(width, ConvertSizedInternalFormat(format, type), getPackAlignment());
    // sized query sanity check
    if (bufSize)
    {
        int requiredSize = outputPitch * height;
        if (requiredSize > *bufSize)
        {
            return error(GL_INVALID_OPERATION);
        }
    }

    mRenderer->readPixels(framebuffer, x, y, width, height, format, type, outputPitch, getPackReverseRowOrder(), getPackAlignment(), pixels);
}

void Context::clear(GLbitfield mask)
{
    Framebuffer *framebufferObject = getDrawFramebuffer();

    if (!framebufferObject || framebufferObject->completeness() != GL_FRAMEBUFFER_COMPLETE)
    {
        return error(GL_INVALID_FRAMEBUFFER_OPERATION);
    }

    DWORD flags = 0;

    if (mask & GL_COLOR_BUFFER_BIT)
    {
        mask &= ~GL_COLOR_BUFFER_BIT;

        if (framebufferObject->getColorbufferType() != GL_NONE)
        {
            flags |= D3DCLEAR_TARGET;
        }
    }

    if (mask & GL_DEPTH_BUFFER_BIT)
    {
        mask &= ~GL_DEPTH_BUFFER_BIT;
        if (mState.depthStencil.depthMask && framebufferObject->getDepthbufferType() != GL_NONE)
        {
            flags |= D3DCLEAR_ZBUFFER;
        }
    }

    GLuint stencilUnmasked = 0x0;

    if (mask & GL_STENCIL_BUFFER_BIT)
    {
        mask &= ~GL_STENCIL_BUFFER_BIT;
        if (framebufferObject->getStencilbufferType() != GL_NONE)
        {
            rx::RenderTarget *depthStencil = framebufferObject->getStencilbuffer()->getDepthStencil();
            if (!depthStencil)
            {
                ERR("Depth stencil pointer unexpectedly null.");
                return;
            }

            unsigned int stencilSize = gl::GetStencilSize(depthStencil->getActualFormat());
            stencilUnmasked = (0x1 << stencilSize) - 1;

            if (stencilUnmasked != 0x0)
            {
                flags |= D3DCLEAR_STENCIL;
            }
        }
    }

    if (mask != 0)
    {
        return error(GL_INVALID_VALUE);
    }

    if (!applyRenderTarget(true))   // Clips the clear to the scissor rectangle but not the viewport
    {
        return;
    }

    D3DCOLOR color = D3DCOLOR_ARGB(unorm<8>(mState.colorClearValue.alpha), 
                                   unorm<8>(mState.colorClearValue.red), 
                                   unorm<8>(mState.colorClearValue.green), 
                                   unorm<8>(mState.colorClearValue.blue));
    float depth = clamp01(mState.depthClearValue);
    int stencil = mState.stencilClearValue & 0x000000FF;


    bool alphaUnmasked = (gl::GetAlphaSize(mRenderTargetDesc.format) == 0) || mState.blend.colorMaskAlpha;

    const bool needMaskedStencilClear = (flags & D3DCLEAR_STENCIL) &&
                                        (mState.depthStencil.stencilWritemask & stencilUnmasked) != stencilUnmasked;
    const bool needMaskedColorClear = (flags & D3DCLEAR_TARGET) &&
                                      !(mState.blend.colorMaskRed && mState.blend.colorMaskGreen &&
                                        mState.blend.colorMaskBlue && alphaUnmasked);

    if (needMaskedColorClear || needMaskedStencilClear)
    {
        // State which is altered in all paths from this point to the clear call is saved.
        // State which is altered in only some paths will be flagged dirty in the case that
        //  that path is taken.
        HRESULT hr;
        if (mMaskedClearSavedState == NULL)
        {
            hr = mDevice->BeginStateBlock();
            ASSERT(SUCCEEDED(hr) || hr == D3DERR_OUTOFVIDEOMEMORY || hr == E_OUTOFMEMORY);

            mDevice->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
            mDevice->SetRenderState(D3DRS_ZFUNC, D3DCMP_ALWAYS);
            mDevice->SetRenderState(D3DRS_ZENABLE, FALSE);
            mDevice->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
            mDevice->SetRenderState(D3DRS_FILLMODE, D3DFILL_SOLID);
            mDevice->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);
            mDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
            mDevice->SetRenderState(D3DRS_CLIPPLANEENABLE, 0);
            mDevice->SetRenderState(D3DRS_COLORWRITEENABLE, 0);
            mDevice->SetRenderState(D3DRS_STENCILENABLE, FALSE);
            mDevice->SetPixelShader(NULL);
            mDevice->SetVertexShader(NULL);
            mDevice->SetFVF(D3DFVF_XYZRHW | D3DFVF_DIFFUSE);
            mDevice->SetStreamSource(0, NULL, 0, 0);
            mDevice->SetRenderState(D3DRS_SEPARATEALPHABLENDENABLE, TRUE);
            mDevice->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
            mDevice->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TFACTOR);
            mDevice->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);
            mDevice->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TFACTOR);
            mDevice->SetRenderState(D3DRS_TEXTUREFACTOR, color);
            mDevice->SetRenderState(D3DRS_MULTISAMPLEMASK, 0xFFFFFFFF);
            
            for(int i = 0; i < MAX_VERTEX_ATTRIBS; i++)
            {
                mDevice->SetStreamSourceFreq(i, 1);
            }

            hr = mDevice->EndStateBlock(&mMaskedClearSavedState);
            ASSERT(SUCCEEDED(hr) || hr == D3DERR_OUTOFVIDEOMEMORY || hr == E_OUTOFMEMORY);
        }

        ASSERT(mMaskedClearSavedState != NULL);

        if (mMaskedClearSavedState != NULL)
        {
            hr = mMaskedClearSavedState->Capture();
            ASSERT(SUCCEEDED(hr));
        }

        mDevice->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
        mDevice->SetRenderState(D3DRS_ZFUNC, D3DCMP_ALWAYS);
        mDevice->SetRenderState(D3DRS_ZENABLE, FALSE);
        mDevice->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
        mDevice->SetRenderState(D3DRS_FILLMODE, D3DFILL_SOLID);
        mDevice->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);
        mDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
        mDevice->SetRenderState(D3DRS_CLIPPLANEENABLE, 0);

        if (flags & D3DCLEAR_TARGET)
        {
            mDevice->SetRenderState(D3DRS_COLORWRITEENABLE,
                                    gl_d3d9::ConvertColorMask(mState.blend.colorMaskRed,
                                                              mState.blend.colorMaskGreen,
                                                              mState.blend.colorMaskBlue,
                                                              mState.blend.colorMaskAlpha));
        }
        else
        {
            mDevice->SetRenderState(D3DRS_COLORWRITEENABLE, 0);
        }

        if (stencilUnmasked != 0x0 && (flags & D3DCLEAR_STENCIL))
        {
            mDevice->SetRenderState(D3DRS_STENCILENABLE, TRUE);
            mDevice->SetRenderState(D3DRS_TWOSIDEDSTENCILMODE, FALSE);
            mDevice->SetRenderState(D3DRS_STENCILFUNC, D3DCMP_ALWAYS);
            mDevice->SetRenderState(D3DRS_STENCILREF, stencil);
            mDevice->SetRenderState(D3DRS_STENCILWRITEMASK, mState.depthStencil.stencilWritemask);
            mDevice->SetRenderState(D3DRS_STENCILFAIL, D3DSTENCILOP_REPLACE);
            mDevice->SetRenderState(D3DRS_STENCILZFAIL, D3DSTENCILOP_REPLACE);
            mDevice->SetRenderState(D3DRS_STENCILPASS, D3DSTENCILOP_REPLACE);
        }
        else
        {
            mDevice->SetRenderState(D3DRS_STENCILENABLE, FALSE);
        }

        mDevice->SetPixelShader(NULL);
        mDevice->SetVertexShader(NULL);
        mDevice->SetFVF(D3DFVF_XYZRHW);
        mDevice->SetRenderState(D3DRS_SEPARATEALPHABLENDENABLE, TRUE);
        mDevice->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
        mDevice->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TFACTOR);
        mDevice->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);
        mDevice->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TFACTOR);
        mDevice->SetRenderState(D3DRS_TEXTUREFACTOR, color);
        mDevice->SetRenderState(D3DRS_MULTISAMPLEMASK, 0xFFFFFFFF);

        for(int i = 0; i < MAX_VERTEX_ATTRIBS; i++)
        {
            mDevice->SetStreamSourceFreq(i, 1);
        }

        float quad[4][4];   // A quadrilateral covering the target, aligned to match the edges
        quad[0][0] = -0.5f;
        quad[0][1] = mRenderTargetDesc.height - 0.5f;
        quad[0][2] = 0.0f;
        quad[0][3] = 1.0f;

        quad[1][0] = mRenderTargetDesc.width - 0.5f;
        quad[1][1] = mRenderTargetDesc.height - 0.5f;
        quad[1][2] = 0.0f;
        quad[1][3] = 1.0f;

        quad[2][0] = -0.5f;
        quad[2][1] = -0.5f;
        quad[2][2] = 0.0f;
        quad[2][3] = 1.0f;

        quad[3][0] = mRenderTargetDesc.width - 0.5f;
        quad[3][1] = -0.5f;
        quad[3][2] = 0.0f;
        quad[3][3] = 1.0f;

        mRenderer->startScene();
        mDevice->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, quad, sizeof(float[4]));

        if (flags & D3DCLEAR_ZBUFFER)
        {
            mDevice->SetRenderState(D3DRS_ZENABLE, TRUE);
            mDevice->SetRenderState(D3DRS_ZWRITEENABLE, TRUE);
            mDevice->Clear(0, NULL, D3DCLEAR_ZBUFFER, color, depth, stencil);
        }

        if (mMaskedClearSavedState != NULL)
        {
            mMaskedClearSavedState->Apply();
        }
    }
    else if (flags)
    {
        mDevice->Clear(0, NULL, flags, color, depth, stencil);
    }

    mRenderer->clear(mask, mState.colorClearValue, mState.depthClearValue, mState.stencilClearValue,
                     framebufferObject);
}

void Context::drawArrays(GLenum mode, GLint first, GLsizei count, GLsizei instances)
{
    if (!mState.currentProgram)
    {
        return error(GL_INVALID_OPERATION);
    }

    D3DPRIMITIVETYPE primitiveType;
    int primitiveCount;

    if(!gl_d3d9::ConvertPrimitiveType(mode, count, &primitiveType, &primitiveCount))
        return error(GL_INVALID_ENUM);

    if (primitiveCount <= 0)
    {
        return;
    }

    if (!applyRenderTarget(false))
    {
        return;
    }

    applyState(mode);

    GLsizei repeatDraw = 1;
    GLenum err = applyVertexBuffer(first, count, instances, &repeatDraw);
    if (err != GL_NO_ERROR)
    {
        return error(err);
    }

    applyShaders();
    applyTextures();

    if (!getCurrentProgramBinary()->validateSamplers(NULL))
    {
        return error(GL_INVALID_OPERATION);
    }

    if (!skipDraw(mode))
    {
        mRenderer->startScene();
        
        if (mode == GL_LINE_LOOP)
        {
            drawLineLoop(count, GL_NONE, NULL, 0);
        }
        else if (instances > 0)
        {
            StaticIndexBuffer *countingIB = mIndexDataManager->getCountingIndices(count);
            if (countingIB)
            {
                if (mAppliedIBSerial != countingIB->getSerial())
                {
                    mDevice->SetIndices(countingIB->getBuffer());
                    mAppliedIBSerial = countingIB->getSerial();
                }

                for (int i = 0; i < repeatDraw; i++)
                {
                    mDevice->DrawIndexedPrimitive(primitiveType, 0, 0, count, 0, primitiveCount);
                }
            }
            else
            {
                ERR("Could not create a counting index buffer for glDrawArraysInstanced.");
                return error(GL_OUT_OF_MEMORY);
            }
        }
        else   // Regular case
        {
            mDevice->DrawPrimitive(primitiveType, 0, primitiveCount);
        }
    }
}

void Context::drawElements(GLenum mode, GLsizei count, GLenum type, const GLvoid *indices, GLsizei instances)
{
    if (!mState.currentProgram)
    {
        return error(GL_INVALID_OPERATION);
    }

    if (!indices && !mState.elementArrayBuffer)
    {
        return error(GL_INVALID_OPERATION);
    }

    D3DPRIMITIVETYPE primitiveType;
    int primitiveCount;

    if(!gl_d3d9::ConvertPrimitiveType(mode, count, &primitiveType, &primitiveCount))
        return error(GL_INVALID_ENUM);

    if (primitiveCount <= 0)
    {
        return;
    }

    if (!applyRenderTarget(false))
    {
        return;
    }

    applyState(mode);

    TranslatedIndexData indexInfo;
    GLenum err = applyIndexBuffer(indices, count, mode, type, &indexInfo);
    if (err != GL_NO_ERROR)
    {
        return error(err);
    }

    GLsizei vertexCount = indexInfo.maxIndex - indexInfo.minIndex + 1;
    GLsizei repeatDraw = 1;
    err = applyVertexBuffer(indexInfo.minIndex, vertexCount, instances, &repeatDraw);
    if (err != GL_NO_ERROR)
    {
        return error(err);
    }

    applyShaders();
    applyTextures();

    if (!getCurrentProgramBinary()->validateSamplers(false))
    {
        return error(GL_INVALID_OPERATION);
    }

    if (!skipDraw(mode))
    {
        mRenderer->startScene();

        if (mode == GL_LINE_LOOP)
        {
            drawLineLoop(count, type, indices, indexInfo.minIndex);   
        }
        else
        {
            for (int i = 0; i < repeatDraw; i++)
            {
                mDevice->DrawIndexedPrimitive(primitiveType, -(INT)indexInfo.minIndex, indexInfo.minIndex, vertexCount, indexInfo.startIndex, primitiveCount);
            }
        }
    }
}

// Implements glFlush when block is false, glFinish when block is true
void Context::sync(bool block)
{
    mRenderer->sync(block);
}

void Context::drawLineLoop(GLsizei count, GLenum type, const GLvoid *indices, int minIndex)
{
    // Get the raw indices for an indexed draw
    if (type != GL_NONE && mState.elementArrayBuffer.get())
    {
        Buffer *indexBuffer = mState.elementArrayBuffer.get();
        intptr_t offset = reinterpret_cast<intptr_t>(indices);
        indices = static_cast<const GLubyte*>(indexBuffer->data()) + offset;
    }

    UINT startIndex = 0;
    bool succeeded = false;

    if (supports32bitIndices())
    {
        const int spaceNeeded = (count + 1) * sizeof(unsigned int);

        if (!mLineLoopIB)
        {
            mLineLoopIB = new StreamingIndexBuffer(mRenderer, INITIAL_INDEX_BUFFER_SIZE, D3DFMT_INDEX32);
        }

        if (mLineLoopIB)
        {
            mLineLoopIB->reserveSpace(spaceNeeded, GL_UNSIGNED_INT);

            UINT offset = 0;
            unsigned int *data = static_cast<unsigned int*>(mLineLoopIB->map(spaceNeeded, &offset));
            startIndex = offset / 4;
            
            if (data)
            {
                switch (type)
                {
                  case GL_NONE:   // Non-indexed draw
                    for (int i = 0; i < count; i++)
                    {
                        data[i] = i;
                    }
                    data[count] = 0;
                    break;
                  case GL_UNSIGNED_BYTE:
                    for (int i = 0; i < count; i++)
                    {
                        data[i] = static_cast<const GLubyte*>(indices)[i];
                    }
                    data[count] = static_cast<const GLubyte*>(indices)[0];
                    break;
                  case GL_UNSIGNED_SHORT:
                    for (int i = 0; i < count; i++)
                    {
                        data[i] = static_cast<const GLushort*>(indices)[i];
                    }
                    data[count] = static_cast<const GLushort*>(indices)[0];
                    break;
                  case GL_UNSIGNED_INT:
                    for (int i = 0; i < count; i++)
                    {
                        data[i] = static_cast<const GLuint*>(indices)[i];
                    }
                    data[count] = static_cast<const GLuint*>(indices)[0];
                    break;
                  default: UNREACHABLE();
                }

                mLineLoopIB->unmap();
                succeeded = true;
            }
        }
    }
    else
    {
        const int spaceNeeded = (count + 1) * sizeof(unsigned short);

        if (!mLineLoopIB)
        {
            mLineLoopIB = new StreamingIndexBuffer(mRenderer, INITIAL_INDEX_BUFFER_SIZE, D3DFMT_INDEX16);
        }

        if (mLineLoopIB)
        {
            mLineLoopIB->reserveSpace(spaceNeeded, GL_UNSIGNED_SHORT);

            UINT offset = 0;
            unsigned short *data = static_cast<unsigned short*>(mLineLoopIB->map(spaceNeeded, &offset));
            startIndex = offset / 2;
            
            if (data)
            {
                switch (type)
                {
                  case GL_NONE:   // Non-indexed draw
                    for (int i = 0; i < count; i++)
                    {
                        data[i] = i;
                    }
                    data[count] = 0;
                    break;
                  case GL_UNSIGNED_BYTE:
                    for (int i = 0; i < count; i++)
                    {
                        data[i] = static_cast<const GLubyte*>(indices)[i];
                    }
                    data[count] = static_cast<const GLubyte*>(indices)[0];
                    break;
                  case GL_UNSIGNED_SHORT:
                    for (int i = 0; i < count; i++)
                    {
                        data[i] = static_cast<const GLushort*>(indices)[i];
                    }
                    data[count] = static_cast<const GLushort*>(indices)[0];
                    break;
                  case GL_UNSIGNED_INT:
                    for (int i = 0; i < count; i++)
                    {
                        data[i] = static_cast<const GLuint*>(indices)[i];
                    }
                    data[count] = static_cast<const GLuint*>(indices)[0];
                    break;
                  default: UNREACHABLE();
                }

                mLineLoopIB->unmap();
                succeeded = true;
            }
        }
    }
    
    if (succeeded)
    {
        if (mAppliedIBSerial != mLineLoopIB->getSerial())
        {
            mDevice->SetIndices(mLineLoopIB->getBuffer());
            mAppliedIBSerial = mLineLoopIB->getSerial();
        }

        mDevice->DrawIndexedPrimitive(D3DPT_LINESTRIP, -minIndex, minIndex, count, startIndex, count);
    }
    else
    {
        ERR("Could not create a looping index buffer for GL_LINE_LOOP.");
        return error(GL_OUT_OF_MEMORY);
    }
}

void Context::recordInvalidEnum()
{
    mInvalidEnum = true;
}

void Context::recordInvalidValue()
{
    mInvalidValue = true;
}

void Context::recordInvalidOperation()
{
    mInvalidOperation = true;
}

void Context::recordOutOfMemory()
{
    mOutOfMemory = true;
}

void Context::recordInvalidFramebufferOperation()
{
    mInvalidFramebufferOperation = true;
}

// Get one of the recorded errors and clear its flag, if any.
// [OpenGL ES 2.0.24] section 2.5 page 13.
GLenum Context::getError()
{
    if (mInvalidEnum)
    {
        mInvalidEnum = false;

        return GL_INVALID_ENUM;
    }

    if (mInvalidValue)
    {
        mInvalidValue = false;

        return GL_INVALID_VALUE;
    }

    if (mInvalidOperation)
    {
        mInvalidOperation = false;

        return GL_INVALID_OPERATION;
    }

    if (mOutOfMemory)
    {
        mOutOfMemory = false;

        return GL_OUT_OF_MEMORY;
    }

    if (mInvalidFramebufferOperation)
    {
        mInvalidFramebufferOperation = false;

        return GL_INVALID_FRAMEBUFFER_OPERATION;
    }

    return GL_NO_ERROR;
}

GLenum Context::getResetStatus()
{
    if (mResetStatus == GL_NO_ERROR)
    {
        // mResetStatus will be set by the markContextLost callback
        // in the case a notification is sent
        mRenderer->testDeviceLost(true);
    }

    GLenum status = mResetStatus;

    if (mResetStatus != GL_NO_ERROR)
    {
        if (mRenderer->testDeviceResettable())
        {
            mResetStatus = GL_NO_ERROR;
        }
    }
    
    return status;
}

bool Context::isResetNotificationEnabled()
{
    return (mResetStrategy == GL_LOSE_CONTEXT_ON_RESET_EXT);
}

bool Context::supportsShaderModel3() const
{
    return mSupportsShaderModel3;
}

float Context::getMaximumPointSize() const
{
    return mSupportsShaderModel3 ? mMaximumPointSize : ALIASED_POINT_SIZE_RANGE_MAX_SM2;
}

int Context::getMaximumVaryingVectors() const
{
    return mSupportsShaderModel3 ? MAX_VARYING_VECTORS_SM3 : MAX_VARYING_VECTORS_SM2;
}

unsigned int Context::getMaximumVertexTextureImageUnits() const
{
    return mSupportsVertexTexture ? MAX_VERTEX_TEXTURE_IMAGE_UNITS_VTF : 0;
}

unsigned int Context::getMaximumCombinedTextureImageUnits() const
{
    return MAX_TEXTURE_IMAGE_UNITS + getMaximumVertexTextureImageUnits();
}

int Context::getMaximumFragmentUniformVectors() const
{
    return mSupportsShaderModel3 ? MAX_FRAGMENT_UNIFORM_VECTORS_SM3 : MAX_FRAGMENT_UNIFORM_VECTORS_SM2;
}

int Context::getMaxSupportedSamples() const
{
    return mRenderer->getMaxSupportedSamples();
}

bool Context::supportsEventQueries() const
{
    return mSupportsEventQueries;
}

bool Context::supportsOcclusionQueries() const
{
    return mSupportsOcclusionQueries;
}

bool Context::supportsDXT1Textures() const
{
    return mSupportsDXT1Textures;
}

bool Context::supportsDXT3Textures() const
{
    return mSupportsDXT3Textures;
}

bool Context::supportsDXT5Textures() const
{
    return mSupportsDXT5Textures;
}

bool Context::supportsFloat32Textures() const
{
    return mSupportsFloat32Textures;
}

bool Context::supportsFloat32LinearFilter() const
{
    return mSupportsFloat32LinearFilter;
}

bool Context::supportsFloat32RenderableTextures() const
{
    return mSupportsFloat32RenderableTextures;
}

bool Context::supportsFloat16Textures() const
{
    return mSupportsFloat16Textures;
}

bool Context::supportsFloat16LinearFilter() const
{
    return mSupportsFloat16LinearFilter;
}

bool Context::supportsFloat16RenderableTextures() const
{
    return mSupportsFloat16RenderableTextures;
}

int Context::getMaximumRenderbufferDimension() const
{
    return mMaxRenderbufferDimension;
}

int Context::getMaximumTextureDimension() const
{
    return mMaxTextureDimension;
}

int Context::getMaximumCubeTextureDimension() const
{
    return mMaxCubeTextureDimension;
}

int Context::getMaximumTextureLevel() const
{
    return mMaxTextureLevel;
}

bool Context::supportsLuminanceTextures() const
{
    return mSupportsLuminanceTextures;
}

bool Context::supportsLuminanceAlphaTextures() const
{
    return mSupportsLuminanceAlphaTextures;
}

bool Context::supportsDepthTextures() const
{
    return mSupportsDepthTextures;
}

bool Context::supports32bitIndices() const
{
    return mSupports32bitIndices;
}

bool Context::supportsNonPower2Texture() const
{
    return mSupportsNonPower2Texture;
}

bool Context::supportsInstancing() const
{
    return mSupportsInstancing;
}

bool Context::supportsTextureFilterAnisotropy() const
{
    return mSupportsTextureFilterAnisotropy;
}

float Context::getTextureMaxAnisotropy() const
{
    return mMaxTextureAnisotropy;
}

bool Context::getCurrentReadFormatType(GLenum *format, GLenum *type)
{
    Framebuffer *framebuffer = getReadFramebuffer();
    if (!framebuffer || framebuffer->completeness() != GL_FRAMEBUFFER_COMPLETE)
    {
        return error(GL_INVALID_OPERATION, false);
    }

    Renderbuffer *renderbuffer = framebuffer->getColorbuffer();
    if (!renderbuffer)
    {
        return error(GL_INVALID_OPERATION, false);
    }

    *format = gl::ExtractFormat(renderbuffer->getActualFormat()); 
    *type = gl::ExtractType(renderbuffer->getActualFormat());

    return true;
}

void Context::detachBuffer(GLuint buffer)
{
    // [OpenGL ES 2.0.24] section 2.9 page 22:
    // If a buffer object is deleted while it is bound, all bindings to that object in the current context
    // (i.e. in the thread that called Delete-Buffers) are reset to zero.

    if (mState.arrayBuffer.id() == buffer)
    {
        mState.arrayBuffer.set(NULL);
    }

    if (mState.elementArrayBuffer.id() == buffer)
    {
        mState.elementArrayBuffer.set(NULL);
    }

    for (int attribute = 0; attribute < MAX_VERTEX_ATTRIBS; attribute++)
    {
        if (mState.vertexAttribute[attribute].mBoundBuffer.id() == buffer)
        {
            mState.vertexAttribute[attribute].mBoundBuffer.set(NULL);
        }
    }
}

void Context::detachTexture(GLuint texture)
{
    // [OpenGL ES 2.0.24] section 3.8 page 84:
    // If a texture object is deleted, it is as if all texture units which are bound to that texture object are
    // rebound to texture object zero

    for (int type = 0; type < TEXTURE_TYPE_COUNT; type++)
    {
        for (int sampler = 0; sampler < MAX_COMBINED_TEXTURE_IMAGE_UNITS_VTF; sampler++)
        {
            if (mState.samplerTexture[type][sampler].id() == texture)
            {
                mState.samplerTexture[type][sampler].set(NULL);
            }
        }
    }

    // [OpenGL ES 2.0.24] section 4.4 page 112:
    // If a texture object is deleted while its image is attached to the currently bound framebuffer, then it is
    // as if FramebufferTexture2D had been called, with a texture of 0, for each attachment point to which this
    // image was attached in the currently bound framebuffer.

    Framebuffer *readFramebuffer = getReadFramebuffer();
    Framebuffer *drawFramebuffer = getDrawFramebuffer();

    if (readFramebuffer)
    {
        readFramebuffer->detachTexture(texture);
    }

    if (drawFramebuffer && drawFramebuffer != readFramebuffer)
    {
        drawFramebuffer->detachTexture(texture);
    }
}

void Context::detachFramebuffer(GLuint framebuffer)
{
    // [OpenGL ES 2.0.24] section 4.4 page 107:
    // If a framebuffer that is currently bound to the target FRAMEBUFFER is deleted, it is as though
    // BindFramebuffer had been executed with the target of FRAMEBUFFER and framebuffer of zero.

    if (mState.readFramebuffer == framebuffer)
    {
        bindReadFramebuffer(0);
    }

    if (mState.drawFramebuffer == framebuffer)
    {
        bindDrawFramebuffer(0);
    }
}

void Context::detachRenderbuffer(GLuint renderbuffer)
{
    // [OpenGL ES 2.0.24] section 4.4 page 109:
    // If a renderbuffer that is currently bound to RENDERBUFFER is deleted, it is as though BindRenderbuffer
    // had been executed with the target RENDERBUFFER and name of zero.

    if (mState.renderbuffer.id() == renderbuffer)
    {
        bindRenderbuffer(0);
    }

    // [OpenGL ES 2.0.24] section 4.4 page 111:
    // If a renderbuffer object is deleted while its image is attached to the currently bound framebuffer,
    // then it is as if FramebufferRenderbuffer had been called, with a renderbuffer of 0, for each attachment
    // point to which this image was attached in the currently bound framebuffer.

    Framebuffer *readFramebuffer = getReadFramebuffer();
    Framebuffer *drawFramebuffer = getDrawFramebuffer();

    if (readFramebuffer)
    {
        readFramebuffer->detachRenderbuffer(renderbuffer);
    }

    if (drawFramebuffer && drawFramebuffer != readFramebuffer)
    {
        drawFramebuffer->detachRenderbuffer(renderbuffer);
    }
}

Texture *Context::getIncompleteTexture(TextureType type)
{
    Texture *t = mIncompleteTextures[type].get();

    if (t == NULL)
    {
        static const GLubyte color[] = { 0, 0, 0, 255 };

        switch (type)
        {
          default:
            UNREACHABLE();
            // default falls through to TEXTURE_2D

          case TEXTURE_2D:
            {
                Texture2D *incomplete2d = new Texture2D(mRenderer, Texture::INCOMPLETE_TEXTURE_ID);
                incomplete2d->setImage(0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, 1, color);
                t = incomplete2d;
            }
            break;

          case TEXTURE_CUBE:
            {
              TextureCubeMap *incompleteCube = new TextureCubeMap(mRenderer, Texture::INCOMPLETE_TEXTURE_ID);

              incompleteCube->setImagePosX(0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, 1, color);
              incompleteCube->setImageNegX(0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, 1, color);
              incompleteCube->setImagePosY(0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, 1, color);
              incompleteCube->setImageNegY(0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, 1, color);
              incompleteCube->setImagePosZ(0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, 1, color);
              incompleteCube->setImageNegZ(0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, 1, color);

              t = incompleteCube;
            }
            break;
        }

        mIncompleteTextures[type].set(t);
    }

    return t;
}

bool Context::skipDraw(GLenum drawMode)
{
    if (drawMode == GL_POINTS)
    {
        // ProgramBinary assumes non-point rendering if gl_PointSize isn't written,
        // which affects varying interpolation. Since the value of gl_PointSize is
        // undefined when not written, just skip drawing to avoid unexpected results.
        if (!getCurrentProgramBinary()->usesPointSize())
        {
            // This is stictly speaking not an error, but developers should be 
            // notified of risking undefined behavior.
            ERR("Point rendering without writing to gl_PointSize.");

            return true;
        }
    }
    else if (isTriangleMode(drawMode))
    {
        if (mState.rasterizer.cullFace && mState.rasterizer.cullMode == GL_FRONT_AND_BACK)
        {
            return true;
        }
    }

    return false;
}

bool Context::isTriangleMode(GLenum drawMode)
{
    switch (drawMode)
    {
      case GL_TRIANGLES:
      case GL_TRIANGLE_FAN:
      case GL_TRIANGLE_STRIP:
        return true;
      case GL_POINTS:
      case GL_LINES:
      case GL_LINE_LOOP:
      case GL_LINE_STRIP:
        return false;
      default: UNREACHABLE();
    }

    return false;
}

void Context::setVertexAttrib(GLuint index, const GLfloat *values)
{
    ASSERT(index < gl::MAX_VERTEX_ATTRIBS);

    mState.vertexAttribute[index].mCurrentValue[0] = values[0];
    mState.vertexAttribute[index].mCurrentValue[1] = values[1];
    mState.vertexAttribute[index].mCurrentValue[2] = values[2];
    mState.vertexAttribute[index].mCurrentValue[3] = values[3];

    mVertexDataManager->dirtyCurrentValue(index);
}

void Context::setVertexAttribDivisor(GLuint index, GLuint divisor)
{
    ASSERT(index < gl::MAX_VERTEX_ATTRIBS);

    mState.vertexAttribute[index].mDivisor = divisor;
}

// keep list sorted in following order
// OES extensions
// EXT extensions
// Vendor extensions
void Context::initExtensionString()
{
    mExtensionString = "";

    // OES extensions
    if (supports32bitIndices())
    {
        mExtensionString += "GL_OES_element_index_uint ";
    }

    mExtensionString += "GL_OES_packed_depth_stencil ";
    mExtensionString += "GL_OES_get_program_binary ";
    mExtensionString += "GL_OES_rgb8_rgba8 ";
    mExtensionString += "GL_OES_standard_derivatives ";

    if (supportsFloat16Textures())
    {
        mExtensionString += "GL_OES_texture_half_float ";
    }
    if (supportsFloat16LinearFilter())
    {
        mExtensionString += "GL_OES_texture_half_float_linear ";
    }
    if (supportsFloat32Textures())
    {
        mExtensionString += "GL_OES_texture_float ";
    }
    if (supportsFloat32LinearFilter())
    {
        mExtensionString += "GL_OES_texture_float_linear ";
    }

    if (supportsNonPower2Texture())
    {
        mExtensionString += "GL_OES_texture_npot ";
    }

    // Multi-vendor (EXT) extensions
    if (supportsOcclusionQueries())
    {
        mExtensionString += "GL_EXT_occlusion_query_boolean ";
    }

    mExtensionString += "GL_EXT_read_format_bgra ";
    mExtensionString += "GL_EXT_robustness ";

    if (supportsDXT1Textures())
    {
        mExtensionString += "GL_EXT_texture_compression_dxt1 ";
    }

    if (supportsTextureFilterAnisotropy())
    {
        mExtensionString += "GL_EXT_texture_filter_anisotropic ";
    }

    mExtensionString += "GL_EXT_texture_format_BGRA8888 ";
    mExtensionString += "GL_EXT_texture_storage ";

    // ANGLE-specific extensions
    if (supportsDepthTextures())
    {
        mExtensionString += "GL_ANGLE_depth_texture ";
    }

    mExtensionString += "GL_ANGLE_framebuffer_blit ";
    if (getMaxSupportedSamples() != 0)
    {
        mExtensionString += "GL_ANGLE_framebuffer_multisample ";
    }

    if (supportsInstancing())
    {
        mExtensionString += "GL_ANGLE_instanced_arrays ";
    }

    mExtensionString += "GL_ANGLE_pack_reverse_row_order ";

    if (supportsDXT3Textures())
    {
        mExtensionString += "GL_ANGLE_texture_compression_dxt3 ";
    }
    if (supportsDXT5Textures())
    {
        mExtensionString += "GL_ANGLE_texture_compression_dxt5 ";
    }

    mExtensionString += "GL_ANGLE_texture_usage ";
    mExtensionString += "GL_ANGLE_translated_shader_source ";

    // Other vendor-specific extensions
    if (supportsEventQueries())
    {
        mExtensionString += "GL_NV_fence ";
    }

    std::string::size_type end = mExtensionString.find_last_not_of(' ');
    if (end != std::string::npos)
    {
        mExtensionString.resize(end+1);
    }
}

const char *Context::getExtensionString() const
{
    return mExtensionString.c_str();
}

void Context::initRendererString()
{
    mRendererString = "ANGLE (";
    mRendererString += mRenderer->getAdapterDescription();
    mRendererString += ")";
}

const char *Context::getRendererString() const
{
    return mRendererString.c_str();
}

void Context::blitFramebuffer(GLint srcX0, GLint srcY0, GLint srcX1, GLint srcY1, 
                              GLint dstX0, GLint dstY0, GLint dstX1, GLint dstY1,
                              GLbitfield mask)
{
    Framebuffer *readFramebuffer = getReadFramebuffer();
    Framebuffer *drawFramebuffer = getDrawFramebuffer();

    if (!readFramebuffer || readFramebuffer->completeness() != GL_FRAMEBUFFER_COMPLETE ||
        !drawFramebuffer || drawFramebuffer->completeness() != GL_FRAMEBUFFER_COMPLETE)
    {
        return error(GL_INVALID_FRAMEBUFFER_OPERATION);
    }

    if (drawFramebuffer->getSamples() != 0)
    {
        return error(GL_INVALID_OPERATION);
    }

    int readBufferWidth = readFramebuffer->getColorbuffer()->getWidth();
    int readBufferHeight = readFramebuffer->getColorbuffer()->getHeight();
    int drawBufferWidth = drawFramebuffer->getColorbuffer()->getWidth();
    int drawBufferHeight = drawFramebuffer->getColorbuffer()->getHeight();

    Rectangle sourceRect;
    Rectangle destRect;

    if (srcX0 < srcX1)
    {
        sourceRect.x = srcX0;
        destRect.x = dstX0;
        sourceRect.width = srcX1 - srcX0;
        destRect.width = dstX1 - dstX0;
    }
    else
    {
        sourceRect.x = srcX1;
        destRect.x = dstX1;
        sourceRect.width = srcX0 - srcX1;
        destRect.width = dstX0 - dstX1;
    }

    if (srcY0 < srcY1)
    {
        sourceRect.height = srcY1 - srcY0;
        destRect.height = dstY1 - dstY0;
        sourceRect.y = srcY0;
        destRect.y = dstY0;
    }
    else
    {
        sourceRect.height = srcY0 - srcY1;
        destRect.height = dstY0 - srcY1;
        sourceRect.y = srcY1;
        destRect.y = dstY1;
    }

    Rectangle sourceScissoredRect = sourceRect;
    Rectangle destScissoredRect = destRect;

    if (mState.rasterizer.scissorTest)
    {
        // Only write to parts of the destination framebuffer which pass the scissor test.
        if (destRect.x < mState.scissor.x)
        {
            int xDiff = mState.scissor.x - destRect.x;
            destScissoredRect.x = mState.scissor.x;
            destScissoredRect.width -= xDiff;
            sourceScissoredRect.x += xDiff;
            sourceScissoredRect.width -= xDiff;

        }

        if (destRect.x + destRect.width > mState.scissor.x + mState.scissor.width)
        {
            int xDiff = (destRect.x + destRect.width) - (mState.scissor.x + mState.scissor.width);
            destScissoredRect.width -= xDiff;
            sourceScissoredRect.width -= xDiff;
        }

        if (destRect.y < mState.scissor.y)
        {
            int yDiff = mState.scissor.y - destRect.y;
            destScissoredRect.y = mState.scissor.y;
            destScissoredRect.height -= yDiff;
            sourceScissoredRect.y += yDiff;
            sourceScissoredRect.height -= yDiff;
        }

        if (destRect.y + destRect.height > mState.scissor.y + mState.scissor.height)
        {
            int yDiff = (destRect.y + destRect.height) - (mState.scissor.y + mState.scissor.height);
            destScissoredRect.height  -= yDiff;
            sourceScissoredRect.height -= yDiff;
        }
    }

    bool blitRenderTarget = false;
    bool blitDepthStencil = false;

    Rectangle sourceTrimmedRect = sourceScissoredRect;
    Rectangle destTrimmedRect = destScissoredRect;

    // The source & destination rectangles also may need to be trimmed if they fall out of the bounds of 
    // the actual draw and read surfaces.
    if (sourceTrimmedRect.x < 0)
    {
        int xDiff = 0 - sourceTrimmedRect.x;
        sourceTrimmedRect.x = 0;
        sourceTrimmedRect.width -= xDiff;
        destTrimmedRect.x += xDiff;
        destTrimmedRect.width -= xDiff;
    }

    if (sourceTrimmedRect.x + sourceTrimmedRect.width > readBufferWidth)
    {
        int xDiff = (sourceTrimmedRect.x + sourceTrimmedRect.width) - readBufferWidth;
        sourceTrimmedRect.width -= xDiff;
        destTrimmedRect.width -= xDiff;
    }

    if (sourceTrimmedRect.y < 0)
    {
        int yDiff = 0 - sourceTrimmedRect.y;
        sourceTrimmedRect.y = 0;
        sourceTrimmedRect.height -= yDiff;
        destTrimmedRect.y += yDiff;
        destTrimmedRect.height -= yDiff;
    }

    if (sourceTrimmedRect.y + sourceTrimmedRect.height > readBufferHeight)
    {
        int yDiff = (sourceTrimmedRect.y + sourceTrimmedRect.height) - readBufferHeight;
        sourceTrimmedRect.height -= yDiff;
        destTrimmedRect.height -= yDiff;
    }

    if (destTrimmedRect.x < 0)
    {
        int xDiff = 0 - destTrimmedRect.x;
        destTrimmedRect.x = 0;
        destTrimmedRect.width -= xDiff;
        sourceTrimmedRect.x += xDiff;
        sourceTrimmedRect.width -= xDiff;
    }

    if (destTrimmedRect.x + destTrimmedRect.width > drawBufferWidth)
    {
        int xDiff = (destTrimmedRect.x + destTrimmedRect.width) - drawBufferWidth;
        destTrimmedRect.width -= xDiff;
        sourceTrimmedRect.width -= xDiff;
    }

    if (destTrimmedRect.y < 0)
    {
        int yDiff = 0 - destTrimmedRect.y;
        destTrimmedRect.y = 0;
        destTrimmedRect.height -= yDiff;
        sourceTrimmedRect.y += yDiff;
        sourceTrimmedRect.height -= yDiff;
    }

    if (destTrimmedRect.y + destTrimmedRect.height > drawBufferHeight)
    {
        int yDiff = (destTrimmedRect.y + destTrimmedRect.height) - drawBufferHeight;
        destTrimmedRect.height -= yDiff;
        sourceTrimmedRect.height -= yDiff;
    }

    bool partialBufferCopy = false;
    if (sourceTrimmedRect.height < readBufferHeight ||
        sourceTrimmedRect.width < readBufferWidth || 
        destTrimmedRect.height < drawBufferHeight ||
        destTrimmedRect.width < drawBufferWidth ||
        sourceTrimmedRect.y != 0 || destTrimmedRect.y != 0 || sourceTrimmedRect.x != 0 || destTrimmedRect.x != 0)
    {
        partialBufferCopy = true;
    }

    if (mask & GL_COLOR_BUFFER_BIT)
    {
        const bool validReadType = readFramebuffer->getColorbufferType() == GL_TEXTURE_2D ||
            readFramebuffer->getColorbufferType() == GL_RENDERBUFFER;
        const bool validDrawType = drawFramebuffer->getColorbufferType() == GL_TEXTURE_2D ||
            drawFramebuffer->getColorbufferType() == GL_RENDERBUFFER;
        if (!validReadType || !validDrawType ||
            readFramebuffer->getColorbuffer()->getActualFormat() != drawFramebuffer->getColorbuffer()->getActualFormat())
        {
            ERR("Color buffer format conversion in BlitFramebufferANGLE not supported by this implementation");
            return error(GL_INVALID_OPERATION);
        }
        
        if (partialBufferCopy && readFramebuffer->getSamples() != 0)
        {
            return error(GL_INVALID_OPERATION);
        }

        blitRenderTarget = true;

    }

    if (mask & (GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT))
    {
        Renderbuffer *readDSBuffer = NULL;
        Renderbuffer *drawDSBuffer = NULL;

        // We support OES_packed_depth_stencil, and do not support a separately attached depth and stencil buffer, so if we have
        // both a depth and stencil buffer, it will be the same buffer.

        if (mask & GL_DEPTH_BUFFER_BIT)
        {
            if (readFramebuffer->getDepthbuffer() && drawFramebuffer->getDepthbuffer())
            {
                if (readFramebuffer->getDepthbufferType() != drawFramebuffer->getDepthbufferType() ||
                    readFramebuffer->getDepthbuffer()->getActualFormat() != drawFramebuffer->getDepthbuffer()->getActualFormat())
                {
                    return error(GL_INVALID_OPERATION);
                }

                blitDepthStencil = true;
                readDSBuffer = readFramebuffer->getDepthbuffer();
                drawDSBuffer = drawFramebuffer->getDepthbuffer();
            }
        }

        if (mask & GL_STENCIL_BUFFER_BIT)
        {
            if (readFramebuffer->getStencilbuffer() && drawFramebuffer->getStencilbuffer())
            {
                if (readFramebuffer->getStencilbufferType() != drawFramebuffer->getStencilbufferType() ||
                    readFramebuffer->getStencilbuffer()->getActualFormat() != drawFramebuffer->getStencilbuffer()->getActualFormat())
                {
                    return error(GL_INVALID_OPERATION);
                }

                blitDepthStencil = true;
                readDSBuffer = readFramebuffer->getStencilbuffer();
                drawDSBuffer = drawFramebuffer->getStencilbuffer();
            }
        }

        if (partialBufferCopy)
        {
            ERR("Only whole-buffer depth and stencil blits are supported by this implementation.");
            return error(GL_INVALID_OPERATION); // only whole-buffer copies are permitted
        }

        if ((drawDSBuffer && drawDSBuffer->getSamples() != 0) || 
            (readDSBuffer && readDSBuffer->getSamples() != 0))
        {
            return error(GL_INVALID_OPERATION);
        }
    }

    if (blitRenderTarget || blitDepthStencil)
    {
        mRenderer->blitRect(readFramebuffer, &sourceTrimmedRect, drawFramebuffer, &destTrimmedRect, blitRenderTarget, blitDepthStencil);
    }
}

VertexDeclarationCache::VertexDeclarationCache() : mMaxLru(0)
{
    for (int i = 0; i < NUM_VERTEX_DECL_CACHE_ENTRIES; i++)
    {
        mVertexDeclCache[i].vertexDeclaration = NULL;
        mVertexDeclCache[i].lruCount = 0;
    }
}

VertexDeclarationCache::~VertexDeclarationCache()
{
    for (int i = 0; i < NUM_VERTEX_DECL_CACHE_ENTRIES; i++)
    {
        if (mVertexDeclCache[i].vertexDeclaration)
        {
            mVertexDeclCache[i].vertexDeclaration->Release();
        }
    }
}

GLenum VertexDeclarationCache::applyDeclaration(IDirect3DDevice9 *device, TranslatedAttribute attributes[], ProgramBinary *programBinary, GLsizei instances, GLsizei *repeatDraw)
{
    *repeatDraw = 1;

    int indexedAttribute = MAX_VERTEX_ATTRIBS;
    int instancedAttribute = MAX_VERTEX_ATTRIBS;

    if (instances > 0)
    {
        // Find an indexed attribute to be mapped to D3D stream 0
        for (int i = 0; i < MAX_VERTEX_ATTRIBS; i++)
        {
            if (attributes[i].active)
            {
                if (indexedAttribute == MAX_VERTEX_ATTRIBS)
                {
                    if (attributes[i].divisor == 0)
                    {
                        indexedAttribute = i;
                    }
                }
                else if (instancedAttribute == MAX_VERTEX_ATTRIBS)
                {
                    if (attributes[i].divisor != 0)
                    {
                        instancedAttribute = i;
                    }
                }
                else break;   // Found both an indexed and instanced attribute
            }
        }

        if (indexedAttribute == MAX_VERTEX_ATTRIBS)
        {
            return GL_INVALID_OPERATION;
        }
    }

    D3DVERTEXELEMENT9 elements[MAX_VERTEX_ATTRIBS + 1];
    D3DVERTEXELEMENT9 *element = &elements[0];

    for (int i = 0; i < MAX_VERTEX_ATTRIBS; i++)
    {
        if (attributes[i].active)
        {
            int stream = i;

            if (instances > 0)
            {
                // Due to a bug on ATI cards we can't enable instancing when none of the attributes are instanced.
                if (instancedAttribute == MAX_VERTEX_ATTRIBS)
                {
                    *repeatDraw = instances;
                }
                else
                {
                    if (i == indexedAttribute)
                    {
                        stream = 0;
                    }
                    else if (i == 0)
                    {
                        stream = indexedAttribute;
                    }

                    UINT frequency = 1;
                    
                    if (attributes[i].divisor == 0)
                    {
                        frequency = D3DSTREAMSOURCE_INDEXEDDATA | instances;
                    }
                    else
                    {
                        frequency = D3DSTREAMSOURCE_INSTANCEDATA | attributes[i].divisor;
                    }
                    
                    device->SetStreamSourceFreq(stream, frequency);
                    mInstancingEnabled = true;
                }
            }

            if (mAppliedVBs[stream].serial != attributes[i].serial ||
                mAppliedVBs[stream].stride != attributes[i].stride ||
                mAppliedVBs[stream].offset != attributes[i].offset)
            {
                device->SetStreamSource(stream, attributes[i].vertexBuffer, attributes[i].offset, attributes[i].stride);
                mAppliedVBs[stream].serial = attributes[i].serial;
                mAppliedVBs[stream].stride = attributes[i].stride;
                mAppliedVBs[stream].offset = attributes[i].offset;
            }

            element->Stream = stream;
            element->Offset = 0;
            element->Type = attributes[i].type;
            element->Method = D3DDECLMETHOD_DEFAULT;
            element->Usage = D3DDECLUSAGE_TEXCOORD;
            element->UsageIndex = programBinary->getSemanticIndex(i);
            element++;
        }
    }

    if (instances == 0 || instancedAttribute == MAX_VERTEX_ATTRIBS)
    {
        if (mInstancingEnabled)
        {
            for (int i = 0; i < MAX_VERTEX_ATTRIBS; i++)
            {
                device->SetStreamSourceFreq(i, 1);
            }

            mInstancingEnabled = false;
        }
    }

    static const D3DVERTEXELEMENT9 end = D3DDECL_END();
    *(element++) = end;

    for (int i = 0; i < NUM_VERTEX_DECL_CACHE_ENTRIES; i++)
    {
        VertexDeclCacheEntry *entry = &mVertexDeclCache[i];
        if (memcmp(entry->cachedElements, elements, (element - elements) * sizeof(D3DVERTEXELEMENT9)) == 0 && entry->vertexDeclaration)
        {
            entry->lruCount = ++mMaxLru;
            if(entry->vertexDeclaration != mLastSetVDecl)
            {
                device->SetVertexDeclaration(entry->vertexDeclaration);
                mLastSetVDecl = entry->vertexDeclaration;
            }

            return GL_NO_ERROR;
        }
    }

    VertexDeclCacheEntry *lastCache = mVertexDeclCache;

    for (int i = 0; i < NUM_VERTEX_DECL_CACHE_ENTRIES; i++)
    {
        if (mVertexDeclCache[i].lruCount < lastCache->lruCount)
        {
            lastCache = &mVertexDeclCache[i];
        }
    }

    if (lastCache->vertexDeclaration != NULL)
    {
        lastCache->vertexDeclaration->Release();
        lastCache->vertexDeclaration = NULL;
        // mLastSetVDecl is set to the replacement, so we don't have to worry
        // about it.
    }

    memcpy(lastCache->cachedElements, elements, (element - elements) * sizeof(D3DVERTEXELEMENT9));
    device->CreateVertexDeclaration(elements, &lastCache->vertexDeclaration);
    device->SetVertexDeclaration(lastCache->vertexDeclaration);
    mLastSetVDecl = lastCache->vertexDeclaration;
    lastCache->lruCount = ++mMaxLru;

    return GL_NO_ERROR;
}

void VertexDeclarationCache::markStateDirty()
{
    for (int i = 0; i < MAX_VERTEX_ATTRIBS; i++)
    {
        mAppliedVBs[i].serial = 0;
    }

    mLastSetVDecl = NULL;
    mInstancingEnabled = true;   // Forces it to be disabled when not used
}

}

extern "C"
{
gl::Context *glCreateContext(const gl::Context *shareContext, rx::Renderer *renderer, bool notifyResets, bool robustAccess)
{
    return new gl::Context(shareContext, renderer, notifyResets, robustAccess);
}

void glDestroyContext(gl::Context *context)
{
    delete context;

    if (context == gl::getContext())
    {
        gl::makeCurrent(NULL, NULL, NULL);
    }
}

void glMakeCurrent(gl::Context *context, egl::Display *display, egl::Surface *surface)
{
    gl::makeCurrent(context, display, surface);
}

gl::Context *glGetCurrentContext()
{
    return gl::getContext();
}

}
