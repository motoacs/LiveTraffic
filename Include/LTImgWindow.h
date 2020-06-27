/// @file       LTImgWindow.h
/// @brief      LiveTraffic-specific enhancements to ImGui / ImgWindow
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

#ifndef LTImgWindow_h
#define LTImgWindow_h

//
// MARK: Constant definitions
//

/// The standard font to use
#define WND_STANDARD_FONT "Resources/fonts/DejaVuSans.ttf"
/// The font's standard size
constexpr int WND_FONT_SIZE = 15;

//
// MARK: ImGui extensions
//

namespace ImGui {

/// Get width of an icon button (calculate on first use)
IMGUI_API float GetWidthIconBtn ();

/// @brief Helper for creating unique IDs
/// @details Required when creating many widgets in a loop, e.g. in a table
IMGUI_API void PushID_formatted(const char* format, ...)    IM_FMTARGS(1);

/// @brief Button with on-hover popup helper text
/// @param label Text on Button
/// @param tip Tooltip text when hovering over the button (or NULL of none)
/// @param colFg Foreground/text color (optional, otherwise no change)
/// @param colBg Background color (optional, otherwise no change)
/// @param size button size, 0 for either axis means: auto size
IMGUI_API bool ButtonTooltip(const char* label,
                             const char* tip = nullptr,
                             ImU32 colFg = IM_COL32(1,1,1,0),
                             ImU32 colBg = IM_COL32(1,1,1,0),
                             const ImVec2& size = ImVec2(0,0));

/// @brief Draws a button with an icon
/// @param icon The icon to draw, expected to be a single char from an icon font
/// @param tooltip Tooltip text when hovering over the button
/// @param rightAligned Align button to the right of the content region?
/// @return Button pressed?
IMGUI_API bool ButtonIcon(const char* icon, const char* tooltip = nullptr, bool rightAligned = true);

/// @brief Same as ImGui::SliderFloat(), but display is in percent, so values are expected to be around 1.0 to be displayed as 100%
IMGUI_API bool SliderPercent(const char* label, float* v, float v_min=0.0f, float v_max=1.0f, const char* format = "%.0f%%", float power = 1.0f);

/// @brief Same as DragFloat, but display is in percent, so values are expected to be around 1.0 to be displayed as 100%
IMGUI_API bool DragPercent(const char* label, float* v, float v_speed = 0.01f, float v_min = 0.0f, float v_max = 1.0f, const char* format = "%.0f%%", float power = 1.0f);

/// @brief Draws a tree node in the current and a Help icon in the last table cell
/// @details All rendering is skipped and `true` returned if `filter` is non-empty.
///          Cursor is at beginning of (next) row afterwards, just continue drawing.
/// @param label to be written into first cell
/// @param helpURL URL opened when the help icon button is clicked
/// @param helpPopup (optional) Tooltip text when hovering over help icon button
/// @param nCol Number of columns of the table (needed to know where the help icon button goes)
/// @param filter (optional) Filter string: Node will only show if `filter` is `nullptr` or empty string
/// @param nOpCl (optional) `-1` force node close, `0` open/close unchanged, `+1` force node open
/// @param flags (optional) Tree node flags, see ImGui::ImGuiTreeNodeFlags_
/// @return Continue drawing in the node? (Tree node open, or a `filter` defined)
IMGUI_API bool TreeNodeHelp(const char* label, const char* helpURL, const char* helpPopup,
                            int nCol, const char* filter = nullptr, int nOpCl = 0,
                            ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_SpanFullWidth);

/// @brief Extension to ImGui::TreeNodeHelp(): Shows a button opening an URL in the 2nd cell
IMGUI_API bool TreeNodeLinkHelp(const char* label,
                                const char* linkLabel, const char* linkURL, const char* linkPopup,
                                const char* helpURL, const char* helpPopup,
                                int nCol, const char* filter = nullptr, int nOpCl = 0,
                                ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_SpanFullWidth);

/// @brief Show this label only if text matches filter string
/// @return Shown?
IMGUI_API bool FilteredLabel(const char* label, const char* filter, bool bEnabled = true);

/// @brief Filter label plus checkbox linked to boolean(integer) dataRef
/// @return Value just changed?
IMGUI_API bool FilteredCfgCheckbox(const char* label, const char* filter, dataRefsLT idx);

/// @brief Filter label plus integer slider linked to dataRef
/// @return Value just changed?
IMGUI_API bool FilteredCfgNumber(const char* label, const char* filter, dataRefsLT idx,
                                 int v_min, int v_max, int v_step = 1, const char* format = "%d");


};

//
// MARK: Screen coordinate helpers
//

/// 2D window position
struct WndPos {
    int x = 0;
    int y = 0;
};

/// 2D rectagle
struct WndRect {
    WndPos tl;          ///< top left
    WndPos br;          ///< bottom right
    
    /// Default Constructor -> all zero
    WndRect () {}
    /// Constructor takes four ints as a convenience
    WndRect (int _l, int _t, int _r, int _b) :
    tl{_l, _t}, br{_r, _b} {}
    /// Constructor taking two positions
    WndRect (const WndPos& _tl, const WndPos& _br) :
    tl(_tl), br(_br) {}
    
    // Accessor to individual coordinates
    int     left () const   { return tl.x; }    ///< reading left
    int&    left ()         { return tl.x; }    ///< writing left
    int     top () const    { return tl.y; }    ///< reading top
    int&    top ()          { return tl.y; }    ///< writing top
    int     right () const  { return br.x; }    ///< reading right
    int&    right ()        { return br.x; }    ///< writing right
    int     bottom () const { return br.y; }    ///< reading bottom
    int&    bottom ()       { return br.y; }    ///< writing bottom
    
    // Clear all to zero
    void    clear () { tl.x = tl.y = br.x = br.y = 0; }
    bool    empty () const { return !tl.x && !tl.y && !br.x && !br.y; }
};


/// Mode the window is to open in / does currently operate in
enum WndMode {
    WND_MODE_NONE = 0,      ///< unknown, not yet set mode
    WND_MODE_FLOAT,         ///< XP11 modern floating window
    WND_MODE_POPOUT,        ///< XP11 popped out window in "first class OS window"
    WND_MODE_VR,            ///< XP11 moved to VR window
    // temporary modes for init/set only:
    WND_MODE_FLOAT_OR_VR,   ///< VR if in VR-mode, otherwise float (initialization use only)
    WND_MODE_FLOAT_CENTERED,///< will be shown centered on main screen
    WND_MODE_FLOAT_CNT_VR,  ///< VR if in VR-mode, centered otherwise
    // temporary mode for closing the window
    WND_MODE_CLOSE,         ///< close the window
};

/// Determine position mode based on mode
inline XPLMWindowPositioningMode toPosMode (WndMode _m)
{
    switch (_m) {
        case WND_MODE_FLOAT:    return xplm_WindowPositionFree;
        case WND_MODE_POPOUT:   return xplm_WindowPopOut;
        case WND_MODE_VR:       return xplm_WindowVR;
        case WND_MODE_FLOAT_OR_VR:
            return dataRefs.IsVREnabled() ? xplm_WindowVR : xplm_WindowPositionFree;
        case WND_MODE_FLOAT_CENTERED:
            return xplm_WindowCenterOnMonitor;
        case WND_MODE_FLOAT_CNT_VR:
            return dataRefs.IsVREnabled() ? xplm_WindowVR : xplm_WindowCenterOnMonitor;
        default:
            return xplm_WindowPositionFree;
    }
}

/// Style: Is it a solid window with all decorations, or a least-intrusive HUD-like window?
enum WndStyle {
    WND_STYLE_NONE = 0, ///< unknown, not yet set style
    WND_STYLE_SOLID,    ///< solid window like settings
    WND_STYLE_HUD,      ///< HUD-like window, transparent, lower layer in wnd-hierarchie
};

/// Determine window decoration based on style
inline XPLMWindowDecoration toDeco (WndStyle _s)
{
    return
    _s == WND_STYLE_HUD ? xplm_WindowDecorationSelfDecoratedResizable :
    xplm_WindowDecorationRoundRectangle;
}

/// Determine window layer based on style
inline XPLMWindowLayer toLayer (WndStyle _s)
{
    return _s == WND_STYLE_HUD ? xplm_WindowLayerFlightOverlay : xplm_WindowLayerFloatingWindows;
}

//
// MARK: LTImgWindow class
//

class LTImgWindow : public ImgWindow
{
public:
    /// The style this window operates in
    const WndStyle wndStyle;
    /// Which Help-URL to open?
    const char* szHelpURL = nullptr;
protected:
    // Helpers for window mode changes, which should not happen during drawing,
    // so we delay them to a flight loop callback
    
    /// Note to myself that a change of window mode is requested
    WndMode nextWinMode = WND_MODE_NONE;
    // Our flight loop callback in case we need one for mode changes
    XPLMFlightLoopID flChangeWndMode = nullptr;
    // Last known in-sim position before moving out
    WndRect rectFloat;
    
public:
    /// Constructor sets up the window basically (no title, not visible yet)
    LTImgWindow (WndMode _mode, WndStyle _style, WndRect _initPos);
    /// Destructor cleans up
    ~LTImgWindow () override;
    
    /// Set the window mode, move the window if needed
    void SetMode (WndMode _mode);
    /// Get current window mode
    WndMode GetMode () const;
    
    /// Get current window geometry as an WndRect structure
    WndRect GetCurrentWindowGeometry () const;
    
    /// @brief Loose keyboard foucs, ie. return focus to X-Plane proper, if I have it now
    /// @return Actually returned focus to X-Plane?
    bool ReturnKeyboardFocus ();
    
protected:
    /// Schedule the callback for window mode changes
    void ScheduleWndModeChange () { XPLMScheduleFlightLoop(flChangeWndMode, -1.0, 1); }

    /// Paints close button, title, decorative lines, and window buttons
    void buildTitleBar (const std::string& _title,
                        bool bCloseBtn = true,
                        bool bWndBtns = true);
    /// Paints close button
    void buildCloseButton ();
    /// Paints resizing buttons as needed as per current window status
    void buildWndButtons ();
    
protected:
    /// flight loop callback for changing the window's mode
    static float cbChangeWndMode(
        float                inElapsedSinceLastCall,
        float                inElapsedTimeSinceLastFlightLoop,
        int                  inCounter,
        void*                inRefcon);
};

/// One-time initializations for all ImGui windows
bool LTImgWindowInit ();

/// Cleanup of any resources
void LTImgWindowCleanup ();


#endif /* ACInfoWnd_h */