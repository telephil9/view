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
Image *img;
Point pos;

enum
{
	Mhflip,
	Mvflip,
	Mrotleft,
	Mrotright,
};
char *menu2str[] =
{
	"flip horiz.",
	"flip vert.",
	"rotate left",
	"rotate right",
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
	static char *oparg[] = {
		[Mhflip]	= "-l",
		[Mvflip]	= "-u",
		[Mrotleft]	= "-r 270",
		[Mrotright]	= "-r 90",
	};
	Image *i;
	int ifd[2], ofd[2];
	char *argv[3] = { "rotate", oparg[op], nil };

	if(pipe(ifd) < 0)
		return nil;
	if(pipe(ofd) < 0){
		close(ifd[0]);
		close(ifd[1]);
		return nil;
	}
	switch(rfork(RFFDG|RFPROC|RFNOWAIT)){
	case -1:
		close(ifd[0]);
		close(ifd[1]);
		close(ofd[0]);
		close(ofd[1]);
		return nil;
	case 0:
		dup(ifd[1], 0);
		dup(ofd[1], 1);
		close(ifd[1]);
		close(ifd[0]);
		close(ofd[1]);
		close(ofd[0]);
		exec("/bin/rotate", argv);
		_exits("exec");
	}
	if(writeimage(ifd[0], img, 1) < 0){
		i = nil;
		goto End;
	}
	i = readimage(display, ofd[0], 1);
End:
	close(ifd[0]);
	close(ifd[1]);
	close(ofd[0]);
	close(ofd[1]);
	return i;
}

void
menu2hit(void)
{
	Image *i;
	int n;

	n = menuhit(2, mctl, &menu2, nil);
	if(n >= 0){
		i = rotate(n);
		freeimage(img);
		img = i;
		pos = subpt(ZP, img->r.min);
		redraw();
	}
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

