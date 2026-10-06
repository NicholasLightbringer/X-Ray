////////////////////////////////////////////////////////////////////////////
//	Module 		: level_changer.cpp
//	Created 	: 10.07.2003
//  Modified 	: 10.07.2003
//	Author		: Dmitriy Iassenev
//	Description : Level change object
////////////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "level_changer.h"
#include "hit.h"
#include "actor.h"
#include "xrserver_objects_alife.h"
#include "level.h"
#include "ai_object_location.h"
#include "ai_space.h"
#include "level_graph.h"
#include "game_level_cross_table.h"
#include "patrol_path.h"
#include "patrol_path_storage.h"

#include "HudManager.h"
#include "UIGameSP.h"

xr_vector<CLevelChanger*>	g_lchangers;

CLevelChanger* GetLevelChangerBySection(LPCSTR section)
{
	if (!section || !section[0])
		return NULL;

	// Ищем среди реально существующих объектов уровня.
	// Это надёжнее, чем g_lchangers: скриптовый вызов может прийти
	// в момент, когда глобальный список ещё не синхронизирован.
	for (u32 i = 0; i < Level().Objects.o_count(); ++i)
	{
		CGameObject* object =
			smart_cast<CGameObject*>(Level().Objects.o_get_by_iterator(i));
		if (!object)
			continue;

		CLevelChanger* changer = smart_cast<CLevelChanger*>(object);
		if (!changer)
			continue;

		if (!xr_strcmp(*changer->cNameSect(), section) ||
			!xr_strcmp(*changer->cName(), section))
		{
			return changer;
		}
	}

	Msg("! GetLevelChangerBySection: not found [%s]", section);

	for (u32 i = 0; i < Level().Objects.o_count(); ++i)
	{
		CGameObject* object =
			smart_cast<CGameObject*>(Level().Objects.o_get_by_iterator(i));
		CLevelChanger* changer = object ? smart_cast<CLevelChanger*>(object) : NULL;
		if (changer)
		{
			Msg("!   available level changer: name=[%s] section=[%s]",
				*changer->cName(), *changer->cNameSect());
		}
	}

	return NULL;
}

static CInifile* g_level_changers_ini = NULL;
static bool g_level_changers_ini_checked = false;

static CInifile* level_changers_ini()
{
	if (g_level_changers_ini_checked)
		return g_level_changers_ini;

	g_level_changers_ini_checked = true;

	string_path file_name;
	FS.update_path(file_name, "$game_config$", "level_changers.ltx");

	if (!FS.exist(file_name))
		return NULL;

	g_level_changers_ini = xr_new<CInifile>(file_name, TRUE);
	return g_level_changers_ini;
}

static void trim_level_changer_string(xr_string& value)
{
	while (!value.empty() &&
		(value[0] == ' ' || value[0] == '\t' || value[0] == '\r' || value[0] == '\n'))
		value.erase(value.begin());

	while (!value.empty())
	{
		const char ch = value[value.size() - 1];
		if (ch != ' ' && ch != '\t' && ch != '\r' && ch != '\n')
			break;
		value.erase(value.size() - 1);
	}
}

static LPCSTR level_changer_config_section(const CLevelChanger* changer)
{
	CInifile* ini = level_changers_ini();
	if (!ini || !changer)
		return NULL;

	LPCSTR name = *changer->cName();
	if (name && name[0] && ini->section_exist(name))
		return name;

	LPCSTR section_name = *changer->cNameSect();
	if (section_name && section_name[0] && ini->section_exist(section_name))
		return section_name;

	return NULL;
}

static bool parse_level_changer_bool(LPCSTR value, bool& result)
{
	if (!value)
		return false;

	xr_string parsed = value;
	trim_level_changer_string(parsed);

	if (!stricmp(parsed.c_str(), "true"))
	{
		result = true;
		return true;
	}

	if (!stricmp(parsed.c_str(), "false"))
	{
		result = false;
		return true;
	}

	return false;
}

static bool level_changer_closed(const CLevelChanger* changer)
{
	CInifile* ini = level_changers_ini();
	LPCSTR section = level_changer_config_section(changer);

	// No config file / no section / no closed key = original behaviour.
	if (!ini || !section || !ini->line_exist(section, "closed"))
		return false;

	LPCSTR value = ini->r_string(section, "closed");
	if (!value || !value[0])
		return false;

	xr_string expression = value;
	trim_level_changer_string(expression);

	bool simple_result = false;
	if (parse_level_changer_bool(expression.c_str(), simple_result))
		return simple_result;

	if (expression[0] != '{')
	{
		Msg("! level_changers.ltx: invalid closed expression in [%s]: %s", section, value);
		return false;
	}

	xr_string::size_type condition_end = expression.find('}');
	if (condition_end == xr_string::npos)
	{
		Msg("! level_changers.ltx: missing '}' in closed expression in [%s]: %s", section, value);
		return false;
	}

	xr_string condition = expression.substr(1, condition_end - 1);
	trim_level_changer_string(condition);

	if (condition.empty() || (condition[0] != '+' && condition[0] != '-'))
	{
		Msg("! level_changers.ltx: invalid info condition in [%s]: %s", section, value);
		return false;
	}

	const bool must_have_info = condition[0] == '+';
	condition.erase(condition.begin());
	trim_level_changer_string(condition);

	if (condition.empty())
	{
		Msg("! level_changers.ltx: empty infoportion in [%s]: %s", section, value);
		return false;
	}

	xr_string results = expression.substr(condition_end + 1);
	trim_level_changer_string(results);
	xr_string::size_type comma = results.find(',');
	if (comma == xr_string::npos)
	{
		Msg("! level_changers.ltx: missing ',' in closed expression in [%s]: %s", section, value);
		return false;
	}

	xr_string true_value = results.substr(0, comma);
	xr_string false_value = results.substr(comma + 1);
	trim_level_changer_string(true_value);
	trim_level_changer_string(false_value);

	bool when_true = false;
	bool when_false = false;
	if (!parse_level_changer_bool(true_value.c_str(), when_true) ||
		!parse_level_changer_bool(false_value.c_str(), when_false))
	{
		Msg("! level_changers.ltx: invalid boolean result in [%s]: %s", section, value);
		return false;
	}

	CActor* actor = Actor();
	const bool has_info = actor && actor->HasInfo(shared_str(condition.c_str()));
	const bool condition_result = must_have_info ? has_info : !has_info;
	return condition_result ? when_true : when_false;
}

static LPCSTR level_changer_tip(const CLevelChanger* changer, bool closed)
{
	CInifile* ini = level_changers_ini();
	LPCSTR section = level_changer_config_section(changer);

	if (ini && section)
	{
		LPCSTR key = closed ? "tip_closed" : "tip_opened";
		if (ini->line_exist(section, key))
			return ini->r_string(section, key);
	}

	return closed ? "st_level_changer_disabled" : "level_changer_invitation";
}

CLevelChanger::~CLevelChanger	()
{
}

void CLevelChanger::Center		(Fvector& C) const
{
	XFORM().transform_tiny		(C,CFORM()->getSphere().P);
}

float CLevelChanger::Radius		() const
{
	return CFORM()->getRadius	();
}

void CLevelChanger::net_Destroy	() 
{
	inherited ::net_Destroy	();
	xr_vector<CLevelChanger*>::iterator it = std::find(g_lchangers.begin(), g_lchangers.end(), this);
	if(it != g_lchangers.end())
		g_lchangers.erase(it);
}

BOOL CLevelChanger::net_Spawn	(CSE_Abstract* DC) 
{
	m_entrance_time				= 0;
	m_bLevelChangerEnabled		= true;
	m_levelChangerInvitation	= "";
	m_levelChangerDisabledMessage = "st_level_changer_disabled";
	CCF_Shape *l_pShape			= xr_new<CCF_Shape>(this);
	collidable.model			= l_pShape;
	
	CSE_Abstract				*l_tpAbstract = (CSE_Abstract*)(DC);
	CSE_ALifeLevelChanger		*l_tpALifeLevelChanger = smart_cast<CSE_ALifeLevelChanger*>(l_tpAbstract);
	R_ASSERT					(l_tpALifeLevelChanger);

	m_game_vertex_id			= l_tpALifeLevelChanger->m_tNextGraphID;
	m_level_vertex_id			= l_tpALifeLevelChanger->m_dwNextNodeID;
	m_position					= l_tpALifeLevelChanger->m_tNextPosition;
	m_angles					= l_tpALifeLevelChanger->m_tAngles;

	m_bSilentMode				= !!l_tpALifeLevelChanger->m_bSilentMode;
	if (ai().get_level_graph()) {
		//. this information should be computed in xrAI
		ai_location().level_vertex	(ai().level_graph().vertex(u32(-1),Position()));
		ai_location().game_vertex	(ai().cross_table().vertex(ai_location().level_vertex_id()).game_vertex_id());
	}

	feel_touch.clear			();
	
	for (u32 i=0; i < l_tpALifeLevelChanger->shapes.size(); ++i) {
		CSE_Shape::shape_def	&S = l_tpALifeLevelChanger->shapes[i];
		switch (S.type) {
			case 0 : {
				l_pShape->add_sphere(S.data.sphere);
				break;
			}
			case 1 : {
				l_pShape->add_box(S.data.box);
				break;
			}
		}
	}

	BOOL						bOk = inherited::net_Spawn(DC);
	if (bOk) {
		l_pShape->ComputeBounds	();
		Fvector					P;
		XFORM().transform_tiny	(P,CFORM()->getSphere().P);
		setEnabled				(TRUE);
	}
	g_lchangers.push_back		(this);
	return						(bOk);
}

void CLevelChanger::shedule_Update(u32 dt)
{
	inherited::shedule_Update	(dt);

	const Fsphere				&s = CFORM()->getSphere();
	Fvector						P;
	XFORM().transform_tiny		(P,s.P);
	feel_touch_update			(P,s.R);

	update_actor_invitation		();
}
void CLevelChanger::feel_touch_new	(CObject *tpObject)
{
	CActor*			l_tpActor = smart_cast<CActor*>(tpObject);
	VERIFY			(l_tpActor);
	if (!l_tpActor->g_Alive())
		return;

	CUIGameSP* pGameSP = smart_cast<CUIGameSP*>(HUD().GetUI()->UIGame());

	if (level_changer_closed(this))
	{
		Fvector reject_pos, reject_angles;
		bool has_reject_pos = get_reject_pos(reject_pos, reject_angles);

		if (pGameSP)
			pGameSP->LevelChangerDisabled(
				level_changer_tip(this, true),
				reject_pos,
				reject_angles,
				has_reject_pos
			);

		m_entrance_time = Device.fTimeGlobal;
		return;
	}

	if (m_bSilentMode) {
		NET_Packet	p;
		p.w_begin	(M_CHANGE_LEVEL);
		p.w			(&m_game_vertex_id,sizeof(m_game_vertex_id));
		p.w			(&m_level_vertex_id,sizeof(m_level_vertex_id));
		p.w_vec3	(m_position);
		p.w_vec3	(m_angles);
		Level().Send(p,net_flags(TRUE));
		return;
	}

	Fvector			p,r;
	bool				b = get_reject_pos(p,r);
	if (pGameSP)
		pGameSP->ChangeLevel(m_game_vertex_id,m_level_vertex_id,m_position,m_angles,p,r,b,level_changer_tip(this, false));

	m_entrance_time	= Device.fTimeGlobal;
}

bool CLevelChanger::get_reject_pos(Fvector& p, Fvector& r)
{
		p.set(0,0,0);
		r.set(0,0,0);
//--		db.actor:set_actor_position(patrol("t_way"):point(0))
//--		local dir = patrol("t_look"):point(0):sub(patrol("t_way"):point(0))
//--		db.actor:set_actor_direction(-dir:getH())

		if(m_ini_file && m_ini_file->section_exist("pt_move_if_reject"))
		{
			LPCSTR p_name = m_ini_file->r_string("pt_move_if_reject", "path");
			const CPatrolPath*		patrol_path = ai().patrol_paths().path(p_name);
			VERIFY					(patrol_path);
			
			const CPatrolPoint*		pt;
			pt						= &patrol_path->vertex(0)->data();
			p						= pt->position();

			Fvector tmp;
			pt						= &patrol_path->vertex(1)->data();
			tmp.sub					(pt->position(),p);
			tmp.getHP				(r.y,r.x);
			return true;
		}
		return false;
}

BOOL CLevelChanger::feel_touch_contact	(CObject *object)
{
	return	(((CCF_Shape*)CFORM())->Contact(object)) && smart_cast<CActor*>(object);
}

void CLevelChanger::update_actor_invitation()
{
	if(m_bSilentMode)						return;
	xr_vector<CObject*>::iterator it		= feel_touch.begin();
	xr_vector<CObject*>::iterator it_e		= feel_touch.end();

	for(;it!=it_e;++it){
		CActor*			l_tpActor = smart_cast<CActor*>(*it);
		VERIFY			(l_tpActor);

		if(m_entrance_time+5.0f < Device.fTimeGlobal){
			CUIGameSP* pGameSP = smart_cast<CUIGameSP*>(HUD().GetUI()->UIGame());

			if (level_changer_closed(this))
			{
				Fvector reject_pos, reject_angles;
				bool has_reject_pos = get_reject_pos(reject_pos, reject_angles);

				if (pGameSP)
					pGameSP->LevelChangerDisabled(
						level_changer_tip(this, true),
						reject_pos,
						reject_angles,
						has_reject_pos
					);

				m_entrance_time = Device.fTimeGlobal;
				continue;
			}

			Fvector			p,r;
			bool				b = get_reject_pos(p,r);
			if(pGameSP)
				pGameSP->ChangeLevel(m_game_vertex_id,m_level_vertex_id,m_position,m_angles,p,r,b,level_changer_tip(this, false));
			m_entrance_time		= Device.fTimeGlobal;
		}
	}
}
