#include <cstdint>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include <atomic>
#include <cstdio>
#include <memory>

#include "parse.h"


struct MemRegion {
  char* data;
  size_t len;
};
std::atomic<std::shared_ptr<MemRegion>> feature_data_mem_region;

#define FEATURE_DATA_PATH "data/combined-data.bin"
#define HEADER_ENTRY_SIZE 32

uint32_t num_features = 0;
char* zeroes = (char[16]){0};


extern "C" void reload_caniuse_data() {
  puts("c++: memory update begin...");
  // open symlink'd file and get size
  int feature_data_fd = open(FEATURE_DATA_PATH, O_RDONLY);
  struct stat feature_data_fstat;
  fstat(feature_data_fd, &feature_data_fstat);
  size_t feature_data_size = feature_data_fstat.st_size;

  // instantiate new memory region, allocate memory for it
  auto region = std::shared_ptr<MemRegion>(
    new MemRegion{new char[feature_data_size], feature_data_size},
    [](MemRegion* r) {
      delete r->data;
      delete r;
    }
  );

  // copy JSON file's contents to new memory region
  read(feature_data_fd, region->data, feature_data_size);
  close(feature_data_fd);

  // atomically switch out old for new
  feature_data_mem_region.store(region, std::memory_order_release);
  puts("c++: memory update success!");

  num_features = *(uint32_t*)(region->data);
  printf("number of features: %i\n", num_features);
}

#define MAX_SEARCH_FEATURES 32
extern "C" SearchResult search(const char* query) {
  auto feature_data = feature_data_mem_region.load(std::memory_order_acquire);
  FeatureSimple* features = new FeatureSimple[MAX_SEARCH_FEATURES];
  int features_len = 0;

  uint64_t seek = 4; // first ID address
  uint32_t addr_id,
           addr_title,
           addr_title_lower,
           addr_description;
  for (int i = 0; i < num_features; i++) {
    addr_id          = *(uint32_t*)(feature_data->data + seek);
    addr_title       = *(uint32_t*)(feature_data->data + seek + 4);
    addr_title_lower = *(uint32_t*)(feature_data->data + seek + 8);
    addr_description = *(uint32_t*)(feature_data->data + seek + 12);
    if (addr_id) {
      uint16_t title_lower_len = *(uint16_t*)(feature_data->data + addr_title_lower);
      auto title_lower = std::basic_string_view<char>(feature_data->data + addr_title_lower + 2, title_lower_len);

      if (title_lower.contains(query)) {
        features[features_len++] = {
          .id = (char*)(feature_data->data + addr_id + 2),
          .title = addr_title
            ? (char*)(feature_data->data + addr_title + 2)
            : zeroes,
          .description = addr_description
            ? (char*)(feature_data->data + addr_description + 2)
            : zeroes,
        };
        if (features_len == MAX_SEARCH_FEATURES) break;
      }
    }
    
    // postlude
    seek += HEADER_ENTRY_SIZE;
  }

  return {
    .features = features,
    .len = features_len, 
  };
}

extern "C" void free_search_results(FeatureSimple* results, int len) {
  delete[] results;
}
