#ifndef PARSE_NEW_H
#define PARSE_NEW_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  char* id;
  char* title;
  char* description;
} FeatureSimple;
typedef struct {
  FeatureSimple* features;
  int len;
} SearchResult;

SearchResult search(const char*);
void free_search_results(FeatureSimple*, int);
void reload_caniuse_data();

#ifdef __cplusplus
}
#endif

#endif
