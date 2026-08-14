#pragma once
#include <wx/intl.h>
#include <wx/taskbar.h>
#include <wx/wx.h>

// Define the Main Window (Frame)
class mainWindow : public wxFrame {
   public:
    // window auto-sizes to fit its content (the macro viewer grid)
    mainWindow(const wxString& title);

    // Remove the tray icon and really shut the app down. Shared by the
    // toolbar Quit button and the tray "quit" menu item.
    void requestQuit();

    // The tray must be destroyed here instead of in requestQuit(): when the
    // quit command comes from the tray menu, the tray's internal hidden
    // window (wxTaskBarIconWindow) is still processing the event, and
    // deleting it synchronously from inside its own event handler trips an
    // assert (pushed event handlers must have been removed). By the time the
    // frame is really destroyed the event has fully unwound, so deleting the
    // tray here is safe.
    ~mainWindow() override;

   private:
    wxTaskBarIcon* tray = nullptr;
    bool m_quitting = false;
    void onQuit(wxCommandEvent& ev);
    void openConfigDir(wxCommandEvent& ev);
    void onClose(wxCloseEvent& ev);
};

// Define the Application Instance Container
class moused : public wxApp {
   public:
    virtual bool OnInit() override;
    virtual int OnExit() override;

   private:
    // i18n: must live for the whole app lifetime (a local in OnInit() would be
    // destroyed right after startup and all translations would be lost)
    wxLocale m_locale;
};