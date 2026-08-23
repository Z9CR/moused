#include <wx/grid.h>
#include <wx/intl.h>
#include <wx/menu.h>
#include <wx/mstream.h>
#include <wx/renderer.h>
#include <wx/taskbar.h>
#include <wx/wx.h>

#include <algorithm>
#include <config.hpp>
#include <filesystem>
#include <iterator>
#include <ui.hpp>
#include <utils.hpp>
#include <mousec_png.hpp>  // generated at build time from assets/mousec.png

#define KEY_ITEM(v, k) {#v, k},
const static struct {
    std::string name;
    int key;
} table[]{KEYS_LIST(KEY_ITEM)};
#undef KEY_ITEM

std::string getKeyNameOf(keyboard::keys key) {
    for (const auto& k : table)
        if (static_cast<keyboard::keys>(k.key) == key) return k.name;
    return std::string("NONE");
}

// display a combo as "LEFT_CONTROL + L", sorted by enum value for stability
std::string comboNameOf(const key_property& prop) {
    auto ks = prop.keys;
    std::sort(ks.begin(), ks.end());
    std::string name;
    for (std::size_t i = 0; i < ks.size(); ++i) {
        if (i > 0) name += " + ";
        name += getKeyNameOf(ks[i]);
    }
    return name.empty() ? std::string("NONE") : name;
}

constexpr int menuQuitBtnId = 0x6 + 7;
constexpr int menuShowMWBtnId = 0x6 + 7 + 114;

constexpr int toolBarItemOpenCfgID = 0x6767;
// MSW sign-extends toolbar WM_COMMAND ids to signed short, so ids must be
// < 0x8000 (classic hex words like 0xcafe don't fit; 0x5eed does)
constexpr int toolBarItemQuitId = 011 + 45 + 14;
constexpr int toolBarItemLanguageId = 0x1 * 919 + 8 - 10;

// language switch dropdown entries; index == wxChoice selection. `language`
// is the wxLanguage passed to wxLocale::Init(), `cfg` is the value persisted
// into [global].language.
static const struct {
    const char* label;  // UTF-8, shown verbatim in its own language
    int language;
    const char* cfg;
} langTable[] = {
    {"System", wxLANGUAGE_DEFAULT, "system"},
    {"English", wxLANGUAGE_ENGLISH_US, "en_US"},
    // the byte string below is "简体中文" encoded as UTF-8, written as hex
    // escapes so this source file stays encoding-agnostic (no /utf-8 flag
    // needed on MSVC)
    {"\xe7\xae\x80\xe4\xbd\x93\xe4\xb8\xad\xe6\x96\x87",
     wxLANGUAGE_CHINESE_SIMPLIFIED, "zh_CN"},
};

// index of the langTable entry matching a [global].language value, or -1
static int langCfgToIndex(const std::string& cfg) {
    for (std::size_t i = 0; i < std::size(langTable); ++i)
        if (cfg == langTable[i].cfg) return static_cast<int>(i);
    return -1;
}

// wxLanguage for a [global].language value (falls back to wxLANGUAGE_DEFAULT)
static int langCfgToWxLanguage(const std::string& cfg) {
    const int idx = langCfgToIndex(cfg);
    return idx < 0 ? wxLANGUAGE_DEFAULT : langTable[idx].language;
}

class taskBarIcon : public wxTaskBarIcon {
   public:
    explicit taskBarIcon(mainWindow* parent) : m_parent(parent) {}

    // MSW requires the popup menu to be re-created on every show, so build a
    // fresh menu here each time instead of caching one.
    virtual wxMenu* CreatePopupMenu() wxOVERRIDE {
        wxMenu* menu = new wxMenu();
        menu->Append(menuShowMWBtnId, _("tray.showMw"));
        menu->Append(menuQuitBtnId, _("tray.quit"));
        return menu;
    }

    void OnMenuEvent(wxCommandEvent& event) {
        if (event.GetId() == menuShowMWBtnId) {
            showMainWindow();
        } else if (event.GetId() == menuQuitBtnId) {
            m_parent->requestQuit();
        }
    }

    void OnLeftDClick(wxTaskBarIconEvent& event) { showMainWindow(); }

   private:
    void showMainWindow() {
        m_parent->Show(true);
        if (m_parent->IsIconized()) m_parent->Iconize(false);
        m_parent->Raise();
        m_parent->SetFocus();
    }

    mainWindow* m_parent;
    wxDECLARE_EVENT_TABLE();
};

// clang-format off
wxBEGIN_EVENT_TABLE(taskBarIcon, wxTaskBarIcon)
    EVT_MENU(menuShowMWBtnId, taskBarIcon::OnMenuEvent)
    EVT_MENU(menuQuitBtnId, taskBarIcon::OnMenuEvent)
    EVT_TASKBAR_LEFT_DCLICK(taskBarIcon::OnLeftDClick)
wxEND_EVENT_TABLE();
// clang-format on

class gridBtnRender : public wxGridCellRenderer {
   public:
    virtual void Draw(wxGrid& grid, wxGridCellAttr& attr, wxDC& dc,
                      const wxRect& rect, int row, int col,
                      bool isSelected) override {
        wxRendererNative::Get().DrawPushButton(&grid, dc, rect, 0);
        wxString label = grid.GetCellValue(row, col);
        dc.DrawLabel(label, rect,
                     wxALIGN_CENTER_HORIZONTAL | wxALIGN_CENTER_VERTICAL);
    }

    virtual wxGridCellRenderer* Clone() const override {
        return new gridBtnRender;
    }

    virtual wxSize GetBestSize(wxGrid& grid, wxGridCellAttr& attr, wxDC& dc,
                               int row, int col) override {
        wxString label = grid.GetCellValue(row, col);
        wxSize textSize = dc.GetTextExtent(label);
        return wxSize(textSize.x + 20, textSize.y + 10);
    }
};

class editorDialog : public wxDialog {
   public:
    editorDialog(wxWindow* parent, key_property& key)
        : wxDialog(parent, wxID_ANY, _("macroEditor.title")) {
        // declare items
#pragma region codeViewer
        constexpr int codeViewerId = 0x191 + 9 * 810;
        wxStaticText* codeViewer;
        if (key.type == script_type::in_line) {
            // show the parsed instructions, one per line
            codeViewer = new wxStaticText(this, codeViewerId,
                                          format_macro_script(key.active));
        } else {
            // replay: only the (resolved) file path is known so far
            codeViewer = new wxStaticText(this, codeViewerId, key.val);
        }
#pragma endregion
#pragma region openCfgFileBtn
        constexpr int openCfgFileBtnId = 0b11 * 4 + 5 * 1 + 4;
        auto openCfgFileBtn =
            new wxButton(this, openCfgFileBtnId, _("macroEditor.open_file"));
        openCfgFileBtn->Bind(wxEVT_BUTTON, [&](wxCommandEvent& event) {
            bool opened = false;
            if (key.type == script_type::in_line) {
                std::filesystem::path cfg(platform_cfg_dir);
                cfg /= config_name;
                // open toml cfg file
                opened = wxLaunchDefaultApplication(cfg.string());
            } else {
                // replay: `val` was already resolved to an absolute path
                std::filesystem::path cfg(key.val);
                if (std::filesystem::exists(cfg))
                    opened = wxLaunchDefaultApplication(cfg.string());
            }
            if (!opened)
                wxMessageBox(
                    _("editorDialog.openCfgFileBtn.failedWhenOpeningFile"));
        });
#pragma endregion
#pragma region editorDialogQuitBtn
        constexpr int editorDialogQuitBtnId = 067 + 67 + 067 + 0x67;
        auto editorDialogQuitBtn = new wxButton(this, editorDialogQuitBtnId,
                                                _("editorDialog.QuitBtn"));
        Bind(wxEVT_BUTTON, &editorDialog::onQuit, this, editorDialogQuitBtnId);
#pragma endregion
#pragma region buttomToolBtns
        auto toolbar = new wxBoxSizer(wxHORIZONTAL);
        toolbar->Add(openCfgFileBtn);
        toolbar->Add(editorDialogQuitBtn);
#pragma endregion
        // box layout
        auto sizer = new wxBoxSizer(wxVERTICAL);
        sizer->Add(codeViewer);
        sizer->Add(toolbar);
        SetSizerAndFit(sizer);
        CentreOnParent();
    };

   private:
    void onQuit(wxCommandEvent& event) { this->Close(); };
};

mainWindow::mainWindow(const wxString& title)
    : wxFrame(nullptr, wxID_ANY, title) {
    // WxWidget uses camelNaming, so do I
    // auto dark&light style
    this->SetBackgroundStyle(wxBackgroundStyle::wxBG_STYLE_SYSTEM);
#pragma region toolbar
    wxToolBar* toolBar = CreateToolBar(wxTB_TEXT | wxTB_NOICONS);
    this->m_toolBar = toolBar;
#pragma region openCfg
    toolBar->AddTool(toolBarItemOpenCfgID, _("toolbar.open_config"),
                     wxNullBitmap);
    Bind(wxEVT_MENU, &mainWindow::openConfigDir, this, toolBarItemOpenCfgID);
#pragma endregion
#pragma region quit
    // MSW sign-extends toolbar WM_COMMAND ids to signed short, so ids must be
    // < 0x8000 (classic hex words like 0xcafe don't fit; 0x5eed does)
    toolBar->AddTool(toolBarItemQuitId, _("toolbar.quit"), wxNullBitmap);
    Bind(wxEVT_MENU, &mainWindow::onQuit, this, toolBarItemQuitId);
#pragma endregion
#pragma region language
    // language switch: a wxChoice whose selection maps 1:1 onto langTable[].
    // Changing it re-initializes the locale immediately (no restart needed);
    // the tray menu and editor dialog are rebuilt on demand and pick up the
    // new language automatically, while refreshLanguage() re-renders the
    // toolbar and grid that are already on screen.
    auto langChoice = new wxChoice(toolBar, toolBarItemLanguageId);
    for (const auto& l : langTable)
        langChoice->Append(wxString::FromUTF8(l.label));
    const int cur = langCfgToIndex(ui_language);
    langChoice->SetSelection(cur < 0 ? 0 : cur);
    toolBar->AddControl(langChoice, _("toolbar.language"));
    langChoice->Bind(wxEVT_CHOICE, &mainWindow::onLanguageSelected, this);
#pragma endregion
    toolBar->Realize();
#pragma endregion

#pragma region macroViewer
    constexpr int macroViewerId = 0xCa + 0xFe + 0xBa + 0xBe;
    wxGrid* macroViewer =
        new wxGrid(this, macroViewerId, wxDefaultPosition, wxDefaultSize,
                   wxWANTS_CHARS, "macroViewer");
    this->m_macroViewer = macroViewer;
    macroViewer->CreateGrid(keys_properties.size(), 3);
    // hide the row-label column (the 1,2,3... on the left)
    macroViewer->SetRowLabelSize(0);
    // all cells become read-only / non-editable
    macroViewer->EnableEditing(false);
    // NOTE: each column needs its OWN attr object — wxGrid::SetColAttr()
    // takes ownership of the pointer, so sharing one attr between two
    // columns double-frees it when the grid is destroyed. That crash on
    // window close used to leave the process visible as "Not Responding"
    // in Task Manager even after its memory/CPU had dropped to zero.
    wxGridCellAttr* attrBtn0 = new wxGridCellAttr();
    attrBtn0->SetRenderer(new gridBtnRender());
    macroViewer->SetColAttr(0, attrBtn0);

    wxGridCellAttr* attrBtn2 = new wxGridCellAttr();
    attrBtn2->SetRenderer(new gridBtnRender());
    macroViewer->SetColAttr(2, attrBtn2);
    // columns' names
    macroViewer->SetColLabelValue(0, _("macroViewer.enabledCol"));
    macroViewer->SetColLabelValue(1, _("macroViewer.keyCol"));
    macroViewer->SetColLabelValue(2, _("macroViewer.editCol"));
    for (int i = 0; i < keys_properties.size(); i++) {
        const auto& current = keys_properties[i];
        macroViewer->SetCellValue(i, 0, current.enabled ? "Y" : "N");
        macroViewer->SetCellValue(i, 1, comboNameOf(current));
        macroViewer->SetCellValue(i, 2, _("macroView.editBtn"));
    }
    macroViewer->Bind(wxEVT_GRID_CELL_LEFT_CLICK, [=](wxGridEvent& evt) {
        const int row = evt.GetRow();
        const int col = evt.GetCol();
        switch (col) {
            case 0:
                if (row >= 0 &&
                    row < static_cast<int>(keys_properties.size())) {
                    // when Switch clicked, reverse enable && disable
                    try {
                        keys_properties[row].enabled =
                            !keys_properties[row].enabled;
                        macroViewer->SetCellValue(
                            row, col, keys_properties[row].enabled ? "Y" : "N");
                        flash_into_config();
                        // re-sync the runtime macros so an enable/disable
                        // toggle takes effect without restarting the app
                        warmup_macros();
                    } catch (const std::exception& e) {
                        // when err, undo switch
                        keys_properties[row].enabled =
                            !keys_properties[row].enabled;
                        macroViewer->SetCellValue(
                            row, col, keys_properties[row].enabled ? "Y" : "N");
                    }
                }
                break;
            case 2:
                if (row >= 0 &&
                    row < static_cast<int>(keys_properties.size())) {
                    // ShowModal() blocks until the dialog is closed and
                    // disables this frame meanwhile, so the grid can't be
                    // clicked again
                    editorDialog dlg(this, keys_properties[row]);
                    dlg.ShowModal();
                }
                break;
        }
        evt.Skip();
    });
#pragma region macroViewerLayout
    // auto-size every column & row so each cell fully shows its content
    // (multi-line macro text makes its row grow taller as needed)
    macroViewer->AutoSize();
    // lay out the grid with a sizer so the frame can size itself to fit
    wxSizer* sizer = new wxBoxSizer(wxVERTICAL);
    sizer->Add(macroViewer, 1, wxEXPAND | wxALL, 4);
    this->SetSizer(sizer);
    // clamp the auto-sized window to its configured maximum size first, so
    // Fit() below cannot grow the window beyond it (grid scrolls if the
    // content is larger). 0 = no upper bound.
    wxSize maxSize(mainwindow_max_width, mainwindow_max_height);
    if (mainwindow_max_width > 0 && mainwindow_max_height > 0)
        this->SetMaxSize(maxSize);
    // resize the frame so every grid cell is fully visible
    this->Fit();
#pragma endregion
#pragma endregion

#pragma region tray
    // NOTE: must create the DERIVED taskBarIcon (not wxTaskBarIcon) so the
    // CreatePopupMenu() override and the event table take effect.
    this->tray = new taskBarIcon(this);

    // Register image handlers BEFORE loading the PNG: wxWidgets ships with
    // zero handlers by default, so wxImage::LoadFile() would fail with
    // "unknown image type" otherwise. wxInitAllImageHandlers() is idempotent.
    wxInitAllImageHandlers();

    // Load the tray icon from the PNG bytes embedded at build time, keeping
    // the executable a single self-contained file (no assets/ dir needed at
    // runtime). NOTE: wxBitmap has no stream LoadFile() on MSW, but
    // wxImage::LoadFile(wxInputStream&) is portable (wxPNGHandler).
    wxIcon ico;
    {
        wxMemoryInputStream mis(mousec_png_data, mousec_png_size);
        wxImage img;
        if (img.LoadFile(mis, wxBITMAP_TYPE_PNG))
            ico.CopyFromBitmap(wxBitmap(img));
    }
    if (!wxTaskBarIconBase::IsAvailable()) {
        // when no system tray on this display, wxGTK3's SetIcon() always
        // returns true even when the icon can never be displayed, so without
        // this check a `silent_launch` app would start fully invisible.
        delete this->tray;
        this->tray = nullptr;
    } else if (!ico.IsOk() || !this->tray->SetIcon(ico)) {
        wxLogError("Moused: cannot create tray icon (embedded PNG invalid)");
        // a broken tray degrades to a normal top-level app: closing the
        // window then really exits (handled in onClose()).
        delete this->tray;
        this->tray = nullptr;
    }

    // closing the frame hides it to the tray instead of exiting
    this->Bind(wxEVT_CLOSE_WINDOW, &mainWindow::onClose, this);
#pragma endregion
}

void mainWindow::requestQuit() {
    // shared by the toolbar Quit button and the tray "quit" menu entry.
    if (this->m_quitting) return;  // re-entrancy guard
    this->m_quitting = true;
    // RemoveIcon() makes the shell forget the tray icon immediately, so it
    // does not linger after the process has exited. NOTE: do NOT delete the
    // tray here — when quit comes from the tray menu, the tray's internal
    // hidden window is still inside its own event handler, and deleting the
    // tray from within that handler triggers a wxWidgets assert. The tray is
    // deleted safely in ~mainWindow() once the event has fully unwound.
    if (this->tray) this->tray->RemoveIcon();
    this->Close();
}

mainWindow::~mainWindow() {
    // Safe point to free the tray: wxTaskBarIconWindow is no longer
    // processing any event here. If quit came from the tray menu, this
    // destructor runs only after that event handler has returned.
    if (this->tray) {
        delete this->tray;
        this->tray = nullptr;
    }
}

void mainWindow::onQuit(wxCommandEvent&) { this->requestQuit(); }

void mainWindow::onLanguageSelected(wxCommandEvent& ev) {
    const int sel = ev.GetSelection();
    if (sel < 0 || sel >= static_cast<int>(std::size(langTable))) return;
    // switch the runtime locale first (every `_()` call from now on returns
    // the new language), then persist the choice and re-render the controls
    // that are already on screen.
    wxGetApp().setLanguage(langTable[sel].language);
    ui_language = langTable[sel].cfg;
    try {
        flash_into_config();
    } catch (const std::exception& e) {
        log_msg("moused: failed to persist language choice: %s\n", e.what());
    }
    this->refreshLanguage();
}

void mainWindow::refreshLanguage() {
    // toolbar buttons + the language control's own label. MSW's SetLabel()
    // re-realizes the native toolbar automatically when the text changed.
    if (this->m_toolBar) {
        if (wxToolBarToolBase* tool =
                this->m_toolBar->FindById(toolBarItemOpenCfgID))
            tool->SetLabel(_("toolbar.open_config"));
        if (wxToolBarToolBase* tool =
                this->m_toolBar->FindById(toolBarItemQuitId))
            tool->SetLabel(_("toolbar.quit"));
        if (wxToolBarToolBase* tool =
                this->m_toolBar->FindById(toolBarItemLanguageId))
            tool->SetLabel(_("toolbar.language"));
    }
    // grid column headers + the "edit" button cells
    if (this->m_macroViewer) {
        this->m_macroViewer->SetColLabelValue(0, _("macroViewer.enabledCol"));
        this->m_macroViewer->SetColLabelValue(1, _("macroViewer.keyCol"));
        this->m_macroViewer->SetColLabelValue(2, _("macroViewer.editCol"));
        for (int i = 0; i < this->m_macroViewer->GetNumberRows(); ++i)
            this->m_macroViewer->SetCellValue(i, 2, _("macroView.editBtn"));
    }
}

// --- language / locale switching ---------------------------------------------

void moused::setLanguage(int language) {
    // wxLocale can only be Init()'d once per object, so switch languages by
    // replacing the object wholesale: its dtor restores the previous locale,
    // then the new one takes over and loads the catalog for the new language.
    m_locale.reset();
    m_locale =
        std::make_unique<wxLocale>(language, wxLOCALE_DONT_LOAD_DEFAULT);
    m_locale->AddCatalog("moused");
}

void moused::applyConfigLanguage() {
    // "system" is exactly what OnInit() already set up (wxLANGUAGE_DEFAULT)
    if (ui_language.empty() || ui_language == "system") return;
    this->setLanguage(langCfgToWxLanguage(ui_language));
}

void mainWindow::onClose(wxCloseEvent& ev) {
    if (this->m_quitting) {
        // requestQuit() already removed the tray icon; allow the window to
        // really close so the app exits.
        ev.Skip();
    } else if (this->tray) {
        // tray still active: closing the window only hides it, the app
        // keeps running in the background (tray double-click or "show" menu
        // re-opens it; "quit" menu really exits).
        this->Hide();
        if (ev.CanVeto()) ev.Veto();
    } else {
        // no tray (e.g. icon failed to load): close really exits
        ev.Skip();
    }
}

void mainWindow::openConfigDir(wxCommandEvent&) {
    if (!wxDirExists(platform_cfg_dir.c_str())) {
        log_msg("moused: file `%s` not found\n", platform_cfg_dir.c_str());
    } else
        wxLaunchDefaultApplication(platform_cfg_dir.c_str());
}
