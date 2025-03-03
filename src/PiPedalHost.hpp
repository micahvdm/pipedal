// Copyright (c) 2022 Robin Davies
//
// Permission is hereby granted, free of charge, to any person obtaining a copy of
// this software and associated documentation files (the "Software"), to deal in
// the Software without restriction, including without limitation the rights to
// use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
// the Software, and to permit persons to whom the Software is furnished to do so,
// subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
// FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
// COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
// IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
// CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

#pragma once

#include <vector>
#include <memory>
#include "json.hpp"
#include "PluginType.hpp"
#include "PedalBoard.hpp"
#include <lilv/lilv.h>
#include "MapFeature.hpp"
#include "LogFeature.hpp"
#include "OptionsFeature.hpp"
#include <filesystem>
#include <cmath>
#include <string>

#include "lv2.h"
#include "Units.hpp"
#include "PluginPreset.hpp"

#include "IEffect.hpp"
#include "PiPedalConfiguration.hpp"


namespace pipedal {


// forward declarations
class Lv2Effect;
class Lv2PedalBoard;
class PiPedalHost;
class JackConfiguration;
class JackChannelSelection;

#ifndef LV2_PROPERTY_GETSET
#define LV2_PROPERTY_GETSET(name) \
    const decltype(name##_) & name() const { return name##_; }; \
    decltype(name##_) & name()  { return name##_; }; \
    void  name(const decltype(name##_) &value) { name##_ = value; }; 
#endif


#ifndef LV2_PROPERTY_GETSET_SCALAR
#define LV2_PROPERTY_GETSET_SCALAR(name) \
    decltype(name##_) name() const { return name##_;}; \
    void  name(decltype(name##_) value) { name##_ = value; }; 
#endif





class Lv2PluginClass {
public:
    friend class PiPedalHost;
private:
    Lv2PluginClass*  parent_ = nullptr; // NOT SERIALIZED!
    std::string parent_uri_;
    std::string display_name_;
    std::string uri_;
    PluginType plugin_type_;
    std::vector<std::shared_ptr<Lv2PluginClass>> children_;


    friend class ::pipedal::PiPedalHost;
    // hide copy constructor.
    Lv2PluginClass(const Lv2PluginClass &other)
    {

    }
    void set_parent(std::shared_ptr<Lv2PluginClass> &parent)
    {
        this->parent_ = parent.get();
    }
    void add_child(std::shared_ptr<Lv2PluginClass> &child) {
        for (size_t i = 0; i < children_.size(); ++i)
        {
            if (children_[i]->uri_ == child->uri_)
            {
                return;
            }
        }
        children_.push_back(child);
    }
public:
    Lv2PluginClass(
        const char*display_name, const char*uri, const char*parent_uri)
    : parent_uri_(parent_uri)
    , display_name_(display_name)
    , uri_(uri)
    , plugin_type_(uri_to_plugin_type(uri))
    {

    }
    Lv2PluginClass() { }
    LV2_PROPERTY_GETSET(uri)
    LV2_PROPERTY_GETSET(display_name)
    LV2_PROPERTY_GETSET(parent_uri)

    const Lv2PluginClass *parent() { return parent_;}

    bool operator==(const Lv2PluginClass &other)
    {
        return uri_ == other.uri_;
    }
    bool is_a(const std::string& classUri) const;

    static json_map::storage_type<Lv2PluginClass> jmap;

};
class Lv2PluginClasses {
private:
    std::vector<std::string>  classes_;
public:
    Lv2PluginClasses()
    {

    }
    Lv2PluginClasses(std::vector<std::string>  classes)
    :classes_(classes)
    {

    }
    const std::vector<std::string>  & classes() const
    {
        return classes_;
    }
    bool is_a(PiPedalHost*lv2Plugins,const char*classUri) const;

    static json_map::storage_type<Lv2PluginClasses> jmap;

};


class Lv2ScalePoint {
private:
    float value_;
    std::string label_;
public:
    Lv2ScalePoint() { }
    Lv2ScalePoint(float value, std::string label)
    : value_(value)
    , label_(label)
    {

    }

    LV2_PROPERTY_GETSET_SCALAR(value);
    LV2_PROPERTY_GETSET(label);
    
    static json_map::storage_type<Lv2ScalePoint> jmap;
};


enum class Lv2BufferType {
    None,
    Event,
    Sequence,
    Unknown
};

class Lv2PortInfo {
public:
    Lv2PortInfo(PiPedalHost*lv2Host,const LilvPlugin*pPlugin,const LilvPort *pPort);
private:
    friend class Lv2PluginInfo;

    uint32_t index_;
    std::string symbol_;
    std::string name_;
    float min_value_, max_value_, default_value_;
    Lv2PluginClasses classes_;
    std::vector<Lv2ScalePoint> scale_points_;
    bool is_input_ = false;
    bool is_output_ = false;

    bool is_control_port_ = false;
    bool is_audio_port_ = false;
    bool is_atom_port_ = false;
    bool is_cv_port_ = false;

    bool is_valid_ = false;
    bool supports_midi_ = false;
    bool supports_time_position_ = false;
    bool is_logarithmic_ = false;
    int display_priority_ = -1;
    int range_steps_ = 0;
    bool trigger_;
    bool integer_property_;
    bool enumeration_property_;
    bool toggled_property_;
    bool not_on_gui_;
    std::string buffer_type_;
    std::string port_group_;

    std::string designation_;
    Units units_ = Units::none;
    std::string comment_;
public:
    bool IsSwitch() const {
        return min_value_ == 0 && max_value_ == 1 
            && (integer_property_ || toggled_property_ || enumeration_property_);
    }
    float rangeToValue(float range) const
    {
        float value;
        if (is_logarithmic_)
        {
            value = std::pow(max_value_/min_value_,range)*min_value_;
        } else {
            value = (max_value_-min_value_)*range+min_value_;
        }
        if (integer_property_ || enumeration_property_)
        {
            value = std::round(value);
        } else if (range_steps_ >= 2)
        {
            value = std::round(value*(range_steps_-1))/(range_steps_-1);
        }
        if (toggled_property_)
        {
            value = value == 0 ? 0: max_value_;
        }
        if (value > max_value_) value = max_value_;
        if (value < min_value_) value = min_value_;
        return value;
    }
    LV2_PROPERTY_GETSET(symbol);
    LV2_PROPERTY_GETSET_SCALAR(index);
    LV2_PROPERTY_GETSET(name);
    LV2_PROPERTY_GETSET(classes);
    LV2_PROPERTY_GETSET(scale_points);
    LV2_PROPERTY_GETSET_SCALAR(min_value);
    LV2_PROPERTY_GETSET_SCALAR(max_value);
    LV2_PROPERTY_GETSET_SCALAR(default_value);

    LV2_PROPERTY_GETSET_SCALAR(is_input);
    LV2_PROPERTY_GETSET_SCALAR(is_output);
    LV2_PROPERTY_GETSET_SCALAR(is_control_port);
    LV2_PROPERTY_GETSET_SCALAR(is_audio_port);
    LV2_PROPERTY_GETSET_SCALAR(is_atom_port);
    LV2_PROPERTY_GETSET_SCALAR(is_cv_port);
    LV2_PROPERTY_GETSET_SCALAR(is_valid);
    LV2_PROPERTY_GETSET_SCALAR(supports_midi);
    LV2_PROPERTY_GETSET_SCALAR(supports_time_position);
    LV2_PROPERTY_GETSET_SCALAR(is_logarithmic);
    LV2_PROPERTY_GETSET_SCALAR(display_priority);
    LV2_PROPERTY_GETSET_SCALAR(range_steps);
    LV2_PROPERTY_GETSET_SCALAR(trigger);
    LV2_PROPERTY_GETSET_SCALAR(integer_property);
    LV2_PROPERTY_GETSET_SCALAR(enumeration_property);
    LV2_PROPERTY_GETSET_SCALAR(toggled_property);
    LV2_PROPERTY_GETSET_SCALAR(not_on_gui);
    LV2_PROPERTY_GETSET(port_group);
    LV2_PROPERTY_GETSET(comment);
    LV2_PROPERTY_GETSET_SCALAR(units);

    LV2_PROPERTY_GETSET(buffer_type);

    Lv2BufferType GetBufferType() {
        if (buffer_type_ == "") return Lv2BufferType::None;
        if (buffer_type_ == LV2_ATOM__Sequence) return Lv2BufferType::Sequence;
        return Lv2BufferType::Unknown;
    }

public:
    Lv2PortInfo() { }
    ~Lv2PortInfo() = default;
    bool is_a(PiPedalHost*lv2Plugins,const char*classUri);

    static json_map::storage_type<Lv2PortInfo> jmap;
    
};

class Lv2PortGroup {
private:
    std::string uri_;
    std::string symbol_;
    std::string name_;

public:
    LV2_PROPERTY_GETSET(uri);
    LV2_PROPERTY_GETSET(symbol);
    LV2_PROPERTY_GETSET(name);

    Lv2PortGroup() { }
    Lv2PortGroup(PiPedalHost*lv2Host,const std::string &groupUri);

    static json_map::storage_type<Lv2PortGroup> jmap;
};

class Lv2PluginInfo {
private:
    friend class PiPedalHost;
public:
    Lv2PluginInfo(PiPedalHost*lv2Host,const LilvPlugin*);
    Lv2PluginInfo() { }
private:
    bool HasFactoryPresets(PiPedalHost*lv2Host, const LilvPlugin*plugin);
    std::string uri_;
    std::string name_;
    std::string plugin_class_;
    std::vector<std::string> supported_features_;
    std::vector<std::string> required_features_;
    std::vector<std::string> optional_features_;
    std::vector<std::string> extensions_;
    bool has_factory_presets_ = false;

    std::string author_name_;
    std::string author_homepage_;

    std::string comment_;
    std::vector<std::shared_ptr<Lv2PortInfo> > ports_;
    std::vector<std::shared_ptr<Lv2PortGroup> > port_groups_;
    bool is_valid_ = false;

    bool IsSupportedFeature(const std::string &feature) const;

public:
    LV2_PROPERTY_GETSET(uri)
    LV2_PROPERTY_GETSET(name)
    LV2_PROPERTY_GETSET(plugin_class)
    LV2_PROPERTY_GETSET(supported_features)
    LV2_PROPERTY_GETSET(required_features)
    LV2_PROPERTY_GETSET(optional_features)
    LV2_PROPERTY_GETSET(author_name)
    LV2_PROPERTY_GETSET(author_homepage)
    LV2_PROPERTY_GETSET(comment)
    LV2_PROPERTY_GETSET(extensions)
    LV2_PROPERTY_GETSET(ports)
    LV2_PROPERTY_GETSET(is_valid)
    LV2_PROPERTY_GETSET(port_groups)
    LV2_PROPERTY_GETSET(has_factory_presets)

    const Lv2PortInfo& getPort(const std::string&symbol)
    {
        for (size_t i = 0; i < ports_.size(); ++i)
        {
            if (ports_[i]->symbol() == symbol)
            {
                return *(ports_[i].get());
            }
        }
        throw PiPedalArgumentException("Port not found.");
    }
    bool hasExtension(const std::string&uri)
    {
        for (int i = 0; i < extensions_.size(); ++i)
        {
            if (extensions_[i] == uri) return true;
        }
        return false;
    }
    bool hasCvPorts() const {
        for (size_t i = 0; i < ports_.size(); ++i)
        {
            if (ports_[i]->is_cv_port())
            {
                return true;
            }
        }
        return false;
    }
    bool hasMidiInput() const {
        for (size_t i = 0; i < ports_.size(); ++i)
        {
            if (ports_[i]->is_atom_port() && ports_[i]->supports_midi() && ports_[i]->is_input())
            {
                return true;
            }
        }
    }
    bool hasMidiOutput() const {
        for (size_t i = 0; i < ports_.size(); ++i)
        {
            if (ports_[i]->is_atom_port() && ports_[i]->supports_midi() && ports_[i]->is_output())
            {
                return true;
            }
        }
    }


public:

    virtual ~Lv2PluginInfo();

    static json_map::storage_type<Lv2PluginInfo> jmap;

};

    class Lv2PluginUiPortGroup
    {
    private:
        std::string symbol_;
        std::string name_;

        std::string parent_group_;
        int32_t program_list_id_ = -1; // used by VST3.
    public:
        LV2_PROPERTY_GETSET(symbol)
        LV2_PROPERTY_GETSET(name)
        LV2_PROPERTY_GETSET(parent_group)
        LV2_PROPERTY_GETSET_SCALAR(program_list_id)

    public:
        Lv2PluginUiPortGroup() {}
        Lv2PluginUiPortGroup(Lv2PortGroup *pPortGroup)
            : symbol_(pPortGroup->symbol()), name_(pPortGroup->name())
        {
        }
        Lv2PluginUiPortGroup(
            const std::string &symbol, const std::string &name,
            const std::string &parent_group, int32_t programListId)
        : symbol_(symbol),name_(name),parent_group_(parent_group),program_list_id_(programListId)
        {

        }

    public:
        static json_map::storage_type<Lv2PluginUiPortGroup> jmap;
    };

    class Lv2PluginUiControlPort
    {
    public:
        Lv2PluginUiControlPort()
        {
        }
        Lv2PluginUiControlPort(const Lv2PluginInfo *pPlugin, const Lv2PortInfo *pPort)
            : symbol_(pPort->symbol()), index_(pPort->index()), name_(pPort->name()), min_value_(pPort->min_value()), max_value_(pPort->max_value()), default_value_(pPort->default_value()), range_steps_(pPort->range_steps()), display_priority_(pPort->display_priority()), is_logarithmic_(pPort->is_logarithmic()), integer_property_(pPort->integer_property()), enumeration_property_(pPort->enumeration_property()), toggled_property_(pPort->toggled_property()), not_on_gui_(pPort->not_on_gui()), scale_points_(pPort->scale_points()), comment_(pPort->comment()), units_(pPort->units())
        {
            // Use symbols to index port groups, instead of uris.
            // symbols are guaranteed to be unique.
            auto &portGroup = pPort->port_group();
            for (int i = 0; i < pPlugin->port_groups().size(); ++i)
            {

                auto &p = pPlugin->port_groups()[i];
                if (p->uri() == portGroup)
                {
                    this->port_group_ = p->symbol();
                    break;
                }
            }
        }

    private:
        std::string symbol_;
        int index_;
        std::string name_;
        float min_value_ = 0, max_value_ = 1, default_value_ = 0;
        int range_steps_ = 0;
        int display_priority_ = -1;
        bool is_logarithmic_ = false;
        bool integer_property_ = false;
        bool enumeration_property_ = false;
        bool toggled_property_ = false;
        bool not_on_gui_ = false;
        std::vector<Lv2ScalePoint> scale_points_;
        std::string port_group_;
        Units units_ = Units::none;
        std::string comment_;
        bool is_bypass_ = false;
        bool is_program_controller_ = false;
        std::string custom_units_;

    public:
        LV2_PROPERTY_GETSET(symbol);
        LV2_PROPERTY_GETSET_SCALAR(index);
        LV2_PROPERTY_GETSET(name);
        LV2_PROPERTY_GETSET(port_group);
        LV2_PROPERTY_GETSET_SCALAR(min_value);
        LV2_PROPERTY_GETSET_SCALAR(max_value);
        LV2_PROPERTY_GETSET_SCALAR(default_value);
        LV2_PROPERTY_GETSET_SCALAR(range_steps);
        LV2_PROPERTY_GETSET_SCALAR(display_priority);
        LV2_PROPERTY_GETSET_SCALAR(is_logarithmic);
        LV2_PROPERTY_GETSET_SCALAR(integer_property);
        LV2_PROPERTY_GETSET_SCALAR(enumeration_property);
        LV2_PROPERTY_GETSET_SCALAR(toggled_property);
        LV2_PROPERTY_GETSET_SCALAR(not_on_gui);
        LV2_PROPERTY_GETSET(scale_points);
        LV2_PROPERTY_GETSET(units);
        LV2_PROPERTY_GETSET(comment);
        LV2_PROPERTY_GETSET_SCALAR(is_bypass);
        LV2_PROPERTY_GETSET_SCALAR(is_program_controller);
        LV2_PROPERTY_GETSET(custom_units);

    public:
        static json_map::storage_type<Lv2PluginUiControlPort> jmap;
    };

    class Lv2PluginUiInfo
    {
    public:
        Lv2PluginUiInfo() {}
        Lv2PluginUiInfo(PiPedalHost *pPlugins, const Lv2PluginInfo *plugin);

    private:
        std::string uri_;
        std::string name_;
        std::string author_name_;
        std::string author_homepage_;
        PluginType plugin_type_;
        std::string plugin_display_type_;
        int audio_inputs_ = 0;
        int audio_outputs_ = 0;
        int has_midi_input_ = false;
        int has_midi_output_ = false;
        std::string description_;
        bool is_vst3_ = false;

        std::vector<Lv2PluginUiControlPort> controls_;
        std::vector<Lv2PluginUiPortGroup> port_groups_;

    public:
        LV2_PROPERTY_GETSET(uri)
        LV2_PROPERTY_GETSET(name)
        LV2_PROPERTY_GETSET(author_name)
        LV2_PROPERTY_GETSET(author_homepage)
        LV2_PROPERTY_GETSET_SCALAR(plugin_type)
        LV2_PROPERTY_GETSET(plugin_display_type)
        LV2_PROPERTY_GETSET_SCALAR(audio_inputs)
        LV2_PROPERTY_GETSET_SCALAR(audio_outputs)
        LV2_PROPERTY_GETSET_SCALAR(has_midi_input)
        LV2_PROPERTY_GETSET_SCALAR(has_midi_output)
        LV2_PROPERTY_GETSET_SCALAR(description)
        LV2_PROPERTY_GETSET(controls)
        LV2_PROPERTY_GETSET(port_groups)
        LV2_PROPERTY_GETSET_SCALAR(is_vst3)

        static json_map::storage_type<Lv2PluginUiInfo> jmap;
    };



class IHost {
public:
    virtual LilvWorld*getWorld() = 0;
    virtual LV2_URID_Map* GetLv2UridMap()  = 0;
    virtual LV2_URID GetLv2Urid(const char*uri) = 0;
    virtual std::string Lv2UriudToString(LV2_URID urid) = 0;
    
    virtual LV2_Feature*const*GetLv2Features() const = 0;
    virtual double GetSampleRate() const = 0;
    virtual void SetMaxAudioBufferSize(size_t size) = 0;
    virtual size_t GetMaxAudioBufferSize() const = 0;
    virtual size_t GetAtomBufferSize() const = 0;

    virtual bool HasMidiInputChannel() const = 0;
    virtual int GetNumberOfInputAudioChannels() const = 0;
    virtual int GetNumberOfOutputAudioChannels() const = 0;
    virtual std::shared_ptr<Lv2PluginInfo> GetPluginInfo(const std::string&uri) const = 0;


    virtual IEffect*CreateEffect(PedalBoardItem &pedalBoard) = 0;
};


class LilvNodePtr {
protected:
    LilvNode *node = nullptr;
    LilvNodePtr(const LilvNodePtr&) {}; // no copy.
    LilvNodePtr(const LilvNode*) {}; // const LilvNodes are owned by lilv by convention.
public:
    LilvNodePtr() { }
    LilvNodePtr(LilvNode*node) { this->node = node; }
    ~LilvNodePtr() { Free(); }
    void Free() { if (node != nullptr) lilv_node_free(node); node = nullptr;}

    operator const LilvNode *()
    {
        return this->node;
    }

    LilvNode **operator&()
    {
        return &(this->node);
    }
    operator bool()
    {
        return this->node != nullptr;
    }
    LilvNodePtr&operator=(LilvNode*node) { Free(); this->node = node; return *this;}
};

}

#if ENABLE_VST3
#include "vst3/Vst3Host.hpp"
#endif


namespace pipedal{




class PiPedalHost: private IHost {
private:
#if ENABLE_VST3
    Vst3Host::Ptr vst3Host;

#endif
    static const char *RDFS_COMMENT_URI;
    class LilvUris {
    public:
        void Initialize(LilvWorld*pWorld);
        void Free();

        LilvNodePtr  rdfsComment;
        LilvNodePtr logarithic_uri;
        LilvNodePtr display_priority_uri;
        LilvNodePtr range_steps_uri;
        LilvNodePtr integer_property_uri;
        LilvNodePtr enumeration_property_uri;
        LilvNodePtr toggle_property_uri;
        LilvNodePtr not_on_gui_property_uri;
        LilvNodePtr midiEventNode;
        LilvNodePtr designationNode;
        LilvNodePtr portGroupUri;
        LilvNodePtr unitsUri;
        LilvNodePtr bufferType_uri;
        LilvNodePtr pset_Preset;
        LilvNodePtr rdfs_label;
        LilvNodePtr symbolUri;
        LilvNodePtr nameUri;

        LilvNodePtr time_Position;
        LilvNodePtr time_barBeat;
        LilvNodePtr time_beatsPerMinute;
        LilvNodePtr time_speed;

        LilvNodePtr appliesTo;
        LilvNodePtr isA;


    };
    bool vst3Enabled = true;

    LilvUris lilvUris;

    LilvNode *get_comment(const std::string &uri);


    size_t maxBufferSize = 1024;
    size_t maxAtomBufferSize = 16*1024;
    bool hasMidiInputChannel;
    int numberOfAudioInputChannels = 2;
    int numberOfAudioOutputChannels = 2;
    double sampleRate = 0;

    std::string vst3CachePath;

    LV2_Feature*const*lv2Features = nullptr;
    MapFeature mapFeature;
    LogFeature logFeature;
    OptionsFeature optionsFeature;

    static void fn_LilvSetPortValueFunc(const char* port_symbol,
                                     void*       user_data,
                                     const void* value,
                                     uint32_t    size,
                                     uint32_t    type);

    LilvWorld *pWorld;
    void free_world();


    std::vector<std::shared_ptr<Lv2PluginInfo> > plugins_;
    std::vector<Lv2PluginUiInfo> ui_plugins_;

    std::map<std::string,std::shared_ptr<Lv2PluginClass> > classesMap;

    friend class Lv2PluginInfo;
    friend class Lv2PortInfo;
    friend class Lv2PortGroup;

    std::shared_ptr<Lv2PluginClass> GetPluginClass(const LilvPluginClass*pClass);
    std::shared_ptr<Lv2PluginClass> MakePluginClass(const LilvPluginClass*pClass);
    Lv2PluginClasses GetPluginPortClass(const LilvPlugin*lilvPlugin,const LilvPort *lilvPort);

    bool classesLoaded = false;

private:
    // IHost implementation 
    virtual LilvWorld*getWorld() {
        return pWorld;
    }
    virtual void SetMaxAudioBufferSize(size_t size) { maxBufferSize = size; }
    virtual size_t GetMaxAudioBufferSize() const { return maxBufferSize; }
    virtual size_t GetAtomBufferSize() const { return maxAtomBufferSize; }
    virtual bool HasMidiInputChannel() const { return hasMidiInputChannel;}
    virtual int GetNumberOfInputAudioChannels() const { return numberOfAudioInputChannels; }
    virtual int GetNumberOfOutputAudioChannels() const { return numberOfAudioOutputChannels;}
    virtual LV2_Feature*const* GetLv2Features() const { return this->lv2Features; }
    virtual LV2_URID_Map* GetLv2UridMap()  {
        return this->mapFeature.GetMap();
    }
    static void PortValueCallback(const char*symbol,void*user_data,const void* value,uint32_t size, uint32_t type);

    virtual IEffect*CreateEffect(PedalBoardItem &pedalBoardItem);
    void LoadPluginClassesFromLilv();
    void AddJsonClassesToMap(std::shared_ptr<Lv2PluginClass> pluginClass);

public:
    PiPedalHost();

    void SetConfiguration(const PiPedalConfiguration&configuration);

    virtual ~PiPedalHost();

    IHost* asIHost() { return this;}

    virtual Lv2PedalBoard * CreateLv2PedalBoard(PedalBoard& pedalBoard);

    void setSampleRate(double sampleRate)
    {
        this->sampleRate = sampleRate;
    }
    double GetSampleRate() const {
        return sampleRate;
    }

    class Urids;

    Urids *urids;

    void OnConfigurationChanged(const JackConfiguration &configuration, const JackChannelSelection &settings);

    std::shared_ptr<Lv2PluginClass> GetPluginClass(const std::string&uri) const;
    bool is_a(const std::string&class_, const std::string & target_class);

    std::shared_ptr<Lv2PluginClass> GetLv2PluginClass() const;

    std::vector<std::shared_ptr<Lv2PluginInfo> > GetPlugins() const { return plugins_; }
    const std::vector<Lv2PluginUiInfo> &GetUiPlugins() const { return ui_plugins_; }

    virtual std::shared_ptr<Lv2PluginInfo> GetPluginInfo(const std::string&uri) const;


    static constexpr const char * DEFAULT_LV2_PATH="/usr/lib/lv2";


    void LoadPluginClassesFromJson(std::filesystem::path jsonFile);
    void Load(const char*lv2Path = PiPedalHost::DEFAULT_LV2_PATH);

    virtual LV2_URID GetLv2Urid(const char*uri)  {
        return this->mapFeature.GetUrid(uri);
    }
    virtual std::string Lv2UriudToString(LV2_URID urid) {
        return this->mapFeature.UridToString(urid);
    }

    PluginPresets GetFactoryPluginPresets(const std::string &pluginUri);
    std::vector<ControlValue>  LoadFactoryPluginPreset(PedalBoardItem*pedalBoardItem,
        const std::string&presetUri);


};

#undef LV2_PROPERTY_GETSET
#undef LV2_PROPERTY_GETSET_SCALAR
} // namespace pipedal.