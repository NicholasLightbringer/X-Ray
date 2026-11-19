#pragma once

#define TEMPLATE_SPECIALIZATION template <\
	typename _Object\
>

#define CStateBloodsuckerVampireApproachAbstract CStateBloodsuckerVampireApproach<_Object>

TEMPLATE_SPECIALIZATION
CStateBloodsuckerVampireApproachAbstract::CStateBloodsuckerVampireApproach(_Object *obj) : inherited(obj)
{
}

TEMPLATE_SPECIALIZATION
CStateBloodsuckerVampireApproachAbstract::~CStateBloodsuckerVampireApproach()
{
}

TEMPLATE_SPECIALIZATION
void CStateBloodsuckerVampireApproachAbstract::initialize()
{
	inherited::initialize();
	object->path().prepare_builder	();	
}

TEMPLATE_SPECIALIZATION
void CStateBloodsuckerVampireApproachAbstract::execute()
{
	// установка параметров функциональных блоков
	object->set_action								(ACT_RUN);
	object->anim().accel_activate					(eAT_Aggressive);
	object->anim().accel_set_braking				(false);
	object->path().set_target_point					(object->EnemyMan.get_enemy()->Position(), object->EnemyMan.get_enemy()->ai_location().level_vertex_id());
	object->path().set_rebuild_time					(object->get_attack_rebuild_time());
	object->path().set_use_covers					(false);
	// Stop in the grab window instead of running into the actor. The old 0.1f
	// target distance conflicts with the execute state's 0.5..Vampire_Distance range.
	float vampire_stop_distance = object->m_vampire_attack_distance * 0.75f;
	if (vampire_stop_distance < 0.6f)
		vampire_stop_distance = 0.6f;
	object->path().set_distance_to_end				(vampire_stop_distance);
	object->set_state_sound							(MonsterSound::eMonsterSoundAggressive);
}

