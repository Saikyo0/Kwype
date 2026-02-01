#include "KwypeLogic.h"

#include <fstream>
#include <algorithm>
#include <numeric>
#include <stdexcept>

const std::unordered_map<char, std::pair<float, float>> KwypeLogic::key_map_ = {
    {'q',{0,0}}, {'w',{1,0}}, {'e',{2,0}}, {'r',{3,0}}, {'t',{4,0}},
    {'y',{5,0}}, {'u',{6,0}}, {'i',{7,0}}, {'o',{8,0}}, {'p',{9,0}},
    {'a',{0.5f,1}}, {'s',{1.5f,1}}, {'d',{2.5f,1}}, {'f',{3.5f,1}},
    {'g',{4.5f,1}}, {'h',{5.5f,1}}, {'j',{6.5f,1}}, {'k',{7.5f,1}},
    {'l',{8.5f,1}},
    {'z',{1.5f,2}}, {'x',{2.5f,2}}, {'c',{3.5f,2}}, {'v',{4.5f,2}},
    {'b',{5.5f,2}}, {'n',{6.5f,2}}, {'m',{7.5f,2}}
};

KwypeLogic::KwypeLogic(
    const std::string& model_path,
    const std::string& classes_path,
    const std::string& max_len_path
)
    : model_path_(model_path),
    classes_path_(classes_path),
    max_len_path_(max_len_path)
{
}

bool KwypeLogic::load() {
    try {
        std::ifstream class_stream(classes_path_);
        if (!class_stream) {
            throw std::runtime_error("Failed to open classes file");
        }
        nlohmann::json j;
        class_stream >> j;
        classes_ = j.get<std::vector<std::string>>();
        std::ifstream len_stream(max_len_path_);
        if (!len_stream) {
            throw std::runtime_error("Failed to open max_len file");
        }

        len_stream >> MAX_LEN_;
        model_ = tflite::FlatBufferModel::BuildFromFile(model_path_.c_str());
        if (!model_) { throw std::runtime_error("Failed to load TFLite model"); }
        tflite::ops::builtin::BuiltinOpResolver resolver;
        tflite::InterpreterBuilder(*model_, resolver)(&interpreter_);
        if (!interpreter_) {
            throw std::runtime_error("Failed to create interpreter");
        }

        interpreter_->AllocateTensors();
        input_index_ = interpreter_->inputs()[0];
        output_index_ = interpreter_->outputs()[0];

        return true;
    }
    catch (const std::exception& e) {
        printf("Model load failed: %s\n", e.what());
        return false;
    }
}

std::vector<Prediction> KwypeLogic::predict(const std::string& seq) {
    if (!interpreter_ || seq.empty()) { return {}; }
    float* input = interpreter_->typed_tensor<float>(input_index_);
    std::fill(
        input,
        input + (MAX_LEN_ * 2),
        0.0f
    );

    int i = 0;
    for (char c : seq) {
        if (i >= MAX_LEN_) break;
        auto it = key_map_.find(c);
        if (it != key_map_.end()) {
            input[i * 2 + 0] = it->second.first;
            input[i * 2 + 1] = it->second.second;
        }
        ++i;
    }

    interpreter_->Invoke();
    const float* output = interpreter_->typed_tensor<float>(output_index_);
    int num_classes = static_cast<int>(classes_.size());
    std::vector<int> idx(num_classes);
    std::iota(idx.begin(), idx.end(), 0);

    std::partial_sort(
        idx.begin(),
        idx.begin() + 3,
        idx.end(),
        [&](int a, int b) {
            return output[a] > output[b];
        }
    );

    std::vector<Prediction> results;
    for (int k = 0; k < 3; ++k) {
        int id = idx[k];
        results.push_back({
            classes_[id],
            output[id]
        });
    }

    return results;
}
