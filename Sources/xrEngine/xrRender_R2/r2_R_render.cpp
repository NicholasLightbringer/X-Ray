#include "stdafx.h"
#include "..\igame_persistent.h"
#include "..\fbasicvisual.h"
#include "..\customhud.h"
#include "..\xr_object.h"
#include "GlowManager.h"
#include "..\xrRender\xrRender_console.h"

IC	bool	pred_sp_sort	(ISpatial*	_1, ISpatial* _2)
{
	float	d1		= _1->spatial.sphere.P.distance_to_sqr	(Device.vCameraPosition);
	float	d2		= _2->spatial.sphere.P.distance_to_sqr	(Device.vCameraPosition);
	return	d1<d2	;
}

void CRender::render_main	(Fmatrix&	m_ViewProjection, bool _fportals)
{
//	Msg						("---begin");
	marker					++;

	// Calculate sector(s) and their objects
	if (pLastSector)		{
		//!!!
		//!!! BECAUSE OF PARALLEL HOM RENDERING TRY TO DELAY ACCESS TO HOM AS MUCH AS POSSIBLE
		//!!!
		{
			// Traverse object database for main and reflected views
			g_SpatialSpace->q_frustum
				(
				lstRenderables,
				ISpatial_DB::O_ORDERED,
				STYPE_RENDERABLE + STYPE_LIGHTSOURCE,
				ViewBase
				);

			// (almost) Exact sorting order (front-to-back)
			std::sort			(lstRenderables.begin(),lstRenderables.end(),pred_sp_sort);

			// Determine visibility for dynamic part of scene
			set_Object							(0);
			u32 uID_LTRACK						= 0xffffffff;
			if (!bRenderingReflection && phase==PHASE_NORMAL)			{
				uLastLTRACK	++;
				if (lstRenderables.size())		uID_LTRACK	= uLastLTRACK%lstRenderables.size();

				// update light-vis for current entity / actor
				CObject*	O					= g_pGameLevel->CurrentViewEntity();
				if (O)		{
					CROS_impl*	R					= (CROS_impl*) O->ROS();
					if (R)		R->update			(O);
				}

				// update light-vis for selected entity
				// track lighting environment
				if (lstRenderables.size())		{
					IRenderable*	renderable		= lstRenderables[uID_LTRACK]->dcast_Renderable	();
					if (renderable)	{
						CROS_impl*		T = (CROS_impl*)renderable->renderable_ROS	();
						if (T)			T->update	(renderable);
					}
				}
			}
		}

		// Traverse sector/portal structure
		PortalTraverser.traverse	
			(
			pLastSector,
			ViewBase,
			Device.vCameraPosition,
			m_ViewProjection,
			(bRenderingReflection ? (CPortalTraverser::VQ_SSA + CPortalTraverser::VQ_FADE) : (CPortalTraverser::VQ_HOM + CPortalTraverser::VQ_SSA + CPortalTraverser::VQ_FADE))
			//. disabled scissoring (HW.Caps.bScissor?CPortalTraverser::VQ_SCISSOR:0)	// generate scissoring info
			);

		// Determine visibility for static geometry hierrarhy
		for (u32 s_it=0; s_it<PortalTraverser.r_sectors.size(); s_it++)
		{
			CSector*	sector		= (CSector*)PortalTraverser.r_sectors[s_it];
			IRender_Visual*	root	= sector->root();
			for (u32 v_it=0; v_it<sector->r_frustums.size(); v_it++)	{
				set_Frustum			(&(sector->r_frustums[v_it]));
				add_Geometry		(root);
			}
		}

		// Traverse dynamic renderables for reflected camera too; main-camera HOM is invalid here.
		{
		// Traverse frustums
		for (u32 o_it=0; o_it<lstRenderables.size(); o_it++)
		{
			ISpatial*	spatial		= lstRenderables[o_it];		spatial->spatial_updatesector	();
			CSector*	sector		= (CSector*)spatial->spatial.sector;
			if	(0==sector)										continue;	// disassociated from S/P structure

			if (spatial->spatial.type & STYPE_LIGHTSOURCE)		{
				if (bRenderingReflection) continue; // no light collection in reflection pass
				// lightsource
				light*			L				= (light*)	(spatial->dcast_Light());
				VERIFY							(L);
				float	lod		= L->get_LOD	();
				if (lod>EPS_L)	{
					vis_data&		vis		= L->get_homdata	( );
					if	(HOM.visible(vis))	Lights.add_light	(L);
				}
				continue					;
			}

			if	(PortalTraverser.i_marker != sector->r_marker)	continue;	// inactive (untouched) sector
			for (u32 v_it=0; v_it<sector->r_frustums.size(); v_it++)	{
				CFrustum&	view	= sector->r_frustums[v_it];
				if (!view.testSphere_dirty(spatial->spatial.sphere.P,spatial->spatial.sphere.R))	continue;

				if (spatial->spatial.type & STYPE_RENDERABLE)
				{
					// renderable
					IRenderable*	renderable		= spatial->dcast_Renderable	();
                    if (!renderable) {
                        CGlow* glow = dynamic_cast<CGlow*>(spatial);
                        if (glow && L_Glows && ps_r2_glow_show) L_Glows->add(glow);
                        break;
                    }

					// Occlusion
					vis_data&		v_orig			= renderable->renderable.visual->vis;
					vis_data		v_copy			= v_orig;
					v_copy.box.xform				(renderable->renderable.xform);
					BOOL			bVisible		= bRenderingReflection ? TRUE : HOM.visible(v_copy);
					v_orig.marker					= v_copy.marker;
					v_orig.accept_frame				= v_copy.accept_frame;
					v_orig.hom_frame				= v_copy.hom_frame;
					v_orig.hom_tested				= v_copy.hom_tested;
					if (!bVisible)					break;	// exit loop on frustums

					// Rendering
					set_Object						(renderable);
					renderable->renderable_Render	();
					set_Object						(0);
				}
				break;	// exit loop on frustums
			}
		}
		}
		if (!bRenderingReflection && g_pGameLevel && (phase==PHASE_NORMAL))	g_pGameLevel->pHUD->Render_Last();		// HUD
	}
	else
	{
		set_Object									(0);
		if (!bRenderingReflection && g_pGameLevel && (phase==PHASE_NORMAL))	g_pGameLevel->pHUD->Render_Last();		// HUD
	}
}

void CRender::render_water_reflection()
{
	// Keep the previous plane as a short-term anchor. The water candidate list is
	// rebuilt from the current visibility traversal and can briefly change while
	// turning the camera, which otherwise causes a one-frame reflection jump.
	BOOL previousPlaneKnown = bWaterReflectionPlaneKnown;
	float previousWaterHeight = mWaterReflectionHeight;
	// The console toggle disables only the custom planar capture. Keeping the
	// validity flag false makes the water shader use its original cubemap fallback.
	bWaterReflectionValid = FALSE;
	if (!ps_r2_water_reflections) return;
	// The reflection may be temporarily unavailable while the camera is underwater
	// or the water plane drops out of the visibility list. Keep the last known plane
	// separately from per-frame texture validity so we can reacquire it on return.
	bWaterReflectionValid = FALSE;
	if (!Target || bRenderingReflection || !pLastSector) return;
	if (!HW.Caps.geometry.dwClipPlanes) return;
	// Do not replace the remembered surface with a lower water body while the
	// camera is below the last valid surface. The renderer has one planar capture;
	// switching to another level here leaves the water shader projecting against
	// the wrong plane after the camera comes back up.
	if (previousPlaneKnown && Device.vCameraPosition.y <= previousWaterHeight + 0.1f) return;

	// A single planar texture represents one horizontal water level. Prefer the
	// closest level beneath the camera, but add a small hysteresis to avoid
	// switching planes for tiny visibility-list changes.
	float waterHeight = 0.f;
	float nearest = flt_max;
	bool found = false;
	for (u32 i=0; i<WaterReflectionHeights.size(); ++i) {
		float h = WaterReflectionHeights[i];
		if (h >= Device.vCameraPosition.y-0.1f) continue;
		float d = Device.vCameraPosition.y-h;
		if (d < nearest) { nearest=d; waterHeight=h; found=true; }
	}
	if (previousPlaneKnown && previousWaterHeight < Device.vCameraPosition.y-0.1f) {
		float previousDistance = Device.vCameraPosition.y-previousWaterHeight;
		// Reuse the last plane if it is still reasonably close and the newly
		// selected plane is not clearly closer. Also bridge an empty candidate frame.
		if (previousDistance < 80.f && (!found || previousDistance <= nearest+1.5f)) {
			waterHeight = previousWaterHeight;
			found = true;
		}
	}
	if (!found) return;

	Fmatrix oldDeviceView = Device.mView;
	Fmatrix oldProject = Device.mProject;
	Fmatrix oldFull = Device.mFullTransform;
	Fvector oldPosition = Device.vCameraPosition;
	Fvector oldDirection = Device.vCameraDirection;
	Fvector oldTop = Device.vCameraTop;
	Fvector oldRight = Device.vCameraRight;
	CFrustum oldViewBase = ViewBase;
	CFrustum* oldViewFrustum = View;
	CSector* oldSector = pLastSector;
	u32 oldPhase = phase;
	bool oldMask0 = pmask[0], oldMask1 = pmask[1], oldMaskWmark = pmask_wmark;
	IRenderable* oldObject = val_pObject;
	Fmatrix* oldTransform = val_pTransform;
	BOOL oldHUD = val_bHUD;
	BOOL oldInvisible = val_bInvisible;
	BOOL oldRecordMP = val_bRecordMP;
	xr_vector<Fbox3,render_alloc<Fbox3> >* oldRecorder = val_recorder;
	R_feedback* oldFeedback = val_feedback;
	u32 oldFeedbackBreak = val_feedback_breakp;

	Fmatrix mirror;
	mirror.identity();
	mirror._22 = -1.f;
	mirror._42 = 2.f*waterHeight;

	mWaterReflectionVP.identity();
	Device.vCameraPosition.set(oldPosition.x,2.f*waterHeight-oldPosition.y,oldPosition.z);
	mirror.transform_dir(Device.vCameraDirection,oldDirection);
	mirror.transform_dir(Device.vCameraTop,oldTop);
	mirror.transform_dir(Device.vCameraRight,oldRight);
	Device.mView.mul(oldDeviceView,mirror); // mirror world coordinates, then apply original view
	// Slightly widen only the reflection capture frustum. This expands the
	// reflected coverage at water edges without increasing shader edge blur.
	Device.mProject._11 *= 0.90f;
	Device.mProject._22 *= 0.90f;
	Device.mFullTransform.mul(Device.mProject,Device.mView);
	mWaterReflectionVP.set(Device.mFullTransform);
	RCache.set_xform_view(Device.mView);
	RCache.set_xform_project(Device.mProject);
	ViewBase.CreateFromMatrix(Device.mFullTransform,FRUSTUM_P_LRTB+FRUSTUM_P_FAR);
	View = &ViewBase;
	Fvector sectorDirection = Device.vCameraDirection;
	CSector* reflectedSector = (CSector*)detectSector(Device.vCameraPosition,sectorDirection);
	pLastSector = reflectedSector ? reflectedSector : oldSector;

	Fplane waterClip;
	waterClip.n.set(0.f,-1.f,0.f);
	waterClip.d = waterHeight;
	RCache.set_ClipPlanes(TRUE,&waterClip,1);

	bRenderingReflection = TRUE;
	phase = PHASE_NORMAL;
	r_pmask(true,false,false);
	set_Object(0);
	val_bHUD = FALSE;
	val_bInvisible = FALSE;
	val_bRecordMP = FALSE;
	val_recorder = NULL;

	render_main(Device.mFullTransform,true);
	Target->phase_scene_begin();
	r_dsgraph_render_graph(0);
	r_dsgraph_render_lods(true,true);
	if (Details) Details->Render();
	// Geometry pass is complete. Disable the user plane before the full-screen resolve.
	RCache.set_ClipPlanes(FALSE, (Fplane*)NULL, 0);
	Target->phase_scene_end();

	bRenderingReflection = FALSE;
	RCache.set_CullMode(CULL_CCW);
	RCache.set_Stencil(FALSE);
	Device.mView.set(oldDeviceView);
	Device.mProject.set(oldProject);
	Device.mFullTransform.set(oldFull);
	Device.vCameraPosition.set(oldPosition);
	Device.vCameraDirection.set(oldDirection);
	Device.vCameraTop.set(oldTop);
	Device.vCameraRight.set(oldRight);
	ViewBase = oldViewBase;
	View = oldViewFrustum;
	pLastSector = oldSector;
	phase = oldPhase;
	r_pmask(oldMask0,oldMask1,oldMaskWmark);
	val_pObject = oldObject;
	val_pTransform = oldTransform;
	val_bHUD = oldHUD;
	val_bInvisible = oldInvisible;
	val_bRecordMP = oldRecordMP;
	val_recorder = oldRecorder;
	val_feedback = oldFeedback;
	val_feedback_breakp = oldFeedbackBreak;
	// Commit the plane only after the reflection pass completed and camera state
	// was restored. Failed/skipped underwater passes must not poison plane memory.
	mWaterReflectionHeight = waterHeight;
	bWaterReflectionPlaneKnown = TRUE;
	RCache.set_xform_view(Device.mView);
	RCache.set_xform_project(Device.mProject);
	RCache.set_xform_world(Fidentity);
	rmNormal();
	bWaterReflectionValid = TRUE;
}

void CRender::render_menu	()
{
	//	Globals
	RCache.set_CullMode				(CULL_CCW);
	RCache.set_Stencil				(FALSE);
	RCache.set_ColorWriteEnable		();

	// Main Render
	{
		Target->u_setrt						(Target->rt_Generic_0,0,0,HW.pBaseZB);		// LDR RT
		g_pGamePersistent->OnRenderPPUI_main()	;	// PP-UI
	}
	// Distort
	{
		Target->u_setrt						(Target->rt_Generic_1,0,0,HW.pBaseZB);		// Now RT is a distortion mask
		CHK_DX(HW.pDevice->Clear			( 0L, NULL, D3DCLEAR_TARGET, color_rgba(127,127,0,127), 1.0f, 0L));
		g_pGamePersistent->OnRenderPPUI_PP	()	;	// PP-UI
	}

	// Actual Display
	Target->u_setrt					( Device.dwWidth,Device.dwHeight,HW.pBaseRT,NULL,NULL,HW.pBaseZB);
	RCache.set_Shader				( Target->s_menu	);
	RCache.set_Geometry				( Target->g_menu	);

	Fvector2						p0,p1;
	u32								Offset;
	u32		C						= color_rgba	(255,255,255,255);
	float	_w						= float(Device.dwWidth);
	float	_h						= float(Device.dwHeight);
	float	d_Z						= EPS_S;
	float	d_W						= 1.f;
	p0.set							(.5f/_w, .5f/_h);
	p1.set							((_w+.5f)/_w, (_h+.5f)/_h );

	FVF::TL* pv						= (FVF::TL*) RCache.Vertex.Lock	(4,Target->g_menu->vb_stride,Offset);
	pv->set							(EPS,			float(_h+EPS),	d_Z,	d_W, C, p0.x, p1.y);	pv++;
	pv->set							(EPS,			EPS,			d_Z,	d_W, C, p0.x, p0.y);	pv++;
	pv->set							(float(_w+EPS),	float(_h+EPS),	d_Z,	d_W, C, p1.x, p1.y);	pv++;
	pv->set							(float(_w+EPS),	EPS,			d_Z,	d_W, C, p1.x, p0.y);	pv++;
	RCache.Vertex.Unlock			(4,Target->g_menu->vb_stride);
	RCache.Render					(D3DPT_TRIANGLELIST,Offset,0,4,0,2);
}

extern u32 g_r;
void CRender::Render		()
{
	g_r						= 1;
	VERIFY					(0==mapDistort.size());

	bool	_menu_pp		= g_pGamePersistent?g_pGamePersistent->OnRenderPPUI_query():false;
	if (_menu_pp)			{
		render_menu			()	;
		return					;
	};
	if( !(g_pGameLevel && g_pGameLevel->pHUD) )	return;
//.	VERIFY					(g_pGameLevel && g_pGameLevel->pHUD);

	// Configure
	RImplementation.o.distortion				= FALSE;		// disable distorion
	Fcolor					sun_color			= ((light*)Lights.sun_adapted._get())->color;
	BOOL					bSUN				= ps_r2_ls_flags.test(R2FLAG_SUN) && (u_diffuse2s(sun_color.r,sun_color.g,sun_color.b)>EPS);
	if (o.sunstatic)		bSUN				= FALSE;
	// Msg						("sstatic: %s, sun: %s",o.sunstatic?"true":"false", bSUN?"true":"false");

	// HOM
	ViewBase.CreateFromMatrix					(Device.mFullTransform, FRUSTUM_P_LRTB + FRUSTUM_P_FAR);
	View										= 0;
	if (!ps_r2_ls_flags.test(R2FLAG_EXP_MT_CALC))	{
		HOM.Enable									();
		HOM.Render									(ViewBase);
	}

	//******* Z-prefill calc - DEFERRER RENDERER
	if (ps_r2_ls_flags.test(R2FLAG_ZFILL))		{
		Device.Statistic->RenderCALC.Begin			();
		float		z_distance	= ps_r2_zfill		;
		Fmatrix		m_zfill, m_project				;
		m_project.build_projection	(
			deg2rad(Device.fFOV/* *Device.fASPECT*/), 
			Device.fASPECT, VIEWPORT_NEAR, 
			z_distance * g_pGamePersistent->Environment().CurrentEnv.far_plane);
		m_zfill.mul	(m_project,Device.mView);
		r_pmask										(true,false);	// enable priority "0"
		set_Recorder								(NULL)		;
		phase										= PHASE_SMAP;
		render_main									(m_zfill,false)	;
		r_pmask										(true,false);	// disable priority "1"
		Device.Statistic->RenderCALC.End				( )			;

		// flush
		Target->phase_scene_prepare					();
		RCache.set_ColorWriteEnable					(FALSE);
		r_dsgraph_render_graph						(0);
		RCache.set_ColorWriteEnable					( );
	} else {
		Target->phase_scene_prepare					();
	}

	//*******
	// Sync point
	Device.Statistic->RenderDUMP_Wait_S.Begin	();
	if (1)
	{
		CTimer	T;							T.Start	();
		BOOL	result						= FALSE;
		HRESULT	hr							= S_FALSE;
		while	((hr=q_sync_point[q_sync_count]->GetData	(&result,sizeof(result),D3DGETDATA_FLUSH))==S_FALSE) {
			if (!SwitchToThread())			Sleep(ps_r2_wait_sleep);
			if (T.GetElapsed_ms() > 500)	{
				result	= FALSE;
				break;
			}
		}
	}
	Device.Statistic->RenderDUMP_Wait_S.End		();
	q_sync_count								= (q_sync_count+1)%2;
	CHK_DX										(q_sync_point[q_sync_count]->Issue(D3DISSUE_END));

	//******* Main calc - DEFERRER RENDERER
	// Main calc
	Device.Statistic->RenderCALC.Begin			();
	r_pmask										(true,false,true);	// enable priority "0",+ capture wmarks
	if (bSUN)									set_Recorder	(&main_coarse_structure);
	else										set_Recorder	(NULL);
	phase										= PHASE_NORMAL;
	WaterReflectionHeights.clear();
	render_main									(Device.mFullTransform,true);
	set_Recorder								(NULL);
	r_pmask										(true,false);	// disable priority "1"
	Device.Statistic->RenderCALC.End			();

	BOOL	split_the_scene_to_minimize_wait		= FALSE;
	if (ps_r2_ls_flags.test(R2FLAG_EXP_SPLIT_SCENE))	split_the_scene_to_minimize_wait=TRUE;

	//******* Main render :: PART-0	-- first
	if (!split_the_scene_to_minimize_wait)
	{
		// level, DO NOT SPLIT
		Target->phase_scene_begin				();
		r_dsgraph_render_hud					();
		r_dsgraph_render_graph					(0);
		r_dsgraph_render_lods					(true,true);
		if(Details)	Details->Render				();
		Target->phase_scene_end					();
	} else {
		// level, SPLIT
		Target->phase_scene_begin				();
		r_dsgraph_render_graph					(0);
		Target->disable_aniso					();
	}

	//******* Occlusion testing of volume-limited light-sources
	Target->phase_occq							();
	LP_normal.clear								();
	LP_pending.clear							();
	{
		// perform tests
		u32	count			= 0;
		light_Package&	LP	= Lights.package;

		// stats
		stats.l_shadowed	= LP.v_shadowed.size();
		stats.l_unshadowed	= LP.v_point.size() + LP.v_spot.size();
		stats.l_total		= stats.l_shadowed + stats.l_unshadowed;

		// perform tests
		count				= _max	(count,LP.v_point.size());
		count				= _max	(count,LP.v_spot.size());
		count				= _max	(count,LP.v_shadowed.size());
		for (u32 it=0; it<count; it++)	{
			if (it<LP.v_point.size())		{
				light*	L			= LP.v_point	[it];
				L->vis_prepare		();
				if (L->vis.pending)	LP_pending.v_point.push_back	(L);
				else				LP_normal.v_point.push_back		(L);
			}
			if (it<LP.v_spot.size())		{
				light*	L			= LP.v_spot		[it];
				L->vis_prepare		();
				if (L->vis.pending)	LP_pending.v_spot.push_back		(L);
				else				LP_normal.v_spot.push_back		(L);
			}
			if (it<LP.v_shadowed.size())	{
				light*	L			= LP.v_shadowed	[it];
				L->vis_prepare		();
				if (L->vis.pending)	LP_pending.v_shadowed.push_back	(L);
				else				LP_normal.v_shadowed.push_back	(L);
			}
		}
	}
	LP_normal.sort							();
	LP_pending.sort							();

	//******* Main render :: PART-1 (second)
	if (split_the_scene_to_minimize_wait)	{
		// skybox can be drawn here
		if (0)
		{
			Target->u_setrt		( Target->rt_Generic_0,	Target->rt_Generic_1,0,HW.pBaseZB );
			RCache.set_CullMode	( CULL_NONE );
			RCache.set_Stencil	( FALSE		);

			// draw skybox
			RCache.set_ColorWriteEnable					();
			CHK_DX(HW.pDevice->SetRenderState			( D3DRS_ZENABLE,	FALSE				));
			g_pGamePersistent->Environment().RenderSky	();
			CHK_DX(HW.pDevice->SetRenderState			( D3DRS_ZENABLE,	TRUE				));
		}

		// level
		Target->phase_scene_begin				();
		r_dsgraph_render_hud					();
		r_dsgraph_render_lods					(true,true);
		if(Details)	Details->Render				();
		Target->phase_scene_end					();
	}

	// Render a separate mirror-camera pass after the main scene G-buffer is complete.
	render_water_reflection();

	// Wall marks
	if(Wallmarks)	{
		Target->phase_wallmarks					();
		g_r										= 0;
		Wallmarks->Render						();				// wallmarks has priority as normal geometry
	}

	// Update incremental shadowmap-visibility solver
	{
		u32 it=0;
		for (it=0; it<Lights_LastFrame.size(); it++)	{
			if (0==Lights_LastFrame[it])	continue	;
			try {
				Lights_LastFrame[it]->svis.flushoccq()	;
			} catch (...)
			{
				Msg	("! Failed to flush-OCCq on light [%d] %X",it,*(u32*)(&Lights_LastFrame[it]));
			}
		}
		Lights_LastFrame.clear	();
	}

	// Directional light - fucking sun
	if (bSUN)	{
		RImplementation.stats.l_visible		++;
		render_sun_near						();
		render_sun							();
		render_sun_filtered					();
		Target->accum_direct_blend			();
	}

	// Lighting, non dependant on OCCQ
	Target->phase_accumulator				();
	HOM.Disable								();
	render_lights							(LP_normal);
	
	// Lighting, dependant on OCCQ
	render_lights							(LP_pending);

	// Postprocess
	Target->phase_combine					();
	VERIFY	(0==mapDistort.size());
}

void CRender::render_forward				()
{
	VERIFY	(0==mapDistort.size());
	RImplementation.o.distortion				= RImplementation.o.distortion_enabled;	// enable distorion

	//******* Main render - second order geometry (the one, that doesn't support deffering)
	//.todo: should be done inside "combine" with estimation of of luminance, tone-mapping, etc.
	{
		// level
		r_pmask									(false,true);			// enable priority "1"
		phase									= PHASE_NORMAL;
		render_main								(Device.mFullTransform,false);//
		r_dsgraph_render_graph					(1)	;					// normal level, secondary priority
		PortalTraverser.fade_render				()	;					// faded-portals
		r_dsgraph_render_sorted					()	;
		if (L_Glows && ps_r2_glow_show) L_Glows->Render();					// strict-sorted geoms
		g_pGamePersistent->Environment().RenderLast()	;					// rain/thunder-bolts
	}

	RImplementation.o.distortion				= FALSE;				// disable distorion
}
