#include "stdafx.h"
#include "bolt.h"
#include "entity.h"
#include "actor.h"
#include "ParticlesObject.h"
#include "PhysicsShell.h"
#include "xr_level_controller.h"

CBolt::CBolt(void) 
{
	m_weight					= .1f;
	m_slot						= BOLT_SLOT;
	m_flags.set					(Fruck, FALSE);
	m_thrower_id				=u16(-1);
	m_bSprintAnimActive		= false;
}

CBolt::~CBolt(void) 
{
}

void CBolt::OnH_A_Chield() 
{
	inherited::OnH_A_Chield();
	CObject* o= H_Parent()->H_Parent();
	if(o)SetInitiator(o->ID());
	
}

void CBolt::OnEvent(NET_Packet& P, u16 type) 
{
	inherited::OnEvent(P,type);
}

bool CBolt::Activate() 
{
	Show();
	return true;
}

void CBolt::Deactivate() 
{
	Hide();
}

void CBolt::Throw() 
{
	CMissile					*l_pBolt = smart_cast<CMissile*>(m_fake_missile);
	if(!l_pBolt)				return;
	l_pBolt->set_destroy_time	(u32(m_dwDestroyTimeMax/phTimefactor));
	inherited::Throw			();
	spawn_fake_missile			();
}

bool CBolt::Useful() const
{
	return false;
}

bool CBolt::Action(s32 cmd, u32 flags) 
{
	if(inherited::Action(cmd, flags)) return true;
/*
	switch(cmd) 
	{
	case kDROP:
		{
			if(flags&CMD_START) 
			{
				m_throw = false;
				if(State() == MS_IDLE) State(MS_THREATEN);
			} 
			else if(State() == MS_READY || State() == MS_THREATEN) 
			{
				m_throw = true; 
				if(State() == MS_READY) State(MS_THROW);
			}
		} 
		return true;
	}
*/
	return false;
}

void CBolt::Destroy()
{
	inherited::Destroy();
}

void CBolt::activate_physic_shell	()
{
	inherited::activate_physic_shell	();
	m_pPhysicsShell->SetAirResistance	(.0001f);
}

void CBolt::SetInitiator			(u16 id)
{
	m_thrower_id=id;
}
u16	CBolt::Initiator				()
{
	return m_thrower_id;
}

void CBolt::UpdateCL()
{
	inherited::UpdateCL();

	// Only switch the idle animation; do not interrupt showing, hiding or throwing.
	if (GetState() != MS_IDLE)
	{
		m_bSprintAnimActive = false;
		return;
	}

	if (!m_pHUD || !hud_sect)
		return;

	CActor* actor = smart_cast<CActor*>(H_Parent());
	if (!actor)
		return;

	CEntity::SEntityState st;
	actor->g_State(st);
	const bool sprint_anim = st.bSprint && pSettings->line_exist(*hud_sect, "anim_idle_sprint");

	if (sprint_anim == m_bSprintAnimActive)
		return;

	m_bSprintAnimActive = sprint_anim;
	if (sprint_anim)
		m_pHUD->animDisplay(m_pHUD->animGet(pSettings->r_string(*hud_sect, "anim_idle_sprint")), TRUE);
	else
		inherited::StartIdleAnim();
}

void CBolt::StartIdleAnim()
{
	CActor* actor = smart_cast<CActor*>(H_Parent());
	if (actor && hud_sect.size() > 0 && pSettings->line_exist(*hud_sect, "anim_idle_sprint"))
	{
		CEntity::SEntityState st;
		actor->g_State(st);
		if (st.bSprint)
		{
			m_bSprintAnimActive = true;
			m_pHUD->animDisplay(m_pHUD->animGet(pSettings->r_string(*hud_sect, "anim_idle_sprint")), TRUE);
			return;
		}
	}

	m_bSprintAnimActive = false;
	inherited::StartIdleAnim();
}
