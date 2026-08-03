#!/usr/bin/env python3
"""Turn a grab of the Amiga track-preview screen into a reusable backdrop.

	python3 tools/clean_trackpreview.py [Bitmap/trackpreview_raw.png Bitmap/trackpreview.png]

The Amiga decrunched a full-screen picture (preview.crunched, set.and.preview.road at
"Reference only/StuntCarRacer.s":10137) and drew the 3D road into a window cut out of it.
A screen grab therefore has one particular course's road baked into the arena floor, and
its name in the title panel, neither of which can be reused.  This removes both:

  1. crop the 320x256 PAL frame down to the 320x200 screen proper;
  2. mask the arena floor colours and take the two big connected components - the floor
     OUTSIDE the road loop and the floor INSIDE it.  A hole-fill will not find the road,
     because it is not enclosed: it runs back up to the grandstands;
  3. the arena is then everything between the left and right extent of that mask on each
     row, which spans the road ring and the ground either side of it;
  4. repaint every pixel there that is not already floor, sampling only ORIGINAL floor
     pixels from a widening neighbourhood.  Peeling inwards and copying a neighbour
     instead would propagate whatever it copied first and streak across the wide areas
     (the foreground wall is ~25 pixels tall); sampling the real dither gets both its
     randomness and, because the search stays local, its light-to-dark vertical gradient.
     This also scrubs the stray speckles a lossy rip leaves in the floor;
  5. blank the title panel's interior and the prompt line, which the port draws at
     runtime (DrawAmigaPreviewScreen in StuntCarRacer.cpp).

Pure standard library - no Pillow.
"""
import random, struct, sys, zlib

# --- geometry, in the 320x256 grab's own coordinates ---------------------------------
CROP_Y   = 28					# the 320x200 screen starts this far down the PAL frame
WX0, WX1 = 12, 307				# the picture window, inside the checkerboard border
WY0, WY1 = 38, 171
PANEL    = (80, 196, 240, 212)	# title panel interior (x0, y0, x1, y1), exclusive ends
PROMPT   = (0, 218, 320, 228)	# the "Hit fire to continue" line

ARENA    = {4, 5, 8}			# the arena floor's dither colours, as palette indices
BACKDROP = 0					# the teal surround
PANEL_BG = 14					# the panel's black interior

SAMPLE_MIN = 24
NEIGH8 = ((1,0),(-1,0),(0,1),(0,-1),(1,1),(1,-1),(-1,1),(-1,-1))


def read_png(path):
	"""Minimal reader for the 8-bit palettised, non-interlaced PNGs this deals with."""
	data = open(path, 'rb').read()
	pos, idat, pal, w, h = 8, b'', None, 0, 0
	while pos < len(data):
		length = struct.unpack('>I', data[pos:pos+4])[0]
		kind   = data[pos+4:pos+8]
		body   = data[pos+8:pos+8+length]
		if kind == b'IHDR':
			w, h, depth, colour = struct.unpack('>IIBB', body[:10])
			if depth != 8 or colour != 3:
				raise SystemExit('%s: expected an 8-bit palettised PNG' % path)
		elif kind == b'PLTE':
			pal = body
		elif kind == b'IDAT':
			idat += body
		pos += length + 12

	raw = zlib.decompress(idat)
	rows, prev, i = [], bytearray(w), 0
	for _ in range(h):
		filt = raw[i]; i += 1
		line = bytearray(raw[i:i+w]); i += w
		if filt == 1:
			for x in range(1, w):
				line[x] = (line[x] + line[x-1]) & 255
		elif filt == 2:
			for x in range(w):
				line[x] = (line[x] + prev[x]) & 255
		elif filt == 3:
			for x in range(w):
				line[x] = (line[x] + ((line[x-1] if x else 0) + prev[x]) // 2) & 255
		elif filt == 4:
			for x in range(w):
				a = line[x-1] if x else 0
				b = prev[x]
				c = prev[x-1] if x else 0
				p = a + b - c
				pa, pb, pc = abs(p-a), abs(p-b), abs(p-c)
				line[x] = (line[x] + (a if (pa <= pb and pa <= pc) else (b if pb <= pc else c))) & 255
		rows.append(line)
		prev = line
	return w, h, pal, rows


def write_png(path, rows, pal):
	"""Write out as plain 24-bit RGB, which is what the game's loader wants."""
	h, w = len(rows), len(rows[0])
	raw = bytearray()
	for row in rows:
		raw.append(0)
		for x in range(w):
			i = row[x]
			raw += bytes((pal[3*i], pal[3*i+1], pal[3*i+2]))

	def chunk(kind, body):
		c = kind + body
		return struct.pack('>I', len(body)) + c + struct.pack('>I', zlib.crc32(c) & 0xffffffff)

	png  = b'\x89PNG\r\n\x1a\n'
	png += chunk(b'IHDR', struct.pack('>IIBBBBB', w, h, 8, 2, 0, 0, 0))
	png += chunk(b'IDAT', zlib.compress(bytes(raw), 9))
	png += chunk(b'IEND', b'')
	open(path, 'wb').write(png)


def main(src, dst):
	random.seed(1989)
	w, h, pal, grid = read_png(src)
	print('%s: %dx%d, %d colours' % (src, w, h, len(pal) // 3))

	W, H = WX1 - WX0 + 1, WY1 - WY0 + 1
	get = lambda x, y: grid[WY0 + y][WX0 + x]

	floor = [[get(x, y) in ARENA for x in range(W)] for y in range(H)]

	# --- the floor's connected components ------------------------------------------
	label, comps = [[-1] * W for _ in range(H)], []
	for sy in range(H):
		for sx in range(W):
			if not floor[sy][sx] or label[sy][sx] >= 0:
				continue
			rid = len(comps)
			label[sy][sx] = rid
			stack, pix = [(sx, sy)], []
			while stack:
				x, y = stack.pop()
				pix.append((x, y))
				for dx, dy in NEIGH8:
					nx, ny = x + dx, y + dy
					if 0 <= nx < W and 0 <= ny < H and floor[ny][nx] and label[ny][nx] < 0:
						label[ny][nx] = rid
						stack.append((nx, ny))
			comps.append(pix)
	comps.sort(key=len, reverse=True)
	print('floor components:', [len(c) for c in comps[:4]])

	# --- the arena region, as a per-row span ---------------------------------------
	span = {}
	for pix in comps[:2]:
		for x, y in pix:
			lo, hi = span.get(y, (x, x))
			span[y] = (min(lo, x), max(hi, x))

	todo = [(x, y) for y, (lo, hi) in span.items()
					for x in range(lo, hi + 1) if not floor[y][x]]
	print('pixels to repaint:', len(todo))

	# --- repaint from the surviving dither -----------------------------------------
	for (x, y) in sorted(todo):
		src_px, r = [], 1
		while len(src_px) < SAMPLE_MIN and r < max(W, H):
			src_px = [get(nx, ny)
					  for ny in range(max(0, y-r), min(H, y+r+1))
					  for nx in range(max(0, x-r), min(W, x+r+1))
					  if floor[ny][nx]]
			r += 2
		grid[WY0 + y][WX0 + x] = random.choice(src_px)

	# --- blank the text the port draws itself --------------------------------------
	for (x0, y0, x1, y1), fill in ((PANEL, PANEL_BG), (PROMPT, BACKDROP)):
		for y in range(y0, y1):
			for x in range(x0, x1):
				grid[y][x] = fill

	write_png(dst, [grid[CROP_Y + y] for y in range(200)], pal)
	print('wrote %s (320x200)' % dst)


if __name__ == '__main__':
	args = sys.argv[1:]
	main(args[0] if args else 'Bitmap/trackpreview_raw.png',
		 args[1] if len(args) > 1 else 'Bitmap/trackpreview.png')
