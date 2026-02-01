#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <memory>

#include <json.hpp>
#include "tensorflow/lite/interpreter.h"
#include "tensorflow/lite/model.h"
#include "tensorflow/lite/kernels/register.h"

struct Prediction {
    std::string label;
    float confidence;
};

class KwypeLogic {
public:
    KwypeLogic(
        const std::string& model_path,
        const std::string& classes_path,
        const std::string& max_len_path
    );

    bool load();
    std::vector<Prediction> predict(const std::string& seq);

private:
    std::string model_path_;
    std::string classes_path_;
    std::string max_len_path_;

    int MAX_LEN_ = 10;
    std::vector<std::string> classes_;
    std::unique_ptr<tflite::FlatBufferModel> model_;
    std::unique_ptr<tflite::Interpreter> interpreter_;

    int input_index_ = -1;
    int output_index_ = -1;

    static const std::unordered_map<char, std::pair<float, float>> key_map_;
};
