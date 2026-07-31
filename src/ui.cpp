#include <ui.hpp>
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
#pragma region quit
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
