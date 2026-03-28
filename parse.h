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

typedef struct {
  char* id;
  char* title;
  char* description;
} Feature;

void reload_caniuse_data();
SearchResult search(const char*);
void free_search_results(FeatureSimple*, int);
Feature* get_feature_by_id(const char*);
void free_feature(Feature*);

#ifdef __cplusplus
}
#endif

#endif
