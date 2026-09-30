//////////////////////////////////////////////////////////////////////
// HudItem.cpp: класс родитель для всех предметов имеющих
//				собственный HUD (CWeapon, CMissile etc)
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "HudItem.h"
#include "physic_item.h"
#include "WeaponHUD.h"
#include "actor.h"
#include "actoreffector.h"
#include "Missile.h"
#include "xrmessages.h"
#include "level.h"
#include "inventory.h"
#include "../CameraBase.h"


CHudItem::CHudItem(void)
{
	m_pHUD				= NULL;
	SetHUDmode			(FALSE);
	m_dwStateTime		= 0;
	m_bRenderHud		= true;

	m_bInertionEnable	= true;
	m_bInertionAllow	= true;
	m_fHudBobbingTime		= 0.f;
	m_fHudBobbingReminder	= 0.f;
	m_fHudJumpEffectTime = 0.f;
	m_fHudLandingEffectTime = 0.f;
	m_fHudLandingDelayTime = 0.f;
	m_bHudWasAirborne = false;
}

CHudItem::~CHudItem(void)
{
	xr_delete			(m_pHUD);
}

DLL_Pure *CHudItem::_construct	()
{
	m_object			= smart_cast<CPhysicItem*>(this);
	VERIFY				(m_object);

	m_item				= smart_cast<CInventoryItem*>(this);
	VERIFY				(m_item);

	return				(m_object);
}

void CHudItem::Load(LPCSTR section)
{
	//загрузить hud, если он нужен
	if(pSettings->line_exist(section,"hud"))
		hud_sect		= pSettings->r_string		(section,"hud");

	if(*hud_sect){
		m_pHUD			= xr_new<CWeaponHUD> (this);
		m_pHUD->Load	(*hud_sect);
		if(pSettings->line_exist(*hud_sect, "allow_inertion")) 
			m_bInertionAllow = !!pSettings->r_bool(*hud_sect, "allow_inertion");
	}else{
		m_pHUD = NULL;
		//если hud не задан, но задан слот, то ошибка
		R_ASSERT2(item().GetSlot() == NO_ACTIVE_SLOT, "active slot is set, but hud for food item is not available");
	}

	m_animation_slot	= pSettings->r_u32(section,"animation_slot");
}

void CHudItem::net_Destroy()
{
	if(m_pHUD)
		m_pHUD->net_DestroyHud	();

	SetHUDmode			(FALSE);
	m_dwStateTime		= 0;
}

void CHudItem::PlaySound	(HUD_SOUND& hud_snd, const Fvector& position)
{
	HUD_SOUND::PlaySound	(hud_snd, position, object().H_Root(), !!GetHUDmode());
}

BOOL  CHudItem::net_Spawn	(CSE_Abstract* DC) 
{
	return TRUE;
}

void CHudItem::renderable_Render()
{
	UpdateXForm	();
	BOOL _hud_render			= ::Render->get_HUD() && GetHUDmode();
	if(_hud_render && !m_pHUD->IsHidden() && !item().IsHidden()){ 
		// HUD render
		if(m_bRenderHud){
			::Render->set_Transform		(&m_pHUD->Transform());
			::Render->add_Visual		(m_pHUD->Visual());
		}
	}
	else {
		if (!object().H_Parent() || (!_hud_render && m_pHUD && !m_pHUD->IsHidden() && !item().IsHidden()))
			on_renderable_Render		();
		else
			if (object().H_Parent()) {
				CInventoryOwner	*owner = smart_cast<CInventoryOwner*>(object().H_Parent());
				VERIFY			(owner);
				CInventoryItem	*self = smart_cast<CInventoryItem*>(this);
				if (owner->attached(self))
					on_renderable_Render();
			}
	}
}

bool CHudItem::Action(s32 cmd, u32 flags) 
{
	return false;
}

void CHudItem::SwitchState(u32 S)
{
	if (OnClient()) return;
	SetNextState( S );	// Very-very important line of code!!! :)

	if (object().Local() && !object().getDestroy())	
	{
		// !!! Just single entry for given state !!!
		NET_Packet		P;
		object().u_EventGen		(P,GE_WPN_STATE_CHANGE,object().ID());
		P.w_u8			(u8(S));
		object().u_EventSend		(P);
	}
}

void CHudItem::OnEvent		(NET_Packet& P, u16 type)
{
	switch (type)
	{
	case GE_WPN_STATE_CHANGE:
		{
			u8				S;
			P.r_u8			(S);
			OnStateSwitch	(u32(S));
		}
		break;
	}
}

void CHudItem::OnStateSwitch	(u32 S)
{
	m_dwStateTime = 0;
	SetState( S );
	if(object().Remote()) SetNextState( S );
}


bool CHudItem::Activate() 
{
	if(m_pHUD) 
		m_pHUD->Init();

	Show();
	OnActiveItem ();
	return true;
}

void CHudItem::Deactivate() 
{
	Hide();
	OnHiddenItem ();
}



void CHudItem::UpdateHudPosition	()
{
	if (m_pHUD && GetHUDmode()){
		if(item().IsHidden()) 
			SetHUDmode(FALSE);

		Fmatrix							trans;

		CActor* pActor = smart_cast<CActor*>(object().H_Parent());
		if(pActor){
			pActor->Cameras().camera_Matrix				(trans);
			UpdateHudInertion							(trans);

			const u32 mstate = pActor->GetMovementState();

			// HUD weapon/hand bobbing.
			// This is intentionally done at the common CHudItem level so the
			// same effect is applied to weapons and CMissile-derived items
			// (including the bolt).
			{
				static const float HUD_BOB_REMINDER_SPEED = 5.f;
				//static const float HUD_BOB_VERTICAL_SCALE = 1.8f;
				static const float HUD_BOB_ROTATION_SCALE = 1.0f;
				static const float HUD_BOB_CROUCH_FACTOR = 0.75f;

				static const float HUD_BOB_SPEED = READ_IF_EXISTS(
					pSettings, r_float, "bobbing_effector", "hud_bobbing_speed", 0.75f);

				static const float HUD_BOB_AMPLITUDE = READ_IF_EXISTS(
					pSettings, r_float, "bobbing_effector", "hud_bobbing_amplitude", 1.0f);

				static const float HUD_BOB_VERTICAL_SCALE = READ_IF_EXISTS(
					pSettings, r_float, "bobbing_effector", "hud_bobbing_vertical", 1.8f);

				if ((mstate & mcAnyMove) &&
					!(mstate & (mcJump | mcFall | mcLanding | mcLanding2)))
				{
					if (m_fHudBobbingReminder < 1.f)
						m_fHudBobbingReminder += HUD_BOB_REMINDER_SPEED * Device.fTimeDelta;
					else
						m_fHudBobbingReminder = 1.f;
				}
				else
				{
					if (m_fHudBobbingReminder > 0.f)
						m_fHudBobbingReminder -= HUD_BOB_REMINDER_SPEED * Device.fTimeDelta;
					else
						m_fHudBobbingReminder = 0.f;
				}

				clamp(m_fHudBobbingReminder, 0.f, 1.f);

				if (!fsimilar(m_fHudBobbingReminder, 0.f))
				{
					m_fHudBobbingTime += Device.fTimeDelta;

					const float crouch_factor =
						(mstate & mcCrouch) ? HUD_BOB_CROUCH_FACTOR : 1.f;

					float A;
					float ST;

					if (isActorAccelerated(mstate, pActor->IsZoomAimingMode()))
					{
						A = pSettings->r_float("bobbing_effector", "run_amplitude") * crouch_factor * HUD_BOB_AMPLITUDE;
						ST = pSettings->r_float("bobbing_effector", "run_speed") * HUD_BOB_SPEED * m_fHudBobbingTime * crouch_factor;
					}
					else
					{
						A = pSettings->r_float("bobbing_effector", "walk_amplitude") * crouch_factor * HUD_BOB_AMPLITUDE;
						ST = pSettings->r_float("bobbing_effector", "walk_speed") * HUD_BOB_SPEED * m_fHudBobbingTime * crouch_factor;
					}

					const float sin_a = _abs(_sin(ST) * A) * m_fHudBobbingReminder;
					const float cos_a = _cos(ST) * A * m_fHudBobbingReminder;

					// trans.j is HUD-up. Invert this term so the weapon/bolt
					// moves DOWN on the footstep instead of floating upward.
					//trans.c.mad(trans.j, -sin_a * HUD_BOB_POSITION_SCALE);
					trans.c.mad(trans.j, -sin_a * HUD_BOB_VERTICAL_SCALE);

					Fmatrix bob_rotation;
					bob_rotation.identity();
					bob_rotation.setHPB(cos_a * HUD_BOB_ROTATION_SCALE,
						                sin_a * HUD_BOB_ROTATION_SCALE,
						                cos_a * HUD_BOB_ROTATION_SCALE);
					trans.mulB_43(bob_rotation);
				}
			}

			// HUD jump / landing effect
			{
				const float HUD_JUMP_DURATION = READ_IF_EXISTS(
					pSettings, r_float, "bobbing_effector", "hud_jump_duration", 0.75f);

				const float HUD_JUMP_DOWN = READ_IF_EXISTS(
					pSettings, r_float, "bobbing_effector", "hud_jump_down", 0.018f);

				const float HUD_JUMP_UP = READ_IF_EXISTS(
					pSettings, r_float, "bobbing_effector", "hud_jump_up", 0.012f);

				const float HUD_JUMP_PITCH_DOWN = READ_IF_EXISTS(
					pSettings, r_float, "bobbing_effector", "hud_jump_pitch_down", 0.035f);

				const float HUD_JUMP_PITCH_UP = READ_IF_EXISTS(
					pSettings, r_float, "bobbing_effector", "hud_jump_pitch_up", -0.025f);

				const float HUD_LANDING_DURATION = READ_IF_EXISTS(
					pSettings, r_float, "bobbing_effector", "hud_landing_duration", 0.22f);

				const float HUD_LANDING_DOWN = READ_IF_EXISTS(
					pSettings, r_float, "bobbing_effector", "hud_landing_down", 0.020f);

				const float HUD_LANDING_PITCH_DOWN = READ_IF_EXISTS(
					pSettings, r_float, "bobbing_effector", "hud_landing_pitch_down", 0.035f);


				const bool airborne = (mstate & (mcJump | mcFall)) != 0;
				const bool landing = (mstate & (mcLanding | mcLanding2)) != 0;


				// ------------------------------------------------------------
				// JUMP
				// Запускается один раз при переходе в воздух.
				// Приземление здесь НЕ проигрывается.
				// ------------------------------------------------------------

				if (airborne && !m_bHudWasAirborne)
				{
					m_fHudJumpEffectTime = 0.f;
				}

				m_bHudWasAirborne = airborne;


				if (airborne && m_fHudJumpEffectTime < HUD_JUMP_DURATION)
				{
					if (airborne)
					{
						m_fHudJumpEffectTime += Device.fTimeDelta;

						float t = m_fHudJumpEffectTime / HUD_JUMP_DURATION;
						clamp(t, 0.f, 1.f);

						float offset = 0.f;
						float pitch = 0.f;

						// Плавность переходов.
						auto smooth = [](float x)
							{
								return x * x * (3.f - 2.f * x);
							};

						// 1. Оружие плавно опускается.
						if (t < 0.35f)
						{
							float k = smooth(t / 0.35f);

							offset = -HUD_JUMP_DOWN * k;
							pitch = HUD_JUMP_PITCH_DOWN * k;
						}

						// 2. Затем плавно поднимается выше исходной позиции.
						else if (t < 1.f)
						{
							float k = smooth((t - 0.35f) / 0.65f);

							offset = -HUD_JUMP_DOWN +
								(HUD_JUMP_DOWN + HUD_JUMP_UP) * k;

							pitch = HUD_JUMP_PITCH_DOWN +
								(HUD_JUMP_PITCH_UP - HUD_JUMP_PITCH_DOWN) * k;
						}

						// После окончания прыжковой фазы
						// удерживаем оружие в верхнем положении
						// до фактического приземления.
						else
						{
							offset = HUD_JUMP_UP;
							pitch = HUD_JUMP_PITCH_UP;
						}

						trans.c.mad(trans.j, offset);

						Fmatrix jump_rotation;
						jump_rotation.identity();
						jump_rotation.rotateX(pitch);

						trans.mulB_43(jump_rotation);
					}
				}


				// ------------------------------------------------------------
				// LANDING
				// Полностью отдельное событие.
				// Стартует только при фактическом переходе в landing.
				// ------------------------------------------------------------

				if (pActor->m_bHudLandingEvent)
				{
					m_fHudLandingEffectTime = 0.0001f;
				}

				if (m_fHudLandingDelayTime > 0.f)
				{
					m_fHudLandingDelayTime -= Device.fTimeDelta;

					if (m_fHudLandingDelayTime <= 0.f)
						m_fHudLandingEffectTime = 0.0001f;
				}

				if (m_fHudLandingEffectTime > 0.f &&
					m_fHudLandingEffectTime < HUD_LANDING_DURATION)
				{
					m_fHudLandingEffectTime += Device.fTimeDelta;

					float t = m_fHudLandingEffectTime / HUD_LANDING_DURATION;
					clamp(t, 0.f, 1.f);

					float offset = 0.f;
					float pitch = 0.f;

					auto smooth = [](float x)
						{
							return x * x * (3.f - 2.f * x);
						};

					// Сначала плавно переходим
					// из верхней точки прыжка в нижнюю точку приземления.
					if (t < 0.35f)
					{
						float k = smooth(t / 0.35f);

						offset = HUD_JUMP_UP +
							(-HUD_LANDING_DOWN - HUD_JUMP_UP) * k;

						pitch = HUD_JUMP_PITCH_UP +
							(HUD_LANDING_PITCH_DOWN - HUD_JUMP_PITCH_UP) * k;
					}

					// Затем плавно возвращаемся в исходное положение.
					else
					{
						float k = smooth((t - 0.35f) / 0.65f);

						offset = -HUD_LANDING_DOWN * (1.f - k);
						pitch = HUD_LANDING_PITCH_DOWN * (1.f - k);
					}

					trans.c.mad(trans.j, offset);

					Fmatrix landing_rotation;
					landing_rotation.identity();
					landing_rotation.rotateX(pitch);

					trans.mulB_43(landing_rotation);
				}
			}
			
			UpdateHudAdditonal							(trans);
			m_pHUD->UpdatePosition						(trans);
		}
	}
}

void CHudItem::UpdateHudAdditonal		(Fmatrix& hud_trans)
{
}

void CHudItem::StartHudInertion()
{
	m_bInertionEnable = true;
}
void CHudItem::StopHudInertion()
{
	m_bInertionEnable = false;
}

static const float PITCH_OFFSET_R	= 0.017f;
static const float PITCH_OFFSET_N	= 0.012f;
static const float PITCH_OFFSET_D	= 0.02f;
static const float ORIGIN_OFFSET	= -0.05f;
static const float TENDTO_SPEED		= 5.f;

void CHudItem::UpdateHudInertion		(Fmatrix& hud_trans)
{
	if (m_pHUD && m_bInertionAllow && m_bInertionEnable){
		Fmatrix								xform;//,xform_orig; 
		Fvector& origin						= hud_trans.c; 
		xform								= hud_trans;

		static Fvector						m_last_dir={0,0,0};

		// calc difference
		Fvector								diff_dir;
		diff_dir.sub						(xform.k, m_last_dir);

		// clamp by PI_DIV_2
		Fvector last;						last.normalize_safe(m_last_dir);
		float dot							= last.dotproduct(xform.k);
		if (dot<EPS){
			Fvector v0;
			v0.crossproduct			(m_last_dir,xform.k);
			m_last_dir.crossproduct	(xform.k,v0);
			diff_dir.sub			(xform.k, m_last_dir);
		}

		// tend to forward
		m_last_dir.mad	(diff_dir,TENDTO_SPEED*Device.fTimeDelta);
		origin.mad		(diff_dir,ORIGIN_OFFSET);

		// pitch compensation
		float pitch		= angle_normalize_signed(xform.k.getP());
		origin.mad		(xform.k,	-pitch * PITCH_OFFSET_D);
		origin.mad		(xform.i,	-pitch * PITCH_OFFSET_R);
		origin.mad		(xform.j,	-pitch * PITCH_OFFSET_N);

		// calc moving inertion
	}
}

void CHudItem::UpdateCL()
{
	m_dwStateTime += Device.dwTimeDelta;

	if(m_pHUD) m_pHUD->Update();
	UpdateHudPosition	();
}

void CHudItem::OnH_A_Chield		()
{
	SetHUDmode		(FALSE);

	if (m_pHUD) {
		if(Level().CurrentEntity() == object().H_Parent() && smart_cast<CActor*>(object().H_Parent()))
			m_pHUD->Visible(true);
		else
			m_pHUD->Visible(false);
	}
}

void CHudItem::OnH_B_Chield		()
{
	OnHiddenItem ();
}

void CHudItem::OnH_B_Independent	(bool just_before_destroy)
{
	SetHUDmode				(FALSE);

	if (m_pHUD)
		m_pHUD->Visible		(false);
	
	StopHUDSounds			();

	UpdateXForm				();
}

void CHudItem::OnH_A_Independent	()
{
}
void CHudItem::animGet	(MotionSVec& lst, LPCSTR prefix)
{
	const MotionID		&M = m_pHUD->animGet(prefix);
	if (M)				lst.push_back(M);
	for (int i=0; i<MAX_ANIM_COUNT; ++i)
	{
		string128		sh_anim;
		sprintf_s			(sh_anim,"%s%d",prefix,i);
		const MotionID	&M = m_pHUD->animGet(sh_anim);
		if (M)			lst.push_back(M);
	}
	R_ASSERT2			(!lst.empty(),prefix);
}
