#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include <atomic>
#include <cstdio>
#include <memory>

#include "parse-new.h"


struct MemRegion {
  char* data;
  size_t len;
};
std::atomic<std::shared_ptr<MemRegion>> feature_data_mem_region;

#define FEATURE_DATA_PATH "data/combined-data.bin"


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

  printf("number of features: %i\n", *(int*)(region->data));
}

#define MAX_SEARCH_FEATURES 32
// extern "C" SearchResult search(const char* query) {
// }


// extern "C" void free_search_results(FeatureSimple* results, int len) {
// }

int main(int argc, char** argv) {
  reload_caniuse_data();

  return 0;
}