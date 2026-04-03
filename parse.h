#ifndef PARSE_NEW_H
#define PARSE_NEW_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define FatPtr(T) struct {\
  T*  data; \
  int len; \
}

typedef struct {
  char* id;
  char* title;
  char* description;
} FeatureSimple;
typedef FatPtr(FeatureSimple) SearchResult;

typedef unsigned char LinkKind;
enum {
  LinkKindString,
  LinkKindMdn,
  LinkKindSpec,
};
typedef struct {
  LinkKind kind;
  char*    display;
  char*    href;
} Link;
typedef struct {
  uint32_t     index;
  char*        id;
  char*        title;
  char*        description;
  FatPtr(Link) links;
} Feature;

typedef FatPtr(char*) GetWatchlistTitlesResult;

void                     reload_caniuse_data();
SearchResult             search(const char*);
void                     free_search_results(FeatureSimple*, int);
Feature*                 get_feature_by_id(const char*);
void                     free_feature(Feature*);
GetWatchlistTitlesResult get_watchlist_titles(const unsigned int[], const int);
void                     free_watchlist_titles(char**);

#ifdef __cplusplus
}
#endif

#endif
