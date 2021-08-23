// Copyright (c) 2021 Robin Davies
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


import {PiPedalArgumentError} from "./PiPedalError";
import Units from './Units';

interface Deserializable<T> {
    deserialize(input: any): T;
}


export class  Port implements Deserializable<Port> {
    deserialize(input: any): Port {
        this.port_index = input.port_index;
        this.symbol = input.symbol;
        this.name = input.name;
        this.min_value = input.min_value;
        this.max_value = input.max_value;
        this.default_value = input.default_value;
        this.scale_points = ScalePoint.deserialize_array(input.scale_points);
        this.is_input = input.is_input;
        this.is_output = input.is_output;
        this.is_control_port = input.is_control_port;
        this.is_audio_port = input.is_audio_port;
        this.is_atom_port = input.is_atom_port;
        this.is_valid = input.is_valid;
        this.supports_midi = input.supports_midi;
        this.port_group = input.port_group;
        return this;
    }

    static EmptyPorts: Port[] = [];

    static deserialize_array(input: any): Port[] {
        let result: Port[] = [];
        for (let i = 0; i < input.length; ++i)
        {
            result[i] = new Port().deserialize(input[i]);
        }
        return result;
    }
    port_index:      number = -1;
    symbol:          string = "";
    name:            string = "";
    min_value:       number = 0;
    max_value:       number = 1;
    default_value:   number = 0.5;
    scale_points:    ScalePoint[] = [];
    is_input:        boolean = false;
    is_output:       boolean = false
    is_control_port: boolean = false;
    is_audio_port:   boolean = false;
    is_atom_port:    boolean = false;
    is_valid:        boolean = false;
    supports_midi:   boolean = false;
    port_group:      string = "";
}

export class PortGroup {
    deserialize(input: any): PortGroup {
        this.symbol = input.symbol;
        this.name = input.name;
        return this;
    }
    static deserialize_array(input: any) : PortGroup[] {
        let result: PortGroup[] = [];
        for (let i = 0; i < input.length; ++i)
        {
            result.push(new PortGroup().deserialize(input[i]));
        }
        return result;
    }

    symbol: string = "";
    name: string = "";
};

export class  Lv2Plugin implements Deserializable<Lv2Plugin> {
    deserialize(input: any): Lv2Plugin
    {
        this.uri = input.uri;
        this.name = input.name;
        this.plugin_class = input.plugin_class;
        this.supported_features = input.supported_features;
        this.required_features = input.required_features;
        this.optional_features = input.optional_features;
        this.author_name = input.author_name;
        this.author_homepage = input.author_homepage;
        this.comment = input.comment;
        this.ports=  Port.deserialize_array(input.ports);
        this.port_groups = PortGroup.deserialize_array(input.port_groups);
        return this;
    }
    static EmptyFeatures: string[] = [];

    uri:                string = "";
    name:               string = "";
    plugin_class:       string = "";
    supported_features: string[] = Lv2Plugin.EmptyFeatures;
    required_features:  string[] = Lv2Plugin.EmptyFeatures;
    optional_features:  string[] = Lv2Plugin.EmptyFeatures;
    author_name:        string = "";
    author_homepage:    string = "";
    comment:            string = "";
    ports:              Port[] = Port.EmptyPorts;
    port_groups: PortGroup[] = [];
}


export class  ScalePoint implements Deserializable<ScalePoint> {
    deserialize(input: any): ScalePoint {
        this.value = input.value;
        this.label = input.label;
        return this;
    }
    static deserialize_array(input: any): ScalePoint[]
    {
        let result: ScalePoint[] = [];
        for (let i = 0; i < input.length; ++i)
        {
            result[i] = new ScalePoint().deserialize(input[i]);
        }
        return result;
    }
    value: number = 0;
    label: string = "";
}

export enum PluginType {
    // Reserved types used in pedalboards.
    None="",
    InvalidPlugin= "InvalidPlugin",
    
    Plugin = "Plugin",
    AllpassPlugin = "AllpassPlugin",
    AmplifierPlugin = "AmplifierPlugin",
    AnalyserPlugin = "AnalyserPlugin",
    BandpassPlugin = "BandpassPlugin",
    ChorusPlugin = "ChorusPlugin",
    CombPlugin = "CombPlugin",
    CompressorPlugin = "CompressorPlugin",
    ConstantPlugin = "ConstantPlugin",
    ConverterPlugin = "ConverterPlugin",
    DelayPlugin = "DelayPlugin",
    DistortionPlugin = "DistortionPlugin",
    DynamicsPlugin = "DynamicsPlugin",
    EQPlugin = "EQPlugin",
    EnvelopePlugin = "EnvelopePlugin",
    ExpanderPlugin = "ExpanderPlugin",
    FilterPlugin = "FilterPlugin",
    FlangerPlugin = "FlangerPlugin",
    FunctionPlugin = "FunctionPlugin",
    GatePlugin = "GatePlugin",
    GeneratorPlugin = "GeneratorPlugin",
    HighpassPlugin = "HighpassPlugin",
    InstrumentPlugin = "InstrumentPlugin",
    LimiterPlugin = "LimiterPlugin",
    LowpassPlugin = "LowpassPlugin",
    MixerPlugin = "MixerPlugin",
    ModulatorPlugin = "ModulatorPlugin",
    MultiEQPlugin = "MultiEQPlugin",
    OscillatorPlugin = "OscillatorPlugin",
    ParaEQPlugin = "ParaEQPlugin",
    PhaserPlugin = "PhaserPlugin",
    PitchPlugin = "PitchPlugin",
    ReverbPlugin = "ReverbPlugin",
    SimulatorPlugin = "SimulatorPlugin",
    SpatialPlugin = "SpatialPlugin",
    SpectralPlugin = "SpectralPlugin",
    UtilityPlugin = "UtilityPlugin",
    WaveshaperPlugin = "WaveshaperPlugin"
}


export class  UiControl implements Deserializable<UiControl> {
    deserialize(input: any): UiControl
    {
        this.symbol = input.symbol;
        this.name = input.name;
        this.index = input.index;
        this.min_value = input.min_value;
        this.max_value = input.max_value;
        this.default_value = input.default_value;
        this.is_logarithmic = input.is_logarithmic;
        this.display_priority = input.display_priority;
        this.range_steps = input.range_steps;
        this.integer_property = input.integer_property;
        this.enumeration_property = input.enumeration_property;
        this.toggled_property = input.toggled_property;
        this.trigger = input.trigger;
        this.not_on_gui = input.not_on_gui;
        this.scale_points = ScalePoint.deserialize_array(input.scale_points);
        this.port_group = input.port_group;
        this.units = input.units as Units;
        return this;

    }
    static deserialize_array(input: any): UiControl[] {
        let result: UiControl[] = [];
        for (let i = 0; i < input.length; ++i)
        {
            result[i] = new UiControl().deserialize(input[i]);
        }
        return result;
    }
    symbol: string = "";
    name:   string = "";
    index: number = -1;
    min_value: number = 0;
    max_value: number = 1;
    default_value:number = 0.5;
    is_logarithmic: boolean = false;
    display_priority: number = -1;
    range_steps: number = 0;
    integer_property:boolean = false;
    enumeration_property: boolean = false;
    trigger: boolean = false;
    not_on_gui: boolean = false;
    toggled_property: boolean = false;
    scale_points: ScalePoint[] = [];
    port_group: string = "";
    units: Units = Units.none;

    isOnOffSwitch() : boolean {
        if (this.isAbToggle()) return false;
        if (this.min_value !== 0 || this.max_value !== 1) return false;
        return (this.toggled_property || this.enumeration_property || this.integer_property);
            ;
    }

    isAbToggle(): boolean {
        if (this.min_value !== 0 || this.max_value !== 1) return false;
        return this.enumeration_property && this.scale_points.length === 2;
    }
    isSelect() : boolean {
        return this.enumeration_property && !this.isOnOffSwitch() && !this.isAbToggle();
    }

    valueToRange(value: number): number {
        if (this.toggled_property) return value === 0 ? 0: 1;

        if (this.integer_property || this.enumeration_property) {
            value = Math.round(value);
        }
        let range = (value - this.min_value) / (this.max_value - this.min_value);
        if (range > 1) range = 1;
        if (range < 0) range = 0;

        if (this.range_steps !== 0) {
            range = Math.round(range*this.range_steps)/this.range_steps;
        }

        return range;
    }

    rangeToValue(range: number) : number {
        if (range < 0) range = 0;
        if (range > 1) range = 1;

        if (this.toggled_property) return range === 0? 0: 1;
        if (this.range_steps !== 0) {
            range = Math.round(range * this.range_steps) / this.range_steps;
        }
        let value = range * (this.max_value - this.min_value) + this.min_value;
        if (this.integer_property || this.enumeration_property) {
            value = Math.round(value);
        }
        return value;
    }
    clampValue(value: number): number {
        return this.rangeToValue(this.valueToRange(value));
    }

    formatValue(value: number): string {
        if (this.integer_property || this.enumeration_property) {
            value = Math.round(value);
        }
        if (this.enumeration_property) {
            for (let i = 0; i < this.scale_points.length; ++i) {
                let scale_point = this.scale_points[i];
                if (scale_point.value === value) {
                    return scale_point.label;
                }
            }
            return "#invalid";
        } else if (this.integer_property) {
            return value.toFixed(0);
        } else {
            if (value >= 100 || value <= -100) {
                return value.toFixed(0);
            }
            if (value >= 10 || value <= -10) {
                return value.toFixed(1);
            }
            return value.toFixed(2);
        }
    }

    
}

export class UiPlugin implements Deserializable<UiPlugin> {
    deserialize(input: any): UiPlugin 
    {
        this.uri = input.uri;
        this.name = input.name;
        this.plugin_type = input.plugin_type as PluginType;
        this.plugin_display_type = input.plugin_display_type;
        this.author_name = input.author_name;
        this.author_homepage = input.author_homepage;
        this.audio_inputs = input.audio_inputs;
        this.audio_outputs = input.audio_outputs;
        this.has_midi_input = input.has_midi_input;
        this.has_midi_output = input.has_midi_output;
        this.description = input.description;
        this.controls = UiControl.deserialize_array(input.controls);
        this.port_groups = PortGroup.deserialize_array(input.port_groups);
        return this;

    }
    static deserialize_array(input: any): UiPlugin[] {
        let result: UiPlugin[] = [];
        for (let i = 0; i < input.length; ++i)
        {
            result[i] = new UiPlugin().deserialize(input[i]);
        }
        return result;
    }
    getControl(key: string): UiControl | undefined {
        for (let i = 0; i < this.controls.length; ++i)
        {
            let control = this.controls[i];
            if (control.symbol === key)
            {
                return control;
            }
        } 
        return undefined;

    }
    getPortGroup(symbol: string): PortGroup
    {
        for (let i = 0; i < this.port_groups.length; ++i)
        {
            let port_group = this.port_groups[i];
            if (port_group.symbol === symbol)
            {
                return port_group;
            }
        }
        throw new PiPedalArgumentError("Port group not found.");
    }

    uri:                 string = "";
    name:                string = "";
    plugin_type:         PluginType = PluginType.InvalidPlugin;
    plugin_display_type: string = "";
    author_name:         string = "";
    author_homepage:     string = "";
    audio_inputs:        number = 0;
    audio_outputs:       number = 0;
    has_midi_input:      number = 0;
    has_midi_output:     number = 0;
    description: string  = "";
    controls:            UiControl[] = [];
    port_groups:  PortGroup[] = [];
}

