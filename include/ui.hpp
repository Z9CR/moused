#pragma once
#include <memory>
#include <wx/grid.h>
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

    // whether the tray icon is active (false when the embedded PNG failed to
    // load) — used at startup so a `silent_launch` app still shows the
    // window instead of becoming invisible when there is no tray.
    bool hasTray() const { return tray != nullptr; }

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
    // kept as members so refreshLanguage() can re-render the translated
    // labels of the controls that are already on screen (the tray menu and
    // editor dialog are built on demand, so they pick up the new language
    // automatically)
    wxToolBar* m_toolBar = nullptr;
    wxGrid* m_macroViewer = nullptr;
    void onQuit(wxCommandEvent& ev);
    void openConfigDir(wxCommandEvent& ev);
    void onLanguageSelected(wxCommandEvent& ev);
    // re-apply every `_()`-based label after the runtime language changed
    void refreshLanguage();
    void onClose(wxCloseEvent& ev);
};

// Define the Application Instance Container
class moused : public wxApp {
   public:
    virtual bool OnInit() override;
    virtual int OnExit() override;

    // Switch the UI language at runtime (no restart needed). wxLocale can only
    // be Init()'d once per object, so the old one is dropped and a fresh one
    // is created for `language`; every subsequent `_()` call then returns the
    // new language.
    void setLanguage(int language);

    // Apply the language persisted in [global].language (already parsed into
    // `ui_language` by read_from_config()). No-op when it is "system", which
    // is exactly what OnInit() set up via wxLANGUAGE_DEFAULT.
    void applyConfigLanguage();

   private:
    // i18n: must live for the whole app lifetime (a local in OnInit() would be
    // destroyed right after startup and all translations would be lost)
    std::unique_ptr<wxLocale> m_locale;
};

// lets ui.cpp call wxGetApp() (defined by wxIMPLEMENT_APP in main.cpp)
wxDECLARE_APP(moused);