// Scratch Cat Duo: presets. Factory presets in code, your own as small XML files in your user folder (kept on this computer, never sent anywhere).
#pragma once
#include "Duo.h"
#include <map>

namespace duoui
{
struct Preset { juce::String name; bool factory = false; std::map<juce::String, float> v; };

class PresetBank
{
public:
    explicit PresetBank(DuoProcessor& p) : proc(p) { reload(); }
    std::vector<Preset> list; int current = 0;

    static juce::File folder() { return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory).getChildFile("Studio 87").getChildFile("Scratch Cat Duo").getChildFile("Presets"); }
    // What a preset stores: every control except the deck source (depends on the project) and the output routing (set once per project).
    bool stored(const juce::String& id) const { return id != "src_a" && id != "src_b" && id != "outmode"; }

    void reload()
    {
        list.clear();
        std::map<juce::String, float> base;
        for (auto* pr : proc.getParameters()) if (auto* r = dynamic_cast<juce::RangedAudioParameter*>(pr)) if (stored(r->paramID)) base[r->paramID] = r->convertFrom0to1(r->getDefaultValue());
        auto mix = [&](std::initializer_list<std::pair<const char*, float>> o) { auto m = base; for (auto& kv : o) m[kv.first] = kv.second; return m; };
        list.push_back({ "Init", true, base });
        list.push_back({ "Wash Out", true, mix({ { "fxtarget", 3 }, { "fxt1", 1 }, { "fxon1", 0 }, { "fxp1_1", 0.25f }, { "fxp2_1", 1 }, { "fxt2", 2 }, { "fxon2", 1 }, { "fxp1_2", 0.75f }, { "fxp2_2", 0.5f }, { "fxt3", 3 }, { "fxon3", 1 }, { "fxp1_3", 0.85f }, { "fxp2_3", 0.7f } }) });
        list.push_back({ "Club blend", true, mix({ { "sync_b", 1 }, { "keylock_b", 1 }, { "xfade", 0.5f }, { "xcurve", 0 } }) });
        list.push_back({ "Battle mixer", true, mix({ { "xcurve", 1 }, { "vinyl_a", 0 }, { "vinyl_b", 0 }, { "hi_a", 3 }, { "hi_b", 3 }, { "xfade", 0 } }) });
        list.push_back({ "Acapella over loop", true, mix({ { "sync_b", 1 }, { "keylock_b", 1 }, { "tempomode", 1 } }) });
        list.push_back({ "Filter drop", true, mix({ { "fxtarget", 3 }, { "fxt1", 1 }, { "fxon1", 1 }, { "fxp1_1", 0.1f }, { "fxp2_1", 1 }, { "fxon2", 0 }, { "fxon3", 0 } }) });
        auto files = folder().findChildFiles(juce::File::findFiles, false, "*.duopreset");
        files.sort();
        for (auto& f : files)
            if (auto xml = juce::XmlDocument::parse(f))
                if (xml->hasTagName("DUOPRESET"))
                {
                    Preset p; p.name = xml->getStringAttribute("name", f.getFileNameWithoutExtension());
                    for (auto* e : xml->getChildIterator()) if (e->hasTagName("P")) p.v[e->getStringAttribute("id")] = (float) e->getDoubleAttribute("v");
                    if (! p.v.empty()) list.push_back(p);
                }
        const auto cur = proc.apvts.state.getProperty("preset", "Init").toString();
        current = 0; for (size_t i = 0; i < list.size(); ++i) if (list[i].name == cur) current = (int) i;
    }
    void apply(int i)
    {
        if (i < 0 || i >= (int) list.size()) return;
        current = i; proc.apvts.state.setProperty("preset", list[(size_t) i].name, nullptr);
        for (auto& kv : list[(size_t) i].v)
            if (stored(kv.first)) if (auto* p = proc.apvts.getParameter(kv.first)) { p->beginChangeGesture(); p->setValueNotifyingHost(p->convertTo0to1(kv.second)); p->endChangeGesture(); }
    }
    bool exists(const juce::String& name) const { for (auto& p : list) if (p.name.equalsIgnoreCase(name)) return true; return false; }
    bool isFactory(const juce::String& name) const { for (auto& p : list) if (p.factory && p.name.equalsIgnoreCase(name)) return true; return false; }
    juce::File fileFor(const juce::String& name) const { return folder().getChildFile(juce::File::createLegalFileName(name) + ".duopreset"); }
    // Saves the current settings under a name. Returns false if the name belongs to a factory preset or the file cannot be written.
    bool saveUser(const juce::String& rawName)
    {
        const auto name = rawName.trim().substring(0, 40); if (name.isEmpty() || isFactory(name)) return false;
        juce::XmlElement x("DUOPRESET"); x.setAttribute("name", name);
        for (auto* pr : proc.getParameters()) if (auto* r = dynamic_cast<juce::RangedAudioParameter*>(pr)) if (stored(r->paramID))
        { auto* e = x.createNewChildElement("P"); e->setAttribute("id", r->paramID); e->setAttribute("v", (double) r->convertFrom0to1(r->getValue())); }
        folder().createDirectory(); const bool ok = x.writeTo(fileFor(name));
        if (ok) { proc.apvts.state.setProperty("preset", name, nullptr); reload(); }
        return ok;
    }
    bool deleteUser(const juce::String& name) { if (isFactory(name)) return false; const bool ok = fileFor(name).deleteFile(); reload(); return ok; }
    bool exportTo(const juce::String& name, const juce::File& dest) const { return fileFor(name).existsAsFile() && fileFor(name).copyFileTo(dest); }
    bool importFrom(const juce::File& src)
    {
        auto xml = juce::XmlDocument::parse(src); if (xml == nullptr || ! xml->hasTagName("DUOPRESET")) return false;
        auto name = xml->getStringAttribute("name", src.getFileNameWithoutExtension()).trim().substring(0, 40); if (name.isEmpty() || isFactory(name)) return false;
        xml->setAttribute("name", name); folder().createDirectory(); const bool ok = xml->writeTo(fileFor(name)); reload(); return ok;
    }
private:
    DuoProcessor& proc;
};
}   // namespace duoui
