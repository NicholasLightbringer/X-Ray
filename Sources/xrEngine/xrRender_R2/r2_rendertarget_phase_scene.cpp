#include "stdafx.h"

// startup
void	CRenderTarget::phase_scene_prepare	()
{
	// Clear depth & stencil
	u_setrt	( Device.dwWidth,Device.dwHeight,HW.pBaseRT,NULL,NULL,HW.pBaseZB );
	CHK_DX	( HW.pDevice->Clear	( 0L, NULL, D3DCLEAR_ZBUFFER|D3DCLEAR_STENCIL, 0x0, 1.0f, 0L) );
}

// begin
void	CRenderTarget::phase_scene_begin	()
{
	// Enable ANISO
	for (u32 i=0; i<HW.Caps.raster.dwStages; i++)
		CHK_DX(HW.pDevice->SetSamplerState( i, D3DSAMP_MAXANISOTROPY, ps_r__tf_Anisotropic	));

	// Targets, use accumulator for temporary storage. Reflection has an independent MRT set.
	if (RImplementation.bRenderingReflection) {
		u_setrt(rt_WaterReflectionPosition,0,0,rt_WaterReflectionZB);
		CHK_DX(HW.pDevice->Clear(0L,NULL,D3DCLEAR_TARGET|D3DCLEAR_ZBUFFER|D3DCLEAR_STENCIL,0,1.0f,0L));
		u_setrt(rt_WaterReflectionNormal,0,0,rt_WaterReflectionZB);
		CHK_DX(HW.pDevice->Clear(0L,NULL,D3DCLEAR_TARGET,0,1.0f,0L));
		u_setrt(rt_WaterReflectionRaw,0,0,rt_WaterReflectionZB);
		CHK_DX(HW.pDevice->Clear(0L,NULL,D3DCLEAR_TARGET,0,1.0f,0L));
		u_setrt(rt_WaterReflectionPosition,rt_WaterReflectionNormal,rt_WaterReflectionRaw,rt_WaterReflectionZB);
	} else {
		if (RImplementation.o.albedo_wo)	u_setrt(rt_Position,rt_Normal,rt_Accumulator,HW.pBaseZB);
		else							u_setrt(rt_Position,rt_Normal,rt_Color,HW.pBaseZB);
	}

	// Stencil - write 0x1 at pixel pos
	RCache.set_Stencil					( TRUE,D3DCMP_ALWAYS,0x01,0xff,0xff,D3DSTENCILOP_KEEP,D3DSTENCILOP_REPLACE,D3DSTENCILOP_KEEP);

	// Misc		- draw only front-faces
	CHK_DX(HW.pDevice->SetRenderState	( D3DRS_TWOSIDEDSTENCILMODE,FALSE				));
	RCache.set_CullMode					( RImplementation.bRenderingReflection ? CULL_CW : CULL_CCW );
	RCache.set_ColorWriteEnable			( );
}

void	CRenderTarget::disable_aniso		()
{
	// Disable ANISO
	for (u32 i=0; i<HW.Caps.raster.dwStages; i++)
		CHK_DX(HW.pDevice->SetSamplerState( i, D3DSAMP_MAXANISOTROPY, 1	));
}

// end
void	CRenderTarget::phase_scene_end		()
{
	disable_aniso	();

	if (RImplementation.bRenderingReflection) {
		phase_water_reflection_copy();
		return;
	}

	if (!RImplementation.o.albedo_wo)		return;

	// transfer from "rt_Accumulator" into "rt_Color"
	u_setrt								( rt_Color,	0,	0,	HW.pBaseZB	);
	RCache.set_CullMode					( CULL_NONE );
	RCache.set_Stencil					(TRUE,D3DCMP_LESSEQUAL,0x01,0xff,0x00);	// stencil should be >= 1
	if (RImplementation.o.nvstencil)	u_stencil_optimize	(FALSE);
	RCache.set_Stencil					(TRUE,D3DCMP_LESSEQUAL,0x01,0xff,0x00);	// stencil should be >= 1
	RCache.set_ColorWriteEnable			();

	// common calc for quad-rendering
	u32		Offset;
	u32		C					= color_rgba	(255,255,255,255);
	float	_w					= float			(Device.dwWidth);
	float	_h					= float			(Device.dwHeight);
	Fvector2					p0,p1;
	p0.set						(.5f/_w, .5f/_h);
	p1.set						((_w+.5f)/_w, (_h+.5f)/_h );
	float	d_Z	= EPS_S, d_W = 1.f;

	// Fill vertex buffer
	FVF::TL* pv					= (FVF::TL*)	RCache.Vertex.Lock	(4,g_combine->vb_stride,Offset);
	pv->set						(EPS,			float(_h+EPS),	d_Z,	d_W, C, p0.x, p1.y);	pv++;
	pv->set						(EPS,			EPS,			d_Z,	d_W, C, p0.x, p0.y);	pv++;
	pv->set						(float(_w+EPS),	float(_h+EPS),	d_Z,	d_W, C, p1.x, p1.y);	pv++;
	pv->set						(float(_w+EPS),	EPS,			d_Z,	d_W, C, p1.x, p0.y);	pv++;
	RCache.Vertex.Unlock		(4,g_combine->vb_stride);

	// if (stencil>=1 && aref_pass)	stencil = light_id
	RCache.set_Element			(s_accum_mask->E[SE_MASK_ALBEDO]);		// masker
	RCache.set_Geometry			(g_combine);
	RCache.Render				(D3DPT_TRIANGLELIST,Offset,0,4,0,2);
}


// Copy the raw albedo of the reflected G-buffer into the texture used by water.
void CRenderTarget::phase_water_reflection_copy()
{
	// Unbind the source before sampling it; D3D9 forbids render-target feedback.
	u_setrt(rt_Generic_0,0,0,HW.pBaseZB);
	u_setrt(rt_WaterReflection,0,0,HW.pBaseZB);
	RCache.set_CullMode(CULL_NONE);
	RCache.set_Stencil(FALSE);
	RCache.set_ColorWriteEnable();

	u32 Offset;
	u32 C = color_rgba(255,255,255,255);
	float w = float(Device.dwWidth), h = float(Device.dwHeight);
	Fvector2 p0,p1;
	p0.set(.5f/w,.5f/h);
	p1.set((w+.5f)/w,(h+.5f)/h);
	float z = EPS_S, rhw = 1.f;

	FVF::TL* pv = (FVF::TL*)RCache.Vertex.Lock(4,g_combine->vb_stride,Offset);
	pv->set(EPS,float(h+EPS),z,rhw,C,p0.x,p1.y); pv++;
	pv->set(EPS,EPS,z,rhw,C,p0.x,p0.y); pv++;
	pv->set(float(w+EPS),float(h+EPS),z,rhw,C,p1.x,p1.y); pv++;
	pv->set(float(w+EPS),EPS,z,rhw,C,p1.x,p0.y); pv++;
	RCache.Vertex.Unlock(4,g_combine->vb_stride);

	RCache.set_Element(s_water_reflection_copy->E[0]);
	RCache.set_Geometry(g_combine);
	RCache.Render(D3DPT_TRIANGLELIST,Offset,0,4,0,2);

	// Leave all reflection textures detached for subsequent water sampling.
	u_setrt(rt_Generic_0,0,0,HW.pBaseZB);
}
