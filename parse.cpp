#include <cstdio>
#include <cstdlib>
#include <simdjson.h>

#define CANIUSE_DATA_JSON_PATH "~/docs/caniuse-data.json"

// simdjson::padded_string caniuse_data_json;

// extern "C" void load_caniuse_data_padded_string() {
//   auto result = simdjson::padded_string::load(CANIUSE_DATA_JSON_PATH);
//   if (result.has_value()) {
//     // caniuse_data_json = result.value();
//   }
// }

float get_support_internal(const char* feature_id) {
  float percentage = 0.0;
  auto result = simdjson::padded_string::load("./caniuse-data.json");
  if (result.has_value()) {
    auto& json = result.value();
    simdjson::ondemand::parser parser;
    simdjson::ondemand::document doc = parser.iterate(json);

    auto data = doc["data"].value().get_object().value();

    auto item_result = data[feature_id];
    if (item_result.has_value()) {
      auto item = data[feature_id].value().get_object().value();

      for (auto field : item) {
        auto key = field.unescaped_key().value();
        if (key == "title") {
          // auto title = field.value().get_string().value();
          // std::cout << "Title: " << title << std::endl;
        } else if (key == "keywords") {
          // auto keywords = field.value().get_string().value();
          // std::cout << "Keywords: " << keywords << std::endl;
        } else if (key == "usage_perc_y") {
          percentage += field.value().get_double().value();
          // auto support_percentage = field.value().get_number().value().as_double();
          // std::cout << "Global support (usage_perc_y): " << support_percentage << std::endl;
        } else if (key == "usage_perc_a") {
          percentage += field.value().get_double().value();
          // auto support_percentage = field.value().get_number().value().as_double();
          // std::cout << "Global support (usage_perc_a): " << support_percentage << std::endl;
        }
      }
    } else {
      // std::cerr << "Keyword '" << feature_id << "' not found!" << std::endl;
      // return 1;
    }
  } else {
    // std::cerr << "Error: " << result.error() << std::endl;
    // return 1;
  }

  return percentage;
}

extern "C" float get_support(const char* feature_id) {
  return get_support_internal(feature_id);
}

// extern "C" char* get_template(const char* name) {
//   char* s = (char*)malloc(65536);
//   sprintf(s, "What is up my bro %s\n", name);
//   return s;
// }
