package main

/*
#cgo CXXFLAGS: -std=c++23
#cgo LDFLAGS: -l curl -l simdjson
#include <stdlib.h>
float get_support(const char *feature_id);
*/
import "C"
import (
	"fmt"
	"unsafe"
)

func GetSupportPercentageForFeature(featureId string) float32 {
	name := C.CString(featureId)
	defer C.free(unsafe.Pointer(name))
	cPercentage := C.get_support(name)
	percentage := float32(cPercentage)
	return percentage
}

func ReloadCaniuseData() {
	fmt.Println("Reloading...")
}
