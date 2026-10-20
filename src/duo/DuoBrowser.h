// Scratch Cat Duo: the track browser at the bottom of the window. Folders (subfolders included), favourites, search, load to A or B, drag onto a deck.
#pragma once
#include "Duo.h"
#include "DuoUi.h"
#include "../core/DuoLibrary.h"
#include <map>
#include <set>

namespace duoui::ui
{
class TrackBrowser : public juce::Component, private juce::ListBoxModel, private juce::AsyncUpdater
{
public:
    explicit TrackBrowser(DuoProcessor& p) : proc(p), folderModel(*this), pool(1)
    {
        for (auto* b : { &addFolder, &addFiles }) { addAndMakeVisible(*b); b->setColour(juce::TextButton::buttonColourId, kBtn); }
        addFolder.setTooltip("Add a folder. Every subfolder is scanned too. The plugin remembers your folders between sessions.");
        addFiles.setTooltip("Add single audio files.");
        addFolder.onClick = [this] { chooser = std::make_unique<juce::FileChooser>("Add a folder of music", juce::File(), "");
            chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories, [this](const juce::FileChooser& fc) { auto f = fc.getResult(); if (f.isDirectory()) addRoot(f.getFullPathName()); }); };
        addFiles.onClick = [this] { chooser = std::make_unique<juce::FileChooser>("Add audio files", juce::File(), "*.wav;*.aif;*.aiff;*.flac;*.ogg;*.mp3;*.m4a");
            chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectMultipleItems | juce::FileBrowserComponent::canSelectFiles, [this](const juce::FileChooser& fc)
            { juce::StringArray a; for (auto& f : fc.getResults()) a.add(f.getFullPathName()); addLoose(a); }); };
        addAndMakeVisible(query); query.setTextToShowWhenEmpty("Search names, folders, formats...", kDim.withAlpha(0.7f)); query.setFont(f(15.f));
        query.onTextChange = [this] { refresh(); }; query.setTooltip("Search by name, folder or format.");
        addAndMakeVisible(folders); addAndMakeVisible(files);
        folders.setModel(&folderModel); files.setModel(this); folders.setRowHeight(24); files.setRowHeight(27);
        for (auto* l : { &folders, &files }) { l->setColour(juce::ListBox::backgroundColourId, kPanel); l->setColour(juce::ListBox::outlineColourId, kLine); l->setOutlineThickness(1); l->getVerticalScrollBar().setColour(juce::ScrollBar::thumbColourId, Colour(0xff4a4d57)); }
        // restore
        for (auto& l : juce::StringArray::fromLines(proc.apvts.state.getProperty("libRoots", "").toString())) if (l.isNotEmpty()) roots.add(l);
        for (auto& l : juce::StringArray::fromLines(proc.apvts.state.getProperty("libFiles", "").toString())) if (l.isNotEmpty()) looseFiles.add(l);
        lib.favouritesFromString(proc.apvts.state.getProperty("libFavs", "").toString().toStdString());
        rescanAll(); rebuildTree();
    }
    ~TrackBrowser() override { cancelPendingUpdate(); pool.removeAllJobs(true, 3000); }

    std::function<void(int deck, const juce::File&)> onLoad;
    std::function<int()> freeDeck;           // which deck a double click loads onto
    juce::String statusText() const { return scanning ? juce::String("SCANNING...") : juce::String((int) lib.all().size()) + " AUDIO FILES"; }

    // public so the editor can feed folders dropped from the file manager
    void addRoot(const juce::String& path)
    {
        if (! roots.contains(path)) roots.add(path); save(); scan(path);
    }
    void addLoose(const juce::StringArray& paths)
    {
        std::vector<duo::LibFile> v;
        for (auto& p : paths) { juce::File f(p); if (! f.existsAsFile() || ! duo::isAudioExt(f.getFileExtension().fromFirstOccurrenceOf(".", false, false).toStdString())) continue; looseFiles.addIfNotAlreadyThere(p); v.push_back(mk(f, "Loose files")); }
        save(); lib.merge(v); rebuildTree(); refresh();
    }
    void resized() override
    {
        auto r = getLocalBounds();
        addFolder.setBounds(0, 0, 120, 31); addFiles.setBounds(128, 0, 102, 31); query.setBounds(238, 0, 895, 31);
        folders.setBounds(5, 37, 245, getHeight() - 37 - 0); files.setBounds(258, 37, getWidth() - 258, getHeight() - 37);
        (void) r;
    }
    void paint(juce::Graphics& g) override
    {
        g.setColour(kDim); g.setFont(f(15.f)); g.drawFittedText("Click a row to select. Use A or B to load, or drag a row onto a deck. Double-click loads on the free deck.", { 1145, 0, getWidth() - 1145, 31 }, juce::Justification::centredRight, 1);
    }
    void tick() { }

private:
    // ---- data ----
    static duo::LibFile mk(const juce::File& f, const juce::String& dir)
    { duo::LibFile x; x.path = f.getFullPathName().toStdString(); x.name = f.getFileName().toStdString(); x.dir = dir.toStdString(); x.ext = duo::lowerStr(f.getFileExtension().fromFirstOccurrenceOf(".", false, false).toStdString()); x.size = (unsigned long long) f.getSize(); return x; }
    void save()
    {
        proc.apvts.state.setProperty("libRoots", roots.joinIntoString("\n"), nullptr); proc.apvts.state.setProperty("libFiles", looseFiles.joinIntoString("\n"), nullptr);
        proc.apvts.state.setProperty("libFavs", juce::String(lib.favouritesToString()), nullptr);
    }
    void scan(const juce::String& root)
    {
        ++scanning; const juce::Component::SafePointer<TrackBrowser> self(this);
        pool.addJob([this, root, self]
        {
            duo::Library tmp; tmp.addFolder(root.toStdString()); auto found = tmp.all();
            juce::MessageManager::callAsync([self, found] { if (self == nullptr) return; self->lib.merge(found); --self->scanning; self->rebuildTree(); self->refresh(); });
        });
        repaint();
    }
    void rescanAll()
    {
        for (auto& r : roots) scan(r);
        std::vector<duo::LibFile> v; for (auto& p : looseFiles) { juce::File f(p); if (f.existsAsFile()) v.push_back(mk(f, "Loose files")); }
        lib.merge(v);
    }
    struct TRow { int kind = 0; std::string path; int depth = 0, count = 0; juce::String text; };   // kind 0 header, 1 favourite, 2 all, 3 folder
    std::vector<TRow> rows; std::string selected;                                                  // selected folder path ("" = all)
    void rebuildTree()
    {
        rows.clear(); std::map<std::string, int> cnt;
        for (auto& f : lib.all())
        {
            std::string d = f.dir; cnt[d]++;
            for (auto& rt : roots) { const std::string r = rt.toStdString(); if (d.size() > r.size() && d.compare(0, r.size(), r) == 0) { std::string p = d; while (p.size() > r.size()) { auto s = p.find_last_of("/\\"); if (s == std::string::npos || s < r.size()) break; p = p.substr(0, s); cnt[p] += 0; if (p != d) cnt[p]++; } } }
        }
        auto subtree = [&](const std::string& path) { int n = 0; for (auto& f : lib.all()) if (duo::inFolder(f.dir, path)) ++n; return n; };
        rows.push_back({ 0, "", 0, 0, "FAVOURITES" });
        if (lib.favourites().empty()) rows.push_back({ 0, "", 0, 0, "Star a folder to keep it here." });
        for (auto& p : lib.favourites()) rows.push_back({ 1, p, 0, subtree(p), juce::File(p).getFileName() });
        rows.push_back({ 0, "", 0, 0, "FOLDERS" });
        rows.push_back({ 2, "", 0, (int) lib.all().size(), "All scanned" });
        std::set<std::string> done;
        for (auto& rt : roots)
        {
            const std::string r = rt.toStdString(); rows.push_back({ 3, r, 0, subtree(r), juce::File(rt).getFileName().isEmpty() ? rt : juce::File(rt).getFileName() }); done.insert(r);
            for (auto& kv : cnt)
            {
                const std::string& d = kv.first; if (d.size() <= r.size() || d.compare(0, r.size(), r) != 0 || done.count(d)) continue;
                int depth = 0; for (size_t i = r.size(); i < d.size(); ++i) if (d[i] == '/' || d[i] == '\\') ++depth;
                if (depth > 3 || rows.size() > 600) continue; done.insert(d); rows.push_back({ 3, d, depth, subtree(d), juce::File(juce::String(d)).getFileName() });
            }
        }
        bool hasLoose = false; for (auto& f : lib.all()) if (f.dir == "Loose files") { hasLoose = true; break; }
        if (hasLoose) rows.push_back({ 3, "Loose files", 0, subtree("Loose files"), "Loose files" });
        folders.updateContent(); folders.repaint();
    }
    std::vector<duo::LibFile> shown;
    void refresh()
    {
        shown = lib.search(query.getText().toStdString(), selected); if (shown.size() > 5000) shown.resize(5000);
        files.updateContent(); files.repaint(); repaint(); if (onStatus) onStatus();
    }
public:
    std::function<void()> onStatus;
private:
    // ---- files list model ----
    int getNumRows() override { return (int) shown.size(); }
    void paintListBoxItem(int row, juce::Graphics& g, int w, int h, bool sel) override
    {
        if (row < 0 || row >= (int) shown.size()) return; auto& x = shown[(size_t) row];
        if (sel) { g.setColour(kAccent.withAlpha(0.18f)); g.fillRect(0, 0, w, h); }
        g.setColour(kLine); g.drawHorizontalLine(h - 1, 0.f, (float) w);
        g.setColour(kInk); g.setFont(f(15.f)); g.drawText(juce::String(x.name), 10, 0, w / 3, h, juce::Justification::centredLeft, true);
        g.setColour(kDim); g.setFont(mono(12.f)); g.drawText(juce::String(x.dir), w / 3 + 20, 0, w / 3 - 20, h, juce::Justification::centredLeft, true);
        g.drawText(juce::String(x.ext).toUpperCase(), w - 270, 0, 60, h, juce::Justification::centredLeft); g.drawText(juce::String((double) x.size / 1048576.0, 1) + " MB", w - 215, 0, 80, h, juce::Justification::centredRight);
        for (int d = 0; d < 2; ++d) { auto b = btn(w, h, d).toFloat(); g.setColour(kBtn); g.fillRoundedRectangle(b, 4.f); g.setColour(deckCol(d)); g.setFont(f(15.f, true)); g.drawText(d == 0 ? "A" : "B", b, juce::Justification::centred); }
    }
    static Rectangle<int> btn(int w, int h, int d) { return { w - 100 + d * 46, 3, 40, h - 6 }; }
    void listBoxItemClicked(int row, const juce::MouseEvent& e) override
    {
        if (row < 0 || row >= (int) shown.size()) return; const int w = files.getVisibleRowWidth(), h = files.getRowHeight();
        for (int d = 0; d < 2; ++d) if (btn(w, h, d).contains(e.getPosition())) { if (onLoad) onLoad(d, juce::File(shown[(size_t) row].path)); return; }
    }
    void listBoxItemDoubleClicked(int row, const juce::MouseEvent&) override { if (row >= 0 && row < (int) shown.size() && onLoad) onLoad(freeDeck ? freeDeck() : 0, juce::File(shown[(size_t) row].path)); }
    juce::var getDragSourceDescription(const juce::SparseSet<int>& rowsSel) override { const int r = rowsSel.size() > 0 ? rowsSel[0] : -1; return r >= 0 && r < (int) shown.size() ? juce::var("duofile:" + juce::String(shown[(size_t) r].path)) : juce::var(); }
    void handleAsyncUpdate() override {}

    // ---- folder tree model ----
    struct FolderModel : juce::ListBoxModel
    {
        explicit FolderModel(TrackBrowser& o) : b(o) {}
        int getNumRows() override { return (int) b.rows.size(); }
        void paintListBoxItem(int row, juce::Graphics& g, int w, int h, bool) override
        {
            if (row < 0 || row >= (int) b.rows.size()) return; auto& r = b.rows[(size_t) row];
            if (r.kind == 0) { g.setColour(kDim); g.setFont(row == 0 || r.text == "FOLDERS" ? caps(11.f, 0.18f) : f(14.f)); g.drawText(r.text, 4, 0, w - 8, h, juce::Justification::centredLeft); return; }
            const bool on = r.path == b.selected && r.kind != 1 ? true : (r.kind == 1 && r.path == b.selected);
            if (on) { g.setColour(kAccent.withAlpha(0.2f)); g.fillRect(0, 1, w, h - 2); }
            g.setColour(on ? kAccent : kInk); g.setFont(f(14.f)); g.drawText(r.text, 8 + r.depth * 12, 0, w - 70 - r.depth * 12, h, juce::Justification::centredLeft, true);
            g.setColour(kDim); g.setFont(mono(12.f)); g.drawText(juce::String(r.count), w - 62, 0, 34, h, juce::Justification::centredRight);
            if (r.kind != 2) { const bool fav = b.lib.isFavourite(r.path); g.setColour(fav ? kYellow : kDim); g.setFont(f(16.f)); g.drawText(fav ? juce::String::charToString(0x2605) : juce::String::charToString(0x2606), w - 26, 0, 22, h, juce::Justification::centred); }
        }
        void listBoxItemClicked(int row, const juce::MouseEvent& e) override
        {
            if (row < 0 || row >= (int) b.rows.size()) return; auto& r = b.rows[(size_t) row]; if (r.kind == 0) return;
            if (e.mods.isPopupMenu() && r.kind == 3 && b.roots.contains(juce::String(r.path)))
            {
                juce::PopupMenu m; m.addItem(1, "Remove this folder from the browser");
                m.showMenuAsync({}, [this, p = r.path](int c) { if (c == 1) b.removeRoot(juce::String(p)); }); return;
            }
            if (r.kind != 2 && e.x > b.folders.getVisibleRowWidth() - 30) { b.lib.toggleFavourite(r.path); b.save(); b.rebuildTree(); return; }
            b.selected = r.path; b.refresh(); b.folders.repaint();
        }
        TrackBrowser& b;
    };
    void removeRoot(const juce::String& p)
    {
        roots.removeString(p); save(); selected.clear(); lib.clear(); rescanAll(); rebuildTree(); refresh();
    }

    DuoProcessor& proc; duo::Library lib; juce::StringArray roots, looseFiles; int scanning = 0;
    juce::TextButton addFolder { "ADD FOLDER" }, addFiles { "ADD FILES" }; juce::TextEditor query; juce::ListBox folders, files;
    FolderModel folderModel; std::unique_ptr<juce::FileChooser> chooser; juce::ThreadPool pool;
};
}   // namespace duoui::ui
