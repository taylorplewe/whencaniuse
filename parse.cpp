#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include <atomic>
#include <cstring>
#include <cstdio>
#include <cstdlib>

#include <simdjson.h>

#include "parse.h"


#define CANIUSE_DATA_SYMLINK "./caniuse-data.json"

struct MemRegion {
  char* data;
  size_t len;
};
std::atomic<std::shared_ptr<MemRegion>> caniuse_json_mem_region;

extern "C" void reload_caniuse_data() {
  puts("c++: memory update begin...");
  // open symlink'd file and get size
  int caniuse_data_fd = open(CANIUSE_DATA_SYMLINK, O_RDONLY);
  struct stat caniuse_data_fstat;
  fstat(caniuse_data_fd, &caniuse_data_fstat);
  size_t caniuse_data_size = caniuse_data_fstat.st_size;

  // instantiate new memory region, allocate memory for it
  auto region = std::shared_ptr<MemRegion>(
    new MemRegion{new char[caniuse_data_size + simdjson::SIMDJSON_PADDING], caniuse_data_size},
    [](MemRegion* r) {
      delete r->data;
      delete r;
    }
  );

  // copy JSON file's contents to new memory region
  read(caniuse_data_fd, region->data, caniuse_data_size);
  memset(region->data + region->len, 0, simdjson::SIMDJSON_PADDING); // manually pad JSON memory with 64 0s at end for SIMD instruction convenience
  close(caniuse_data_fd);

  // atomically switch out old for new
  caniuse_json_mem_region.store(region, std::memory_order_release);
  puts("c++: memory update success!");

  // printf("first three chars: %c%c%c\n", region->data[0], region->data[1], region->data[2]);
}

#define MAX_SEARCH_FEATURES 32
extern "C" SearchResult search(const char* query) {
  FeatureSimple* features = new FeatureSimple[MAX_SEARCH_FEATURES];
  int features_len = 0;

  auto json_region = caniuse_json_mem_region.load(std::memory_order_acquire);
  simdjson::ondemand::parser parser;
  simdjson::ondemand::document doc = parser.iterate(simdjson::padded_string_view(json_region->data, json_region->len, json_region->len + simdjson::SIMDJSON_PADDING));
  simdjson::ondemand::object data = doc["data"].value().get_object();

  for (auto feature : data) {
    std::basic_string_view<char> feature_id = feature.unescaped_key();
    if (feature_id.starts_with(query)) {
      printf("c++: found feature\n");
      auto new_feature_id = (char*)malloc(feature_id.length());
      auto new_feature_title = (char*)malloc(32);
      auto new_feature_description = (char*)malloc(32);
      feature_id.copy(new_feature_id, feature_id.length());
      strcpy(new_feature_title, "feature title");
      strcpy(new_feature_description, "feature description");
      features[features_len] = {
        .id = new_feature_id,
        .title = new_feature_title,
        .description = new_feature_description,
      };

      features_len++;
      if (features_len == MAX_SEARCH_FEATURES) {
        break;
      }
    }
  }

  // auto item_result = data[query];
  // if (item_result.has_value()) {
  //   simdjson::ondemand::object item = data[query].value().get_object();

  //   for (auto field : item) {
  //     std::basic_string_view<char> key = field.unescaped_key();
  //     if (key == "usage_perc_y") {
  //       percentage += field.value().get_double().value();
  //     } else if (key == "usage_perc_a") {
  //       percentage += field.value().get_double().value();
  //     }
  //   }
  // }

  // return percentage;
  printf("c++: returning %d features...\n", features_len);
  return {
    .features = features,
    .len = features_len, 
  };
}

extern "C" void free_search_results(FeatureSimple* results, int len) {
  for (int i = 0; i < len; i++) {
    free(results[i].id);
    free(results[i].title);
    free(results[i].description);
  }
  delete[] results;
}
