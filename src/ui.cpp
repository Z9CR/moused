#include <wx/grid.h>
#include <wx/menu.h>
#include <wx/renderer.h>
#include <wx/wx.h>
#include <wx/intl.h>
#include <algorithm>
#include <config.hpp>
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
        auto* sizer = new wxBoxSizer(wxVERTICAL);
        sizer->Add(new wxStaticText(this, wxID_ANY, comboNameOf(key)), 0,
                   wxALIGN_CENTER | wxALL, 12);
        sizer->Add(new wxStaticText(this, wxID_ANY, key.code), 0,
                   wxALIGN_CENTER | wxALL, 12);
        sizer->Add(new wxButton(this, wxID_OK, _("macroEditor.close")), 0,
                   wxALIGN_CENTER | wxALL, 12);
        SetSizerAndFit(sizer);
        CentreOnParent();
    };
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
    toolBar->AddTool(toolBarItemOpenCfgID, _("toolbar.open_config"), wxNullBitmap);
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
    wxGridCellAttr* attr = new wxGridCellAttr();
    attr->SetRenderer(new gridBtnRender());
    macroViewer->SetColAttr(2, attr);
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
        if (evt.GetCol() == 2) {
            const int row = evt.GetRow();
            if (row >= 0 && row < static_cast<int>(keys_properties.size())) {
                // ShowModal() blocks until the dialog is closed and disables
                // this frame meanwhile, so the grid can't be clicked again
                editorDialog dlg(this, keys_properties[row]);
                dlg.ShowModal();
            }
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