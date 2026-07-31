#pragma once
#include <wx/wx.h>

// Define the Main Window (Frame)
class mainwindow : public wxFrame
{
public:
    mainwindow(const wxString &title);
};

// Define the Application Instance Container
class moused : public wxApp
{
public:
    virtual bool OnInit() override;
};
