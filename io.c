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
#define NANOSVG_ALL_COLOR_KEYWORDS
#define NANOSVG_IMPLEMENTATION
#include "nanosvg.h"
#define NANOSVGRAST_IMPLEMENTATION
#include "nanosvgrast.h"

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
loadsvg(char *filename)
{
	Image *i;
	NSVGimage *image;
	NSVGrasterizer *rast;
	uchar *data;
	int w, h, sz;

	image = nsvgParseFromFile(filename, "px", 96);
	if(image==nil)
		sysfatal("svg parse: %r");
	w = image->width;
	h = image->height;
	rast = nsvgCreateRasterizer();
	if(rast==nil)
		sysfatal("create rasterizer: %r");
	sz = w*h*4;
	data = malloc(sz);
	if(data==nil)
		sysfatal("malloc: %r");
	nsvgRasterize(rast, image, 0, 0, 1.0, data, w, h, w*4);
	nsvgDelete(image);
	nsvgDeleteRasterizer(rast);
	lockdisplay(display);
	i = eallocimage(w, h, ABGR32, 0, DNofill);
	if(loadimage(i, i->r, data, sz)<sz)
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
		i = loadsvg(filename);
		break;		
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

