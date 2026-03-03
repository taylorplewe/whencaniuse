#include <atomic>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstdio>
#include <cstdlib>
#include <simdjson.h>

#define CANIUSE_DATA_SYMLINK "./caniuse-data.json"

// char caniuse_data[0x800000]; // roughly 8.4MB. At time of writing (March 3, 2026) the caniuse data JSON is about 4.5MB so this is just being extra safe.

struct MemoryMapRegion {
  char* data;
  size_t size;
};
std::atomic<std::shared_ptr<MemoryMapRegion>> caniuse_data;

extern "C" void reload_caniuse_data() {
  // open symlink'd file and get size
  int caniuse_data_fd = open(CANIUSE_DATA_SYMLINK, O_RDONLY);
  struct stat caniuse_data_fstat;
  fstat(caniuse_data_fd, &caniuse_data_fstat);
  size_t caniuse_data_size = caniuse_data_fstat.st_size;

  // ask the kernel to map the file's memory into our address space (very fast)
  auto caniuse_data_mem = (char*)mmap(nullptr, caniuse_data_size, PROT_READ, MAP_PRIVATE, caniuse_data_fd, 0);
  close(caniuse_data_fd);
  if (caniuse_data_mem == (char*)MAP_FAILED) {
    return;
  }

  auto region = std::shared_ptr<MemoryMapRegion>(
    new MemoryMapRegion{caniuse_data_mem, caniuse_data_size},
    [](MemoryMapRegion* r) {
      munmap(r->data, r->size);
      delete r;
    }
  );

  caniuse_data.store(region, std::memory_order_release);

  // std::atomic_load()
  // auto result = simdjson::padded_string::load(CANIUSE_DATA_JSON_PATH);
  // if (result.has_value()) {
    // caniuse_data_json = result.value();
  // }
}

extern "C" float get_support(const char* feature_id) {
  float percentage = 0.0;
  auto result = simdjson::padded_string::load(CANIUSE_DATA_SYMLINK);
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
        if (key == "usage_perc_y") {
          percentage += field.value().get_double().value();
        } else if (key == "usage_perc_a") {
          percentage += field.value().get_double().value();
        }
      }
    }
  }

  return percentage;
}
