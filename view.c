#include <u.h>
#include <libc.h>
#include <draw.h>
#include <thread.h>
#include <mouse.h>
#include <keyboard.h>
#include <plumb.h>
#include <stb.h>
#include "a.h"

enum
{
	Emouse,
	Eresize,
	Ekeyboard,
	Eplumb,
};

int mainstacksize=16384;

Mousectl *mctl;
Keyboardctl *kctl;
Image *bg;
Image *orig;
Image *img;
Point pos;
int zoomlevel;

const char* zoomlevels[] = {
	"25%", "33%", "50%", "75%",
	"100%",
	"150%", "200%", "300%", "400%",
};

enum
{
	Defaultzoomlevel = 4,
	Nzoomlevels = nelem(zoomlevels),
};

enum
{
	Mzcat,
	Mzoomin,
	Mzoomout,
	Morigsize,
	Mrcat,
	Mrotcw,
	Mrotccw,
	Mflip,
	Mflop,
	Mmcat,
	Mpipeto,
};
char *menu2str[] =
{
	"_Zoom",
	"in",
	"out",
	"reinit.",
	"_Rotate",
	"cw",
	"ccw",
	"flip",
	"flop",
	"_Misc",
	"pipe...",
	nil,
};
Menu menu2 = { menu2str };

enum
{
	Mopen,
	Mexit,
};
char *menu3str[] =
{
	"open",
	"exit",
	nil,
};
Menu menu3 = { menu3str };

void redraw(void);

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
loadfromfile(char *filename)
{
	Image *i;

	i = load(filename);
	if(i == nil)
		return -1;
	freeimage(orig);
	orig = nil;
	freeimage(img);
	img = i;
	pos = subpt(ZP, img->r.min);
	redraw();
	return 0;
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
	if(img != nil)
		draw(screen, rectaddpt(img->r, addpt(pos, screen->r.min)), img, nil, img->r.min);
	flushimage(display, 1);
	unlockdisplay(display);
}

void
pan(Point Δ)
{
	Rectangle r, ir;

	if(img == nil)
		return;
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
plumbproc(void *v)
{
	Plumbmsg *m;
	Channel *c;
	int fd;

	c = v;
	threadsetname("plumbproc");
	fd = plumbopen("image", OREAD);
	if(fd < 0)
		sysfatal("plumbopen: %r");
	for(;;){
		m = plumbrecv(fd);
		if(m == nil)
			sysfatal("plumbrecv: %r");
		sendp(c, m);
	}
}

/* FIXME: the whole temp file logic is bad and we risk
   never deleting the file */
void
evtplumb(Plumbmsg *m)
{
	int rm;
	char *a, *f;

	rm = 0;
	a = plumblookup(m->attr, "action");
	if(a != nil && strncmp(a, "showdata", 8) == 0){
		f = smprint("/tmp/view.%ld.%d", time(nil), getpid());
		if(writefile(f, m->data, m->ndata) < 0){
			fprint(2, "cannot write showdata: %r\n");
			goto Err;
		}
		rm = 1;
	}else{
		f = strdup(m->data);
	}
	if(loadfromfile(f) < 0)
		fprint(2, "cannot load plumbed image: %r"); /* XXX: visual report */
Err:
	plumbfree(m);
	if(rm)
		remove(f);
	free(f);
}


void
evtresize(int new)
{
	if(new && getwindow(display, Refnone)<0)
		sysfatal("getwindow: %r");
	redraw();
}

Image*
rotate(int op)
{
	const char *cmd;

	switch(op){
	case Mflop:
		cmd = "rotate -l";
		break;
	case Mflip:
		cmd = "rotate -u";
		break;
	case Mrotccw:
		cmd = "rotate -r 270";
		break;
	case Mrotcw:
		cmd = "rotate -r 90";
		break;
	default:
		werrstr("invalid rotate op");
		return nil;
	}
	return ipipeto(img, cmd);
}

void
zoom(int zop)
{
	Image *i;
	char cmd[255];

	switch(zop){
	case Mzoomin:
		if(zoomlevel + 1 >= Nzoomlevels)
			return;
		++zoomlevel;
		break;
	case Mzoomout:
		if(zoomlevel == 0)
			return;
		--zoomlevel;
		break;
	case Morigsize:
		if(zoomlevel == Defaultzoomlevel)
			return;
		zoomlevel = Defaultzoomlevel;
		img = orig;
		goto Redraw;
	}
	if(snprint(cmd, sizeof cmd, "resize -x %s", zoomlevels[zoomlevel]) <= 0){
		fprint(2, "error creating zoom command: %r\n"); /* XXX */
		return;
	}
	i = ipipeto(orig == nil ? img : orig, cmd);
	if(i == nil){
		fprint(2, "unable to zoom image: %r\n"); /* XXX */
		return;
	}
	if(orig == nil)
		orig = img;
	else
		freeimage(img);
	img = i;
Redraw:
	pos = subpt(ZP, img->r.min);
	redraw();
}

void
menu2hit(void)
{
	Image *i;
	int n;
	char buf[255];

	n = sepmenuhit(2, mctl, &menu2, nil);
	if(img == nil)
		return;
	switch(n){
	case Mzoomin:
	case Mzoomout:
	case Morigsize:
		zoom(n);
		return;
	case Mflip:
	case Mflop:
	case Mrotccw:
	case Mrotcw:
		i = rotate(n);
		if(i == nil){
			fprint(2, "unable to rotate image: %r\n");
			return;
		}
		break;
	case Mpipeto:
		if(enter("command:", buf, sizeof buf, mctl, kctl, nil) <= 0)
			return;
		i = ipipeto(img, buf);
		if(i == nil){
			fprint(2, "unable to pipe image: %r\n");
			return;
		}
		break;
	default:
		return;
	}
	freeimage(img);
	img = i;
	pos = subpt(ZP, img->r.min);
	redraw();
}

void
menu3hit(void)
{
	char buf[255];
	int n;

	n = menuhit(3, mctl, &menu3, nil);
	switch(n){
	case Mopen:
		if(enter("open:", buf, sizeof buf, mctl, kctl, nil) > 0){
			if(loadfromfile(buf) < 0)
				fprint(2, "cannot open file '%s': %r\n", buf);
		}
		break;
	case Mexit:
		threadexitsall(nil);
		break;
	}
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
	}else if(m.buttons == 2){
		menu2hit();
	}else if(m.buttons == 4){
		menu3hit();
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
	Plumbmsg *pm;
	Channel *plumbc;
	Alt alts[] = {
		{ nil, &m,  CHANRCV },
		{ nil, nil, CHANRCV },
		{ nil, &k,  CHANRCV },
		{ nil, &pm,	CHANRCV },
		{ nil, nil, CHANEND },
	}; 

	img = nil;
	zoomlevel = Defaultzoomlevel;
	ARGBEGIN{
	default:
		usage();
		break;
	}ARGEND;
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
	if((plumbc = chancreate(sizeof(Plumbmsg*), 0)) == nil)
		sysfatal("chancreate: %r");
	alts[Emouse].c = mctl->c;
	alts[Eresize].c = mctl->resizec;
	alts[Ekeyboard].c = kctl->c;
	alts[Eplumb].c = plumbc;
	initbg();
	proccreate(plumbproc, plumbc, 8192);
	if(*argv != nil){
		img = load(*argv);
		pos = subpt(ZP, img->r.min);
	}
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
		case Eplumb:
			evtplumb(pm);
			break;
		}
	}
}

