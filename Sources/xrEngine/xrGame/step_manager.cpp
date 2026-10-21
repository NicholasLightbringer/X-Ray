#include "stdafx.h"
#include "../skeletonanimated.h"
#include "step_manager_defs.h"
#include "step_manager.h"
#include "entity_alive.h"
#include "level.h"
#include "gamepersistent.h"
#include "material_manager.h"
#include "profiler.h"
#include "IKLimbsController.h"

CStepManager::CStepManager()
{
	m_last_step_phase = 0.f;
	m_step_phase_offset = 0.f;
	m_step_phase_initialized = false;
	m_step_phase_last_output = 0.f;
	m_step_phase_has_output = false;
}

CStepManager::~CStepManager()
{
}

DLL_Pure *CStepManager::_construct	()
{
	m_object			= smart_cast<CEntityAlive*>(this);
	VERIFY				(m_object);
	return				(m_object);
}

void CStepManager::reload(LPCSTR section)
{
	m_legs_count		= pSettings->r_u8		(section, "LegsCount");
	LPCSTR anim_section = pSettings->r_string	(section, "step_params");

	if (!pSettings->section_exist(anim_section)) return;
	VERIFY((m_legs_count>=MIN_LEGS_COUNT) && (m_legs_count<=MAX_LEGS_COUNT));

	SStepParam			param; 
	param.step[0].time = 0.1f;	// avoid warning

	LPCSTR				anim_name, val;
	string16			cur_elem;

	CKinematicsAnimated	*skeleton_animated = smart_cast<CKinematicsAnimated*>(m_object->Visual());

	for (u32 i=0; pSettings->r_line(anim_section,i,&anim_name,&val); ++i) {
		_GetItem (val,0,cur_elem);

		param.cycles = u8(atoi(cur_elem));
		R_ASSERT(param.cycles >= 1);

		for (u32 j=0;j<m_legs_count;j++) {
			_GetItem	(val,1+j*2,		cur_elem);		param.step[j].time	= float(atof(cur_elem));
			_GetItem	(val,1+j*2+1,	cur_elem);		param.step[j].power	= float(atof(cur_elem));
			VERIFY		(_valid(param.step[j].power));			
		}
		
		MotionID motion_id = skeleton_animated->ID_Cycle_Safe(anim_name);
		if (!motion_id) continue;
		
		m_steps_map.insert(mk_pair(motion_id, param));
	}
	
	// reload foot bones
	for (u32 i = 0; i < MAX_LEGS_COUNT; i++) m_foot_bones[i] = BI_NONE;
	reload_foot_bones	();

	
	m_time_anim_started	= 0;
	m_blend				= 0;
}

void CStepManager::on_animation_start(MotionID motion_id, CBlend *blend)
{
	m_blend	= blend;
	if (!m_blend) return;

	if(m_object->character_ik_controller	())
		m_object->character_ik_controller	()->PlayLegs(blend);

	m_time_anim_started = Device.dwTimeGlobal;
	m_last_step_phase = 0.f;
	// Keep the accumulated phase across animation changes. The first sample
	// of the new animation will be aligned to the previous output below.
	m_step_phase_initialized = false; 
	
	// искать текущую анимацию в STEPS_MAP
	STEPS_MAP_IT it = m_steps_map.find(motion_id);
	if (it == m_steps_map.end()) {
		m_step_info.disable = true;
		return;
	}

	m_step_info.disable		= false;
	m_step_info.params		= it->second;
	m_step_info.cur_cycle	= 1;					// all cycles are 1-based

	for (u32 i=0; i<m_legs_count; i++) {
		m_step_info.activity[i].handled	= false;
		m_step_info.activity[i].cycle	= m_step_info.cur_cycle;
	}


	VERIFY					(m_blend);
}


float CStepManager::get_step_phase()
{
	if (m_step_info.disable || !m_blend || m_blend->timeTotal <= EPS || m_step_info.params.cycles == 0)
		return -1.f;

	// Step markers are stored as normalized times within each animation cycle.
	// Build their positions over the complete animation and find the interval
	// containing the current animation time.
	float markers[MAX_LEGS_COUNT * 255];
	u32 marker_count = 0;
	const u32 cycles = m_step_info.params.cycles;

	for (u32 cycle = 0; cycle < cycles; ++cycle)
	{
		for (u32 leg = 0; leg < m_legs_count; ++leg)
		{
			const float marker = (float(cycle) + m_step_info.params.step[leg].time) / float(cycles);
			if (marker >= 0.f && marker < 1.f && marker_count < sizeof(markers) / sizeof(markers[0]))
				markers[marker_count++] = marker;
		}
	}

	if (marker_count < 2)
		return -1.f;

	// Sort the small marker list in ascending order.
	for (u32 i = 1; i < marker_count; ++i)
	{
		const float value = markers[i];
		u32 j = i;
		while (j > 0 && markers[j - 1] > value)
		{
			markers[j] = markers[j - 1];
			--j;
		}
		markers[j] = value;
	}

	const float position = fmod(m_blend->timeCurrent, m_blend->timeTotal) / m_blend->timeTotal;
	if (position < 0.f)
		return -1.f;

	float previous = markers[marker_count - 1] - 1.f;
	float next = markers[0] + 1.f;
	for (u32 i = 0; i < marker_count; ++i)
	{
		if (markers[i] <= position)
			previous = markers[i];
		if (markers[i] > position)
		{
			next = markers[i];
			break;
		}
	}

	const float interval = next - previous;
	if (interval <= EPS)
		return -1.f;

	const float local_phase = (position - previous) / interval;

	// Preserve phase across footstep boundaries. Each step interval advances
	// the phase by one half-cycle (PI radians), so position/rotation remain
	// continuous when the local phase wraps from 1 back to 0.
	if (!m_step_phase_initialized)
	{
		// A new locomotion animation may start at an arbitrary point in its
		// cycle. Align its phase to the previous output instead of resetting the
		// HUD phase at the animation boundary. Use even cycle offsets because
		// the HUD oscillator maps one phase unit to PI radians.
		if (m_step_phase_has_output)
			m_step_phase_offset = 2.f * floorf((m_step_phase_last_output - local_phase) * 0.5f + 0.5f);
		else
			m_step_phase_offset = 0.f;
		m_last_step_phase = local_phase;
		m_step_phase_initialized = true;
	}
	else
	{
		if (local_phase + 0.5f < m_last_step_phase)
			m_step_phase_offset += 1.f;
		m_last_step_phase = local_phase;
	}

	m_step_phase_last_output = m_step_phase_offset + local_phase;
	m_step_phase_has_output = true;
	return m_step_phase_last_output;
}

void CStepManager::update()
{
	START_PROFILE("Step Manager")

	if (m_step_info.disable)	return;
	if (!m_blend)				return;

	SGameMtlPair* mtl_pair		= m_object->material().get_current_pair();
	if (!mtl_pair)				return;

	// получить параметры шага
	SStepParam	&step		= m_step_info.params;
	u32		cur_time		= Device.dwTimeGlobal;

	// время одного цикла анимации
	float cycle_anim_time	= get_blend_time() / step.cycles;

	// пройти по всем ногам и проверить время
	for (u32 i=0; i<m_legs_count; i++) {

		// если событие уже обработано для этой ноги, то skip
		if (m_step_info.activity[i].handled && (m_step_info.activity[i].cycle == m_step_info.cur_cycle)) continue;

		// вычислить смещённое время шага в соответствии с параметрами анимации ходьбы
		u32 offset_time = m_time_anim_started + u32(1000 * (cycle_anim_time * (m_step_info.cur_cycle-1) + cycle_anim_time * step.step[i].time));
		if (offset_time <= cur_time){

			// Играть звук
			if (!mtl_pair->StepSounds.empty() && is_on_ground() ) 
			{
				Fvector sound_pos = m_object->Position();
				sound_pos.y += 0.5;
				GET_RANDOM(mtl_pair->StepSounds).play_no_feedback(m_object,0,0,&sound_pos,&m_step_info.params.step[i].power);
			}

			// Играть партиклы
			if (!mtl_pair->CollideParticles.empty())	{
				LPCSTR ps_name = *mtl_pair->CollideParticles[::Random.randI(0,mtl_pair->CollideParticles.size())];

				//отыграть партиклы столкновения материалов
				CParticlesObject* ps = CParticlesObject::Create(ps_name,TRUE);

				// вычислить позицию и направленность партикла
				Fmatrix pos; 

				// установить направление
				pos.k.set(Fvector().set(0.0f,1.0f,0.0f));
				Fvector::generate_orthonormal_basis(pos.k, pos.j, pos.i);

				// установить позицию
				pos.c.set(get_foot_position(ELegType(i)));

				ps->UpdateParent(pos,Fvector().set(0.f,0.f,0.f));
				GamePersistent().ps_needtoplay.push_back(ps);
			}

			// Play Camera FXs
			event_on_step();

			// обновить поле handle
			m_step_info.activity[i].handled	= true;
			m_step_info.activity[i].cycle	= m_step_info.cur_cycle;
		}
	}

	// определить текущий цикл
	if (m_step_info.cur_cycle < step.cycles) m_step_info.cur_cycle = 1 + u8(float(cur_time - m_time_anim_started) / (1000.f * cycle_anim_time));

	// если анимация циклическая...
	u32 time_anim_end = m_time_anim_started + u32(get_blend_time() * 1000);		// время завершения работы анимации
	if (!m_blend->stop_at_end && (time_anim_end < cur_time)) {
		
		m_time_anim_started		= time_anim_end;
		m_step_info.cur_cycle	= 1;

		for (u32 i=0; i<m_legs_count; i++) {
			m_step_info.activity[i].handled	= false;
			m_step_info.activity[i].cycle	= m_step_info.cur_cycle;
		}
	}
	STOP_PROFILE
}

//////////////////////////////////////////////////////////////////////////
// Function for foot processing
//////////////////////////////////////////////////////////////////////////
Fvector	CStepManager::get_foot_position(ELegType leg_type)
{
	R_ASSERT2(m_foot_bones[leg_type] != BI_NONE, "foot bone had not been set");

	CKinematics *pK					= smart_cast<CKinematics*>(m_object->Visual());
	const Fmatrix& bone_transform = pK->LL_GetBoneInstance(m_foot_bones[leg_type]).mTransform;	

	Fmatrix					global_transform;
	global_transform.mul_43	(m_object->XFORM(),bone_transform);

	return global_transform.c;
}

void CStepManager::load_foot_bones	(CInifile::Sect &data)
{
	for (CInifile::SectCIt I=data.Data.begin(); I!=data.Data.end(); ++I){
		const CInifile::Item& item	= *I;

		u16 index = smart_cast<CKinematics*>(m_object->Visual())->LL_BoneID(*item.second);
		VERIFY3(index != BI_NONE, "foot bone not found", *item.second);

		if (xr_strcmp(*item.first, "front_left") == 0) 			m_foot_bones[eFrontLeft]	= index;
		else if (xr_strcmp(*item.first, "front_right")== 0)		m_foot_bones[eFrontRight]	= index;
		else if (xr_strcmp(*item.first, "back_right")== 0)		m_foot_bones[eBackRight]	= index;
		else if (xr_strcmp(*item.first, "back_left")== 0)		m_foot_bones[eBackLeft]		= index;
	}
}

void CStepManager::reload_foot_bones()
{
	CInifile* ini = smart_cast<CKinematics*>(m_object->Visual())->LL_UserData();
	if(ini&&ini->section_exist("foot_bones")){
		load_foot_bones(ini->r_section("foot_bones"));
	}
	else {
		if (!pSettings->line_exist(*m_object->cNameSect(),"foot_bones"))
			R_ASSERT2(false,"section [foot_bones] not found in monster user_data");
		load_foot_bones(pSettings->r_section(pSettings->r_string(*m_object->cNameSect(),"foot_bones")));
	}

	// проверка на соответсвие
	int count = 0;
	for (u32 i = 0; i < MAX_LEGS_COUNT; i++) 
		if (m_foot_bones[i] != BI_NONE) count++;

	VERIFY(count == m_legs_count);
}

float CStepManager::get_blend_time()
{
	return 	(m_blend->timeTotal / m_blend->speed);
}
