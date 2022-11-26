#include "KeybindingState.h"

#include <array>
#include <unordered_map>

/*
    This code ensures that if ImGuiKey enum values change everything still works correctly...
*/

enum OFS_Key : int32_t
{
    OFS_Key_None = 0,
    OFS_Key_Tab = 512,
    OFS_Key_LeftArrow,
    OFS_Key_RightArrow,
    OFS_Key_UpArrow,
    OFS_Key_DownArrow,
    OFS_Key_PageUp,
    OFS_Key_PageDown,
    OFS_Key_Home,
    OFS_Key_End,
    OFS_Key_Insert,
    OFS_Key_Delete,
    OFS_Key_Backspace,
    OFS_Key_Space,
    OFS_Key_Enter,
    OFS_Key_Escape,
    OFS_Key_LeftCtrl, OFS_Key_LeftShift, OFS_Key_LeftAlt, OFS_Key_LeftSuper,
    OFS_Key_RightCtrl, OFS_Key_RightShift, OFS_Key_RightAlt, OFS_Key_RightSuper,
    OFS_Key_Menu,
    OFS_Key_0, OFS_Key_1, OFS_Key_2, OFS_Key_3, OFS_Key_4, OFS_Key_5, OFS_Key_6, OFS_Key_7, OFS_Key_8, OFS_Key_9,
    OFS_Key_A, OFS_Key_B, OFS_Key_C, OFS_Key_D, OFS_Key_E, OFS_Key_F, OFS_Key_G, OFS_Key_H, OFS_Key_I, OFS_Key_J,
    OFS_Key_K, OFS_Key_L, OFS_Key_M, OFS_Key_N, OFS_Key_O, OFS_Key_P, OFS_Key_Q, OFS_Key_R, OFS_Key_S, OFS_Key_T,
    OFS_Key_U, OFS_Key_V, OFS_Key_W, OFS_Key_X, OFS_Key_Y, OFS_Key_Z,
    OFS_Key_F1, OFS_Key_F2, OFS_Key_F3, OFS_Key_F4, OFS_Key_F5, OFS_Key_F6,
    OFS_Key_F7, OFS_Key_F8, OFS_Key_F9, OFS_Key_F10, OFS_Key_F11, OFS_Key_F12,
    OFS_Key_Apostrophe,        
    OFS_Key_Comma,             
    OFS_Key_Minus,             
    OFS_Key_Period,            
    OFS_Key_Slash,             
    OFS_Key_Semicolon,         
    OFS_Key_Equal,             
    OFS_Key_LeftBracket,       
    OFS_Key_Backslash,         
    OFS_Key_RightBracket,      
    OFS_Key_GraveAccent,       
    OFS_Key_CapsLock,
    OFS_Key_ScrollLock,
    OFS_Key_NumLock,
    OFS_Key_PrintScreen,
    OFS_Key_Pause,
    OFS_Key_Keypad0, OFS_Key_Keypad1, OFS_Key_Keypad2, OFS_Key_Keypad3, OFS_Key_Keypad4,
    OFS_Key_Keypad5, OFS_Key_Keypad6, OFS_Key_Keypad7, OFS_Key_Keypad8, OFS_Key_Keypad9,
    OFS_Key_KeypadDecimal,
    OFS_Key_KeypadDivide,
    OFS_Key_KeypadMultiply,
    OFS_Key_KeypadSubtract,
    OFS_Key_KeypadAdd,
    OFS_Key_KeypadEnter,
    OFS_Key_KeypadEqual,

    OFS_Key_GamepadStart,      
    OFS_Key_GamepadBack,       
    OFS_Key_GamepadFaceLeft,   
    OFS_Key_GamepadFaceRight,  
    OFS_Key_GamepadFaceUp,     
    OFS_Key_GamepadFaceDown,   
    OFS_Key_GamepadDpadLeft,   
    OFS_Key_GamepadDpadRight,  
    OFS_Key_GamepadDpadUp,     
    OFS_Key_GamepadDpadDown,   
    OFS_Key_GamepadL1,         
    OFS_Key_GamepadR1,         
    OFS_Key_GamepadL2,         
    OFS_Key_GamepadR2,         
    OFS_Key_GamepadL3,         
    OFS_Key_GamepadR3,         
    OFS_Key_GamepadLStickLeft, 
    OFS_Key_GamepadLStickRight,
    OFS_Key_GamepadLStickUp,   
    OFS_Key_GamepadLStickDown, 
    OFS_Key_GamepadRStickLeft, 
    OFS_Key_GamepadRStickRight,
    OFS_Key_GamepadRStickUp,   
    OFS_Key_GamepadRStickDown, 

    OFS_Key_MouseLeft, OFS_Key_MouseRight, OFS_Key_MouseMiddle, OFS_Key_MouseX1, OFS_Key_MouseX2, OFS_Key_MouseWheelX, OFS_Key_MouseWheelY,
};

enum OFS_KeyMod : int32_t
{
    OFS_KeyMod_None  = 0,
    OFS_KeyMod_Ctrl  = 1 << 12,
    OFS_KeyMod_Shift = 1 << 13,
    OFS_KeyMod_Alt   = 1 << 14,
    OFS_KeyMod_Super = 1 << 15,
    OFS_KeyMod_Mask_ = 0xF000,
};

static std::unordered_map<OFS_Key, ImGuiKey> GetOfsToImGuiMap() noexcept
{
    std::unordered_map<OFS_Key, ImGuiKey> mapping = 
    {
        {OFS_Key_None, ImGuiKey_None}, 
        {OFS_Key_Tab, ImGuiKey_Tab}, 
        {OFS_Key_LeftArrow, ImGuiKey_LeftArrow}, 
        {OFS_Key_RightArrow, ImGuiKey_RightArrow}, 
        {OFS_Key_UpArrow, ImGuiKey_UpArrow}, 
        {OFS_Key_DownArrow, ImGuiKey_DownArrow}, 
        {OFS_Key_PageUp, ImGuiKey_PageUp}, 
        {OFS_Key_PageDown, ImGuiKey_PageDown}, 
        {OFS_Key_Home, ImGuiKey_Home}, 
        {OFS_Key_End, ImGuiKey_End}, 
        {OFS_Key_Insert, ImGuiKey_Insert}, 
        {OFS_Key_Delete, ImGuiKey_Delete}, 
        {OFS_Key_Backspace, ImGuiKey_Backspace}, 
        {OFS_Key_Space, ImGuiKey_Space}, 
        {OFS_Key_Enter, ImGuiKey_Enter}, 
        {OFS_Key_Escape, ImGuiKey_Escape}, 
        {OFS_Key_LeftCtrl, ImGuiKey_LeftCtrl}, 
        {OFS_Key_LeftShift, ImGuiKey_LeftShift}, 
        {OFS_Key_LeftAlt, ImGuiKey_LeftAlt}, 
        {OFS_Key_LeftSuper, ImGuiKey_LeftSuper}, 
        {OFS_Key_RightCtrl, ImGuiKey_RightCtrl}, 
        {OFS_Key_RightShift, ImGuiKey_RightShift}, 
        {OFS_Key_RightAlt, ImGuiKey_RightAlt}, 
        {OFS_Key_RightSuper, ImGuiKey_RightSuper}, 
        {OFS_Key_Menu, ImGuiKey_Menu}, 
        {OFS_Key_0, ImGuiKey_0}, 
        {OFS_Key_1, ImGuiKey_1}, 
        {OFS_Key_2, ImGuiKey_2}, 
        {OFS_Key_3, ImGuiKey_3}, 
        {OFS_Key_4, ImGuiKey_4}, 
        {OFS_Key_5, ImGuiKey_5}, 
        {OFS_Key_6, ImGuiKey_6}, 
        {OFS_Key_7, ImGuiKey_7}, 
        {OFS_Key_8, ImGuiKey_8}, 
        {OFS_Key_9, ImGuiKey_9}, 
        {OFS_Key_A, ImGuiKey_A}, 
        {OFS_Key_B, ImGuiKey_B}, 
        {OFS_Key_C, ImGuiKey_C}, 
        {OFS_Key_D, ImGuiKey_D}, 
        {OFS_Key_E, ImGuiKey_E}, 
        {OFS_Key_F, ImGuiKey_F}, 
        {OFS_Key_G, ImGuiKey_G}, 
        {OFS_Key_H, ImGuiKey_H}, 
        {OFS_Key_I, ImGuiKey_I}, 
        {OFS_Key_J, ImGuiKey_J}, 
        {OFS_Key_K, ImGuiKey_K}, 
        {OFS_Key_L, ImGuiKey_L}, 
        {OFS_Key_M, ImGuiKey_M}, 
        {OFS_Key_N, ImGuiKey_N}, 
        {OFS_Key_O, ImGuiKey_O}, 
        {OFS_Key_P, ImGuiKey_P}, 
        {OFS_Key_Q, ImGuiKey_Q}, 
        {OFS_Key_R, ImGuiKey_R}, 
        {OFS_Key_S, ImGuiKey_S}, 
        {OFS_Key_T, ImGuiKey_T}, 
        {OFS_Key_U, ImGuiKey_U}, 
        {OFS_Key_V, ImGuiKey_V}, 
        {OFS_Key_W, ImGuiKey_W}, 
        {OFS_Key_X, ImGuiKey_X}, 
        {OFS_Key_Y, ImGuiKey_Y}, 
        {OFS_Key_Z, ImGuiKey_Z}, 
        {OFS_Key_F1, ImGuiKey_F1}, 
        {OFS_Key_F2, ImGuiKey_F2}, 
        {OFS_Key_F3, ImGuiKey_F3}, 
        {OFS_Key_F4, ImGuiKey_F4}, 
        {OFS_Key_F5, ImGuiKey_F5}, 
        {OFS_Key_F6, ImGuiKey_F6}, 
        {OFS_Key_F7, ImGuiKey_F7}, 
        {OFS_Key_F8, ImGuiKey_F8}, 
        {OFS_Key_F9, ImGuiKey_F9}, 
        {OFS_Key_F10, ImGuiKey_F10}, 
        {OFS_Key_F11, ImGuiKey_F11}, 
        {OFS_Key_F12, ImGuiKey_F12}, 
        {OFS_Key_Apostrophe, ImGuiKey_Apostrophe},         
        {OFS_Key_Comma, ImGuiKey_Comma},              
        {OFS_Key_Minus, ImGuiKey_Minus},              
        {OFS_Key_Period, ImGuiKey_Period},             
        {OFS_Key_Slash, ImGuiKey_Slash},              
        {OFS_Key_Semicolon, ImGuiKey_Semicolon},          
        {OFS_Key_Equal, ImGuiKey_Equal},              
        {OFS_Key_LeftBracket, ImGuiKey_LeftBracket},        
        {OFS_Key_Backslash, ImGuiKey_Backslash},          
        {OFS_Key_RightBracket, ImGuiKey_RightBracket},       
        {OFS_Key_GraveAccent, ImGuiKey_GraveAccent},        
        {OFS_Key_CapsLock, ImGuiKey_CapsLock}, 
        {OFS_Key_ScrollLock, ImGuiKey_ScrollLock}, 
        {OFS_Key_NumLock, ImGuiKey_NumLock}, 
        {OFS_Key_PrintScreen, ImGuiKey_PrintScreen}, 
        {OFS_Key_Pause, ImGuiKey_Pause}, 
        {OFS_Key_Keypad0, ImGuiKey_Keypad0}, 
        {OFS_Key_Keypad1, ImGuiKey_Keypad1}, 
        {OFS_Key_Keypad2, ImGuiKey_Keypad2}, 
        {OFS_Key_Keypad3, ImGuiKey_Keypad3}, 
        {OFS_Key_Keypad4, ImGuiKey_Keypad4}, 
        {OFS_Key_Keypad5, ImGuiKey_Keypad5}, 
        {OFS_Key_Keypad6, ImGuiKey_Keypad6}, 
        {OFS_Key_Keypad7, ImGuiKey_Keypad7}, 
        {OFS_Key_Keypad8, ImGuiKey_Keypad8}, 
        {OFS_Key_Keypad9, ImGuiKey_Keypad9}, 
        {OFS_Key_KeypadDecimal, ImGuiKey_KeypadDecimal}, 
        {OFS_Key_KeypadDivide, ImGuiKey_KeypadDivide}, 
        {OFS_Key_KeypadMultiply, ImGuiKey_KeypadMultiply}, 
        {OFS_Key_KeypadSubtract, ImGuiKey_KeypadSubtract}, 
        {OFS_Key_KeypadAdd, ImGuiKey_KeypadAdd}, 
        {OFS_Key_KeypadEnter, ImGuiKey_KeypadEnter}, 
        {OFS_Key_KeypadEqual, ImGuiKey_KeypadEqual}, 
        {OFS_Key_GamepadStart, ImGuiKey_GamepadStart},       
        {OFS_Key_GamepadBack, ImGuiKey_GamepadBack},        
        {OFS_Key_GamepadFaceLeft, ImGuiKey_GamepadFaceLeft},    
        {OFS_Key_GamepadFaceRight, ImGuiKey_GamepadFaceRight},   
        {OFS_Key_GamepadFaceUp, ImGuiKey_GamepadFaceUp},      
        {OFS_Key_GamepadFaceDown, ImGuiKey_GamepadFaceDown},    
        {OFS_Key_GamepadDpadLeft, ImGuiKey_GamepadDpadLeft},    
        {OFS_Key_GamepadDpadRight, ImGuiKey_GamepadDpadRight},   
        {OFS_Key_GamepadDpadUp, ImGuiKey_GamepadDpadUp},      
        {OFS_Key_GamepadDpadDown, ImGuiKey_GamepadDpadDown},    
        {OFS_Key_GamepadL1, ImGuiKey_GamepadL1},          
        {OFS_Key_GamepadR1, ImGuiKey_GamepadR1},          
        {OFS_Key_GamepadL2, ImGuiKey_GamepadL2},          
        {OFS_Key_GamepadR2, ImGuiKey_GamepadR2},          
        {OFS_Key_GamepadL3, ImGuiKey_GamepadL3},          
        {OFS_Key_GamepadR3, ImGuiKey_GamepadR3},          
        {OFS_Key_GamepadLStickLeft, ImGuiKey_GamepadLStickLeft},  
        {OFS_Key_GamepadLStickRight, ImGuiKey_GamepadLStickRight}, 
        {OFS_Key_GamepadLStickUp, ImGuiKey_GamepadLStickUp},    
        {OFS_Key_GamepadLStickDown, ImGuiKey_GamepadLStickDown},  
        {OFS_Key_GamepadRStickLeft, ImGuiKey_GamepadRStickLeft},  
        {OFS_Key_GamepadRStickRight, ImGuiKey_GamepadRStickRight}, 
        {OFS_Key_GamepadRStickUp, ImGuiKey_GamepadRStickUp},    
        {OFS_Key_GamepadRStickDown, ImGuiKey_GamepadRStickDown},  
        {OFS_Key_MouseLeft, ImGuiKey_MouseLeft}, 
        {OFS_Key_MouseRight, ImGuiKey_MouseRight}, 
        {OFS_Key_MouseMiddle, ImGuiKey_MouseMiddle}, 
        {OFS_Key_MouseX1, ImGuiKey_MouseX1}, 
        {OFS_Key_MouseX2, ImGuiKey_MouseX2}, 
        {OFS_Key_MouseWheelX, ImGuiKey_MouseWheelX}, 
        {OFS_Key_MouseWheelY, ImGuiKey_MouseWheelY}, 
    };
    return mapping;
}
static std::unordered_map<ImGuiKey, OFS_Key> GetImGuiToOfsMap() noexcept
{
    auto ofsToIm = GetOfsToImGuiMap();
    std::unordered_map<ImGuiKey, OFS_Key> mapping;
    mapping.reserve(ofsToIm.size());
    for(auto& pair : ofsToIm)
    {
        mapping.insert(std::make_pair(pair.second, pair.first));
    }
    return mapping;
}

static int32_t ConvertModsToOfs(int32_t mods) noexcept
{
    int32_t ofsMods = 0;
    if(mods & ImGuiMod_Ctrl)
        ofsMods |= OFS_KeyMod_Ctrl;
    if(mods & ImGuiMod_Shift) 
        ofsMods |= OFS_KeyMod_Shift;
    if(mods & ImGuiMod_Alt)
        ofsMods |= OFS_KeyMod_Alt;
    if(mods & ImGuiMod_Super)
        ofsMods |= OFS_KeyMod_Super;
    return ofsMods;
}

static int32_t ConvertModsToImGui(int32_t mods) noexcept
{
    int32_t imMods = 0;
    if(mods & OFS_KeyMod_Ctrl)
        imMods |= ImGuiMod_Ctrl;
    if(mods & OFS_KeyMod_Shift) 
        imMods |= ImGuiMod_Shift;
    if(mods & OFS_KeyMod_Alt)
        imMods |= ImGuiMod_Alt;
    if(mods & OFS_KeyMod_Super)
        imMods |= ImGuiMod_Super;
    return imMods;
}

void OFS_KeybindingState::ConvertToOFS() noexcept
{
    if(!convertedToImGui) return;
    auto mapping = GetImGuiToOfsMap();
    for(auto& trigger : Triggers)
    {
        auto it = mapping.find(trigger.ImKey());
        FUN_ASSERT(it != mapping.end(), "Couldn't find mapping for key.");
        if(it != mapping.end())
        {
            trigger.Key = it->second;
            trigger.Mod = ConvertModsToOfs(trigger.Mod);
        }
        else 
        {
            // This shouldn't happen
            LOGF_ERROR("Couldn't find mapping for key. Trigger for \"%s\" has been reset.", trigger.MappedActionId.c_str());
            trigger.Key = OFS_Key_None;
            trigger.Mod = OFS_KeyMod_None;
        }
    }
    Triggers.sort();
    convertedToImGui = false;
}

void OFS_KeybindingState::ConvertToImGui() noexcept
{
    if(convertedToImGui) return;
    auto mapping = GetOfsToImGuiMap();
    for(auto& trigger : Triggers)
    {
        auto it = mapping.find(static_cast<OFS_Key>(trigger.Key));
        FUN_ASSERT(it != mapping.end(), "Couldn't find mapping for key.");
        if(it != mapping.end())
        {
            trigger.Key = it->second;
            trigger.Mod = ConvertModsToImGui(trigger.Mod);
        }
        else 
        {
            // This shouldn't happen
            LOGF_ERROR("Couldn't find mapping for key. Trigger for \"%s\" has been reset.", trigger.MappedActionId.c_str());
            trigger.Key = ImGuiKey_None;
            trigger.Mod = ImGuiMod_None;
        }
    }
    Triggers.sort();
    convertedToImGui = true;
}