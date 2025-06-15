Downloader
--
This script reads some config written in tsv passed by argv[1] and downloads
videos using `yt-dlp` to specified filename (format is mp4 and added to name by
the script, no need to write extension yourself)

First value of entry is filename, second is ID of the video on youtube. Note
that trailing newline is required otherwise script won't download last entry.

Example
--
Contents of the `example-config`:
```
somedestination	somevideoid
somedestination1	somevideoid1
```
Command:  
```sh
downloader example-config
```

Dependencies
--
- [`yt-dlp`](https://github.com/yt-dlp/yt-dlp)  
