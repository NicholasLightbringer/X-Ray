#pragma once

#include "bloodsucker_vampire_execute.h"
#include "../states/state_hide_from_point.h"
#include "bloodsucker_vampire_approach.h"
#include "bloodsucker_vampire_hide.h"
#include "../../../clsid_game.h"
#include "../../../actor.h"
#include "../../../inventory_space.h"

#define TEMPLATE_SPECIALIZATION template <\
	typename _Object\
>

#define CStateBloodsuckerVampireAbstract CStateBloodsuckerVampire<_Object>

#define RUN_AWAY_DISTANCE			50.f

TEMPLATE_SPECIALIZATION
CStateBloodsuckerVampireAbstract::CStateBloodsuckerVampire(_Object *obj) : inherited(obj), m_time_last_vampire(0), m_weapon_hidden(false), enemy(0)
{
	add_state	(eStateVampire_ApproachEnemy,	xr_new<CStateBloodsuckerVampireApproach<_Object> >	(obj));
	add_state	(eStateVampire_Execute,			xr_new<CStateBloodsuckerVampireExecute<_Object> >	(obj));
	add_state	(eStateVampire_RunAway,			xr_new<CStateMonsterHideFromPoint<_Object> >		(obj));
	add_state	(eStateVampire_Hide,			xr_new<CStateBloodsuckerVampireHide<_Object> >		(obj));
}

TEMPLATE_SPECIALIZATION
void CStateBloodsuckerVampireAbstract::reinit()
{
	inherited::reinit	();

	if (m_weapon_hidden) {
		Actor()->SetWeaponHideState(INV_STATE_BLOCK_ALL, false);
		m_weapon_hidden = false;
	}
	m_time_last_vampire	= 0;
}

TEMPLATE_SPECIALIZATION
void CStateBloodsuckerVampireAbstract::initialize()
{
	inherited::initialize						();
	object->start_invisible_predator			();

	enemy	= object->EnemyMan.get_enemy		();
	m_weapon_hidden = false;
	if (Actor()) {
		Actor()->SetWeaponHideState(INV_STATE_BLOCK_ALL, true);
		m_weapon_hidden = true;
	}

	object->sound().play						(CAI_Bloodsucker::eVampireStartHunt);
}

TEMPLATE_SPECIALIZATION
void CStateBloodsuckerVampireAbstract::reselect_state()
{
	u32 state_id = u32(-1);

	if (prev_substate == eStateVampire_ApproachEnemy) {
		if (get_state(eStateVampire_Execute)->check_start_conditions())
			state_id = eStateVampire_Execute;
		else
			state_id = eStateVampire_ApproachEnemy;
	} else {
		// A finished Execute state ends the outer vampire state directly.
		// Do not route the grab animation into the old RunAway/Hide stage.
		state_id = eStateVampire_ApproachEnemy;
	}

	if (state_id == u32(-1))
		state_id = eStateVampire_ApproachEnemy;

	select_state(state_id);
}

TEMPLATE_SPECIALIZATION
void CStateBloodsuckerVampireAbstract::check_force_state()
{
	// check if we can start execute
	if (prev_substate == eStateVampire_ApproachEnemy) {
		if (get_state(eStateVampire_Execute)->check_start_conditions())
			current_substate = u32(-1);
	}
}


TEMPLATE_SPECIALIZATION
void CStateBloodsuckerVampireAbstract::finalize()
{
	inherited::finalize();

	object->stop_invisible_predator	();
	object->SatisfyVampire			();
	m_time_last_vampire				= Device.dwTimeGlobal;
	if (m_weapon_hidden) {
		Actor()->SetWeaponHideState(INV_STATE_BLOCK_ALL, false);
		m_weapon_hidden = false;
	}
}

TEMPLATE_SPECIALIZATION
void CStateBloodsuckerVampireAbstract::critical_finalize()
{
	inherited::critical_finalize	();
	
	object->stop_invisible_predator	();
	object->SatisfyVampire			();
	m_time_last_vampire				= Device.dwTimeGlobal;
	if (m_weapon_hidden) {
		Actor()->SetWeaponHideState(INV_STATE_BLOCK_ALL, false);
		m_weapon_hidden = false;
	}
}

TEMPLATE_SPECIALIZATION
bool CStateBloodsuckerVampireAbstract::check_start_conditions()
{
	if (!object->WantVampire()) return false;
	if (object->berserk_always) return false;

	const CEntityAlive *enemy = object->EnemyMan.get_enemy();
	if (!enemy || (enemy->CLS_ID != CLSID_OBJECT_ACTOR)) return false;
	if (!object->EnemyMan.see_enemy_now()) return false;
	if (object->CControlledActor::is_controlling()) return false;

	const CActor *actor = smart_cast<const CActor *>(enemy);
	if (!actor || actor->input_external_handler_installed()) return false;

	const float dist = object->MeleeChecker.distance_to_enemy(enemy);
	if (dist > object->m_vampire_trigger_distance) return false;
	if (m_time_last_vampire + object->m_vampire_min_delay > Device.dwTimeGlobal) return false;

	const float roll = Random.randF(0.f, 1.f);
	if (roll >= object->m_vampire_chance) {
		object->SatisfyVampire();
		m_time_last_vampire = Device.dwTimeGlobal;
		return false;
	}

	return true;
}

TEMPLATE_SPECIALIZATION
bool CStateBloodsuckerVampireAbstract::check_completion()
{
	// CState::execute() finalizes a completed child and sets current_substate
	// to -1 while keeping its former id in prev_substate. End the grab state
	// here so the manager can return to normal combat without entering Hide.
	if ((prev_substate == eStateVampire_Execute) && (current_substate == u32(-1)))
		return true;

	if (current_substate == eStateVampire_ApproachEnemy) {
		const CEntityAlive *current_enemy = object->EnemyMan.get_enemy();
		if (!current_enemy || object->Position().distance_to(current_enemy->Position()) > object->m_vampire_trigger_distance + 1.f)
			return true;
	}

	if ((current_substate == eStateVampire_Hide) && get_state_current()->check_completion())
		return true;

	if (enemy != object->EnemyMan.get_enemy())
		return true;

	if ((current_substate != eStateVampire_Execute) && object->CControlledActor::is_controlling())
		return true;

	return false;
}

TEMPLATE_SPECIALIZATION
void CStateBloodsuckerVampireAbstract::setup_substates()
{
	state_ptr state = get_state_current();

	if (current_substate == eStateVampire_RunAway) {

		SStateHideFromPoint		data;
		data.point				= object->EnemyMan.get_enemy_position();
		data.accelerated		= true;
		data.braking			= false;
		data.accel_type			= eAT_Aggressive;
		data.distance			= RUN_AWAY_DISTANCE;
		data.action.action		= ACT_RUN;
		data.action.sound_type	= MonsterSound::eMonsterSoundAggressive;
		data.action.sound_delay = object->db().m_dwAttackSndDelay;
		data.action.time_out	= 15000;

		state->fill_data_with(&data, sizeof(SStateHideFromPoint));

		return;
	}
}

#undef TEMPLATE_SPECIALIZATION
#undef CStateBloodsuckerVampireAbstract

