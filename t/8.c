#include <stdio.h>
#include <strings.h>

#include <comp421/yalnix.h>
#include <comp421/iolib.h>
#include <comp421/filesystem.h>

/*
 *  Works like "ls".
 *
 *  Uncomment #define below to test symbolic links too.
 */

/* #define TEST_SYMLINKS */

int
main(int argc, char **argv)
{
    int fd;
    int nch;
    struct dir_entry entry;
    char *name;
    struct Stat sb;
    char buffer[DIRNAMELEN+1];
#ifdef  TEST_SYMLINKS
    char link[MAXPATHNAMELEN+1];
#endif
    char typechar;

    name = (argc > 1) ? argv[1] : ".";

    if (ChDir(name) == ERROR) {
	fprintf(stderr, "Can't ChDir to %s\n", name);
	Shutdown();
	Exit(1);
    }
	
    if ((fd = Open(".")) == ERROR) {
	fprintf(stderr, "Can't Open . in %s\n", name);
	Shutdown();
	Exit(1);
    }

    while (1) {
	nch = Read(fd, (char *)&entry, sizeof(entry));
	if (nch == 0)
	    break;
	else if (nch < 0) {
	    fprintf(stderr, "ERROR Reading from directory\n");
	    Shutdown();
	    Exit(1);
	} else if (nch != sizeof(entry)) {
	    fprintf(stderr, "Read wrong byte count %d\n", nch);
	    Shutdown();
	    Exit(1);
	}
	if (entry.inum == 0) continue;
	bcopy(entry.name, buffer, DIRNAMELEN);
	buffer[DIRNAMELEN+1] = '\0';
	/* was LinkStat, when supporting symlinks ??? */
	if (Stat(buffer, &sb) == ERROR) {
	    fprintf(stderr, "Can't LinkStat %s\n", buffer);
	    Shutdown();
	    Exit(1);
	}
	switch (sb.type) {
	    case INODE_REGULAR:	typechar = ' '; break;
	    case INODE_DIRECTORY:	typechar = 'd'; break;

	    default:		typechar = '?'; break;
	}
	printf("%4d %c %3d %5d %s",
	    entry.inum, typechar, sb.nlink, sb.size, buffer);

	    printf("\n");
    }

    Shutdown();
    return (0);
}
