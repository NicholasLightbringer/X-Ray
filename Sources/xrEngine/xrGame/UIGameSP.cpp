#include "pch_script.h"
#include "uigamesp.h"
#include "actor.h"
#include "level.h"

#include "game_cl_Single.h"
#include "ui/UIPdaAux.h"
#include "xr_level_controller.h"
#include "actorcondition.h"
#include "../xr_ioconsole.h"
#include "object_broker.h"
#include "GameTaskManager.h"
#include "GameTask.h"

#include "ui/UIInventoryWnd.h"
#include "ui/UITradeWnd.h"
#include "ui/UIPdaWnd.h"
#include "ui/UITalkWnd.h"
#include "ui/UICarBodyWnd.h"
#include "ui/UIMessageBox.h"

CUIGameSP::CUIGameSP()
{
	m_game			= NULL;
	
	InventoryMenu	= xr_new<CUIInventoryWnd>	();
	PdaMenu			= xr_new<CUIPdaWnd>			();
	TalkMenu		= xr_new<CUITalkWnd>		();
	UICarBodyMenu	= xr_new<CUICarBodyWnd>		();
	UIChangeLevelWnd= xr_new<CChangeLevelWnd>		();
}

CUIGameSP::~CUIGameSP() 
{
	delete_data(InventoryMenu);
	delete_data(PdaMenu);	
	delete_data(TalkMenu);
	delete_data(UICarBodyMenu);
	delete_data(UIChangeLevelWnd);
}

void CUIGameSP::shedule_Update(u32 dt)
{
	inherited::shedule_Update			(dt);
	CActor *pActor = smart_cast<CActor*>(Level().CurrentEntity());
	if(!pActor)							return;
	if(pActor->g_Alive())				return;

	HideShownDialogs						();
}

void CUIGameSP::HideShownDialogs()
{
	CUIDialogWnd* mir				= MainInputReceiver();
	if( mir			&&
			(	mir==InventoryMenu	||
				mir==PdaMenu		||
				mir==TalkMenu		||
				mir==UICarBodyMenu
			)
		)
	mir->GetHolder()->StartStopMenu			(mir,true);

}

void CUIGameSP::SetClGame (game_cl_GameState* g)
{
	inherited::SetClGame				(g);
	m_game = smart_cast<game_cl_Single*>(g);
	R_ASSERT							(m_game);
}


bool CUIGameSP::IR_OnKeyboardPress(int dik) 
{
	if(inherited::IR_OnKeyboardPress(dik)) return true;

	if( Device.Paused()		) return false;

	CActor *pActor = smart_cast<CActor*>(Level().CurrentEntity());
	if(!pActor)								return false;
	if( pActor && !pActor->g_Alive() )		return false;

	switch ( get_binded_action(dik) )
	{
	case kINVENTORY: 
		if( !MainInputReceiver() || MainInputReceiver()==InventoryMenu){
			m_game->StartStopMenu(InventoryMenu,true);
			return true;
		}break;

	case kACTIVE_JOBS:
		if( !MainInputReceiver() || MainInputReceiver()==PdaMenu){
			PdaMenu->SetActiveSubdialog(eptQuests);
			m_game->StartStopMenu(PdaMenu,true);
			return true;
		}break;

	case kMAP:
		if( !MainInputReceiver() || MainInputReceiver()==PdaMenu){
			PdaMenu->SetActiveSubdialog(eptMap);
			m_game->StartStopMenu(PdaMenu,true);
			return true;
		}break;

	case kCONTACTS:
		if( !MainInputReceiver() || MainInputReceiver()==PdaMenu){
			PdaMenu->SetActiveSubdialog(eptContacts);
			m_game->StartStopMenu(PdaMenu,true);
			return true;
		}break;

	case kSCORES:
		{
			SDrawStaticStruct* ss	= AddCustomStatic("main_task", true);
			SGameTaskObjective* o	= pActor->GameTaskManager().ActiveObjective();
			if(!o)
				ss->m_static->SetTextST	("st_no_active_task");
			else
				ss->m_static->SetTextST	(*(o->description));

		}break;
	}
	return false;
}

bool CUIGameSP::IR_OnKeyboardRelease(int dik) 
{
	if(inherited::IR_OnKeyboardRelease(dik)) return true;

	if( is_binded(kSCORES, dik))
			RemoveCustomStatic		("main_task");

	return false;
}


void CUIGameSP::StartTalk()
{
	m_game->StartStopMenu(TalkMenu,true);
}

void CUIGameSP::StartCarBody(CInventoryOwner* pOurInv, CInventoryOwner* pOthers)
{
	if( MainInputReceiver() )		return;
	UICarBodyMenu->InitCarBody		(pOurInv,  pOthers);
	m_game->StartStopMenu			(UICarBodyMenu,true);
}
void CUIGameSP::StartCarBody(CInventoryOwner* pOurInv, CInventoryBox* pBox)
{
	if( MainInputReceiver() )		return;
	UICarBodyMenu->InitCarBody		(pOurInv,  pBox);
	m_game->StartStopMenu			(UICarBodyMenu,true);
}

void CUIGameSP::ReInitShownUI() 
{ 
	if (InventoryMenu->IsShown()) 
		InventoryMenu->InitInventory_delayed(); 
	else if(UICarBodyMenu->IsShown())
		UICarBodyMenu->UpdateLists_delayed();
	
};


extern ENGINE_API BOOL bShowPauseString;
void CUIGameSP::ChangeLevel				(GameGraph::_GRAPH_ID game_vert_id, u32 level_vert_id, Fvector pos, Fvector ang, Fvector pos2, Fvector ang2, bool b, LPCSTR invitation)
{
	if( !MainInputReceiver() || MainInputReceiver()!=UIChangeLevelWnd)
	{
		UIChangeLevelWnd->m_game_vertex_id		= game_vert_id;
		UIChangeLevelWnd->m_level_vertex_id		= level_vert_id;
		UIChangeLevelWnd->m_position			= pos;
		UIChangeLevelWnd->m_angles				= ang;
		UIChangeLevelWnd->m_position_cancel		= pos2;
		UIChangeLevelWnd->m_angles_cancel		= ang2;
		UIChangeLevelWnd->m_b_position_cancel	= b;
		UIChangeLevelWnd->SetLevelChangerMessage(invitation, false);
		m_game->StartStopMenu					(UIChangeLevelWnd,true);
	}
}

void CUIGameSP::LevelChangerDisabled(LPCSTR message, Fvector pos, Fvector ang, bool b)
{
	if( !MainInputReceiver() || MainInputReceiver()!=UIChangeLevelWnd)
	{
		UIChangeLevelWnd->m_position_cancel = pos;
		UIChangeLevelWnd->m_angles_cancel = ang;
		UIChangeLevelWnd->m_b_position_cancel = b;
		UIChangeLevelWnd->SetLevelChangerMessage(message, true);
		m_game->StartStopMenu(UIChangeLevelWnd,true);
	}
}

void CUIGameSP::reset_ui()
{
	inherited::reset_ui				();
	InventoryMenu->Reset			();
	PdaMenu->Reset					();
	TalkMenu->Reset					();
	UICarBodyMenu->Reset			();
	UIChangeLevelWnd->Reset			();
}

CChangeLevelWnd::CChangeLevelWnd		()
{
	m_messageBox			= xr_new<CUIMessageBox>();	m_messageBox->SetAutoDelete(true);
	AttachChild				(m_messageBox);
	m_messageBox->Init		("message_box_change_level");
	m_defaultMessage		= m_messageBox->GetText();
	m_defaultOkButtonText = m_messageBox->GetOkButtonText();
	m_levelChangerMessage.clear();
	m_bLevelChangerBlocked = false;
	SetWndPos				(m_messageBox->GetWndPos());
	m_messageBox->SetWndPos	(0.0f,0.0f);
	SetWndSize				(m_messageBox->GetWndSize());
}
void CChangeLevelWnd::SendMessage(CUIWindow *pWnd, s16 msg, void *pData)
{
	if(pWnd==m_messageBox){
		if(msg==MESSAGE_BOX_YES_CLICKED){
			if (m_bLevelChangerBlocked)
				OnCancel();
			else
				OnOk();
		}else
		if(msg==MESSAGE_BOX_NO_CLICKED){
			OnCancel								();
		}
	}else
		inherited::SendMessage(pWnd, msg, pData);
}

void CChangeLevelWnd::OnOk()
{
	if (m_bLevelChangerBlocked)
		return;

	Game().StartStopMenu					(this, true);
	NET_Packet								p;
	p.w_begin								(M_CHANGE_LEVEL);
	p.w										(&m_game_vertex_id,sizeof(m_game_vertex_id));
	p.w										(&m_level_vertex_id,sizeof(m_level_vertex_id));
	p.w_vec3								(m_position);
	p.w_vec3								(m_angles);

	Level().Send							(p,net_flags(TRUE));
}

void CChangeLevelWnd::OnCancel()
{
	Game().StartStopMenu					(this, true);

	if(m_b_position_cancel){
		Actor()->MoveActor(m_position_cancel, m_angles_cancel);
	}
}

bool CChangeLevelWnd::OnKeyboard(int dik, EUIMessages keyboard_action)
{
	if(keyboard_action==WINDOW_KEY_PRESSED)
	{
		if (m_bLevelChangerBlocked)
			return true;

		if(is_binded(kQUIT, dik) )
			OnCancel();
		return true;
	}
	return inherited::OnKeyboard(dik, keyboard_action);
}

void CChangeLevelWnd::SetLevelChangerMessage(LPCSTR message, bool blocked)
{
	m_levelChangerMessage = message ? message : "";
	m_bLevelChangerBlocked = blocked;
}

bool g_block_pause	= false;
void CChangeLevelWnd::Show()
{
	if (m_levelChangerMessage.empty())
		m_messageBox->SetText(m_defaultMessage.c_str());
	else
		m_messageBox->SetText(m_levelChangerMessage.c_str());

	if (m_bLevelChangerBlocked)
	{
		m_messageBox->SetButtonsVisible(true, false, false);
		m_messageBox->SetOkButtonTextST("st_level_changer_close");
	}
	else
	{
		m_messageBox->SetButtonsVisible(true);
		m_messageBox->SetOkButtonText(m_defaultOkButtonText.c_str());
	}

	g_block_pause							= true;
	Device.Pause							(TRUE, TRUE, TRUE, "CChangeLevelWnd_show");
	bShowPauseString						= FALSE;
}

void CChangeLevelWnd::Hide()
{
	g_block_pause							= false;
	Device.Pause							(FALSE, TRUE, TRUE, "CChangeLevelWnd_hide");
}

