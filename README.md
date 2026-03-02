# CanIUse Subscribe
Pick any upcoming web feature. Get notified as soon as it hits a certain browser support threshold.

Support data is pulled from [caniuse.com](https://caniuse.com) (hence the name), specifically the [GitHub repo containing the raw data](https://github.com/Fyrd/caniuse).

## Building
This server runs on a Raspberry Pi 5 running Debian 12. It doesn't really make sense for it to be built anywhere else, but for those curious, here you go.

Requirements:
- [Go](https://go.dev/) >=1.24
- [Datastar](https://data-star.dev/guide/getting_started#data-*) v1.0.0 - the [minified datastar.js file](https://cdn.jsdelivr.net/gh/starfederation/datastar@1.0.0-RC.8/bundles/datastar.js) (13KB) must be downloaded to the root directory.
```sh
go get
go build
./caniuse-subscribe
```
