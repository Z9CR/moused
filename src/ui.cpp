#include <ui.hpp>
#include <config.hpp>
#include <utils.hpp>
#include <wx/wx.h>
#include <wx/menu.h>

mainWindow::mainWindow(const wxString &title, int winw, int winh)
    : wxFrame(nullptr, wxID_ANY, title, wxDefaultPosition, wxSize(winw, winh))
{
    // WxWidget uses camelNaming, so do I
    // auto dark&light style
    this->SetBackgroundStyle(wxBackgroundStyle::wxBG_STYLE_SYSTEM);
#pragma region toolbar
    wxToolBar *toolBar = CreateToolBar(wxTB_TEXT | wxTB_NOICONS);
#pragma region openCfg
    constexpr int toolBarItemOpenCfgID = 0x6767;
    toolBar->AddTool(toolBarItemOpenCfgID, _("open config"), wxNullBitmap);
    Bind(wxEVT_MENU, &mainWindow::openConfigDir, this, toolBarItemOpenCfgID);
#pragma endregion
#pragma region quit
    // MSW sign-extends toolbar WM_COMMAND ids to signed short, so ids must be
    // < 0x8000 (classic hex words like 0xcafe don't fit; 0x5eed does)
    constexpr int toolBarItemQuitId = 0x5eed;
    toolBar->AddTool(toolBarItemQuitId, _("quit"), wxNullBitmap); 
    Bind(wxEVT_MENU, &mainWindow::onQuit, this, toolBarItemQuitId);
#pragma endregion
    toolBar->Realize();
#pragma endregion


}

void mainWindow::onQuit(wxCommandEvent &) {
    this->Close();
}

void mainWindow::openConfigDir(wxCommandEvent &) {
    if(!wxDirExists(platform_cfg_dir)) {
        log_msg("moused: file `%s` not found\n", platform_cfg_dir);
    }
    else
        wxLaunchDefaultApplication(platform_cfg_dir);
}