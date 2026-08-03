#pragma once
#include <wx/wx.h>

// Define the Main Window (Frame)
class mainWindow : public wxFrame
{
public:
    mainWindow(const wxString &title, int winw, int winh);
private:
    void onQuit(wxCommandEvent &ev);
    void openConfigDir(wxCommandEvent &ev);
};

// Define the Application Instance Container
class moused : public wxApp
{
public:
    virtual bool OnInit() override;
    virtual int OnExit() override;
};
