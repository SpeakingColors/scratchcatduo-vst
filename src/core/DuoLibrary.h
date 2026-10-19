// Scratch Cat Duo: track browser logic. Recursive folder scan, audio format filter, search, persisted favourite folders. Pure C++ (no JUCE),
// so it is unit tested. The editor feeds it the folder picked with a juce::FileChooser and stores favourites in the plugin state.
#pragma once
#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace duo {

struct LibFile { std::string path, name, dir, ext; unsigned long long size = 0; };

inline std::string lowerStr(std::string s) { for (auto& c : s) c = (char) std::tolower((unsigned char) c); return s; }
inline bool isAudioExt(const std::string& e) {
    static const char* ok[] = { "wav", "mp3", "flac", "ogg", "oga", "opus", "aif", "aiff", "m4a", "aac", "wma" };
    const std::string l = lowerStr(e); for (auto* o : ok) if (l == o) return true; return false;
}

class Library {
public:
    // Scans a folder and every subfolder. Returns how many audio files were added. Unreadable folders are skipped.
    int addFolder(const std::string& root) {
        namespace fs = std::filesystem; int added = 0; std::error_code ec;
        if (!fs::is_directory(root, ec)) return 0;
        for (fs::recursive_directory_iterator it(root, fs::directory_options::skip_permission_denied, ec), end; !ec && it != end; it.increment(ec)) {
            if (!it->is_regular_file(ec)) continue;
            std::string ext = it->path().extension().string(); if (!ext.empty() && ext[0] == '.') ext.erase(0, 1);
            if (!isAudioExt(ext)) continue;
            const std::string p = it->path().string();
            if (std::any_of(files.begin(), files.end(), [&](const LibFile& f) { return f.path == p; })) continue;
            LibFile f; f.path = p; f.name = it->path().filename().string(); f.dir = it->path().parent_path().string(); f.ext = lowerStr(ext);
            f.size = (unsigned long long) it->file_size(ec); files.push_back(f); ++added;
        }
        std::sort(files.begin(), files.end(), [](const LibFile& a, const LibFile& b) { return a.dir == b.dir ? a.name < b.name : a.dir < b.dir; });
        return added;
    }
    // Case insensitive match on name, folder and format. Empty query returns everything. `folder` limits to a folder and its subfolders.
    std::vector<LibFile> search(const std::string& query, const std::string& folder = "") const {
        std::vector<LibFile> out; const std::string q = lowerStr(query), fo = lowerStr(folder);
        for (auto& f : files) {
            if (!fo.empty() && lowerStr(f.dir).compare(0, fo.size(), fo) != 0) continue;
            if (!q.empty() && lowerStr(f.name + " " + f.dir + " " + f.ext).find(q) == std::string::npos) continue;
            out.push_back(f);
        }
        return out;
    }
    const std::vector<LibFile>& all() const { return files; }
    void clear() { files.clear(); }

    // Favourite folders, kept in the plugin state as one path per line.
    bool isFavourite(const std::string& p) const { return std::find(favs.begin(), favs.end(), p) != favs.end(); }
    void toggleFavourite(const std::string& p) { auto it = std::find(favs.begin(), favs.end(), p); if (it == favs.end()) favs.push_back(p); else favs.erase(it); }
    const std::vector<std::string>& favourites() const { return favs; }
    std::string favouritesToString() const { std::string s; for (auto& f : favs) s += f + "\n"; return s; }
    void favouritesFromString(const std::string& s) { favs.clear(); std::istringstream in(s); std::string l; while (std::getline(in, l)) if (!l.empty()) favs.push_back(l); }

private:
    std::vector<LibFile> files; std::vector<std::string> favs;
};
}
