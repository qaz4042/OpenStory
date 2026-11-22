#pragma once
#include "Locale.h"
#include "../IO/KeyAction.h"
namespace ms::localization
{
    inline const char* action_name(KeyAction::Id action)
    {
        switch (action)
        {
        case KeyAction::EQUIPMENT: return get(Key::ActionEQUIPMENT);
        case KeyAction::ITEMS: return get(Key::ActionITEMS);
        case KeyAction::STATS: return get(Key::ActionSTATS);
        case KeyAction::SKILLS: return get(Key::ActionSKILLS);
        case KeyAction::FRIENDS: return get(Key::ActionFRIENDS);
        case KeyAction::WORLDMAP: return get(Key::ActionWORLDMAP);
        case KeyAction::MAPLECHAT: return get(Key::ActionMAPLECHAT);
        case KeyAction::MINIMAP: return get(Key::ActionMINIMAP);
        case KeyAction::QUESTLOG: return get(Key::ActionQUESTLOG);
        case KeyAction::KEYBINDINGS: return get(Key::ActionKEYBINDINGS);
        case KeyAction::SAY: return get(Key::ActionSAY);
        case KeyAction::WHISPER: return get(Key::ActionWHISPER);
        case KeyAction::PARTYCHAT: return get(Key::ActionPARTYCHAT);
        case KeyAction::FRIENDSCHAT: return get(Key::ActionFRIENDSCHAT);
        case KeyAction::MENU: return get(Key::ActionMENU);
        case KeyAction::QUICKSLOTS: return get(Key::ActionQUICKSLOTS);
        case KeyAction::TOGGLECHAT: return get(Key::ActionTOGGLECHAT);
        case KeyAction::GUILD: return get(Key::ActionGUILD);
        case KeyAction::GUILDCHAT: return get(Key::ActionGUILDCHAT);
        case KeyAction::PARTY: return get(Key::ActionPARTY);
        case KeyAction::NOTIFIER: return get(Key::ActionNOTIFIER);
        case KeyAction::CHARINFO: return get(Key::ActionCHARINFO);
        case KeyAction::MONSTERBOOK: return get(Key::ActionMONSTERBOOK);
        case KeyAction::CASHSHOP: return get(Key::ActionCASHSHOP);
        case KeyAction::ALLIANCECHAT: return get(Key::ActionALLIANCECHAT);
        case KeyAction::CHANGECHANNEL: return get(Key::ActionCHANGECHANNEL);
        case KeyAction::MEDALS: return get(Key::ActionMEDALS);
        case KeyAction::BOSSPARTY: return get(Key::ActionBOSSPARTY);
        case KeyAction::MAINMENU: return get(Key::ActionMAINMENU);
        case KeyAction::SCREENSHOT: return get(Key::ActionSCREENSHOT);
        case KeyAction::PROFESSION: return get(Key::ActionPROFESSION);
        case KeyAction::ITEMPOT: return get(Key::ActionITEMPOT);
        case KeyAction::EVENT: return get(Key::ActionEVENT);
        case KeyAction::SILENTCRUSADE: return get(Key::ActionSILENTCRUSADE);
        case KeyAction::MANAGELEGION: return get(Key::ActionMANAGELEGION);
        case KeyAction::BATTLEANALYSIS: return get(Key::ActionBATTLEANALYSIS);
        case KeyAction::BITS: return get(Key::ActionBITS);
        case KeyAction::GUIDE: return get(Key::ActionGUIDE);
        case KeyAction::ENHANCEEQUIP: return get(Key::ActionENHANCEEQUIP);
        case KeyAction::MONSTERCOLLECTION: return get(Key::ActionMONSTERCOLLECTION);
        case KeyAction::SOULWEAPON: return get(Key::ActionSOULWEAPON);
        case KeyAction::MAPLENEWS: return get(Key::ActionMAPLENEWS);
        case KeyAction::FAMILY: return get(Key::ActionFAMILY);
        case KeyAction::PICKUP: return get(Key::ActionPICKUP);
        case KeyAction::SIT: return get(Key::ActionSIT);
        case KeyAction::ATTACK: return get(Key::ActionATTACK);
        case KeyAction::JUMP: return get(Key::ActionJUMP);
        case KeyAction::INTERACT_HARVEST: return get(Key::ActionINTERACT_HARVEST);
        case KeyAction::FACE1: return get(Key::ActionFACE1);
        case KeyAction::FACE2: return get(Key::ActionFACE2);
        case KeyAction::FACE3: return get(Key::ActionFACE3);
        case KeyAction::FACE4: return get(Key::ActionFACE4);
        case KeyAction::FACE5: return get(Key::ActionFACE5);
        case KeyAction::FACE6: return get(Key::ActionFACE6);
        case KeyAction::FACE7: return get(Key::ActionFACE7);
        default: return get(Key::Unassigned);
        }
    }
}
