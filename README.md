# WhenCanIUse
> [!IMPORTANT]
> This project is still a work in progress.

Pick any upcoming web feature. Get notified as soon as it hits a certain browser support threshold.

Support data is pulled from [caniuse.com](https://caniuse.com) (hence the name), specifically the [GitHub repo containing the raw data](https://github.com/Fyrd/caniuse).

## Building
This server runs on a Raspberry Pi 5 running Debian 12. It doesn't really make sense for it to be built anywhere else, but for those curious, here you go.

Requirements:
- [Go](https://go.dev/) >=1.24
- [Datastar](https://data-star.dev) v1.0.0 - the [minified datastar.js file](https://cdn.jsdelivr.net/gh/starfederation/datastar@1.0.0-RC.8/bundles/datastar.js) (13KB) must be downloaded to the root directory.
```sh
go get
go build
./whencaniuse
```

## Structure
- The website portion is fully server-side-rendered via a Go web server which sends page updates via [Datastar](https://data-star.dev). Other than the aforementioned minified `datastar.js`, this site contains no Javascript.
- Caniuse data ([`fulldata-json/data-2.0.json` from github.com/Fyrd/caniuse](https://raw.githubusercontent.com/Fyrd/caniuse/refs/heads/main/fulldata-json/data-2.0.json)) is downloaded via a cronjob once a day to the server at 3:00 AM MDT. That cronjob then sends a signal (`SIGUSR1`) to the whencaniuse server telling it to update its data buffer with the new data.
- Any time data from the aforementioned >4KB JSON file is required, it is parsed in C++ using [simdjson](https://simdjson.org/). The Go code communicates with the C++ code via [cgo](https://go.dev/wiki/cgo)
- Current plan is to use Amazon SES for sending the emails

## Notes
- I've struggled choosing between a Go server and a Rust one. I didn't want to just write the whole web server in C++ because there is no built-in first-party TCP network utils; there is with Go and Rust. Go has HTML templating built into the standard library, so that's a plus, though it may not even be needed for a site this simple. Go is _pretty_ performant, being a compiled language, but is garbage collected, and cannot match the performance of Rust. However, the difference is negligable compared to network speed, which is the real bottleneck when it comes to how fast the web page feels. Nonetheless, I'm considering switching to Rust because there's no overhead/added code when calling into C, unlike with Go. In addition, there's still quite a bit more logic I need to write (in both languages) before the site is done; I'm wondering if the performance difference will be noticable by the end.
