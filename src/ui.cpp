#include <ui.hpp>
#include <wx/wx.h>

mainwindow::mainwindow(const wxString &title)
    : wxFrame(nullptr, wxID_ANY, title, wxDefaultPosition, wxSize(400, 300))
{
}
