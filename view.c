#include <u.h>
#include <libc.h>
#include <draw.h>
#include <thread.h>
#include <mouse.h>
#include <keyboard.h>
#include <stb.h>
#include "a.h"

enum
{
	Emouse,
	Eresize,
	Ekeyboard,
};

int mainstacksize=16384;

Mousectl *mctl;
Keyboardctl *kctl;
Image *bg;
Image *img;
Point pos;

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

void
initbg(void)
{
	Image *gray;
	gray = eallocimage(1, 1, screen->chan, 1, 0xefefefff);
	bg = eallocimage(16, 16, screen->chan, 1, DWhite);
	draw(bg, bg->r, display->white, nil, ZP);
	draw(bg, Rect(0,0,8,8), gray, nil, ZP);	
	draw(bg, Rect(8,8,16,16), gray, nil, ZP);	
}

void
redraw(void)
{
	lockdisplay(display);
	draw(screen, screen->r, bg, nil, ZP);
	draw(screen, rectaddpt(img->r, addpt(pos, screen->r.min)), img, nil, img->r.min);
	flushimage(display, 1);
	unlockdisplay(display);
}

void
pan(Point Δ)
{
	Rectangle r, ir;

	if(Δ.x == 0 && Δ.y == 0)
		return;
	r = rectaddpt(img->r, addpt(pos, screen->r.min));
	pos = addpt(pos, Δ);
	ir = rectaddpt(r, Δ);
	lockdisplay(display);
	draw(screen, screen->r, bg, nil, ZP);
	draw(screen, ir, img, nil, img->r.min);
	unlockdisplay(display);
}

void
evtresize(int new)
{
	if(new && getwindow(display, Refnone)<0)
		sysfatal("getwindow: %r");
	redraw();
}

void
evtmouse(Mouse m)
{
	Point o;

	if(m.buttons == 1){
		for(;;){
			o = mctl->xy;
			if(readmouse(mctl) < 0)
				break;
			if((mctl->buttons & 1) == 0)
				break;
			pan(subpt(mctl->xy, o));
		}
	}
}

void
evtkey(Rune k)
{
	switch(k){
	case 'q':
	case Kdel:
		threadexitsall(nil);
		break;
	}
}

void
usage(void)
{
	fprint(2, "usage: %s [<filename>]\n", argv0);
	threadexitsall("usage");
}

void
threadmain(int argc, char **argv)
{
	Mouse m;
	Rune k;
	Alt alts[] = {
		{ nil, &m,  CHANRCV },
		{ nil, nil, CHANRCV },
		{ nil, &k,  CHANRCV },
		{ nil, nil, CHANEND },
	}; 

	ARGBEGIN{}ARGEND;
	if(argc > 1)
		usage();
	if(initdraw(nil, nil, argv0) < 0)
		sysfatal("initdraw: %r");
	unlockdisplay(display);
	display->locking = 1;
	if((mctl=initmouse(nil, screen)) == nil)
		sysfatal("initmouse: %r");
	if((kctl=initkeyboard(nil)) == nil)
		sysfatal("initkeyboard: %r");
	alts[Emouse].c = mctl->c;
	alts[Eresize].c = mctl->resizec;
	alts[Ekeyboard].c = kctl->c;
	initbg();
	img = load(*argv);
	pos = subpt(ZP, img->r.min);
	evtresize(0);
	for(;;){
		switch(alt(alts)){
		case Emouse:
			evtmouse(m);
			break;
		case Eresize:
			evtresize(1);
			break;
		case Ekeyboard:
			evtkey(k);
			break;
		}
	}
}

