#include <atomic>
#include <cstring>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstdio>
#include <cstdlib>
#include <simdjson.h>

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

extern "C" float get_support(const char* feature_id) {
  float percentage = 0.0;

  auto json_region = caniuse_json_mem_region.load(std::memory_order_acquire);
  simdjson::ondemand::parser parser;
  simdjson::ondemand::document doc = parser.iterate(simdjson::padded_string_view(json_region->data, json_region->len, json_region->len + simdjson::SIMDJSON_PADDING));

  auto data = doc["data"].value().get_object().value();

  auto item_result = data[feature_id];
  if (item_result.has_value()) {
    auto item = data[feature_id].value().get_object().value();

    for (auto field : item) {
      auto key = field.unescaped_key().value();
      if (key == "usage_perc_y") {
        percentage += field.value().get_double().value();
      } else if (key == "usage_perc_a") {
        percentage += field.value().get_double().value();
      }
    }
  }

  return percentage;
}
