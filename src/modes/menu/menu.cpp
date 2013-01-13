///////////////////////////////////////////////////////////////////////////////
//            Copyright (C) 2004-2010 by The Allacrost Project
//                         All Rights Reserved
//
// This code is licensed under the GNU GPL version 2. It is free software
// and you may modify it and/or redistribute it under the terms of this license.
// See http://www.gnu.org/copyleft/gpl.html for details.
///////////////////////////////////////////////////////////////////////////////

/** ****************************************************************************
*** \file    menu.cpp
*** \author  Daniel Steuernol steu@allacrost.org
*** \author  Andy Gardner chopperdave@allacrost.org
*** \author  Nik Nadig (IkarusDowned) nihonnik@gmail.com
*** \brief   Source file for menu mode interface.
*** ***************************************************************************/

#include <iostream>
#include <sstream>

#include "engine/system.h"
#include "engine/input.h"
#include "engine/audio/audio.h"
#include "modes/pause.h"

#include "engine/mode_manager.h"
#include "menu.h"

using namespace hoa_utils;
using namespace hoa_audio;
using namespace hoa_video;
using namespace hoa_gui;
using namespace hoa_system;
using namespace hoa_mode_manager;
using namespace hoa_input;
using namespace hoa_global;
using namespace hoa_pause;
using namespace hoa_menu::private_menu;

namespace hoa_menu
{

namespace private_menu {

//! \brief Functions that initialize the numerous option boxes
static void SetupOptionBoxCommonSettings(OptionBox *ob)
{
    // Set all the default options
    ob->SetTextStyle(TextStyle("title22"));
    ob->SetPosition(142.0f, 85.0f);
    ob->SetDimensions(115.0f, 50.0f, 1, 1, 1, 1);
    ob->SetAlignment(VIDEO_X_LEFT, VIDEO_Y_CENTER);
    ob->SetOptionAlignment(VIDEO_X_CENTER, VIDEO_Y_CENTER);
    ob->SetSelectMode(VIDEO_SELECT_SINGLE);
    ob->SetHorizontalWrapMode(VIDEO_WRAP_MODE_STRAIGHT);
    ob->SetCursorOffset(-52.0f, -20.0f);
}

AbstractMenuState::AbstractMenuState(const char *state_name, MenuMode *menu_mode):
    _state_name(state_name),
    _menu_mode(menu_mode),
    _from_state(NULL)
{
    // Init the controls parameters.
    _time_text.SetTextStyle(TextStyle("text22"));
    _time_text.SetAlignment(VIDEO_X_LEFT, VIDEO_Y_CENTER);
    _time_text.SetDimensions(200.0f, 30.0f);
    _time_text.SetPosition(110.0f, 620.0f);

    _drunes_text.SetTextStyle(TextStyle("text22"));
    _drunes_text.SetAlignment(VIDEO_X_LEFT, VIDEO_Y_CENTER);
    _drunes_text.SetDimensions(200.0f, 30.0f);
    _drunes_text.SetPosition(110.0f, 650.0f);
}

void AbstractMenuState::Update()
{
    // if the current state is set to active, to an active update instead and return
    if(_IsActive())
    {
        _ActiveWindowUpdate();
        return;
    }
    // handle a cancel press. in the case that we are at the main_menu state, pop the ModeManager off
    // the Mode stack as well.
    if(InputManager->CancelPress())
    {
        _menu_mode->_menu_sounds["cancel"].Play();
        if(_menu_mode->_current_menu_state == &(_menu_mode->_main_menu_state))
            ModeManager->Pop();
        // do instance specific cancel logic
        _OnCancel();
        return;
    }
    // handle left / right option box movement
    else if(InputManager->LeftPress())
    {
        _options.InputLeft();
        return;
    }
    else if(InputManager->RightPress())
    {
        _options.InputRight();
        return;
    }
    // play a sound if the option is selected
    else if(InputManager->ConfirmPress())
    {
        if(_options.IsOptionEnabled((_options.GetSelection())))
            _menu_mode->_menu_sounds["confirm"].Play();
        _options.InputConfirm();
    }
    // return the event type from the option
    int32 event = _options.GetEvent();
    // update the current option box for this state, thus clearing the event flag
    // if we don't do this, then upon return we enter right back into the state we wanted
    // to return from
    _options.Update();

    if(event == VIDEO_OPTION_CONFIRM) {
        uint32 selection = _options.GetSelection();
        AbstractMenuState *next_state = GetTransitionState(selection);
        // if the next state is the state we came from, it is similar to "cancel"
        if(next_state == _from_state)
        {
            _OnCancel();
            return;
        }
        // otherwise, if the state is valid and not this state itself, handle the transition
        else if(next_state != NULL && next_state != this)
        {
            // change the static current menu state
            _menu_mode->_current_menu_state = next_state;
            // run entry-specific code
            _menu_mode->_current_menu_state->_OnEntry(this);

        }

    }

    // update the current state
    _OnUpdateState();
    // update the options for the currently active state
    _menu_mode->_current_menu_state->GetOptions()->Update();

    UpdateTimeAndDrunes();
}

void AbstractMenuState::UpdateTimeAndDrunes()
{
    std::ostringstream os_time;
    uint8 hours = SystemManager->GetPlayHours();
    uint8 minutes = SystemManager->GetPlayMinutes();
    uint8 seconds = SystemManager->GetPlaySeconds();
    os_time << (hours < 10 ? "0" : "") << static_cast<uint32>(hours) << ":";
    os_time << (minutes < 10 ? "0" : "") << static_cast<uint32>(minutes) << ":";
    os_time << (seconds < 10 ? "0" : "") << static_cast<uint32>(seconds);

    // TODO: hoa_utils::ustring should be able to take const modifiers in its operators....
    static hoa_utils::ustring time_ustr_base = UTranslate("Time: ");
    static hoa_utils::ustring drunes_ustr_base = UTranslate("Drunes: ");
    _time_text.SetDisplayText(time_ustr_base + MakeUnicodeString(os_time.str()));
    _drunes_text.SetDisplayText(drunes_ustr_base + MakeUnicodeString(NumberToString(GlobalManager->GetDrunes())));
}

void AbstractMenuState::Draw()
{
    static const Color grayed(0.35f, 0.35f, 0.35f, 1.0f);
    // Draw the saved screen background
    // For that, set the system coordinates to the size of the window (same with the save-screen)
    int32 width = VideoManager->GetScreenWidth();
    int32 height = VideoManager->GetScreenHeight();
    VideoManager->SetCoordSys(0.0f, static_cast<float>(width), 0.0f, static_cast<float>(height));

    VideoManager->SetDrawFlags(VIDEO_X_LEFT, VIDEO_Y_BOTTOM, 0);

    VideoManager->Move(0.0f, 0.0f);
    _menu_mode->_saved_screen.Draw();

    // Restore the Coordinate system (that one is menu mode coodinate system)
    VideoManager->SetStandardCoordSys();
    VideoManager->SetDrawFlags(VIDEO_X_LEFT, VIDEO_Y_TOP, VIDEO_BLEND, 0);

    // Move to the top left corner
    VideoManager->Move(0.0f, 0.0f);

    _menu_mode->_main_options_window.Draw();

    // do instance specific main window rendering
    _OnDrawMainWindow();
    // do instance specific side window rendering
    _OnDrawSideWindow();
    // Draw currently active options box
    _options.Draw();

}

void AbstractMenuState::_OnDrawSideWindow()
{
    _menu_mode->_character_window0.Draw();
    _menu_mode->_character_window1.Draw();
    _menu_mode->_character_window2.Draw();
    _menu_mode->_character_window3.Draw();
}

void AbstractMenuState::_DrawEquipmentInfo(hoa_global::GlobalCharacter *character)
{

    VideoManager->Text()->Draw(UTranslate("STR: ") + MakeUnicodeString(NumberToString(character->GetStrength())));

    VideoManager->MoveRelative(0, 20);
    VideoManager->Text()->Draw(UTranslate("VIG: ") + MakeUnicodeString(NumberToString(character->GetVigor())));

    VideoManager->MoveRelative(0, 20);
    VideoManager->Text()->Draw(UTranslate("FRT: ") + MakeUnicodeString(NumberToString(character->GetFortitude())));

    VideoManager->MoveRelative(0, 20);
    VideoManager->Text()->Draw(UTranslate("PRO: ") + MakeUnicodeString(NumberToString(character->GetProtection())));

    VideoManager->MoveRelative(0, 20);
    VideoManager->Text()->Draw(UTranslate("AGI: ") + MakeUnicodeString(NumberToString(character->GetAgility())));

    VideoManager->MoveRelative(0, 20);
    VideoManager->Text()->Draw(UTranslate("EVD: ") + MakeUnicodeString(NumberToString(character->GetEvade()) + "%"));

    VideoManager->Move(310, 577);

    VideoManager->Text()->Draw(UTranslate("Current Equipment:"));

    VideoManager->MoveRelative(0, 20);
    VideoManager->Text()->Draw(UTranslate("Weapon"));

    VideoManager->MoveRelative(0, 20);
    VideoManager->Text()->Draw(UTranslate("Head"));

    VideoManager->MoveRelative(0, 20);
    VideoManager->Text()->Draw(UTranslate("Torso"));

    VideoManager->MoveRelative(0, 20);
    VideoManager->Text()->Draw(UTranslate("Arm"));

    VideoManager->MoveRelative(0, 20);
    VideoManager->Text()->Draw(UTranslate("Legs"));

    VideoManager->Move(400, 577);

    VideoManager->MoveRelative(0, 20);
    GlobalWeapon *wpn = character->GetWeaponEquipped();
    VideoManager->Text()->Draw(UTranslate("ATK: ") + MakeUnicodeString(NumberToString(wpn ? wpn->GetPhysicalAttack() : 0)));

    VideoManager->MoveRelative(0, 20);
    GlobalArmor *head_armor = character->GetHeadArmorEquipped();
    VideoManager->Text()->Draw(UTranslate("DEF: ") + MakeUnicodeString(NumberToString(head_armor ? head_armor->GetPhysicalDefense() : 0)));

    VideoManager->MoveRelative(0, 20);
    GlobalArmor *torso_armor = character->GetTorsoArmorEquipped();
    VideoManager->Text()->Draw(UTranslate("DEF: ") + MakeUnicodeString(NumberToString(torso_armor ? torso_armor->GetPhysicalDefense() : 0)));

    VideoManager->MoveRelative(0, 20);
    GlobalArmor *arm_armor = character->GetArmArmorEquipped();
    VideoManager->Text()->Draw(UTranslate("DEF: ") + MakeUnicodeString(NumberToString(arm_armor ? arm_armor->GetPhysicalDefense() : 0)));

    VideoManager->MoveRelative(0, 20);
    GlobalArmor *leg_armor = character->GetLegArmorEquipped();
    VideoManager->Text()->Draw(UTranslate("DEF: ") + MakeUnicodeString(NumberToString(leg_armor ? leg_armor->GetPhysicalDefense() : 0)));

    VideoManager->Move(550, 577);

    VideoManager->MoveRelative(0, 20);
    VideoManager->Text()->Draw(UTranslate("M.ATK: ") + MakeUnicodeString(NumberToString(wpn ? wpn->GetMetaphysicalAttack() : 0)));

    VideoManager->MoveRelative(0, 20);
    VideoManager->Text()->Draw(UTranslate("M.DEF: ") + MakeUnicodeString(NumberToString(head_armor ? head_armor->GetMetaphysicalDefense() : 0)));

    VideoManager->MoveRelative(0, 20);
    VideoManager->Text()->Draw(UTranslate("M.DEF: ") + MakeUnicodeString(NumberToString(torso_armor ? torso_armor->GetMetaphysicalDefense() : 0)));

    VideoManager->MoveRelative(0, 20);
    VideoManager->Text()->Draw(UTranslate("M.DEF: ") + MakeUnicodeString(NumberToString(arm_armor ? arm_armor->GetMetaphysicalDefense() : 0)));

    VideoManager->MoveRelative(0, 20);
    VideoManager->Text()->Draw(UTranslate("M.DEF: ") + MakeUnicodeString(NumberToString(leg_armor ? leg_armor->GetMetaphysicalDefense() : 0)));
    VideoManager->SetDrawFlags(VIDEO_X_CENTER, VIDEO_Y_BOTTOM, 0);
}

void AbstractMenuState::_DrawBottomMenu()
{
    _menu_mode->_bottom_window.Draw();

    VideoManager->SetDrawFlags(VIDEO_X_LEFT, VIDEO_Y_BOTTOM, 0);
    VideoManager->Move(150, 580);
    // Display Location
    _menu_mode->_locale_name.Draw();

    // Draw Played Time
    _time_text.Draw();

    // Display the current funds that the party has
    _drunes_text.Draw();

    if(!_menu_mode->_locale_graphic.GetFilename().empty()) {
        VideoManager->SetDrawFlags(VIDEO_X_RIGHT, VIDEO_Y_BOTTOM, 0);
        VideoManager->SetDrawFlags(VIDEO_X_LEFT, VIDEO_Y_BOTTOM, 0);
        VideoManager->Move(390, 685);
        _menu_mode->_locale_graphic.Draw();
    }
}


void AbstractMenuState::_OnCancel()
{
    // as long as the calling state is valid and not equal to this, simply switch back to it
    if(_from_state && _from_state != this)
        _menu_mode->_current_menu_state = _from_state;
}


void MainMenuState::Reset()
{
    // Setup the main options box
    SetupOptionBoxCommonSettings(&_options);
    _options.SetDimensions(745.0f, 50.0f, MAIN_OPTIONS_SIZE, 1, MAIN_OPTIONS_SIZE, 1);

    // Generate the strings
    std::vector<ustring> options;
    options.push_back(UTranslate("Inventory"));
    options.push_back(UTranslate("Skills"));
    options.push_back(UTranslate("Party"));
    options.push_back(UTranslate("Quests"));
    options.push_back(UTranslate("Map"));

    // Add strings and set default selection.
    _options.SetOptions(options);
    _options.SetSelection(MAIN_OPTIONS_INVENTORY);
}

AbstractMenuState* MainMenuState::GetTransitionState(uint32 selection)
{
    switch(selection)
    {
        case MAIN_OPTIONS_INVENTORY:
            return &(_menu_mode->_inventory_state);
            break;
        case MAIN_OPTIONS_SKILLS:
            return &(_menu_mode->_skills_state);
            break;
        case MAIN_OPTIONS_PARTY:
            return &(_menu_mode->_party_state);
            break;
        case MAIN_OPTIONS_QUESTS:
            return &(_menu_mode->_quests_state);
            break;
        case MAIN_OPTIONS_WORLDMAP:
            return &(_menu_mode->_world_map_state);
            break;
        default:
            PRINT_ERROR << "MENU ERROR: Invalid option in " << GetStateName() << "::GetTransitionState" << std::endl;
            break;

    }
    return NULL;
}

void MainMenuState::_OnUpdateState()
{
    uint32 draw_window = _options.GetSelection();
    switch(draw_window) {
    case MAIN_OPTIONS_WORLDMAP:
    {
        _menu_mode->_world_map_window.Update();
        break;
    }
    default:
        break;
    };
}

void MainMenuState::_OnDrawMainWindow()
{

    uint32 draw_window = _options.GetSelection();

    // Draw the chosen window
    switch(draw_window) {
        case MAIN_OPTIONS_INVENTORY: {
            AbstractMenuState::_DrawBottomMenu();
            _menu_mode->_inventory_window.Draw();
            break;
        }
        case MAIN_OPTIONS_SKILLS: {
            AbstractMenuState::_DrawBottomMenu();
            _menu_mode->_skills_window.Draw();
            break;
        }
        case MAIN_OPTIONS_QUESTS: {
            static const ustring quest_view_message = UTranslate("Select to view Quest Log.");
            _menu_mode->_bottom_window.Draw();
            _menu_mode->_help_information.SetDisplayText(quest_view_message);
            _menu_mode->_help_information.Draw();
            _menu_mode->_quest_window.Draw();
            break;
        }
        case MAIN_OPTIONS_WORLDMAP:
        {
            //TODO there is a visual bug here with the help information not showing properly. need to fix.
            static const ustring world_map_window_message = UTranslate("Select to view current world map.\nUse left / right to cycle through locations. Press 'cancel' to return");
            _menu_mode->_bottom_window.Draw();
            _menu_mode->_help_information.SetDisplayText(world_map_window_message);
            _menu_mode->_help_information.Draw();
            // actual drawing of thebottom window will occur upon transition
            // to the worls map state
            _menu_mode->_world_map_window.Draw();
            break;
        }
        case MAIN_OPTIONS_PARTY:
        default: {
            AbstractMenuState::_DrawBottomMenu();
            _menu_mode->_party_window.Draw();
            break;
        }
    } // switch draw_window

}

void MainMenuState::_OnDrawSideWindow()
{
    if(_options.GetSelection() == MAIN_OPTIONS_QUESTS)
        _menu_mode->_quest_list_window.Draw();
    else if (_options.GetSelection() != MAIN_OPTIONS_WORLDMAP)
        AbstractMenuState::_OnDrawSideWindow();
}
void InventoryState::Reset()
{
    // Setup the option box
    SetupOptionBoxCommonSettings(&_options);
    _options.SetDimensions(555.0f, 50.0f, INV_OPTIONS_SIZE, 1, INV_OPTIONS_SIZE, 1);

    // Generate the strings
    std::vector<ustring> options;
    options.push_back(UTranslate("Items"));
    options.push_back(UTranslate("Equip"));
    options.push_back(UTranslate("Remove"));
    options.push_back(UTranslate("Back"));

    // Add strings and set default selection.
    _options.SetOptions(options);
    _options.SetSelection(INV_OPTIONS_USE);

}

AbstractMenuState* InventoryState::GetTransitionState(uint32 selection)
{
    switch(selection)
    {
        case INV_OPTIONS_EQUIP:
        case INV_OPTIONS_REMOVE:
            return &(_menu_mode->_equip_state);
        case INV_OPTIONS_BACK:
            return &(_menu_mode->_main_menu_state);
        case INV_OPTIONS_USE:
            _menu_mode->_inventory_window.Activate(true);
            break;
        default:
            break;
    };
    return NULL;
}

void InventoryState::_ActiveWindowUpdate()
{
    _menu_mode->_inventory_window.Update();
}

bool InventoryState::_IsActive()
{
    return _menu_mode->_inventory_window.IsActive();
}

void InventoryState::_OnDrawMainWindow()
{

    uint32 draw_window = _options.GetSelection();
    _DrawBottomMenu();
    // Inventory state has multiple state types to draw, including the Equip transition state.
    switch(draw_window)
    {
        case INV_OPTIONS_EQUIP:
            _menu_mode->_equip_window.Draw();
            break;
        case INV_OPTIONS_REMOVE:
            _menu_mode->_equip_window.Draw();
            break;
        case INV_OPTIONS_USE:
        case INV_OPTIONS_BACK:
        default:
            _menu_mode->_inventory_window.Draw();
            break;
    }

}

void InventoryState::_DrawItemDescription(hoa_global::GlobalObject &obj,
                                          hoa_video::StillImage &item_image,
                                          hoa_gui::TextBox &description)
{
    int32 key_pos_x = 100 + obj.GetIconImage().GetWidth() - item_image.GetWidth() - 3;
    int32 key_pos_y = 600 + obj.GetIconImage().GetHeight() - item_image.GetHeight() - 3;
    VideoManager->Move(key_pos_x, key_pos_y);
    item_image.Draw();
    VideoManager->Move(185, 600);
    description.Draw();
}

void InventoryState::_DrawBottomMenu()
{
    static const ustring inventory_help_message = UTranslate("Select an item to Equip or Use.");
    _menu_mode->_bottom_window.Draw();



    VideoManager->SetDrawFlags(VIDEO_X_LEFT, VIDEO_Y_BOTTOM, 0);
    VideoManager->Move(150, 580);

    if(_menu_mode->_inventory_window._active_box == ITEM_ACTIVE_CATEGORY)
    {
        _menu_mode->_help_information.SetDisplayText(inventory_help_message);
        _menu_mode->_help_information.Draw();
    }
     //if we are out of items, the bottom view should do no work
    if(GlobalManager->GetInventory()->size() == 0 || _menu_mode->_inventory_window._item_objects.size() == 0)
        return;

    GlobalObject *obj = _menu_mode->_inventory_window._item_objects[ _menu_mode->_inventory_window._inventory_items.GetSelection() ];
    const GLOBAL_OBJECT obj_type = obj->GetObjectType();
    if(_menu_mode->_inventory_window._active_box == ITEM_ACTIVE_LIST) {


        VideoManager->SetDrawFlags(VIDEO_X_LEFT, VIDEO_Y_CENTER, 0);

        VideoManager->Move(100, 600);
        obj->GetIconImage().Draw();
        VideoManager->MoveRelative(65, -15);
        VideoManager->Text()->Draw(obj->GetName());
        VideoManager->SetDrawFlags(VIDEO_X_LEFT, VIDEO_Y_BOTTOM, 0);
        _menu_mode->_inventory_window._description.Draw();

        switch(obj_type)
        {
            case GLOBAL_OBJECT_KEY_ITEM:
                _DrawItemDescription(*obj, _menu_mode->_key_item_symbol, _menu_mode->_key_item_description);
                break;
            case GLOBAL_OBJECT_SHARD:
                _DrawItemDescription(*obj, _menu_mode->_shard_symbol, _menu_mode->_shard_description);
                break;
            default:
                break;
        };

        //Draw help text


    }
    else if(_menu_mode->_inventory_window._active_box == ITEM_ACTIVE_CHAR)
    {
        //character is selected, check the obj_type for equipable items
        uint32 actor_index = _menu_mode->_inventory_window._char_select.GetSelection();
        GlobalCharacter *ch = dynamic_cast<GlobalCharacter *>(GlobalManager->GetActiveParty()->GetActorAtIndex(actor_index));

        bool is_equipable_armor = false;
        bool is_equipable_weapon = false;
        GlobalArmor *selected_armor = NULL;
        GlobalWeapon *selected_weapon = NULL;

        ustring equipment_name;
        uint32 physical_attribute = 0;
        uint32 magical_attribute = 0;

        uint32 current_phys_attribute = 0;
        uint32 current_mag_attribute = 0;
        //check the obj_type again to see if its a weapon or armor
        switch(obj_type)
        {
            case GLOBAL_OBJECT_WEAPON:
            {
                selected_weapon = dynamic_cast<GlobalWeapon *>(obj);
                uint32 usability_bitmask = selected_weapon->GetUsableBy();
                is_equipable_weapon = usability_bitmask & ch->GetID();
                break;
            }
            case GLOBAL_OBJECT_HEAD_ARMOR:
            case GLOBAL_OBJECT_TORSO_ARMOR:
            case GLOBAL_OBJECT_ARM_ARMOR:
            case GLOBAL_OBJECT_LEG_ARMOR:
            {
                selected_armor = dynamic_cast<GlobalArmor *>(obj);
                uint32 usability_bitmask = selected_armor->GetUsableBy();
                is_equipable_armor = usability_bitmask & ch->GetID();
                break;
            }
            default:
                return;
                break;
        }
        if(is_equipable_armor && selected_armor)
        {
            //draw the equipment stats and change info
            AbstractMenuState::_DrawEquipmentInfo(ch);
            equipment_name = selected_armor->GetName();
            physical_attribute = selected_armor->GetPhysicalDefense();
            magical_attribute = selected_armor->GetMetaphysicalDefense();

            GlobalArmor *current_armor = NULL;
            switch(obj_type)
            {
                case GLOBAL_OBJECT_LEG_ARMOR:
                    current_armor = ch->GetLegArmorEquipped();
                    break;
                case GLOBAL_OBJECT_HEAD_ARMOR:
                    current_armor = ch->GetHeadArmorEquipped();
                    break;
                case GLOBAL_OBJECT_TORSO_ARMOR:
                    current_armor = ch->GetTorsoArmorEquipped();
                    break;
                case GLOBAL_OBJECT_ARM_ARMOR:
                    current_armor = ch->GetArmArmorEquipped();
                    break;
                default:
                    break;

            };
            if(current_armor) {
                current_phys_attribute = current_armor->GetPhysicalDefense();
                current_mag_attribute = current_armor->GetMetaphysicalDefense();
            }
            //draw the equipment info as armor
            EquipState::DrawEquipmentInfo(equipment_name,
                                          false,
                                          physical_attribute, magical_attribute,
                                          current_phys_attribute, current_mag_attribute);
        }
        else if(is_equipable_weapon && selected_weapon)
        {
            //draw the equipment stats and change info
            AbstractMenuState::_DrawEquipmentInfo(ch);
            //similar to armor but no need to do location checking
            equipment_name = selected_weapon->GetName();
            physical_attribute = selected_weapon->GetPhysicalAttack();
            magical_attribute = selected_weapon->GetMetaphysicalAttack();
            GlobalWeapon* current_weapon = ch->GetWeaponEquipped();
            if(current_weapon)
            {
                current_phys_attribute = current_weapon->GetPhysicalAttack();
                current_mag_attribute = current_weapon->GetMetaphysicalAttack();
            }
            //draw the equipment info as a weapon
            EquipState::DrawEquipmentInfo(equipment_name, true,
                                          physical_attribute, magical_attribute,
                                          current_phys_attribute, current_mag_attribute);

        }
        else
        {
            //otherwise print a message
            // NOTE: If more flexibility is needed down the road, load this from script
            const static ustring cannot_equip = UTranslate("This character cannot equip this item.");
            _menu_mode->_help_information.SetDisplayText(cannot_equip);
            _menu_mode->_help_information.Draw();

        }
    }
}


void PartyState::_ActiveWindowUpdate()
{
    _menu_mode->_party_window.Update();
    UpdateTimeAndDrunes();
}

bool PartyState::_IsActive()
{
    return _menu_mode->_party_window.GetActiveState();
}

void PartyState::Reset()
{
    // Setup the status option box
    SetupOptionBoxCommonSettings(&_options);
    _options.SetDimensions(415.0f, 50.0f, PARTY_OPTIONS_SIZE, 1, PARTY_OPTIONS_SIZE, 1);

    // Generate the strings
    std::vector<ustring> options;
    options.push_back(UTranslate("View/Reorder"));
    options.push_back(UTranslate("Back"));

    // Add strings and set default selection.
    _options.SetOptions(options);
    _options.SetSelection(PARTY_OPTIONS_VIEW_ALTER);
}

AbstractMenuState* PartyState::GetTransitionState(uint32 selection)
{
    switch(selection)
    {
        case PARTY_OPTIONS_BACK:
            return &(_menu_mode->_main_menu_state);
        case PARTY_OPTIONS_VIEW_ALTER:
            _menu_mode->_party_window.Activate(true);
            break;
        default:
            break;
    };
    return NULL;
}

void PartyState::_DrawBottomMenu()
{
    static const ustring change_position_message = UTranslate("Select a character to change position with.");
    static const ustring change_formation_mesage = UTranslate("Select a character to change formation.");
    static const ustring general_help_message = UTranslate("View character Information.\nSelect a character to change formation.");

    _menu_mode->_bottom_window.Draw();

    VideoManager->SetDrawFlags(VIDEO_X_LEFT, VIDEO_Y_BOTTOM, 0);
    VideoManager->Move(150, 580);

    //show a helpfull message
    if(_IsActive())
    {
        if(_menu_mode->_party_window.GetActiveState() == FORM_ACTIVE_SECOND)
            _menu_mode->_help_information.SetDisplayText(change_position_message);
        else
            _menu_mode->_help_information.SetDisplayText(change_formation_mesage);
    }
    else
        _menu_mode->_help_information.SetDisplayText(general_help_message);

    _menu_mode->_help_information.Draw();
}
void PartyState::_OnDrawMainWindow()
{

    _DrawBottomMenu();
    _menu_mode->_party_window.Draw();

}

void SkillsState::_ActiveWindowUpdate()
{
    _menu_mode->_skills_window.Update();
}

bool SkillsState::_IsActive()
{
    return _menu_mode->_skills_window.IsActive();
}

void SkillsState::Reset()
{
    // Setup the option box
    SetupOptionBoxCommonSettings(&_options);
    _options.SetDimensions(415.0f, 50.0f, SKILLS_OPTIONS_SIZE, 1, SKILLS_OPTIONS_SIZE, 1);

    // Generate the strings
    std::vector<ustring> options;
    options.push_back(UTranslate("Use"));
    options.push_back(UTranslate("Back"));

    // Add strings and set default selection.
    _options.SetOptions(options);
    _options.SetSelection(SKILLS_OPTIONS_USE);
}

AbstractMenuState* SkillsState::GetTransitionState(uint32 selection)
{

    switch(selection)
    {
        case SKILLS_OPTIONS_BACK:
            return &(_menu_mode->_main_menu_state);
        case SKILLS_OPTIONS_USE:
            _menu_mode->_skills_window.Activate(true);
            break;
        default:
            break;

    }
    return NULL;
}

void SkillsState::_OnDrawMainWindow()
{

    _DrawBottomMenu();
    _menu_mode->_skills_window.Draw();

}
void SkillsState::_DrawBottomMenu()
{
    _menu_mode->_bottom_window.Draw();

    VideoManager->SetDrawFlags(VIDEO_X_LEFT, VIDEO_Y_BOTTOM, 0);
    VideoManager->Move(150, 580);
    _menu_mode->_skills_window._description.Draw();
}

void EquipState::_ActiveWindowUpdate()
{
    _menu_mode->_equip_window.Update();
}

bool EquipState::_IsActive()
{
    return _menu_mode->_equip_window.IsActive();
}

void EquipState::Reset()
{
    // Setup the status option box
    SetupOptionBoxCommonSettings(&_options);
    _options.SetDimensions(100.0f, 50.0f, EQUIP_OPTIONS_SIZE, 1, EQUIP_OPTIONS_SIZE, 1);

    // Generate the strings
    std::vector<ustring> options;
    options.push_back(UTranslate("Back"));

    // Add strings and set default selection.
    _options.SetOptions(options);
    _options.SetSelection(EQUIP_OPTIONS_BACK);
}

AbstractMenuState* EquipState::GetTransitionState(uint32 selection)
{
    switch(selection)
    {
        case EQUIP_OPTIONS_BACK:
            return &(_menu_mode->_inventory_state);
        default:
            break;
    };
    return NULL;
}

void EquipState::_OnEntry(AbstractMenuState *from_state)
{
    AbstractMenuState::_OnEntry(from_state);
    // equip state must handle removal as well as equip. we check to see where we transitioned from...
    if(_from_state == &_menu_mode->_inventory_state)
    {
        // if its from the inventory EQUIP selection, activate the window with the equip flag set to true
        if(_from_state->GetOptions()->GetSelection() == InventoryState::INV_OPTIONS_EQUIP)
            _menu_mode->_equip_window.Activate(true, true);
        // otherwise, it was frmo the REMOVE selection, activate the window with the equip flag set to false
        else
            _menu_mode->_equip_window.Activate(true, false);
    }

}

void EquipState::_OnDrawMainWindow()
{
    _DrawBottomMenu();
    _menu_mode->_equip_window.Draw();
}

void EquipState::DrawEquipmentInfo(const ustring &equipment_name,
                                   bool is_weapon, uint32 physical_attribute,
                                   uint32 magical_attribute, uint32 current_phys_attribute,
                                   uint32 current_mag_attribute)
{
    // Display the info
    VideoManager->Move(755, 577);
    VideoManager->Text()->Draw(equipment_name);
    VideoManager->MoveRelative(0, 20);

    if (is_weapon)
        VideoManager->Text()->Draw(UTranslate("ATK:"));
    else
        VideoManager->Text()->Draw(UTranslate("DEF:"));
    VideoManager->MoveRelative(0, 20);
    VideoManager->Text()->Draw(MakeUnicodeString(NumberToString(physical_attribute)));

    if (physical_attribute - current_phys_attribute != 0) {
        std::string sign_start;
        Color text_color;
        if ((int32)(physical_attribute - current_phys_attribute) > 0) {
            sign_start = "+";
            text_color = Color::green;
        } else {
            sign_start = "";
            text_color = Color::red;
        }

        ustring diff_stat = MakeUnicodeString(sign_start)
                            + MakeUnicodeString(NumberToString(physical_attribute - current_phys_attribute));
        VideoManager->MoveRelative(60, 0);
        VideoManager->Text()->Draw(diff_stat, TextStyle("text22", text_color));
        VideoManager->MoveRelative(-60, 0);
    }
    VideoManager->MoveRelative(0, 20);

    if (is_weapon)
        VideoManager->Text()->Draw(UTranslate("M.ATK:"));
    else
        VideoManager->Text()->Draw(UTranslate("M.DEF:"));
    VideoManager->MoveRelative(0, 20);
    VideoManager->Text()->Draw(MakeUnicodeString(NumberToString(magical_attribute)));

    if (magical_attribute - current_mag_attribute != 0) {
        std::string sign_start;
        Color text_color;
        if ((int32)(magical_attribute - current_mag_attribute) > 0) {
            sign_start = "+";
            text_color = Color::green;
        } else {
            sign_start = "";
            text_color = Color::red;
        }

        ustring diff_stat = MakeUnicodeString(sign_start)
                            + MakeUnicodeString(NumberToString(magical_attribute - current_mag_attribute));
        VideoManager->MoveRelative(60, 0);
        VideoManager->Text()->Draw(diff_stat, TextStyle("text22", text_color));
        VideoManager->MoveRelative(-60, 0);
    }

    VideoManager->MoveRelative(0, 20);
}

void EquipState::_DrawBottomMenu()
{
    _menu_mode->_bottom_window.Draw();

    VideoManager->SetDrawFlags(VIDEO_X_LEFT, VIDEO_Y_BOTTOM, 0);
    VideoManager->Move(150, 580);
    GlobalCharacter *ch = dynamic_cast<GlobalCharacter *>(GlobalManager->GetActiveParty()->GetActorAtIndex(_menu_mode->_equip_window._char_select.GetSelection()));
    AbstractMenuState::_DrawEquipmentInfo(ch);
    // Show the selected equipment stats (and diff with the current one.)
    if(_menu_mode->_equip_window._active_box == EQUIP_ACTIVE_LIST) {
        ustring equipment_name;
        uint32 physical_attribute = 0;
        uint32 magical_attribute = 0;

        uint32 current_phys_attribute = 0;
        uint32 current_mag_attribute = 0;

        switch(_menu_mode->_equip_window._equip_select.GetSelection()) {
        case EQUIP_WEAPON: {
            int32 selection = _menu_mode->_equip_window._equip_list.GetSelection();
            GlobalWeapon *weapon = GlobalManager->GetInventoryWeapons()->at(_menu_mode->_equip_window._equip_list_inv_index.at(selection));

            equipment_name = weapon->GetName();
            physical_attribute = weapon->GetPhysicalAttack();
            magical_attribute = weapon->GetMetaphysicalAttack();

            GlobalWeapon *current_wpn = ch->GetWeaponEquipped();
            if(current_wpn) {
                current_phys_attribute = current_wpn->GetPhysicalAttack();
                current_mag_attribute = current_wpn->GetMetaphysicalAttack();
            }

            break;
        } // case EQUIP_WEAPON
        case EQUIP_HEADGEAR: {
            int32 selection = _menu_mode->_equip_window._equip_list.GetSelection();
            GlobalArmor *armor = GlobalManager->GetInventoryHeadArmor()->at(_menu_mode->_equip_window._equip_list_inv_index.at(selection));

            equipment_name = armor->GetName();
            physical_attribute = armor->GetPhysicalDefense();
            magical_attribute = armor->GetMetaphysicalDefense();

            GlobalArmor *current_armor = ch->GetHeadArmorEquipped();
            if(current_armor) {
                current_phys_attribute = current_armor->GetPhysicalDefense();
                current_mag_attribute = current_armor->GetMetaphysicalDefense();
            }

            break;
        } // case EQUIP_HEADGEAR
        case EQUIP_BODYARMOR: {
            int32 selection = _menu_mode->_equip_window._equip_list.GetSelection();
            GlobalArmor *armor = GlobalManager->GetInventoryTorsoArmor()->at(_menu_mode->_equip_window._equip_list_inv_index.at(selection));

            equipment_name = armor->GetName();
            physical_attribute = armor->GetPhysicalDefense();
            magical_attribute = armor->GetMetaphysicalDefense();

            GlobalArmor *current_armor = ch->GetTorsoArmorEquipped();
            if(current_armor) {
                current_phys_attribute = current_armor->GetPhysicalDefense();
                current_mag_attribute = current_armor->GetMetaphysicalDefense();
            }
            break;
        } // case EQUIP_BODYARMOR
        case EQUIP_OFFHAND: {
            int32 selection = _menu_mode->_equip_window._equip_list.GetSelection();
            GlobalArmor *armor = GlobalManager->GetInventoryArmArmor()->at(_menu_mode->_equip_window._equip_list_inv_index.at(selection));

            equipment_name = armor->GetName();
            physical_attribute = armor->GetPhysicalDefense();
            magical_attribute = armor->GetMetaphysicalDefense();

            GlobalArmor *current_armor = ch->GetArmArmorEquipped();
            if(current_armor) {
                current_phys_attribute = current_armor->GetPhysicalDefense();
                current_mag_attribute = current_armor->GetMetaphysicalDefense();
            }
            break;
        } // case EQUIP_OFFHAND
        case EQUIP_LEGGINGS: {
            int32 selection = _menu_mode->_equip_window._equip_list.GetSelection();
            GlobalArmor *armor = GlobalManager->GetInventoryLegArmor()->at(_menu_mode->_equip_window._equip_list_inv_index.at(selection));

            equipment_name = armor->GetName();
            physical_attribute = armor->GetPhysicalDefense();
            magical_attribute = armor->GetMetaphysicalDefense();

            GlobalArmor *current_armor = ch->GetLegArmorEquipped();
            if(current_armor) {
                current_phys_attribute = current_armor->GetPhysicalDefense();
                current_mag_attribute = current_armor->GetMetaphysicalDefense();
            }
            break;
        } // case EQUIP_LEGGINGS

        default:
            break;
        } // switch
        DrawEquipmentInfo(equipment_name, _menu_mode->_equip_window._equip_select.GetSelection() == EQUIP_WEAPON,
                          physical_attribute, magical_attribute, current_phys_attribute, current_mag_attribute);


    } // if EQUIP_ACTIVE_LIST
}

AbstractMenuState *QuestState::GetTransitionState(uint32 selection)
{
    switch(selection)
    {
        case QUEST_OPTIONS_BACK:
            return &(_menu_mode->_main_menu_state);
            break;
        case QUEST_OPTIONS_VIEW:
            _menu_mode->_quest_list_window.Activate(true);
            break;
        default:
            break;

    }
    return NULL;
}

void QuestState::Reset()
{
    // Setup the status option box
    SetupOptionBoxCommonSettings(&_options);
    _options.SetDimensions(415.0f, 50.0f, QUEST_OPTIONS_SIZE, 1, QUEST_OPTIONS_SIZE, 1);

    // Generate the strings
    std::vector<ustring> options;
    options.push_back(UTranslate("View"));
    options.push_back(UTranslate("Back"));

    // Add strings and set default selection.
    _options.SetOptions(options);
    _options.SetSelection(QUEST_OPTIONS_BACK);
}

void QuestState::_ActiveWindowUpdate()
{
    _menu_mode->_quest_window.Update();
    _menu_mode->_quest_list_window.Update();
}

bool QuestState::_IsActive()
{
    return _menu_mode->_quest_list_window.IsActive();
}

void QuestState::_OnDrawMainWindow()
{
    _DrawBottomMenu();
    _menu_mode->_quest_window.Draw();

}

void QuestState::_OnDrawSideWindow()
{
    _menu_mode->_quest_list_window.Draw();

}

void QuestState::_OnEntry(AbstractMenuState *from_state)
{
    AbstractMenuState::_OnEntry(from_state);
    //automatically go into the quest list window
    _menu_mode->_quest_list_window.Activate(true);

    //set the option cursor to view
    _options.SetSelection(QUEST_OPTIONS_VIEW);
    _options.SetCursorState(VIDEO_CURSOR_STATE_VISIBLE);

}

void QuestState::_DrawBottomMenu()
{
    _menu_mode->_bottom_window.Draw();
}

/////////////////////////////////////
// World Map State
/////////////////////////////////////

WorldMapState::WorldMapState(MenuMode *menu_mode):
    AbstractMenuState("WorldMapState", menu_mode),
    _location_image(NULL)
{
    _location_text.SetPosition(102, 556);
    _location_text.SetDimensions(500.0f, 50.0f);
    _location_text.SetTextStyle(TextStyle("title22"));
    _location_text.SetAlignment(VIDEO_X_LEFT, VIDEO_Y_CENTER);

}

void WorldMapState::Reset()
{
    // defensive update to set up the initial values such as the
    // window offset and such
    _menu_mode->_world_map_window.Update();
}

void WorldMapState::_OnDrawMainWindow()
{
    _menu_mode->_world_map_window.Draw();
    _DrawBottomMenu();
}

void WorldMapState::_DrawBottomMenu()
{
    _menu_mode->_bottom_window.Draw();
    if(_IsActive())
    {
        VideoManager->SetDrawFlags(VIDEO_X_LEFT, VIDEO_Y_BOTTOM, 0);
        VideoManager->Move(150, 580);
        // Display Location
        _location_text.Draw();
        if(_location_image != NULL && !_location_image->GetFilename().empty())
        {
            VideoManager->SetDrawFlags(VIDEO_X_RIGHT, VIDEO_Y_BOTTOM, 0);
            VideoManager->SetDrawFlags(VIDEO_X_LEFT, VIDEO_Y_BOTTOM, 0);
            VideoManager->Move(390, 685);
            _location_image->Draw();
        }

    }
}

bool WorldMapState::_IsActive()
{
    return _menu_mode->_world_map_window.IsActive();
}

void WorldMapState::_ActiveWindowUpdate()
{
    _menu_mode->_world_map_window.Update();
    if(!_IsActive())
        _OnCancel();
    else
    {
        //draw the current viewing location information
        WorldMapLocation *current_location = _menu_mode->_world_map_window.GetCurrentViewingLocation();
        if(current_location == NULL)
        {
            _location_image = NULL;
            _location_text.ClearText();
            return;
        }
        _location_text.SetDisplayText(current_location->_location_name);
        _location_image = &(current_location->_image);

    }
}

void WorldMapState::_OnEntry(AbstractMenuState *from_state)
{
    AbstractMenuState::_OnEntry(from_state);
    //automatically go into the world map window
    _menu_mode->_world_map_window.Activate(true);
}

} // namespace private_menu

bool MENU_DEBUG = false;

MenuMode *MenuMode::_current_instance = NULL;

// Window size helpers
const uint32 win_start_x = (1024 - 800) / 2 - 40;
const uint32 win_start_y = (768 - 600) / 2 + 15;
const uint32 win_width = 208;

////////////////////////////////////////////////////////////////////////////////
// MenuMode class -- Initialization and Destruction Code
////////////////////////////////////////////////////////////////////////////////

MenuMode::MenuMode() :
    _main_menu_state(this),
    _inventory_state(this),
    _party_state(this),
    _skills_state(this),
    _equip_state(this),
    _quests_state(this),
    _world_map_state(this),
    _message_window(NULL)

{
    IF_PRINT_WARNING(MENU_DEBUG)
            << "MENU: MenuMode constructor invoked." << std::endl;

    _locale_name.SetPosition(win_start_x + 40, win_start_y + 457);
    _locale_name.SetDimensions(500.0f, 50.0f);
    _locale_name.SetTextStyle(TextStyle("title22"));
    _locale_name.SetAlignment(VIDEO_X_LEFT, VIDEO_Y_CENTER);
    _locale_name.SetDisplayText(GlobalManager->GetMapHudName());

    // Initialize the location graphic
    _locale_graphic = GlobalManager->GetMapImage();
    if(!_locale_graphic.GetFilename().empty())
        _locale_graphic.SetDimensions(480, 95);
    else
        _locale_graphic.SetDimensions(0, 0);

    try {
        _saved_screen = VideoManager->CaptureScreen();
    } catch(const Exception &e) {
        PRINT_ERROR << e.ToString() << std::endl;
    }

    // Load key items related resources.
    if(!_key_item_symbol.Load("img/menus/key.png"))
        PRINT_WARNING << "Couldn't load key item symbol." << std::endl;

    _key_item_description.SetPosition(165, 600);
    _key_item_description.SetDimensions(700.0f, 50.0f);
    _key_item_description.SetTextStyle(TextStyle("text20"));
    _key_item_description.SetAlignment(VIDEO_X_LEFT, VIDEO_Y_CENTER);
    _key_item_description.SetDisplayText(UTranslate("This item is a key item and can be neither used nor sold."));

    // Load shards related resources.
    if(!_shard_symbol.Load("img/menus/shard.png"))
        PRINT_WARNING << "Couldn't load shard symbol." << std::endl;

    _shard_description.SetPosition(165, 600);
    _shard_description.SetDimensions(700.0f, 50.0f);
    _shard_description.SetTextStyle(TextStyle("text20"));
    _shard_description.SetAlignment(VIDEO_X_LEFT, VIDEO_Y_CENTER);
    _shard_description.SetDisplayText(UTranslate("This item is a crystal shard and can be associated with equipment."));

    _help_information.SetPosition(250, 570);
    _help_information.SetDimensions(500, 100);
    _help_information.SetTextStyle(TextStyle("text20"));
    _help_information.SetDisplayMode(VIDEO_TEXT_INSTANT);
    _help_information.SetAlignment(VIDEO_X_LEFT, VIDEO_Y_CENTER);
    _help_information.SetTextAlignment(VIDEO_X_LEFT, VIDEO_Y_CENTER);

    //////////// Setup the menu windows
    // The character windows
    ReloadCharacterWindows();

    // The bottom window for the menu
    _bottom_window.Create(static_cast<float>(win_width * 4 + 16), 140 + 16, VIDEO_MENU_EDGE_ALL);
    _bottom_window.SetPosition(static_cast<float>(win_start_x), static_cast<float>(win_start_y + 442));

    _main_options_window.Create(static_cast<float>(win_width * 4 + 16), 60, ~VIDEO_MENU_EDGE_BOTTOM, VIDEO_MENU_EDGE_BOTTOM);
    _main_options_window.SetPosition(static_cast<float>(win_start_x), static_cast<float>(win_start_y - 50));

    // Set up the party window
    _party_window.Create(static_cast<float>(win_width * 4 + 16), 448, VIDEO_MENU_EDGE_ALL);
    _party_window.SetPosition(static_cast<float>(win_start_x), static_cast<float>(win_start_y + 10));

    //Set up the skills window
    _skills_window.Create(static_cast<float>(win_width * 4 + 16), 448, VIDEO_MENU_EDGE_ALL);
    _skills_window.SetPosition(static_cast<float>(win_start_x), static_cast<float>(win_start_y + 10));

    //Set up the equipment window
    _equip_window.Create(static_cast<float>(win_width * 4 + 16), 448, VIDEO_MENU_EDGE_ALL);
    _equip_window.SetPosition(static_cast<float>(win_start_x), static_cast<float>(win_start_y + 10));

    // Set up the inventory window
    _inventory_window.Create(static_cast<float>(win_width * 4 + 16), 448, VIDEO_MENU_EDGE_ALL);
    _inventory_window.SetPosition(static_cast<float>(win_start_x), static_cast<float>(win_start_y + 10));

    //set up the quest window
    _quest_window.Create(static_cast<float>(win_width * 4 + 16), 448, VIDEO_MENU_EDGE_ALL);
    _quest_window.SetPosition(static_cast<float>(win_start_x), static_cast<float>(win_start_y + 10));

    //set up the quest list window
    _quest_list_window.Create(360,448, VIDEO_MENU_EDGE_ALL, VIDEO_MENU_EDGE_TOP | VIDEO_MENU_EDGE_BOTTOM);
    _quest_list_window.SetPosition(static_cast<float>(win_start_x), static_cast<float>(win_start_y + 10));

    //set up the world map window
    _world_map_window.Create(static_cast<float>(win_width * 4 + 16), 448, VIDEO_MENU_EDGE_ALL);
    _world_map_window.SetPosition(static_cast<float>(win_start_x), static_cast<float>(win_start_y + 10));

    _current_menu_state = &_main_menu_state;

    // Load menu sounds
    _menu_sounds["confirm"] = SoundDescriptor();
    _menu_sounds["confirm"].LoadAudio("snd/confirm.wav");
    _menu_sounds["cancel"] = SoundDescriptor();
    _menu_sounds["cancel"].LoadAudio("snd/cancel.wav");
    _menu_sounds["bump"] = SoundDescriptor();
    _menu_sounds["bump"].LoadAudio("snd/bump.wav");

    if(_current_instance != NULL) {
        IF_PRINT_WARNING(MENU_DEBUG)
                << "MENU WARNING: attempting to create a new instance of MenuMode when one already seems to exist" << std::endl;
    }
    _current_instance = this;
} // MenuMode::MenuMode()



MenuMode::~MenuMode()
{
    IF_PRINT_WARNING(MENU_DEBUG)
            << "MENU: MenuMode destructor invoked." << std::endl;

    // Destroy all menu windows
    _bottom_window.Destroy();
    _character_window0.Destroy();
    _character_window1.Destroy();
    _character_window2.Destroy();
    _character_window3.Destroy();
    _inventory_window.Destroy();
    _quest_list_window.Destroy();
    _party_window.Destroy();
    _skills_window.Destroy();
    _main_options_window.Destroy();
    _equip_window.Destroy();
    _quest_window.Destroy();
    _world_map_window.Destroy();

    // Free sounds
    _menu_sounds["confirm"].FreeAudio();
    _menu_sounds["bump"].FreeAudio();
    _menu_sounds["cancel"].FreeAudio();

    _current_instance = NULL;

    if(_message_window != NULL)
        delete _message_window;
} // MenuMode::~MenuMode()


// Resets configuration/data for the class as appropriate
void MenuMode::Reset()
{
    // Top left corner coordinates in menu mode are always (0,0)
    VideoManager->SetStandardCoordSys();

    // Show all windows (make them visible)
    _bottom_window.Show();
    _main_options_window.Show();
    _character_window0.Show();
    _character_window1.Show();
    _character_window2.Show();
    _character_window3.Show();
    _inventory_window.Show();
    _quest_list_window.Show();
    _party_window.Show();
    _skills_window.Show();
    _equip_window.Show();
    _quest_window.Show();
    _world_map_window.Show();

    _inventory_window.Activate(false);

    // Reset states
    _main_menu_state.Reset();
    _inventory_state.Reset();
    _party_state.Reset();
    _skills_state.Reset();
    _equip_state.Reset();
    _quests_state.Reset();

    // set initial state to main menu
    _current_menu_state = &_main_menu_state;

} // void MenuMode::Reset()

////////////////////////////////////////////////////////////////////////////////
// MenuMode class -- Update Code
////////////////////////////////////////////////////////////////////////////////

void MenuMode::Update()
{
    if(InputManager->QuitPress() == true) {
        ModeManager->Push(new PauseMode(true));
        return;
    } else if(InputManager->PausePress() == true) {
        ModeManager->Push(new PauseMode(false));
        return;
    }

    // check the message window
    if(_message_window != NULL) {
        _message_window->Update();
        if(InputManager->ConfirmPress() || InputManager->CancelPress()) {
            delete _message_window;
            _message_window = NULL;
        }
        return;
    }

    _current_menu_state->Update();

} // void MenuMode::Update()

////////////////////////////////////////////////////////////////////////////////
// MenuMode class -- Draw Code
////////////////////////////////////////////////////////////////////////////////
void MenuMode::Draw()
{
    _current_menu_state->Draw();

    if(_message_window != NULL)
        _message_window->Draw();
} // void MenuMode::Draw()


void MenuMode::ReloadCharacterWindows()
{
    GlobalParty &characters = *GlobalManager->GetActiveParty();

    // Setup character windows based on active party size
    switch(characters.GetPartySize()) {
    case 4:
        _character_window3.SetCharacter(dynamic_cast<GlobalCharacter *>(characters.GetActorAtIndex(3)));
    case 3:
        _character_window2.SetCharacter(dynamic_cast<GlobalCharacter *>(characters.GetActorAtIndex(2)));
    case 2:
        _character_window1.SetCharacter(dynamic_cast<GlobalCharacter *>(characters.GetActorAtIndex(1)));
    case 1:
        _character_window0.SetCharacter(dynamic_cast<GlobalCharacter *>(characters.GetActorAtIndex(0)));
        break;
    default:
        PRINT_ERROR << "No characters in party!" << std::endl;
        ModeManager->Pop();
        break;
    }

    // Width of each character window is 360 px.
    // Each char window will have an additional 16 px for the left border
    // The 4th (last) char window will have another 16 px for the right border
    // Height of the char window is 98 px.
    // The bottom window in the main view is 192 px high, and the full width which will be 216 * 4 + 16
    _character_window0.Create(360, 98, ~VIDEO_MENU_EDGE_BOTTOM, VIDEO_MENU_EDGE_BOTTOM);
    _character_window0.SetPosition(static_cast<float>(win_start_x), static_cast<float>(win_start_y + 10));

    _character_window1.Create(360, 98, ~VIDEO_MENU_EDGE_BOTTOM, VIDEO_MENU_EDGE_BOTTOM | VIDEO_MENU_EDGE_TOP);
    _character_window1.SetPosition(static_cast<float>(win_start_x), static_cast<float>(win_start_y + 118));

    _character_window2.Create(360, 98, ~VIDEO_MENU_EDGE_BOTTOM, VIDEO_MENU_EDGE_BOTTOM | VIDEO_MENU_EDGE_TOP);
    _character_window2.SetPosition(static_cast<float>(win_start_x), static_cast<float>(win_start_y + 226));

    _character_window3.Create(360, 98, ~VIDEO_MENU_EDGE_BOTTOM, VIDEO_MENU_EDGE_TOP | VIDEO_MENU_EDGE_BOTTOM);
    _character_window3.SetPosition(static_cast<float>(win_start_x), static_cast<float>(win_start_y + 334));
}

} // namespace hoa_menu
