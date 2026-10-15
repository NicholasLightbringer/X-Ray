#include "stdafx.h"
#pragma hdrstop

#pragma warning(push)
#pragma warning(disable:4995)
#include <d3dx9.h>
#pragma warning(pop)

#include "frustum.h"

void CBackend::OnFrameEnd	()
{
#ifndef DEDICATED_SERVER
	for (u32 stage=0; stage<HW.Caps.raster.dwStages; stage++)
		CHK_DX(HW.pDevice->SetTexture(0,0));
	CHK_DX				(HW.pDevice->SetStreamSource	(0,0,0,0));
	CHK_DX				(HW.pDevice->SetIndices			(0));
	CHK_DX				(HW.pDevice->SetVertexShader	(0));
	CHK_DX				(HW.pDevice->SetPixelShader		(0));
	Invalidate			();
#endif
}

void CBackend::OnFrameBegin	()
{
#ifndef DEDICATED_SERVER
	PGO					(Msg("PGO:*****frame[%d]*****",Device.dwFrame));
	Memory.mem_fill		(&stat,0,sizeof(stat));
	Vertex.Flush		();
	Index.Flush			();
	set_Stencil			(FALSE);
#endif
}

void CBackend::Invalidate	()
{
	pRT[0]						= NULL;
	pRT[1]						= NULL;
	pRT[2]						= NULL;
	pRT[3]						= NULL;
	pZB							= NULL;

	decl						= NULL;
	vb							= NULL;
	ib							= NULL;
	vb_stride					= 0;

	state						= NULL;
	ps							= NULL;
	vs							= NULL;
	ctable						= NULL;

	T							= NULL;
	M							= NULL;
	C							= NULL;

	colorwrite_mask				= u32(-1);

	for (u32 ps_it =0; ps_it<16;)	textures_ps	[ps_it++]	= 0;
	for (u32 vs_it =0; vs_it< 5;)	textures_vs	[vs_it++]	= 0;
#ifdef _EDITOR
	for (u32 m_it =0; m_it< 8;)		matrices	[m_it++]	= 0;
#endif
}

void	CBackend::set_ClipPlanes	(u32 _enable, Fplane*	_planes /*=NULL */, u32 count/* =0*/)
{
	if (0==HW.Caps.geometry.dwClipPlanes)	return;
	if (!_enable)	{
		CHK_DX	(HW.pDevice->SetRenderState(D3DRS_CLIPPLANEENABLE,FALSE));
		return;
	}

	// Enable and setup planes
	VERIFY	(_planes && count);
	if		(count>HW.Caps.geometry.dwClipPlanes)	count=HW.Caps.geometry.dwClipPlanes;

	D3DXMATRIX			worldToClipMatrixIT;
	D3DXMatrixInverse	(&worldToClipMatrixIT,NULL,(D3DXMATRIX*)&Device.mFullTransform);
	D3DXMatrixTranspose	(&worldToClipMatrixIT,&worldToClipMatrixIT);
	for		(u32 it=0; it<count; it++)		{
		Fplane&		P			= _planes	[it];
		D3DXPLANE	planeWorld	(-P.n.x,-P.n.y,-P.n.z,-P.d), planeClip;
		D3DXPlaneNormalize		(&planeWorld,	&planeWorld);
		D3DXPlaneTransform		(&planeClip,	&planeWorld, &worldToClipMatrixIT);
		CHK_DX					(HW.pDevice->SetClipPlane(it,planeClip));
	}

	// Enable them
	u32		e_mask	= (1<<count)-1;
	CHK_DX	(HW.pDevice->SetRenderState(D3DRS_CLIPPLANEENABLE,e_mask));
}

#ifndef DEDICATED_SREVER
void	CBackend::set_ClipPlanes	(u32 _enable, Fmatrix*	_xform  /*=NULL */, u32 fmask/* =0xff */)
{
	if (0==HW.Caps.geometry.dwClipPlanes)	return;
	if (!_enable)	{
		CHK_DX	(HW.pDevice->SetRenderState(D3DRS_CLIPPLANEENABLE,FALSE));
		return;
	}
	VERIFY		(_xform && fmask);
	CFrustum	F;
	F.CreateFromMatrix	(*_xform,fmask);
	set_ClipPlanes		(_enable,F.planes,F.p_count);
}

static void NormalizeHUDArmTextureName(LPCSTR source, string_path destination)
{
	destination[0] = 0;
	if (!source || !source[0])
		return;

	strcpy_s(destination, sizeof(string_path), source);
	xr_strlwr(destination);
	for (LPSTR p = destination; *p; ++p)
	{
		if (*p == '/')
			*p = '\\';
	}

	// Texture resource names are relative to the textures directory and have no extension.
	if (0 == strncmp(destination, "textures\\", 9))
		memmove(destination, destination + 9, xr_strlen(destination + 9) + 1);

	LPSTR ext = strext(destination);
	if (ext && (0 == stricmp(ext, ".dds") || 0 == stricmp(ext, ".tga") ||
		0 == stricmp(ext, ".bmp") || 0 == stricmp(ext, ".ogm")))
		*ext = 0;
}

void CBackend::SetHUDArmTextureOverride(LPCSTR base_texture, LPCSTR replacement_texture)
{
	string_path base_name;
	string_path replacement_name;
	NormalizeHUDArmTextureName(base_texture, base_name);
	NormalizeHUDArmTextureName(replacement_texture, replacement_name);

	// A bare replacement name inherits the base texture's folder.
	if (replacement_name[0] && !strchr(replacement_name, '\\') && !strchr(replacement_name, '/'))
	{
		LPSTR separator = strrchr(base_name, '\\');
		if (separator)
		{
			string_path directory;
			size_t directory_length = size_t(separator - base_name + 1);
			strncpy_s(directory, sizeof(directory), base_name, directory_length);
			string_path resolved;
			strconcat(sizeof(resolved), resolved, directory, replacement_name);
			strcpy_s(replacement_name, sizeof(replacement_name), resolved);
		}
	}

	LPCSTR old_base = m_sHUDArmBaseTexture.size() ? m_sHUDArmBaseTexture.c_str() : "";
	LPCSTR old_replacement = m_sHUDArmReplacementTextureName.size() ? m_sHUDArmReplacementTextureName.c_str() : "";
	if (0 == xr_strcmp(old_base, base_name) && 0 == xr_strcmp(old_replacement, replacement_name))
		return;

	m_hHUDArmReplacementTexture.destroy();
	m_sHUDArmBaseTexture = base_name[0] ? base_name : NULL;
	m_sHUDArmReplacementTextureName = replacement_name[0] ? replacement_name : NULL;
	if (base_name[0] && replacement_name[0] && 0 != xr_strcmp(base_name, replacement_name))
		m_hHUDArmReplacementTexture.create(replacement_name);

	// Force the backend to rebind materials after the override changes.
	T = NULL;
	for (u32 i = 0; i < _countof(textures_ps); ++i)
		textures_ps[i] = NULL;
}

void CBackend::SetHUDArmTexturePass(BOOL active)
{
	bool new_state = (active != FALSE);
	if (m_bHUDArmTexturePass != new_state)
	{
		m_bHUDArmTexturePass = new_state;
		T = NULL;
	}
}

void CBackend::set_Textures			(STextureList* _T)
{
	if (T == _T)	return;
	T				= _T;
	u32 _last_ps	= 0;
	u32 _last_vs	= 0;
	STextureList::iterator	_it		= _T->begin	();
	STextureList::iterator	_end	= _T->end	();
	for (; _it!=_end; _it++)
	{
		std::pair<u32,ref_texture>&		loader	=	*_it;
		u32			load_id		= loader.first		;
		CTexture*	load_surf	= &*loader.second	;
		if (m_bHUDArmTexturePass && load_id < 256 && load_surf &&
			m_sHUDArmBaseTexture.size() && m_hHUDArmReplacementTexture._get())
		{
			string_path actual_name;
			NormalizeHUDArmTextureName(load_surf->cName.c_str(), actual_name);
			if (0 == xr_strcmp(actual_name, m_sHUDArmBaseTexture.c_str()))
				load_surf = m_hHUDArmReplacementTexture._get();
		}
		if (load_id<256)
		{
			// ordinary pixel surface
			if (load_id>_last_ps)		_last_ps	=	load_id;
			if (textures_ps[load_id]!=load_surf)	{
				textures_ps[load_id]	= load_surf			;
#ifdef DEBUG
				stat.textures			++;
#endif
				if (load_surf)			{
					PGO					(Msg("PGO:tex%d:%s",load_id,load_surf->cName.c_str()));
					load_surf->bind		(load_id);
//					load_surf->Apply	(load_id);
				}
			}
		} else {
			// d-map or vertex	
			u32		load_id_remapped	= load_id-256;
			if (load_id_remapped>_last_vs)	_last_vs	=	load_id_remapped;
			if (textures_vs[load_id_remapped]!=load_surf)	{
				textures_vs[load_id_remapped]	= load_surf			;
#ifdef DEBUG
				stat.textures	++;
#endif
				if (load_surf)	{
					PGO					(Msg("PGO:tex%d:%s",load_id,load_surf->cName.c_str()));
					load_surf->bind		(load_id);
//					load_surf->Apply	(load_id);
				}
			}
		}
	}

	// clear remaining stages (PS)
	for (++_last_ps; _last_ps<16 && textures_ps[_last_ps]; _last_ps++)	{
		textures_ps[_last_ps]			= 0;
		CHK_DX							(HW.pDevice->SetTexture(_last_ps,NULL));
	}
	// clear remaining stages (VS)
	for (++_last_vs; _last_vs<5 && textures_vs[_last_vs]; _last_vs++)		{
		textures_vs[_last_vs]			= 0;
		CHK_DX							(HW.pDevice->SetTexture(_last_vs+256,NULL));
	}
}
#else

void	CBackend::set_ClipPlanes	(u32 _enable, Fmatrix*	_xform  /*=NULL */, u32 fmask/* =0xff */) {}
void CBackend::set_Textures			(STextureList* _T) {}

#endif