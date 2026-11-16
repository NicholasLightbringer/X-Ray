#include "StdAfx.h"
#include "UIKeyBinding.h"
#include "UIXmlInit.h"
#include "xrUIXmlParser.h"
#include "UIEditKeyBind.h"
#include "UIScrollView.h"
#include "../xr_level_controller.h"
#include "../string_table.h"

CUIKeyBinding::CUIKeyBinding()
{
	for (int i=0; i<3; i++)
		AttachChild		(&m_header[i]);

	AttachChild			(&m_frame);
}

void CUIKeyBinding::InitFromXml(CUIXml& xml_doc, LPCSTR path)
{
	CUIXmlInit::InitWindow		(xml_doc, path, 0, this);
	string256					buf;
	m_scroll_wnd				= xr_new<CUIScrollView>(); m_scroll_wnd->SetAutoDelete(true); AttachChild(m_scroll_wnd);
	CUIXmlInit::InitScrollView	(xml_doc, strconcat(sizeof(buf),buf,path,":scroll_view"),0, m_scroll_wnd);

	CUIXmlInit::InitFrameWindow	(xml_doc, strconcat(sizeof(buf),buf,path,":frame"),		0, &m_frame);
	CUIXmlInit::InitLabel		(xml_doc, strconcat(sizeof(buf),buf,path,":header_1"),	0, &m_header[0]);
	CUIXmlInit::InitLabel		(xml_doc, strconcat(sizeof(buf),buf,path,":header_2"),	0, &m_header[1]);
	CUIXmlInit::InitLabel		(xml_doc, strconcat(sizeof(buf),buf,path,":header_3"),	0, &m_header[2]);

	FillUpList					(xml_doc, path);
}

void CUIKeyBinding::FillUpList(CUIXml& xml_doc_ui, LPCSTR path_ui)
{
	string256		buf;
	CUIXml			xml_doc;
	CStringTable	st;
	xml_doc.Init							(CONFIG_PATH, UI_PATH, "ui_keybinding.xml");

	int groupsCount = xml_doc.GetNodesNum	("",0,"group");

	for (int i = 0; i<groupsCount; i++)
	{
		// add group
		shared_str grp_name					= xml_doc.ReadAttrib("group",i,"name");
		R_ASSERT							(xr_strlen(grp_name));

		CUIStatic* pItem					= xr_new<CUIStatic>();
		CUIXmlInit::InitStatic				(xml_doc_ui, strconcat(sizeof(buf),buf,path_ui,":scroll_view:item_group"),	0, pItem);
		pItem->SetTextST					(grp_name.c_str());
		m_scroll_wnd->AddWindow				(pItem, true);

		// add group items
		int commandsCount					= xml_doc.GetNodesNum("group",i,"command");
		XML_NODE* tab_node					= xml_doc.NavigateToNode("group",i);
		xml_doc.SetLocalRoot				(tab_node);

		for (int j = 0; j<commandsCount; j++)
		{
			// first field of list item
			shared_str command_id			= xml_doc.ReadAttrib("command",j,"id");

			pItem							= xr_new<CUIStatic>();
			CUIXmlInit::InitStatic			(xml_doc_ui, strconcat(sizeof(buf),buf,path_ui,":scroll_view:item_key"),	0, pItem);
			pItem->SetTextST				(command_id.c_str());
			m_scroll_wnd->AddWindow			(pItem, true);

			shared_str exe					= xml_doc.ReadAttrib("command",j,"exe");

#ifdef DEBUG
			if ( kNOTBINDED == action_name_to_id(*exe) )
			{
				Msg("action [%s] not exist. update data",exe.c_str());
				continue;
			}
#endif
			
			float item_width				= m_header[1].GetWidth()-3.0f;
			float item_pos					= m_header[1].GetWndPos().x;
			CUIEditKeyBind* pEditKB			= xr_new<CUIEditKeyBind>(true);pEditKB->SetAutoDelete(true);
			pEditKB->Init					(item_pos, 0, item_width, pItem->GetWndSize().y);
			pEditKB->Register				(*exe,"key_binding");
			pItem->AttachChild				(pEditKB);

			item_width						= m_header[2].GetWidth()-3.0f;
			item_pos						= m_header[2].GetWndPos().x;
			pEditKB							= xr_new<CUIEditKeyBind>(false);pEditKB->SetAutoDelete(true);
			pEditKB->Init					(item_pos, 0, item_width, pItem->GetWndSize().y);
			pEditKB->Register				(*exe,"key_binding");
			pItem->AttachChild				(pEditKB);
		}
		xml_doc.SetLocalRoot				(xml_doc.GetRoot());
	}

	// Extra HUD actions are added in code because ui_keybinding.xml is game data,
	// not part of the source archive.
	CUIStatic* pHudGroup = xr_new<CUIStatic>();
	CUIXmlInit::InitStatic(xml_doc_ui, strconcat(sizeof(buf), buf, path_ui, ":scroll_view:item_group"), 0, pHudGroup);
	pHudGroup->SetText("HUD controls");
	m_scroll_wnd->AddWindow(pHudGroup, true);

	struct SHudBinding
	{
		LPCSTR action;
		LPCSTR title;
	};
	static const SHudBinding hud_bindings[] =
	{
		{ "hud_hide", "Hide HUD" },
		{ "hud_show", "Show HUD" },
		{ "minimap_toggle", "Toggle mini-map" }
	};

	const u32 hud_binding_count = (u32)(sizeof(hud_bindings) / sizeof(hud_bindings[0]));
	for (u32 i = 0; i < hud_binding_count; ++i)
	{
		CUIStatic* pHudItem = xr_new<CUIStatic>();
		CUIXmlInit::InitStatic(xml_doc_ui, strconcat(sizeof(buf), buf, path_ui, ":scroll_view:item_key"), 0, pHudItem);
		pHudItem->SetText(hud_bindings[i].title);
		m_scroll_wnd->AddWindow(pHudItem, true);

		float item_width = m_header[1].GetWidth() - 3.0f;
		float item_pos = m_header[1].GetWndPos().x;
		CUIEditKeyBind* pEditKB = xr_new<CUIEditKeyBind>(true);
		pEditKB->SetAutoDelete(true);
		pEditKB->Init(item_pos, 0, item_width, pHudItem->GetWndSize().y);
		pEditKB->Register(hud_bindings[i].action, "key_binding");
		pHudItem->AttachChild(pEditKB);

		item_width = m_header[2].GetWidth() - 3.0f;
		item_pos = m_header[2].GetWndPos().x;
		pEditKB = xr_new<CUIEditKeyBind>(false);
		pEditKB->SetAutoDelete(true);
		pEditKB->Init(item_pos, 0, item_width, pHudItem->GetWndSize().y);
		pEditKB->Register(hud_bindings[i].action, "key_binding");
		pHudItem->AttachChild(pEditKB);
	}
#ifdef DEBUG
    CheckStructure							(xml_doc);
#endif
}

#ifdef DEBUG
void CUIKeyBinding::CheckStructure(CUIXml& xml_doc)
{
	bool first = true;
	CUIStatic*	pItem = false;
	
	for (int i=0; true; i++)
	{
		LPCSTR action_name = actions[i].action_name;
		if (action_name)
		{
			// These are appended dynamically in FillUpList above.
			if (0 == xr_strcmp(action_name, "hud_hide") ||
				0 == xr_strcmp(action_name, "hud_show") ||
				0 == xr_strcmp(action_name, "minimap_toggle"))
				continue;

			if (IsActionExist(action_name, xml_doc))
				continue;
			else
			{
				if (first)
				{
					pItem					= xr_new<CUIStatic>();
					pItem->SetWndPos		(Fvector2().set(0,0));
					pItem->SetWndSize		(Fvector2().set(m_scroll_wnd->GetWndSize().x,20.0f));
					pItem->SetText			("NEXT ITEMS NOT DESCRIBED IN COMMAND DESC LIST");
					m_scroll_wnd->AttachChild(pItem);
					first					= false;
				}

				pItem						= xr_new<CUIStatic>();
				pItem->SetWndPos			(Fvector2().set(0,0));
				pItem->SetWndSize			(Fvector2().set(m_scroll_wnd->GetWndSize().x,20.0f));
				pItem->SetText				(action_name);
				m_scroll_wnd->AttachChild	(pItem);
			}
		}
		else
			break;				
	}
}

bool CUIKeyBinding::IsActionExist(LPCSTR action, CUIXml& xml_doc)
{
	bool ret = false;
	int groupsCount = xml_doc.GetNodesNum("",0,"group");

	for (int i = 0; i<groupsCount; i++){
		// add group items
		int commandsCount = xml_doc.GetNodesNum("group",i,"command");
		XML_NODE* tab_node = xml_doc.NavigateToNode("group",i);
		xml_doc.SetLocalRoot(tab_node);

		for (int j = 0; j<commandsCount; j++){
			// first field of list item
			shared_str command_id = xml_doc.ReadAttrib("command",j,"exe");
			if (0 == xr_strcmp(action, *command_id))
			{
				ret = true;
				break;
			}
		}
		xml_doc.SetLocalRoot(xml_doc.GetRoot());
		if (ret)
			break;
	}
	return ret;
}
#endif
