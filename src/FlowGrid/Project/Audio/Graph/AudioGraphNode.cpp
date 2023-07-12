#include "AudioGraph.h"

#include "imgui.h"
#include "implot.h"
#include "miniaudio.h"

#include "Helper/String.h"
#include "Project/Audio/AudioDevice.h"
#include "ma_monitor_node/fft_data.h"

AudioGraphNode::AudioGraphNode(ComponentArgs &&args)
    : Component(std::move(args)), Graph(static_cast<const AudioGraph *>(Parent)) {
    Volume.RegisterChangeListener(this);
    Muted.RegisterChangeListener(this);
}
AudioGraphNode::~AudioGraphNode() {
    Field::UnregisterChangeListener(this);
}

void AudioGraphNode::OnFieldChanged() {
    if (Muted.IsChanged() || Volume.IsChanged()) UpdateVolume();
}

void AudioGraphNode::Set(ma_node *node) { Node = node; }

Count AudioGraphNode::InputBusCount() const { return ma_node_get_input_bus_count(Node); }

// Output node (graph endpoint) technically has an output bus, but it doesn't work like other nodes and we treat it strictly as a sink.
Count AudioGraphNode::OutputBusCount() const { return IsOutput() ? 0 : ma_node_get_output_bus_count(Node); }
Count AudioGraphNode::InputChannelCount(Count bus) const { return ma_node_get_input_channels(Node, bus); }
Count AudioGraphNode::OutputChannelCount(Count bus) const { return ma_node_get_output_channels(Node, bus); }

void AudioGraphNode::Init() {
    Set(DoInit());
    UpdateMonitors();
    UpdateVolume();
}

void AudioGraphNode::UpdateVolume() {
    if (On) ma_node_set_output_bus_volume(Node, 0, Muted ? 0.f : float(Volume));
}

void AudioGraphNode::UpdateMonitors() {
    if (InputBusCount() > 0) {
        if (Monitor && !InputMonitorNode) {
            InputMonitorNode = std::unique_ptr<ma_monitor_node, MonitorDeleter>(new ma_monitor_node());
            const auto *device = audio_device.Get();
            const ma_uint32 buffer_size = device->playback.internalPeriodSizeInFrames;
            ma_monitor_node_config config = ma_monitor_node_config_init(InputChannelCount(0), device->playback.internalSampleRate, buffer_size);
            int result = ma_monitor_node_init(Graph->Get(), &config, nullptr, InputMonitorNode.get());
            if (result != MA_SUCCESS) { throw std::runtime_error(std::format("Failed to initialize input monitor node: {}", result)); }
        } else if (!Monitor && InputMonitorNode) {
            InputMonitorNode.reset();
        }
    }

    if (OutputBusCount() > 0) {
        if (Monitor && !OutputMonitorNode) {
            OutputMonitorNode = std::unique_ptr<ma_monitor_node, MonitorDeleter>(new ma_monitor_node());
            const auto *device = audio_device.Get();
            const ma_uint32 buffer_size = device->playback.internalPeriodSizeInFrames;
            ma_monitor_node_config config = ma_monitor_node_config_init(OutputChannelCount(0), device->playback.internalSampleRate, buffer_size);
            int result = ma_monitor_node_init(Graph->Get(), &config, nullptr, OutputMonitorNode.get());
            if (result != MA_SUCCESS) { throw std::runtime_error(std::format("Failed to initialize output monitor node: {}", result)); }
        } else if (!Monitor && OutputMonitorNode) {
            OutputMonitorNode.reset();
        }
    }
}

void AudioGraphNode::Update() {
    const bool is_initialized = Node != nullptr;
    if (On && !is_initialized) Init();
    else if (!On && is_initialized) Uninit();

    UpdateMonitors();
    UpdateVolume();
}

void AudioGraphNode::SplitterDeleter::operator()(ma_splitter_node *splitter) {
    ma_splitter_node_uninit(splitter, nullptr);
}
void AudioGraphNode::MonitorDeleter::operator()(ma_monitor_node *monitor) {
    ma_monitor_node_uninit(monitor, nullptr);
}

void AudioGraphNode::Uninit() {
    if (Node == nullptr) return;

    SplitterNodes.clear();
    DoUninit();
    ma_node_uninit(Node, nullptr);
    Set(nullptr);
}

void AudioGraphNode::ConnectTo(AudioGraphNode &to) {
    if (to.InputMonitorNode) ma_node_attach_output_bus(to.InputMonitorNode.get(), 0, to.Node, 0);
    if (OutputMonitorNode) ma_node_attach_output_bus(Node, 0, OutputMonitorNode.get(), 0);

    to.InputNodes.insert(this);
    OutputNodes.insert(&to);

    auto *currently_connected_to = ((ma_node_base *)OutputNode())->pOutputBuses[0].pInputNode;
    if (currently_connected_to != nullptr) {
        // Connecting a single source to multiple destinations requires a splitter node.
        // We chain splitters together to support any number of destinations.
        // Note: `new` is necessary here because we use a custom deleter.
        SplitterNodes.emplace_back(new ma_splitter_node());
        ma_splitter_node *splitter = SplitterNodes.back().get();
        ma_splitter_node_config splitter_config = ma_splitter_node_config_init(OutputChannelCount(0));
        int result = ma_splitter_node_init(Graph->Get(), &splitter_config, nullptr, splitter);
        if (result != MA_SUCCESS) throw std::runtime_error(std::format("Failed to initialize splitter node: {}", result));

        ma_node_attach_output_bus(splitter, 0, currently_connected_to, 0);
        ma_node_attach_output_bus(splitter, 1, to.InputNode(), 0);
        ma_node_attach_output_bus(OutputNode(), 0, splitter, 0);
    } else {
        ma_node_attach_output_bus(OutputNode(), 0, to.InputNode(), 0);
    }
}

void AudioGraphNode::DisconnectAll() {
    ma_node_detach_output_bus(OutputNode(), 0);
    SplitterNodes.clear();
    InputNodes.clear();
    OutputNodes.clear();
}

using namespace ImGui;

void RenderSpectrogram(fft_data *fft) {
    const auto *data = fft->data;
    const Count N = fft->N;
    const Count N_2 = N / 2;
    static std::vector<float> frequency(N_2);
    static std::vector<float> magnitude(N_2);
    frequency.resize(N_2);
    magnitude.resize(N_2);
    const float fs = float(Count(audio_device.SampleRate));
    const float fs_n = fs / float(N);
    for (Count i = 0; i < N_2; i++) {
        frequency[i] = fs_n * float(i);
        const float mag_linear = sqrtf(data[i][0] * data[i][0] + data[i][1] * data[i][1]) / float(N_2);
        magnitude[i] = ma_volume_linear_to_db(mag_linear);
    }

    if (ImPlot::BeginPlot("Spectrogram", {-1, 160})) {
        static const float MIN_DB = -80;
        ImPlot::SetupAxes("Frequency bin", "Magnitude (dB)");
        ImPlot::SetupAxisLimits(ImAxis_X1, 0, fs / 2, ImGuiCond_Always);
        ImPlot::SetupAxisLimits(ImAxis_Y1, MIN_DB, 0, ImGuiCond_Always);
        ImPlot::PushStyleVar(ImPlotStyleVar_Marker, ImPlotMarker_None);
        // ImPlot::PushStyleColor(ImPlotCol_Line, {1.f, 0.f, 0.f, 1.f});
        ImPlot::PlotShaded("Spectrogram", frequency.data(), magnitude.data(), N_2, MIN_DB);
        // ImPlot::PopStyleColor();
        ImPlot::PopStyleVar();
        ImPlot::EndPlot();
    }
}

void AudioGraphNode::RenderMonitor(IO io) const {
    const auto *monitor_node = GetMonitorNode(io);
    if (monitor_node == nullptr) return;

    if (ImPlot::BeginPlot(StringHelper::Capitalize(to_string(io)).c_str(), {-1, 160})) {
        const Count frame_count = monitor_node->bufferSizeInFrames;

        ImPlot::SetupAxes("Buffer frame", "Value");
        ImPlot::SetupAxisLimits(ImAxis_X1, 0, frame_count, ImGuiCond_Always);
        ImPlot::SetupAxisLimits(ImAxis_Y1, -1.1, 1.1, ImGuiCond_Always);
        if (IsActive) {
            for (Count channel_index = 0; channel_index < ChannelCount(io, 0); channel_index++) {
                const std::string channel_name = std::format("Channel {}", channel_index);
                ImPlot::PushStyleVar(ImPlotStyleVar_Marker, ImPlotMarker_None);
                ImPlot::PlotLine(channel_name.c_str(), monitor_node->pBuffer, frame_count);
                ImPlot::PopStyleVar();
            }
        }
        ImPlot::EndPlot();
    }

    RenderSpectrogram(monitor_node->fft);
}

void AudioGraphNode::Render() const {
    if (!IsOutput()) On.Draw(); // Output node cannot be turned off, since it's the graph endpoint.

    SameLine();
    if (IsActive) {
        PushStyleColor(ImGuiCol_Text, {0.0f, 1.0f, 0.0f, 1.0f});
        TextUnformatted("Active");
    } else {
        PushStyleColor(ImGuiCol_Text, {1.0f, 0.0f, 0.0f, 1.0f});
        TextUnformatted("Inactive");
    }
    PopStyleColor();

    TextUnformatted("Inputs:");
    SameLine();
    if (InputNodes.empty()) {
        TextUnformatted("None");
    } else {
        std::string inputs;
        for (const auto *node : InputNodes) {
            inputs += node->Name;
            inputs += ", ";
        }
        inputs.resize(inputs.size() - 2);
        TextUnformatted(inputs.c_str());
    }

    TextUnformatted("Outputs:");
    SameLine();
    if (OutputNodes.empty()) {
        TextUnformatted("None");
    } else {
        std::string outputs;
        for (const auto *node : OutputNodes) {
            outputs += node->Name;
            outputs += ", ";
        }
        outputs.resize(outputs.size() - 2);
        TextUnformatted(outputs.c_str());
    }

    Muted.Draw();
    SameLine();
    Volume.Draw();
    Monitor.Draw();
    if (Monitor) {
        for (IO io : IO_All) RenderMonitor(io);
    }
}
