/*****************************************************
* Copyright 2016 Bumjin Im, Patrick Granahan         *
*                                                    *
* COMP 421/521 Lab 3 (Yalnix yfs) test code          *
*    symlink_readlink.c                              *
*                                                    *
* Author                                             *
*  Bumjin Im (imbumjin@rice.edu)                     *
*  Patrick  (pjgranahan@rice.edu)                    *
*                                                    *
* Created Apr. 22. 2016.                             *
*****************************************************/

#include <comp421/yalnix.h>
#include <comp421/iolib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define assert(message, test) do { if (!(test)) { Shutdown(); printf(message); return 1; } } while (0)

int main()
{
	int fd, ret;
	char buf[MAXPATHNAMELEN];

	// Creating targets
	fd = Create("/file1");
	assert("Failed Create(1)\n", fd == 0);
	ret = Write(fd, "ABCDEFGHIJ", 10);
	assert("Failed Write(1)\n", ret == 10);
	Close(fd);
	ret = MkDir("/dir1");
	assert("Failed Mkdir(1)", ret == 0);

	fd = Create("/dir1/file2");
	assert("Failed Create(2)\n", fd == 0);
	ret = Write(fd, "0123456789", 10);
	assert("Failed Write(2)\n", ret == 10);
	Close(fd);
	ret = MkDir("/dir2");
	assert("Failed Mkdir(2)", ret == 0);

	fd = Create("/dir1/file3");
	assert("Failed Create(3)\n", fd == 0);
	ret = Write(fd, "abcdefghijk", 10);
	assert("Failed Write(3)\n", ret == 10);
	Close(fd);
	ret = MkDir("/dir1/dir3");
	assert("Failed Mkdir(3)", ret == 0);

	printf("All tests passed\n");
	return Shutdown();
}
