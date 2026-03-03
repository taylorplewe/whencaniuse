#include <atomic>
#include <cstring>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstdio>
#include <cstdlib>
#include <simdjson.h>

#define CANIUSE_DATA_SYMLINK "./caniuse-data.json"

// char caniuse_data[0x800000]; // roughly 8.4MB. At time of writing (March 3, 2026) the caniuse data JSON is about 4.5MB so this is just being extra safe.


struct FatPtr {
  char* data;
  size_t len;
};
std::atomic<std::shared_ptr<FatPtr>> caniuse_data;

// std::atomic<std::shared_ptr<char>> caniuse_data;

extern "C" void reload_caniuse_data() {
  puts("c++: beginning memory swap");

  // open symlink'd file and get size
  int caniuse_data_fd = open(CANIUSE_DATA_SYMLINK, O_RDONLY);
  struct stat caniuse_data_fstat;
  fstat(caniuse_data_fd, &caniuse_data_fstat);
  size_t caniuse_data_size = caniuse_data_fstat.st_size;

  auto new_buf = std::shared_ptr<char[]>(
    new char[caniuse_data_size + 64]
  );

  read(caniuse_data_fd, new_buf.get(), caniuse_data_size);
  memset(new_buf.get() + caniuse_data_size, 0, 64);

  

  std::atomic_store(&caniuse_data, new_buf);

  puts("c++: memory swap success!");
}

extern "C" float get_support(const char* feature_id) {
  float percentage = 0.0;
  // auto result = simdjson::padded_string::load(CANIUSE_DATA_SYMLINK);
  // if (result.has_value()) {
    // auto& json = result.value();

    auto region = caniuse_data.get();

    simdjson::ondemand::parser parser;
    simdjson::ondemand::document doc = parser.iterate(simdjson::padded_string_view(region->data, region->size));

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
  // }

  return percentage;
}
