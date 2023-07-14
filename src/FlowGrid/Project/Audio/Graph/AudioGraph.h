#pragma once

#include "AudioGraphNode.h"
#include "Core/Container/AdjacencyList.h"
#include "Core/Primitive/Float.h"
// xxx should not depend on any specific node types
#include "Project/Audio/Faust/FaustNode.h"
#include "Project/Audio/TestToneNode.h"

struct ma_device;
struct ma_node_graph;

// Corresponds to `ma_node_graph`.
struct AudioGraph : Component, Field::ChangeListener {
    AudioGraph(ComponentArgs &&);
    ~AudioGraph();

    void OnFieldChanged() override;

    static void AudioCallback(ma_device *, void *output, const void *input, Count frame_count);

    void RenderConnections() const;
    void Update();

    ma_node_graph *Get() const;

    struct InputNode : AudioGraphNode {
        InputNode(ComponentArgs &&);

        ma_node *DoInit() override;
        void DoUninit() override;
    };

    struct OutputNode : AudioGraphNode {
        using AudioGraphNode::AudioGraphNode;

        ma_node *DoInit() override;
    };

    struct Nodes : Component {
        Nodes(ComponentArgs &&);
        ~Nodes();

        // Iterate over all children, converting each element from a `Component *` to a `Node *`.
        // Usage: `for (const Node *node : Nodes) ...`
        struct Iterator : std::vector<Component *>::const_iterator {
            Iterator(auto it) : std::vector<Component *>::const_iterator(it) {}
            const AudioGraphNode *operator*() const { return static_cast<const AudioGraphNode *>(std::vector<Component *>::const_iterator::operator*()); }
            AudioGraphNode *operator*() { return static_cast<AudioGraphNode *>(std::vector<Component *>::const_iterator::operator*()); }
        };
        Iterator begin() const { return Children.cbegin(); }
        Iterator end() const { return Children.cend(); }

        Iterator begin() { return Children.begin(); }
        Iterator end() { return Children.end(); }

        void Init();
        void Update();
        void Uninit();

        const AudioGraph *Graph;

        // Order declarations from early to late in the signal path.
        // This has the benefit of making it faster to disconnect the output buses of all nodes.
        // From the "Thread Safety and Locking" section of the miniaudio docs:
        // "The cost of detaching nodes earlier in the pipeline (data sources, for example) will be cheaper than the cost of detaching higher
        //  level nodes, such as some kind of final post-processing endpoint.
        //  If you need to do mass detachments, detach starting from the lowest level nodes and work your way towards the final endpoint node."
        Prop(TestToneNode, TestTone);
        Prop(InputNode, Input); // `ma_data_source_node` whose `ma_data_source` is a `ma_audio_buffer_ref` pointing directly to the input buffer.
        Prop(FaustNode, Faust);
        Prop(OutputNode, Output);

    private:
        void Render() const override;
    };

    struct Style : Component {
        using Component::Component;

        struct Matrix : Component {
            using Component::Component;

            Prop_(Float, CellSize, "?The size of each matrix cell, as a multiple of line height.", 1, 1, 3);
            Prop_(Float, CellGap, "?The gap between matrix cells.", 1, 0, 10);
            Prop_(
                Float, MaxLabelSpace,
                "?The matrix is placed to make room for the longest input/output node label, up to this limit (as a multiple of line height), at which point the labels will be ellipsified.\n"
                "(Use Style->ImGui->InnerItemSpacing->X for spacing between labels and cells.)",
                8, 4, 16
            );

        protected:
            void Render() const override;
        };

        Prop(Matrix, Matrix);
    };

    Prop(Nodes, Nodes);
    Prop(AdjacencyList, Connections);
    Prop(Style, Style);

private:
    void Init();
    void Uninit();
    void UpdateConnections();

    void Render() const override {}
};
