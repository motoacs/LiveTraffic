/// @file       ACInfoWnd.cpp
/// @brief      Aircraft information window showing details for a selected aircraft
/// @author     Birger Hoppe
/// @copyright  (c) 2018-2020 Birger Hoppe
/// @copyright  Permission is hereby granted, free of charge, to any person obtaining a
///             copy of this software and associated documentation files (the "Software"),
///             to deal in the Software without restriction, including without limitation
///             the rights to use, copy, modify, merge, publish, distribute, sublicense,
///             and/or sell copies of the Software, and to permit persons to whom the
///             Software is furnished to do so, subject to the following conditions:\n
///             The above copyright notice and this permission notice shall be included in
///             all copies or substantial portions of the Software.\n
///             THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
///             IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
///             FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
///             AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
///             LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
///             OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
///             THE SOFTWARE.

#include "LiveTraffic.h"

//
// MARK: ACIWnd Implementation
//

/// Initial size of an A/c Info Window (XP coordinates: l,t;r,b with t > b)
const WndRect ACI_INIT_SIZE = WndRect(0, 500, 320, 0);
/// Resizing limits (minimum and maximum sizes)
const WndRect ACI_RESIZE_LIMITS = WndRect(200, 200, 640, 9999);

constexpr float ACI_AUTO_CHECK_PERIOD = 1.00f;  ///< How often to check for AUTO a/c change? [s]
constexpr float ACI_TREE_V_SEP        =10.00f;  ///< Separation between tree sections
constexpr float ACI_STD_FONT_SCALE    = 0.85f;  ///< Standard font scaling
constexpr float ACI_STD_TRANSPARENCY  = 0.30f;  ///< Standard background transparency

static float ACI_LABEL_SIZE = NAN;              ///< Width of first column, which displays static labels
static float ACI_AUTO_CB_SIZE = NAN;            ///< Width of AUTO checkbox

// Constructor shows a window for the given a/c key
ACIWnd::ACIWnd(const std::string& _acKey, WndMode _mode) :
LTImgWindow(_mode, WND_STYLE_HUD, ACI_INIT_SIZE),
bAuto(_acKey.empty()),              // if _acKey empty -> AUTO mode
keyEntry(_acKey)                    // the passed-in input is taken as the user's entry
{
    // Set up window basics
    SetWindowTitle(GetWndTitle());
    SetWindowResizingLimits(ACI_RESIZE_LIMITS.tl.x, ACI_RESIZE_LIMITS.tl.y,
                            ACI_RESIZE_LIMITS.br.x, ACI_RESIZE_LIMITS.br.y);
    SetVisible(true);
    
    // Add myself to the list of ACI windows
    listACIWnd.push_back(this);
    
    // Search for a matching a/c
    if (bAuto)
        UpdateFocusAc();
    else
        SearchAndSetFlightData();
}

// Desctructor cleans up
ACIWnd::~ACIWnd()
{
    // remove myself from the list of windows
    listACIWnd.remove(this);
}

// Set the a/c key - no validation, if invalid window will clear
void ACIWnd::SetAcKey (const LTFlightData::FDKeyTy& _key)
{
    keyEntry = (acKey = _key);      // remember the key
    SetWindowTitle(GetWndTitle());  // set the window's title
    ReturnKeyboardFocus();          // give up keyboard focus in case we had it
}

// Clear the a/c key, ie. display no data
void ACIWnd::ClearAcKey ()
{
    acKey.clear();
    keyEntry.clear();
    SetWindowTitle(GetWndTitle());
}

// Set AUTO mode
void ACIWnd::SetAuto (bool _b)
{
    bAuto = _b;
    if (!_b)
        lastAutoCheck = 0.0f;
    else
        UpdateFocusAc();
}

// Return the text to be used as window title
std::string ACIWnd::GetWndTitle () const
{
    return
    (acKey.empty() ? std::string(ACI_WND_TITLE) : std::string(acKey)) +
    (bAuto ? " (AUTO)" : "");
}

// Taking user's temporary input `keyEntry` searches for a valid a/c, sets acKey on success
bool ACIWnd::SearchAndSetFlightData ()
{
    mapLTFlightDataTy::const_iterator fdIter = mapFd.cend();
    
    trim(keyEntry);
    if (!keyEntry.empty()) {
        // is it a small integer number, i.e. used as index?
        if (keyEntry.length() <= 3 &&
            keyEntry.find_first_not_of("0123456789") == std::string::npos)
        {
            int i = std::stoi(keyEntry);
            // let's find the i-th aircraft by looping over all flight data
            // and count those objects, which have an a/c
            if (i > 0) for (fdIter = mapFd.cbegin();
                 fdIter != mapFd.cend();
                 ++fdIter)
            {
                if (fdIter->second.hasAc())         // has an a/c
                    if ( --i == 0 )                 // and it's the i-th!
                        break;
            }
        }
        else
        {
            // search the map of flight data by text key
            fdIter =
            std::find_if(mapFd.cbegin(), mapFd.cend(),
                         [&](const mapLTFlightDataTy::value_type& mfd)
                         { return mfd.second.IsMatch(keyEntry); }
                         );
        }
    }
    
    // found?
    if (fdIter != mapFd.cend()) {
        SetAcKey(fdIter->second.key());         // save the a/c key so we can start rendering its info
        return true;
    }
    
    // not found
    acKey.clear();
    return false;
}

// Get my defined aircraft
/// @details As aircraft can be removed any frame this needs to be called
///          over and over again and can return `nullptr`.
/// @note Deleting aircraft happens in a flight loop callback,
///       which is the same thread as this here is running in.
///       So we can safely assume the returned pointer is valid while
///       rendering the window. But not any longer.
LTFlightData* ACIWnd::GetFlightData () const
{
    // short-cut if there's no key
    if (acKey.empty())
        return nullptr;
    
    // find the flight data by key
    mapLTFlightDataTy::iterator fdIter = mapFd.find(acKey);
    // return flight data if found
    return fdIter != mapFd.end() ? &fdIter->second : nullptr;
}

// switch to another focus a/c?
bool ACIWnd::UpdateFocusAc ()
{
    // just return if not in AUTO mode
    if (!bAuto) return false;
    
    // do that only every so often
    if (dataRefs.GetMiscNetwTime() < lastAutoCheck + ACI_AUTO_CHECK_PERIOD)
        return false;
    lastAutoCheck = dataRefs.GetMiscNetwTime();
    
    // find the current focus a/c and if different from current one then switch
    const LTFlightData* pFocusAc = LTFlightData::FindFocusAc(DataRefs::GetViewHeading());
    if (pFocusAc && pFocusAc->key() != acKey) {
        SetAcKey(pFocusAc->key());      // set the new focus a/c
        return true;
    }
    
    // nothing found?
    if (!pFocusAc) {
        // Clear the a/c key
        ClearAcKey();
    }
    
    // no new a/c
    return false;
}

// Some setup before UI building starts, here text size calculations
ImGuiWindowFlags_ ACIWnd::beforeBegin()
{
    // If not yet donw calculate some common widths
    if (std::isnan(ACI_LABEL_SIZE)) {
        /// Size of longest text plus some room for tree indenttion, rounded up to the next 10
        ImGui::SetWindowFontScale(1.0f);
        ACI_LABEL_SIZE   = std::ceil(ImGui::CalcTextSize("___Call Sign | Squawk_").x / 10.0f) * 10.0f;
        ACI_AUTO_CB_SIZE = std::ceil(ImGui::CalcTextSize("_____AUTO").x / 10.0f) * 10.0f;
    }
    
    // Set background transparency
    ImGuiStyle& style = ImGui::GetStyle();
    style.Colors[ImGuiCol_WindowBg] = ImColor(0.0f, 0.0f, 0.0f, fTransparency);
    
    return ImGuiWindowFlags_None;
}

// Main function to render the window's interface
void ACIWnd::buildInterface()
{
    // (maybe) update the focus a/c
    UpdateFocusAc();
    
    // Scale the font for this window
    ImGui::SetWindowFontScale(fFontScale);
    
    // --- Title Bar ---
    buildTitleBar(GetWndTitle());
    
    // --- Start the table, which will hold our values
    if (ImGui::BeginTable("ACInfo", 2,
                          ImGuiTableFlags_ScrollY |
                          ImGuiTableFlags_ScrollFreezeLeftColumn))
    {
        // The data we will deal with, can be NULL!
        const double ts = dataRefs.GetSimTime();
        const LTFlightData* pFD = GetFlightData();
        const LTAircraft* pAc   = pFD ? pFD->GetAircraft() : nullptr;
        // Try fetching fresh static / dynamic data
        if (pFD) {
            pFD->TryGetSafeCopy(stat);
            pFD->TryGetSafeCopy(dyn);
        }
        const Doc8643* pDoc8643 = pFD ? stat.pDoc8643 : nullptr;
        const LTChannel* pChannel = pFD ? dyn.pChannel : nullptr;

        // Set up the columns of the table
        ImGui::TableSetupColumn("Item",  ImGuiTableColumnFlags_WidthFixed   | ImGuiTableColumnFlags_NoSort, ACI_LABEL_SIZE * fFontScale);
        ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch | ImGuiTableColumnFlags_NoSort);
    
        // --- Identification ---
        ImGui::TableNextRow();
        bool bOpen = ImGui::TreeNodeEx("A/C key", ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_SpanFullWidth);
        ImGui::TableNextCell();
        if (ImGui::BeginTable("KeyOrAUTO", 2))
        {
            ImGui::TableSetupColumn("Edit", ImGuiTableColumnFlags_WidthStretch | ImGuiTableColumnFlags_NoSort);
            ImGui::TableSetupColumn("Auto", ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoSort, ACI_AUTO_CB_SIZE * fFontScale);
            ImGui::TableNextRow();
            if (ImGui::InputText("##NewKey", &keyEntry,
                                 ImGuiInputTextFlags_CharsUppercase |
                                 ImGuiInputTextFlags_CharsNoBlank |
                                 ImGuiInputTextFlags_EnterReturnsTrue))
            {
                // Enter pressed in key entry field
                bAuto = false;
                SearchAndSetFlightData();
            }
            ImGui::TableNextCell();
            if (ImGui::Checkbox("AUTO", &bAuto))
                lastAutoCheck = 0.0f;       // enforce search for a/c next frame
            
            ImGui::EndTable();
        }
        
        if (bOpen) {
            buildRow("Registration",    stat.reg,           pFD);
            buildRow("ICAO Type",       stat.acTypeIcao,    pFD);
            buildRow("ICAO Class",      pDoc8643 ? pDoc8643->classification : "-", pDoc8643);
            buildRow("Manufacturer",    stat.man,           pFD);
            buildRow("Model",           stat.mdl,           pFD);
            buildRow("Operator",
                     stat.opIcao.empty() ? stat.op : stat.opIcao + ": " + stat.op,
                     pFD);

            // end of the tree
            ImGui::TreePop();
        }
        
        // --- Flight Info / Tracking data ---
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + ACI_TREE_V_SEP*fFontScale);
        ImGui::TableNextRow();
        if (ImGui::TreeNodeEx("Call Sign | Squawk",
                              ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_SpanFullWidth))
        {
            // Node is open, add individual lines per value
            if (pFD) {
                std::string s (stat.call);
                s += " | ";
                s += dyn.GetSquawk();
                ImGui::TableNextCell();
                ImGui::TextUnformatted(s.c_str());
            }
            
            buildRow("Flight: Route",  stat.flightRoute(), pFD);
            buildRow("Simulated Time", dataRefs.GetSimTimeString().c_str(), true);
            
            // last received tracking data
            const double lstDat = pFD ? (pFD->GetYoungestTS() - ts) : -99999.9;
            if (-10000 <= lstDat && lstDat <= 10000)
                buildRow("Last Data [s]", lstDat, pFD, "%+.1f");
            else
                buildRow("Last Data [s]", "~", pFD);
            buildRow("Channel", pChannel ? pChannel->ChName() : "?", pChannel);
            
            // end of the tree
            ImGui::TreePop();
        } else {
            // Node is closed: Combine call sign, squawk, flight no into one cell
            if (pFD) {
                std::string s (stat.call);
                s += " | ";
                s += dyn.GetSquawk();
                if (!stat.flight.empty()) {
                    s += " | ";
                    s += stat.flight;
                }
                ImGui::TableNextCell();
                ImGui::TextUnformatted(s.c_str());
            }
        }
        
        // --- Global Window configuration ---
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + ACI_TREE_V_SEP*fFontScale);
        ImGui::TableNextRow();
        if (ImGui::TreeNodeEx("Settings", ImGuiTreeNodeFlags_SpanFullWidth))
        {
            ImGui::TableNextCell();
            ImGui::TextUnformatted("Drag controls with mouse:");
            
            buildRowLabel("Font Scaling");
            ImGui::DragPercent("##FontScaling", &fFontScale, 0.01f, 0.2f, 2.0f);
            
            buildRowLabel("Transparency");
            ImGui::DragPercent("##Transparency", &fTransparency);

            ImGui::TableNextRow();
            ImGui::TableNextCell();
            if (ImGui::Button("Reset to defaults")) {
                fFontScale      = ACI_STD_FONT_SCALE;
                fTransparency   = ACI_STD_TRANSPARENCY;
            }

            ImGui::TreePop();
        }

        // --- End of the table
        ImGui::EndTable();
    }
    
    // Reset font scaling
    ImGui::SetWindowFontScale(1.0f);
}

// Add a label to the list of a/c info
void ACIWnd::buildRowLabel (const std::string& label)
{
    ImGui::TableNextRow();
    ImGui::TextUnformatted(label.c_str());
    ImGui::TableNextCell();
}

// Add a label and a value to the list of a/c info
void ACIWnd::buildRow (const std::string& label,
                       const std::string& val,
                       bool bShowVal)
{
    buildRowLabel(label);
    if (bShowVal)
        ImGui::TextUnformatted(val.c_str());
}

// Add a label and a value to the list of a/c info
void ACIWnd::buildRow (const std::string& label,
                       int iVal, bool bShowVal,
                       const char* szFormat)
{
    buildRowLabel(label);
    if (bShowVal)
        ImGui::Text(szFormat, iVal);

}

// Add a label and a value to the list of a/c info
void ACIWnd::buildRow (const std::string& label,
                       double fVal, bool bShowVal,
                       const char* szFormat)
{
    buildRowLabel(label);
    if (bShowVal)
        ImGui::Text(szFormat, fVal);
}



//
// MARK: Static ACIWnd functions
//

float ACIWnd::fFontScale    = ACI_STD_FONT_SCALE;   // Font scaling factor for ACI Windows
float ACIWnd::fTransparency = ACI_STD_TRANSPARENCY; // Transparency level for ACI Windows
bool  ACIWnd::bAreShown     = true;                 // Are the ACI windows currently displayed or hidden?

// we keep a list of all created windows
std::list<ACIWnd*> ACIWnd::listACIWnd;

// static function: creates a new window
ACIWnd* ACIWnd::OpenNewWnd (const std::string& _acKey, WndMode _mode)
{
    // creation of windows only makes sense if windows are shown
    if (!AreShown())
        ToggleHideShowAll();
    
    // now create the new window
    return new ACIWnd(_acKey, _mode);
}

// move all windows into/out of VR
void ACIWnd::MoveAllVR (bool bIntoVR)
{
    // move into VR
    if (bIntoVR) {
        for (ACIWnd* pWnd: listACIWnd) {
            if (pWnd->GetMode() == WND_MODE_FLOAT)
                pWnd->SetMode(WND_MODE_VR);
        }
    }
    // move out of VR
    else {
        for (ACIWnd* pWnd: listACIWnd) {
            if (pWnd->GetMode() == WND_MODE_VR)
                pWnd->SetMode(WND_MODE_FLOAT);
        }
    }
}

// show/hide all windows
bool ACIWnd::ToggleHideShowAll()
{
    // Toggle
    bAreShown = !bAreShown;
    
    // now apply that new state to all windows
    for (ACIWnd* pWnd: listACIWnd)
        pWnd->SetVisible(bAreShown);
    
    // return new state
    return bAreShown;
}

// close all windows
void ACIWnd::CloseAll()
{
    // we don't close us when in VR camera view
    if (dataRefs.IsVREnabled() && LTAircraft::IsCameraViewOn())
        return;
    
    // keep closing the first window until map empty
    while (!listACIWnd.empty()) {
        ACIWnd* pWnd = *listACIWnd.begin();
        delete pWnd;                        // destructor removes from list
    }
}
