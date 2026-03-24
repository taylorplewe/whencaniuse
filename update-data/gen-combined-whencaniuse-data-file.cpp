#include <algorithm>
#include <ios>
#include <string_view>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <simdjson.h>
// #include <vector>

// #include <algorithm>
#include <fstream>
#include <iostream>

#include <cctype>
#include <cstring>
#include <cstdio>
#include <cstdlib>


#define HEADER_ENTRY_SIZE 32
// this is the natural vector size of my Raspberry Pi 5's CPU
#define SIMD_VECTOR_SIZE 16
#define CANIUSE_DATA_PATH "caniuse-data.json"
#define BCD_DATA_PATH "bcd-data.json"
#define WEB_FEATURES_DATA_PATH "web-features-data.json"
#define OUT_FILE "../data/combined-data.bin"


// NOTE: I did this thinking maybe the CPU would have better cache hits if all the titles followed one after the other in memory, etc. but zero speed difference occurred :(
enum PassKind {
  CountFeatures,
  WriteIds,
  WriteTitles,
  WriteTitlesLower,
  WriteDescriptions,
  End,
};


void process_caniuse_section(std::ofstream&);
void process_bcd_section(std::ofstream&);
void process_web_features_section(std::ofstream&);
void write_value_simd_padded(std::ofstream&, char*, int);

uint32_t num_features = 0;
uint32_t header_pos = 0;
char* zeroes = (char[16]){0};
PassKind pass = ::CountFeatures;

char title_lower_buf[2048];

int main(int argc, char** argv) {
  std::ofstream out;
  out.open(OUT_FILE, std::ofstream::trunc);

  // first, count how many features there are
  process_caniuse_section(out);
  process_bcd_section(out);
  process_web_features_section(out);

  printf("num features: %u\n", num_features);
  fflush(stdout);

  out.write((char*)&num_features, 4);

  // write 0s to file making space for the header; actual data can now be appended onto the end, header contains pointers to the data
  void* header_zeroes = calloc(num_features, HEADER_ENTRY_SIZE);
  out.write((char*)header_zeroes, num_features * HEADER_ENTRY_SIZE);
  free(header_zeroes);

  // then actually add to the file
  for (int i = PassKind::WriteIds; i < PassKind::End; i++) {
    pass = static_cast<PassKind>(i);
    header_pos = i * 4;
    process_caniuse_section(out);
    process_bcd_section(out);
    process_web_features_section(out);
  }

  out.close();
}

void process_caniuse_section(std::ofstream& out) {
  auto json = simdjson::padded_string::load(CANIUSE_DATA_PATH);
  if (json.error()) {
    std::cout << "ERROR: couldn't open caniuse file" << std::endl << json.error() << std::endl;
  }

  simdjson::ondemand::parser parser;
  simdjson::ondemand::document doc = parser.iterate(json);
  simdjson::ondemand::object data = doc["data"].value().get_object();

  bool first = true;
  for (auto feature : data) {
    std::basic_string_view<char> feature_id = feature.unescaped_key();

    auto key_text = std::basic_string_view<char>(feature_id);
    std::string title_text;
    std::string description_text;

    simdjson::ondemand::object feature_obj = feature.value();
    for (auto field : feature_obj) {
      std::basic_string_view<char> field_name = field.unescaped_key();
      if (field_name == "title") {
        title_text = std::basic_string<char>(field.value().get_string().value());
      } else if (field_name == "description") {
        description_text = std::basic_string<char>(field.value().get_string().value());
      }
    }

    switch (pass) {
      case ::CountFeatures:
        num_features++;
        break;
      case ::WriteIds: {
        out.seekp(0, std::ios_base::end);
        uint32_t key_pos = out.tellp();
        uint16_t key_len = key_text.size();
        out.write((char*)&key_len, 2);
        out.write(key_text.data(), key_text.size());

        out.seekp(header_pos, std::ios_base::beg);
        out.write((char*)&key_pos, 4);
        header_pos += HEADER_ENTRY_SIZE;
        break;
      }
      case ::WriteTitles: {
        out.seekp(0, std::ios_base::end);
        uint32_t title_pos = out.tellp();
        uint16_t title_len = title_text.size();
        out.write((char*)&title_len, 2);
        out.write(title_text.data(), title_text.size());

        out.seekp(header_pos, std::ios_base::beg);
        out.write((char*)&title_pos, 4);
        header_pos += HEADER_ENTRY_SIZE;
        break;
      }
      case ::WriteTitlesLower: {
        memcpy(title_lower_buf, title_text.data(), title_text.size());
        std::transform(title_lower_buf, title_lower_buf+title_text.size(), title_lower_buf, ::tolower);

        out.seekp(0, std::ios_base::end);
        uint32_t title_lower_pos = out.tellp();
        uint16_t title_lower_len = title_text.size();
        out.write((char*)&title_lower_len, 2);
        write_value_simd_padded(out, title_lower_buf, title_text.size());

        out.seekp(header_pos, std::ios_base::beg);
        out.write((char*)&title_lower_pos, 4);
        header_pos += HEADER_ENTRY_SIZE;

        break;
      }
      case ::WriteDescriptions: {
        out.seekp(0, std::ios_base::end);
        uint32_t description_pos = out.tellp();
        uint16_t description_len = description_text.size();
        out.write((char*)&description_len, 2);
        out.write(description_text.data(), description_text.size());

        out.seekp(header_pos, std::ios_base::beg);
        out.write((char*)&description_pos, 4);
        header_pos += HEADER_ENTRY_SIZE;
        break;
      }
      default:
        break;
    }
  }
}

std::string get_bcd_description_or_id(simdjson::ondemand::object& compat, std::string& id) {
  auto description_res = compat["description"];
  if (description_res.has_value()) {
    return std::basic_string<char>(description_res.value().get_string().value());
  }
  return id;
}

/// given a JSON object, presumably a subsection of MDN's browser-compat-data, append all features downstream of here with __compat entries to the output JSON
void append_bcd_feature_tree(
  std::vector<std::string> bcd_level_names,
  std::vector<std::string> bcd_level_names_lower,
  simdjson::ondemand::object& obj,
  std::ofstream& out
) {
  for (auto field : obj) {
    std::basic_string_view<char> id = field.unescaped_key();
    auto sub_obj = field.value().get_object();
    if (sub_obj.has_value()) {
      if (id == "__compat") {
        // add this feature to the list
        simdjson::ondemand::object& compat_obj = sub_obj.value();
        std::string title = get_bcd_description_or_id(compat_obj, bcd_level_names.back());

        auto key_text = std::basic_string<char>("mdn-");
        for (int i = 0; i < bcd_level_names_lower.size(); i++) {
          if (i > 0) {
            key_text.push_back('_');
          }
          key_text.append(bcd_level_names_lower.at(i));
        }

        std::string value_text;
        auto top_level_name = bcd_level_names_lower.at(0);
        // I'm not really sure why he decided to not include the following categories from MDN, especially mediatypes (image formats)
        if (top_level_name == "webextensions" || top_level_name == "webdriver" || top_level_name == "mediatypes") {
          continue;
        } else if (top_level_name == "api") {
          value_text.append(bcd_level_names.at(1));
          value_text.append(" API");
          if (bcd_level_names.size() > 2) {
            value_text.append(": ");
            for (int i = 2; i < bcd_level_names.size() - 1; i++) {
              value_text.append(bcd_level_names.at(i));
              value_text.append(": ");
            }
            value_text.append(title);
          }
        } else if (top_level_name == "css") {
          value_text.append("CSS ");
          auto second_level_name = bcd_level_names.at(1);
          if (second_level_name == "at-rules") {
            value_text.append("at-rule: ");
          } else if (second_level_name == "properties") {
            value_text.append("property: ");
          } else if (second_level_name == "selectors") {
            value_text.append("selector: ");
          } else if (second_level_name == "types") {
            value_text.append("type: ");
          }
          for (int i = 2; i < bcd_level_names.size() - 1; i++) {
            value_text.append(bcd_level_names.at(i));
            value_text.append(": ");
          }
          value_text.append(title);
        } else if (top_level_name == "html") {
          value_text.append("HTML ");
          auto second_level_name = bcd_level_names.at(1);
          if (second_level_name == "elements") {
            value_text.append("element: ");
          } else if (second_level_name == "global_attributes") {
            value_text.append("attribute: ");
          }
          for (int i = 2; i < bcd_level_names.size() - 1; i++) {
            value_text.append(bcd_level_names.at(i));
            value_text.append(": ");
          }
          value_text.append(title);
        } else if (top_level_name == "http") {
          value_text.append("HTTP ");
          auto second_level_name = bcd_level_names.at(1);
          if (second_level_name == "data-url" && bcd_level_names.size() > 2) {
            value_text.append("dataURL: ");
          } else if (second_level_name == "headers") {
            value_text.append("header: ");
          } else if (second_level_name == "methods") {
            value_text.append("method: ");
          } else if (second_level_name == "mixed-content" && bcd_level_names.size() > 2) {
            value_text.append("mixed content: ");
          } else if (second_level_name == "status") {
            value_text.append("status: ");
          }
          for (int i = 2; i < bcd_level_names.size() - 1; i++) {
            value_text.append(bcd_level_names.at(i));
            value_text.append(": ");
          }
          value_text.append(title);
        } else if (top_level_name == "javascript") {
          value_text.append("JavaScript ");
          auto second_level_name = bcd_level_names.at(1);
          if (second_level_name == "builtins") {
            value_text.append("builtin: ");
          } else if (second_level_name == "classes" && bcd_level_names.size() > 2) {
            value_text.append("class: ");
          } else if (second_level_name == "functions" && bcd_level_names.size() > 2) {
            value_text.append("function: ");
          } else if (second_level_name == "grammar") {
            value_text.append("grammar: ");
          } else if (second_level_name == "operators") {
            value_text.append("operator: ");
          } else if (second_level_name == "regular_expressions") {
            value_text.append("regular expressions: ");
          } else if (second_level_name == "statements") {
            value_text.append("statements: ");
          }
          for (int i = 2; i < bcd_level_names.size() - 1; i++) {
            value_text.append(bcd_level_names.at(i));
            value_text.append(": ");
          }
          value_text.append(title);
        } else if (top_level_name == "mathml") {
          value_text.append("MathML ");
          auto second_level_name = bcd_level_names.at(1);
          if (second_level_name == "attribute_values" && bcd_level_names.size() > 2) {
            value_text.append("attribute: ");
          } else if (second_level_name == "elements") {
            value_text.append("element: ");
          } else if (second_level_name == "global_attributes") {
            value_text.append("global attribute: ");
          }
          for (int i = 2; i < bcd_level_names.size() - 1; i++) {
            value_text.append(bcd_level_names.at(i));
            value_text.append(": ");
          }
          value_text.append(title);
        } else if (top_level_name == "webassembly") {
          value_text.append("WebAssembly");
          if (bcd_level_names.size() == 2 && bcd_level_names.at(1) == "api") {
            value_text.append(" API");
          } else {
            value_text.append(": ");
            for (int i = 2; i < bcd_level_names.size() - 1; i++) {
              value_text.append(bcd_level_names.at(i));
              value_text.append(": ");
            }
            value_text.append(title);
          }
        } else {
          for (int i = 0; i < bcd_level_names.size() - 1; i++) {
            value_text.append(bcd_level_names.at(i));
            value_text.append(": ");
          }
          value_text.append(title);
        }

        // switch (pass) {
        //   case ::CountFeatures:
        //     num_features++;
        //     break;
        //   case ::WriteIds: {
        //     out.seekp(0, std::ios_base::end);
        //     uint32_t key_pos = out.tellp();
        //     uint16_t key_len = key_text.size();
        //     out.write((char*)&key_len, 2);
        //     out.write(key_text.data(), key_text.size());

        //     out.seekp(header_pos, std::ios_base::beg);
        //     out.write((char*)&key_pos, 4);
        //     header_pos += HEADER_ENTRY_SIZE;
        //     break;
        //   }
        //   case ::WriteTitles: {
        //     out.seekp(0, std::ios_base::end);
        //     uint32_t title_pos = out.tellp();
        //     uint16_t title_len = title_text.size();
        //     out.write((char*)&title_len, 2);
        //     write_value_simd_padded(out, title_text.data(), title_text.size());

        //     out.seekp(header_pos, std::ios_base::beg);
        //     out.write((char*)&title_pos, 4);
        //     header_pos += HEADER_ENTRY_SIZE;
        //     break;
        //   }
        //   case ::WriteTitlesLower: {
        //     // TODO

        //     break;
        //   }
        //   case ::WriteDescriptions: {
        //     out.seekp(0, std::ios_base::end);
        //     uint32_t description_pos = out.tellp();
        //     uint16_t description_len = description_text.size();
        //     out.write((char*)&description_len, 2);
        //     out.write(description_text.data(), description_text.size());

        //     out.seekp(header_pos, std::ios_base::beg);
        //     out.write((char*)&description_pos, 4);
        //     header_pos += HEADER_ENTRY_SIZE;
        //   }
        //   default:
        //     break;
        // }

        
        switch (pass) {
          case ::CountFeatures:
            num_features++;
            break;
          case ::WriteIds: {
            out.seekp(0, std::ios_base::end);
            uint32_t key_pos = out.tellp();
            uint16_t key_len = key_text.size();
            out.write((char*)&key_len, 2);
            out.write(key_text.data(), key_text.size());

            out.seekp(header_pos, std::ios_base::beg);
            out.write((char*)&key_pos, 4);
            header_pos += HEADER_ENTRY_SIZE;
            break;
          }
          case ::WriteTitles: {
            // uint32_t title_pos = out.tellp();
            // uint16_t title_len = value_text.size();
            // out.write((char*)&title_len, 2);
            // write_value_simd_padded(out, value_text.data(), value_text.size());


            out.seekp(0, std::ios_base::end);
            uint32_t title_pos = out.tellp();
            uint16_t title_len = value_text.size();
            out.write((char*)&title_len, 2);
            out.write(value_text.data(), value_text.size());

            out.seekp(header_pos, std::ios_base::beg);
            out.write((char*)&title_pos, 4);
            header_pos += HEADER_ENTRY_SIZE;
            break;
          }
          case ::WriteTitlesLower: {
            memcpy(title_lower_buf, value_text.data(), value_text.size());
            std::transform(title_lower_buf, title_lower_buf+value_text.size(), title_lower_buf, ::tolower);

            out.seekp(0, std::ios_base::end);
            uint32_t title_lower_pos = out.tellp();
            uint16_t title_lower_len = value_text.size();
            out.write((char*)&title_lower_len, 2);
            write_value_simd_padded(out, title_lower_buf, value_text.size());

            out.seekp(header_pos, std::ios_base::beg);
            out.write((char*)&title_lower_pos, 4);
            header_pos += HEADER_ENTRY_SIZE;

            break;
          }
          case ::WriteDescriptions:
          default:
            break;
        }

        // if (just_counting) {
        //   num_features++;
        //   continue;
        // }

        // out.seekp(0, std::ios_base::end);

        // uint32_t key_pos = out.tellp();
        // uint16_t key_len = key_text.size();
        // out.write((char*)&key_len, 2);
        // out.write(key_text.data(), key_text.size());

        // uint32_t title_pos = out.tellp();
        // uint16_t title_len = value_text.size();
        // out.write((char*)&title_len, 2);
        // write_value_simd_padded(out, value_text.data(), value_text.size());

        // out.seekp(header_pos, std::ios_base::beg);
        // out.write((char*)&key_pos, 4);
        // out.write((char*)&title_pos, 4);
        // out.write(zeroes, 4);

        // header_pos += HEADER_ENTRY_SIZE;
      } else {
        // keep traversing down the tree
        auto new_level_names = bcd_level_names;
        auto new_level_names_lower = bcd_level_names_lower;
        new_level_names.push_back(std::basic_string<char>(id));
        new_level_names_lower.push_back(std::basic_string<char>(id));
        std::transform(
          new_level_names_lower.back().cbegin(),
          new_level_names_lower.back().cend(),
          new_level_names_lower.back().begin(),
          [](int c){
            return c == '@'
              ? '-'
              : c >= 'A' && c <= 'Z'
                ? c + ('a' - 'A') // 0x20
                : c;
          }
        );
        append_bcd_feature_tree(
          new_level_names,
          new_level_names_lower,
          sub_obj.value(),
          out
        );
      }
    }
  }
}
void process_bcd_section(std::ofstream& out) {
  auto json = simdjson::padded_string::load(BCD_DATA_PATH);
  if (json.error()) {
    std::cout << "ERROR: couldn't open MDN BCD file" << json.error() << std::endl;
  }

  simdjson::ondemand::parser parser;
  simdjson::ondemand::document doc = parser.iterate(json);
  auto doc_obj = doc.get_object().value();

  std::vector<std::string> bcd_level_names;
  std::vector<std::string> bcd_level_names_lower;
  append_bcd_feature_tree(
    bcd_level_names,
    bcd_level_names_lower,
    doc_obj,
    out
  );
}

void process_web_features_section(std::ofstream& out) {
  auto json = simdjson::padded_string::load(WEB_FEATURES_DATA_PATH);
  if (json.error()) {
    std::cout << "ERROR: couldn't open web features file" << json.error() << std::endl;
  }

  simdjson::ondemand::parser parser;
  simdjson::ondemand::document doc = parser.iterate(json);
  simdjson::ondemand::object features = doc["features"].value();
  for (auto feature : features) {
    std::basic_string_view<char> key = feature.unescaped_key();
    auto key_text = std::basic_string<char>("wf-");
    key_text.append(key);
    auto title_text = std::basic_string<char>(key);
    std::string description_text;

    simdjson::ondemand::object feature_obj = feature.value();
    for (auto field : feature_obj) {
      std::basic_string_view<char> field_name = field.unescaped_key();
      if (field_name == "name") {
        title_text = std::basic_string<char>(field.value().get_string().value());
      } else if (field_name == "description_html") {
        description_text = std::basic_string<char>(field.value().get_string().value());
      }
    }

    // switch (pass) {
    //   case ::CountFeatures:
    //     num_features++;
    //     break;
    //   case ::WriteIds: {
    //     out.seekp(0, std::ios_base::end);
    //     uint32_t key_pos = out.tellp();
    //     uint16_t key_len = key_text.size();
    //     out.write((char*)&key_len, 2);
    //     out.write(key_text.data(), key_text.size());

    //     out.seekp(header_pos, std::ios_base::beg);
    //     out.write((char*)&key_pos, 4);
    //     header_pos += HEADER_ENTRY_SIZE;
    //     break;
    //   }
    //   case ::WriteTitles: {
    //     out.seekp(0, std::ios_base::end);
    //     uint32_t title_pos = out.tellp();
    //     uint16_t title_len = title_text.size();
    //     out.write((char*)&title_len, 2);
    //     write_value_simd_padded(out, title_text.data(), title_text.size());

    //     out.seekp(header_pos, std::ios_base::beg);
    //     out.write((char*)&title_pos, 4);
    //     header_pos += HEADER_ENTRY_SIZE;
    //     break;
    //   }
    //   case ::WriteTitlesLower: {
    //     // TODO

    //     break;
    //   }
    //   case ::WriteDescriptions: {
    //     out.seekp(0, std::ios_base::end);
    //     uint32_t description_pos = out.tellp();
    //     uint16_t description_len = description_text.size();
    //     out.write((char*)&description_len, 2);
    //     out.write(description_text.data(), description_text.size());

    //     out.seekp(header_pos, std::ios_base::beg);
    //     out.write((char*)&description_pos, 4);
    //     header_pos += HEADER_ENTRY_SIZE;
    //   }
    //   default:
    //     break;
    // }

    
    switch (pass) {
      case ::CountFeatures:
        num_features++;
        break;
      case ::WriteIds: {
        out.seekp(0, std::ios_base::end);
        uint32_t key_pos = out.tellp();
        uint16_t key_len = key_text.size();
        out.write((char*)&key_len, 2);
        out.write(key_text.data(), key_text.size());

        out.seekp(header_pos, std::ios_base::beg);
        out.write((char*)&key_pos, 4);
        header_pos += HEADER_ENTRY_SIZE;
        break;
      }
      case ::WriteTitles: {
        out.seekp(0, std::ios_base::end);
        uint32_t title_pos = out.tellp();
        uint16_t title_len = title_text.size();
        out.write((char*)&title_len, 2);
        out.write(title_text.data(), title_text.size());

        out.seekp(header_pos, std::ios_base::beg);
        out.write((char*)&title_pos, 4);
        header_pos += HEADER_ENTRY_SIZE;
        break;
      }
      case ::WriteTitlesLower: {
        memcpy(title_lower_buf, title_text.data(), title_text.size());
        std::transform(title_lower_buf, title_lower_buf+title_text.size(), title_lower_buf, ::tolower);

        out.seekp(0, std::ios_base::end);
        uint32_t title_lower_pos = out.tellp();
        uint16_t title_lower_len = title_text.size();
        out.write((char*)&title_lower_len, 2);
        write_value_simd_padded(out, title_lower_buf, title_text.size());

        out.seekp(header_pos, std::ios_base::beg);
        out.write((char*)&title_lower_pos, 4);
        header_pos += HEADER_ENTRY_SIZE;

        break;
      }
      case ::WriteDescriptions: {
        out.seekp(0, std::ios_base::end);
        uint32_t description_pos = out.tellp();
        uint16_t description_len = description_text.size();
        out.write((char*)&description_len, 2);
        out.write(description_text.data(), description_text.size());

        out.seekp(header_pos, std::ios_base::beg);
        out.write((char*)&description_pos, 4);
        header_pos += HEADER_ENTRY_SIZE;
        break;
      }
      default:
        break;
    }

    // if (just_counting) {
    //   num_features++;
    //   continue;
    // }

    // out.seekp(0, std::ios_base::end);

    // uint32_t key_pos = out.tellp();
    // uint16_t key_len = key_text.size();
    // out.write((char*)&key_len, 2);
    // out.write(key_text.data(), key_text.size());

    // uint32_t title_pos = out.tellp();
    // uint16_t title_len = title_text.size();
    // out.write((char*)&title_len, 2);
    // write_value_simd_padded(out, title_text.data(), title_text.size());

    // uint32_t description_pos = out.tellp();
    // uint16_t description_len = description_text.size();
    // out.write((char*)&description_len, 2);
    // out.write(description_text.data(), description_text.size());

    // out.seekp(header_pos, std::ios_base::beg);
    // out.write((char*)&key_pos, 4);
    // out.write((char*)&title_pos, 4);
    // out.write((char*)&description_pos, 4);

    // header_pos += HEADER_ENTRY_SIZE;
  }
}

void write_value_simd_padded(std::ofstream& out, char* data, int len) {
  out.write(data, len);
  if (len % SIMD_VECTOR_SIZE != 0) {
    out.write(zeroes, SIMD_VECTOR_SIZE - (len % SIMD_VECTOR_SIZE));
  }
}
