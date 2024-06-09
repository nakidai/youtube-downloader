Downloader
--
This script reads some config written in json passed by argv[1] and downloads
videos using `yt-dlp` to specified filename (format is mp4 and added to name by
the script, no need to write extension yourself)

Example
--
Contents of the `example-config.json`:  
```json
[
    ["https://youtu.be/somevideoid", "somedestination"],
    ["https://youtu.be/somevideoid1", "somedestination1"]
]
```
Command:  
```sh
downloader example-config.json
```

Dependencies
--
- [`yt-dlp`](https://github.com/yt-dlp/yt-dlp)  
- [`jsmn-find`](https://github.com/lcsmuller/jsmn-find) (included in the source)  
- [`jsmn`](https://github.com/zserge/jsmn) (dependency of the `jsmn-find`) (included in the source)  
- [`chash`](https://github.com/c-ware/chash) (dependency of the `jsmn-find`) (included in the source)  
