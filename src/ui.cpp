#include <wx/grid.h>
#include <wx/intl.h>
#include <wx/menu.h>
#include <wx/renderer.h>
#include <wx/wx.h>

#include <algorithm>
#include <config.hpp>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <ui.hpp>
#include <utils.hpp>

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
            codeViewer = new wxStaticText(this, codeViewerId, key.val);
        } else {
            // type = file
            std::filesystem::path cfg(key.val);
            if (cfg.is_absolute()) {
                std::ifstream f(key.val);
                std::stringstream buffer{};
                buffer << f.rdbuf();
                codeViewer = new wxStaticText(this, codeViewerId, buffer.str());
            } else if (cfg.is_relative()) {
                cfg = std::filesystem::path(platform_cfg_dir) / cfg;
                std::ifstream f(cfg.string());
                std::stringstream buffer{};
                buffer << f.rdbuf();
                codeViewer = new wxStaticText(this, codeViewerId, buffer.str());
            }
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
                // type = file
                std::filesystem::path cfg(key.val);
                if (cfg.is_absolute())
                    // open abs path cfg
                    opened = wxLaunchDefaultApplication(cfg.string());
                else if (cfg.is_relative()) {
                    cfg = std::filesystem::path(platform_cfg_dir) / cfg;
                    if (std::filesystem::exists(cfg))
                        // open relative path cfg
                        opened = wxLaunchDefaultApplication(cfg.string());
                }
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
#pragma region openCfg
    constexpr int toolBarItemOpenCfgID = 0x6767;
    toolBar->AddTool(toolBarItemOpenCfgID, _("toolbar.open_config"),
                     wxNullBitmap);
    Bind(wxEVT_MENU, &mainWindow::openConfigDir, this, toolBarItemOpenCfgID);
#pragma endregion
#pragma region quit
    // MSW sign-extends toolbar WM_COMMAND ids to signed short, so ids must be
    // < 0x8000 (classic hex words like 0xcafe don't fit; 0x5eed does)
    constexpr int toolBarItemQuitId = 011 + 45 + 14;
    toolBar->AddTool(toolBarItemQuitId, _("toolbar.quit"), wxNullBitmap);
    Bind(wxEVT_MENU, &mainWindow::onQuit, this, toolBarItemQuitId);
#pragma endregion
    toolBar->Realize();
#pragma endregion

#pragma region macroViewer
    constexpr int macroViewerId = 0xCa + 0xFe + 0xBa + 0xBe;
    wxGrid* macroViewer =
        new wxGrid(this, macroViewerId, wxDefaultPosition, wxDefaultSize,
                   wxWANTS_CHARS, "macroViewer");
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
                        keys_properties[row].enabled = !keys_properties[row].enabled;
                        macroViewer->SetCellValue(row, col, keys_properties[row].enabled ? "Y" : "N");
                        flash_into_config();
                    }
                    catch (const std::exception& e) {
                        // when err, undo switch
                        keys_properties[row].enabled = !keys_properties[row].enabled;
                        macroViewer->SetCellValue(row, col, keys_properties[row].enabled ? "Y" : "N");
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
    // (multi-line Lua code makes its row grow taller as needed)
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
}

void mainWindow::onQuit(wxCommandEvent&) { this->Close(); }

void mainWindow::openConfigDir(wxCommandEvent&) {
    if (!wxDirExists(platform_cfg_dir.c_str())) {
        log_msg("moused: file `%s` not found\n", platform_cfg_dir.c_str());
    } else
        wxLaunchDefaultApplication(platform_cfg_dir.c_str());
}
