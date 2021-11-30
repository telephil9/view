#include <u.h>
#include <libc.h>
#include <draw.h>
#include <thread.h>
#include <mouse.h>
#include <keyboard.h>
#include "a.h"

#define NULL nil
#define STBI_NO_STDIO
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"


Image*
load9(char *filename)
{
	Image *i;
	int fd;

	fd = open(filename, OREAD);
	if(fd < 0)
		sysfatal("open: %r");
	i = readimage(display, fd, 1);
	if(i == nil)
		sysfatal("readimage: %r");
	close(fd);
	return i;
}

Image*
loadany(char *filename)
{
	Image *i;
	uchar *buf, *out;
	int n, w, h, c;
	ulong chan;

	buf = readfile(filename, &n);
	if(buf == nil)
		sysfatal("readfile: %r");
	out = stbi_load_from_memory(buf, n, &w, &h, &c, 4);
	free(buf);
	if(out==nil)
		sysfatal("stbi_load_from_memory: %r");
	chan = c==3 ? XBGR32 : ABGR32;
	lockdisplay(display);
	i = eallocimage(w, h, chan, 0, DNofill);
	if(loadimage(i, i->r, out, 4*w*h)<0)
		sysfatal("loadimage: %r");
	unlockdisplay(display);
	return i;
}

Image*
load(char *filename)
{
	Image *i;
	int f;

	i = nil;
	f = fileformat(filename);
	if(f < 0)
		sysfatal("load: %r");
	switch(f){
	case SVG:
		fprint(2, "SVG files not handled\n");
		threadexitsall("SVG files not handled");		
	case NINE:
		i = load9(filename);
		break;
	case GIF:
	case JPEG:
	case PNG:
	case BMP:
		i = loadany(filename);
		break;
	}
	return i;
}

int
save(Image *i, char *f)
{
	int fd, r;

	if(access(f, 0) < 0)
		fd = create(f, OWRITE, 0644);
	else
		fd = open(f, OWRITE|OTRUNC);
	if(fd < 0)
		return -1;
	r = writeimage(fd, i, 1);
	close(fd);
	return r;
}

int
export(Image *i, char *f)
{
	int fd, pfd[2], r;
	Waitmsg *m;

	r = -1;
	if(access(f, 0) < 0)
		fd = create(f, OWRITE, 0644);
	else
		fd = open(f, OWRITE|OTRUNC);
	if(fd < 0)
		return -1;
	if(pipe(pfd) < 0){
		close(fd);
		return -1;
	}
	switch(rfork(RFFDG|RFPROC)){
	case -1:
		goto Err;
	case 0:
		dup(pfd[1], 0);
		dup(fd, 1);
		close(pfd[1]);
		close(pfd[0]);
		execl("/bin/rc", "rc", "-c", "topng", nil);
		_exits("exec");
	}
	if(writeimage(pfd[0], i, 1) < 0)
		goto Err;
	m = wait();
	if(m->msg[0] == 0)
		r = 0;
Err:
	close(fd);
	close(pfd[1]);
	close(pfd[0]);
	return r;
}

