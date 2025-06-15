#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ENTRY_SZ 256

#define COMMAND_SZ (ENTRY_SZ*3)

char command[COMMAND_SZ], filename[ENTRY_SZ], id[ENTRY_SZ];

void download(void)
{
    int res;

    res = snprintf(
        command, sizeof(command),
        "yt-dlp 'https://youtu.be/%s' -o '%s.mp4' -f \"bestvideo[ext=mp4]+bestaudio[ext=m4a]/best[ext=mp4]/best\"",
        id, filename
    );
    if (res < 0)
        err(1, "snprintf()");

    printf("$ %s\n", command);
    system(command);
}

int main(int argc, char **argv)
{
    enum Mode
    {
        FILENAME = '\n',
        ID = '\t'
    } mode = FILENAME;
    int ch, i;
    FILE *fp;

    if (argc != 2)
        return fprintf(stderr, "usage: downloader <config>\n"), 1;

    fp = fopen(argv[1], "r");
    if (!fp)
        err(1, "fopen()");

    i = 0;
    while ((ch = fgetc(fp)) != -1)
        switch (ch)
        {
        break; case FILENAME: case ID:
            if (mode == ch)
                errx(1, "invalid config");

            mode = ch, i = 0;
            if (mode == FILENAME)
                download();
        break; default:
            if (i == ENTRY_SZ - 1)
                err(1, "%s is too large", mode == ID ? "id" : "filename");
            (mode == ID ? id : filename)[i++] = ch;
        }
}
