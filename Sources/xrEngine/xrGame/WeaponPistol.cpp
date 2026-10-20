#include "stdafx.h"
#include "weaponpistol.h"
#include "WeaponHUD.h"
#include "ParticlesObject.h"
#include "actor.h"

CWeaponPistol::CWeaponPistol(LPCSTR name) : CWeaponCustomPistol(name)
{
	m_eSoundClose		= ESoundTypes(SOUND_TYPE_WEAPON_RECHARGING /*| eSoundType*/);
	m_opened = false;
	m_bPending = false;
}

CWeaponPistol::~CWeaponPistol(void)
{
}

void CWeaponPistol::net_Destroy()
{
	inherited::net_Destroy();

	// sounds
	HUD_SOUND::DestroySound(sndClose);
}


void CWeaponPistol::Load	(LPCSTR section)
{
	inherited::Load		(section);

	HUD_SOUND::LoadSound(section, "snd_close", sndClose, m_eSoundClose);

	animGet				(mhud_pistol.mhud_empty,		pSettings->r_string(*hud_sect, "anim_empty"));
	animGet				(mhud_pistol.mhud_shot_l,		pSettings->r_string(*hud_sect, "anim_shot_last"));
	animGet				(mhud_pistol.mhud_close,		pSettings->r_string(*hud_sect, "anim_close"));
	animGet				(mhud_pistol.mhud_show_empty,	pSettings->r_string(*hud_sect, "anim_draw_empty"));
	animGet				(mhud_pistol.mhud_reload_empty,	pSettings->r_string(*hud_sect, "anim_reload_empty"));

	// Empty-magazine sprint idle is optional; keep the regular empty idle as fallback.
	if (pSettings->line_exist(*hud_sect, "anim_idle_sprint_empty"))
		animGet(mhud_pistol.mhud_idle_sprint_empty, pSettings->r_string(*hud_sect, "anim_idle_sprint_empty"));

	string128			str;

	// Vehicle (_r) animations are optional. If a specific _r motion set is
	// absent, use the corresponding standard set instead of asserting/crashing.
	strconcat(sizeof(str), str, pSettings->r_string(*hud_sect, "anim_empty"), "_r");
	animGetOptional(mhud_pistol_r.mhud_empty, str, mhud_pistol.mhud_empty);

	strconcat(sizeof(str), str, pSettings->r_string(*hud_sect, "anim_shot_last"), "_r");
	animGetOptional(mhud_pistol_r.mhud_shot_l, str, mhud_pistol.mhud_shot_l);

	strconcat(sizeof(str), str, pSettings->r_string(*hud_sect, "anim_close"), "_r");
	animGetOptional(mhud_pistol_r.mhud_close, str, mhud_pistol.mhud_close);

	strconcat(sizeof(str), str, pSettings->r_string(*hud_sect, "anim_draw_empty"), "_r");
	animGetOptional(mhud_pistol_r.mhud_show_empty, str, mhud_pistol.mhud_show_empty);

	strconcat(sizeof(str), str, pSettings->r_string(*hud_sect, "anim_reload_empty"), "_r");
	animGetOptional(mhud_pistol_r.mhud_reload_empty, str, mhud_pistol.mhud_reload_empty);

	if (pSettings->line_exist(*hud_sect, "anim_idle_sprint_empty"))
	{
		strconcat(sizeof(str), str, pSettings->r_string(*hud_sect, "anim_idle_sprint_empty"), "_r");
		animGetOptional(mhud_pistol_r.mhud_idle_sprint_empty, str, mhud_pistol.mhud_idle_sprint_empty);
	}

	strconcat(sizeof(str), str, pSettings->r_string(*hud_sect, "anim_idle"), "_r");
	animGetOptional(wm_mhud_r.mhud_idle, str, mhud.mhud_idle);

	strconcat(sizeof(str), str, pSettings->r_string(*hud_sect, "anim_reload"), "_r");
	animGetOptional(wm_mhud_r.mhud_reload, str, mhud.mhud_reload);

	strconcat(sizeof(str), str, pSettings->r_string(*hud_sect, "anim_draw"), "_r");
	animGetOptional(wm_mhud_r.mhud_show, str, mhud.mhud_show);

	strconcat(sizeof(str), str, pSettings->r_string(*hud_sect, "anim_holster"), "_r");
	animGetOptional(wm_mhud_r.mhud_hide, str, mhud.mhud_hide);

	strconcat(sizeof(str), str, pSettings->r_string(*hud_sect, "anim_shoot"), "_r");
	animGetOptional(wm_mhud_r.mhud_shots, str, mhud.mhud_shots);

	if (IsZoomEnabled())
	{
		strconcat(sizeof(str), str, pSettings->r_string(*hud_sect, "anim_idle_aim"), "_r");
		animGetOptional(wm_mhud_r.mhud_idle_aim, str, mhud.mhud_idle_aim);
	}

}

void CWeaponPistol::animGetOptional(MotionSVec& lst, LPCSTR prefix, const MotionSVec& fallback)
{
	lst.clear();

	const MotionID& M = m_pHUD->animGet(prefix);
	if (M)
		lst.push_back(M);

	for (int i = 0; i < MAX_ANIM_COUNT; ++i)
	{
		string128 sh_anim;
		sprintf_s(sh_anim, "%s%d", prefix, i);
		const MotionID& motion = m_pHUD->animGet(sh_anim);
		if (motion)
			lst.push_back(motion);
	}

	// A missing _r set falls back to the standard animations for this motion.
	if (lst.empty())
	{
		for (u32 i = 0; i < fallback.size(); ++i)
			lst.push_back(fallback[i]);
	}
}

void CWeaponPistol::OnH_B_Chield		()
{
	inherited::OnH_B_Chield		();
	m_opened = false;
}

void CWeaponPistol::PlayAnimShow	()
{
	VERIFY(GetState()==eShowing);
	if(iAmmoElapsed >= 1)
		m_opened = false;
	else
		m_opened = true;
		
	if(m_opened){ 
		CWeaponPistol::WWPMotions& m = wwpm_current();
		m_pHUD->animPlay(random_anim(m.mhud_show_empty),FALSE, this, GetState());
	}else{ 
		CWeaponMagazined::SWMmotions& m = swm_current();
		m_pHUD->animPlay(random_anim(m.mhud_show),FALSE, this, GetState());
	}
}

void CWeaponPistol::PlayAnimIdle	()
{
	VERIFY(GetState()==eIdle);
	if(m_opened){ 
		CWeaponPistol::WWPMotions& m = wwpm_current();
		CActor* A = smart_cast<CActor*>(H_Parent());
		if (!IsZoomed() && A && m.mhud_idle_sprint_empty.size())
		{
			CEntity::SEntityState st;
			A->g_State(st);
			if (st.bSprint)
			{
				m_pHUD->animPlay(random_anim(m.mhud_idle_sprint_empty), TRUE, NULL, GetState());
				return;
			}
		}
		m_pHUD->animPlay(random_anim(m.mhud_empty), TRUE, NULL, GetState());
	}else{
		CActor* A = smart_cast<CActor*>(H_Parent());
		if(A && A->Holder()){
			MotionSVec* m = (IsZoomed())?&wm_mhud_r.mhud_idle_aim:&wm_mhud_r.mhud_idle;
			m_pHUD->animPlay(random_anim(*m), TRUE, NULL, GetState());
		}else
			inherited::PlayAnimIdle		();
	}
}

MotionID CWeaponPistol::PlayAnimReload	()
{
	VERIFY(GetState()==eReload);
	MotionID motion;
	if(m_opened){
		CWeaponPistol::WWPMotions& m = wwpm_current();
		motion = random_anim(m.mhud_reload_empty);
	}else{
		CWeaponMagazined::SWMmotions& m = swm_current();
		motion = random_anim(m.mhud_reload);
	}
	m_pHUD->animPlay(motion, TRUE, this, GetState());

	// Keep the pistol state synchronized with the normal post-reload idle/holster set.
	m_opened = false;
	return motion;
}


void CWeaponPistol::PlayAnimHide()
{
	VERIFY(GetState()==eHiding);
	if(m_opened) 
	{
		PlaySound			(sndClose,get_LastFP());
		CWeaponPistol::WWPMotions& m = wwpm_current();
		m_pHUD->animPlay	(random_anim(m.mhud_close), TRUE, this, GetState());
	} 
	else 
		inherited::PlayAnimHide();
}

void CWeaponPistol::PlayAnimShoot	()
{
	VERIFY(GetState()==eFire || GetState()==eFire2);
	if(iAmmoElapsed > 1) 
	{
		CWeaponMagazined::SWMmotions& m = swm_current();
		m_pHUD->animPlay	(random_anim(m.mhud_shots), FALSE, this, GetState());
		m_opened = false;
	}
	else 
	{
		CWeaponPistol::WWPMotions& m = wwpm_current();
		m_pHUD->animPlay	(random_anim(m.mhud_shot_l), FALSE, this, GetState()); 
		m_opened = true; 
	}
}


void CWeaponPistol::switch2_Reload()
{
//.	if(GetState()==eReload) return;
	inherited::switch2_Reload();
}

void CWeaponPistol::OnAnimationEnd(u32 state)
{
	if(state == eHiding && m_opened) 
	{
		m_opened = false;
//		switch2_Hiding();
	} 
	inherited::OnAnimationEnd(state);
}

void CWeaponPistol::OnShot		()
{
	// Sound
	PlaySound		(*m_pSndShotCurrent,get_LastFP());

	AddShotEffector	();
	
	PlayAnimShoot	();

	// Shell Drop
	Fvector vel; 
	PHGetLinearVell(vel);
	OnShellDrop					(get_LastSP(),  vel);

	// ����� �� ������
	
	StartFlameParticles	();
	R_ASSERT2(!m_pFlameParticles || !m_pFlameParticles->IsLooped(),
			  "can't set looped particles system for shoting with pistol");
	
	//��� �� ������
	StartSmokeParticles	(get_LastFP(), vel);
}

void CWeaponPistol::UpdateCL()
{
	inherited::UpdateCL();

	// Inventory operations can change the magazine without playing weapon
	// animations. Synchronize the pistol animation set while it is idle.
	// Do not touch m_opened during reload/show/hide/fire state transitions.
	if (GetState() == eIdle)
		m_opened = (iAmmoElapsed == 0);
}

void CWeaponPistol::UpdateSounds()
{
	inherited::UpdateSounds();

	if (sndClose.playing()) sndClose.set_position	(get_LastFP());
}

CWeaponPistol::WWPMotions&	 CWeaponPistol::wwpm_current	()
{
	CActor* A = smart_cast<CActor*>(H_Parent());
	if(A && A->Holder()){	
//		Msg("right-hand animation playing");
		return				mhud_pistol_r;
	}
//	Msg("double-hands animation playing");
	return					mhud_pistol;
}

CWeaponMagazined::SWMmotions&	 CWeaponPistol::swm_current	()
{
	CActor* A = smart_cast<CActor*>(H_Parent());
	if(A && A->Holder()){
//.		Msg("right-hand animation playing");
		return				wm_mhud_r;
	}
//.	Msg("double-hands animation playing");
	return					mhud;
}
