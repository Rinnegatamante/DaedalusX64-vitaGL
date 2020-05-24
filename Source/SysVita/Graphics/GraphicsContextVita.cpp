#include "stdafx.h"
#include "Graphics/GraphicsContext.h"

#include <psp2/gxm.h>
#include <psp2/display.h>

#include <vitaGL.h>

#include "Config/ConfigOptions.h"
#include "Core/ROM.h"
#include "Debug/DBGConsole.h"
#include "Debug/Dump.h"
#include "Graphics/ColourValue.h"
#include "Graphics/PngUtil.h"
#include "SysPSP/Graphics/VideoMemoryManager.h"
#include "Utility/IO.h"
#include "Utility/Preferences.h"
#include "Utility/Profiler.h"
#include "Utility/VolatileMem.h"
#include "SysVita/UI/Menu.h"

#include "HLEGraphics/BaseRenderer.h"

extern bool pause_emu;
bool gWaitRendering = false;

uint16_t *gIndexBufferForRect;
uint16_t *gIndexBufferPtr;
float *gVertexBuffer;
uint32_t *gColorBuffer;
float *gTexCoordBuffer;
uint16_t *gIndexBuffer;
float *gVertexBufferPtr;
uint32_t *gColorBufferPtr;
float *gTexCoordBufferPtr;
bool new_frame = true;

extern float gamma_val;

class IGraphicsContext : public CGraphicsContext
{
public:
	IGraphicsContext();
	virtual ~IGraphicsContext();

	bool				Initialise();
	bool				IsInitialised() const { return mInitialised; }

	void				SwitchToChosenDisplay();
	void				SwitchToLcdDisplay();
	void				StoreSaveScreenData();

	void				ClearAllSurfaces();

	void				ClearToBlack();
	void				ClearZBuffer();
	void				ClearColBuffer(const c32 &colour);
	void				ClearColBufferAndDepth(const c32 &colour);

	void				BeginFrame();
	void				EndFrame();
	void				UpdateFrame(bool wait_for_vbl);
	void				GetScreenSize(u32 * width, u32 * height) const;

	void				SetDebugScreenTarget( ETargetSurface buffer );

	void				ViewportType(u32 *d_width, u32 *d_height) const;
	void				DumpScreenShot();
	void				DumpNextScreen()			{ mDumpNextScreen = 2; }

private:
	void				SaveScreenshot(const char* filename, s32 x, s32 y, u32 width, u32 height);

private:
	bool				mInitialised;

	u32					mDumpNextScreen;
};

//*************************************************************************************
//
//*************************************************************************************
template<> bool CSingleton< CGraphicsContext >::Create()
{
#ifdef DAEDALUS_ENABLE_ASSERTS
	DAEDALUS_ASSERT_Q(mpInstance == nullptr);
#endif
	mpInstance = new IGraphicsContext();
	return mpInstance->Initialise();
}

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

IGraphicsContext::IGraphicsContext()
	:	mInitialised(false)
	,	mDumpNextScreen(false)
{	
	gVertexBufferPtr = (float*)malloc(0x600000);
	gColorBufferPtr = (uint32_t*)malloc(0x600000);
	gTexCoordBufferPtr = (float*)malloc(0x600000);
	gIndexBufferPtr = (uint16_t*)malloc(0x200000);
	gVertexBuffer = gVertexBufferPtr;
	gColorBuffer = gColorBufferPtr;
	gTexCoordBuffer = gTexCoordBufferPtr;
	
	// Reserving first 4 indices for 2D renders
	for (int i = 0; i < 4; i++) {
		gIndexBufferPtr[i] = i;
	}
	gIndexBufferForRect = gIndexBufferPtr;
	gIndexBufferPtr += 4;
	gIndexBuffer = gIndexBufferPtr;
}

IGraphicsContext::~IGraphicsContext()
{
	vglEnd();
}

bool IGraphicsContext::Initialise()
{
	mInitialised = true;
	return true;
}

void IGraphicsContext::ClearAllSurfaces()
{
}

void IGraphicsContext::ClearToBlack()
{
	glDepthMask(GL_TRUE);
	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
	glClearDepth( 1.0f );
	glClear((GLbitfield)(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT) );
}

void IGraphicsContext::ClearZBuffer()
{
	glDepthMask(GL_TRUE);
	glClearDepth( 1.0f );
	glClear( GL_DEPTH_BUFFER_BIT );
}

void IGraphicsContext::ClearColBuffer(const c32 & colour)
{
	glClearColor( colour.GetRf(), colour.GetGf(), colour.GetBf(), colour.GetAf() );
	glClear( GL_COLOR_BUFFER_BIT );
}

void IGraphicsContext::ClearColBufferAndDepth(const c32 & colour)
{
	glDepthMask(GL_TRUE);
	glClearDepth( 1.0f );
	glClearColor( colour.GetRf(), colour.GetGf(), colour.GetBf(), colour.GetAf() );
	glClear((GLbitfield)(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT) );
}

void IGraphicsContext::BeginFrame()
{
	vglStartRendering();
	glEnableClientState(GL_VERTEX_ARRAY);
	gVertexBuffer = gVertexBufferPtr;
	gColorBuffer = gColorBufferPtr;
	gTexCoordBuffer = gTexCoordBufferPtr;
	gIndexBuffer = gIndexBufferPtr;
	
	if (new_frame) {
		CGraphicsContext::Get()->ClearToBlack();
		new_frame = false;
	}
}

void IGraphicsContext::EndFrame()
{
	glViewport(0, 0, SCR_WIDTH, SCR_HEIGHT);
	glScissor( 0, 0, SCR_WIDTH, SCR_HEIGHT);
	if (gamma_val != 1.0f) gRenderer->DoGamma(gamma_val);
	DrawInGameMenu();
	if (gWaitRendering) glFinish();
}

void IGraphicsContext::UpdateFrame(bool wait_for_vbl)
{
	vglStopRendering();
	new_frame = true;
	if (pause_emu) {
		BeginFrame();
		EndFrame();
		UpdateFrame(false);
	}
}

void IGraphicsContext::SetDebugScreenTarget(ETargetSurface buffer)
{

}

void IGraphicsContext::ViewportType(u32 *d_width, u32 *d_height) const
{
	GetScreenSize(d_width, d_height);
}

void IGraphicsContext::SaveScreenshot(const char* filename, s32 x, s32 y, u32 width, u32 height)
{
}

void IGraphicsContext::DumpScreenShot()
{
}

void IGraphicsContext::StoreSaveScreenData()
{
}

void IGraphicsContext::GetScreenSize(u32 * p_width, u32 * p_height) const
{
	// Note: Change these if you change SCR_WIDTH/SCR_HEIGHT
	switch (gAspectRatio) {
	case RATIO_4_3:
		*p_width  = 725;
		*p_height = SCR_HEIGHT;
		break;
	case RATIO_ORIG:
		*p_width  = 640;
		*p_height = 480;
		break;
	default:
		*p_width  = SCR_WIDTH;
		*p_height = SCR_HEIGHT;
		break;
	}
}
