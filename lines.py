from math import sqrt, copysign, pi, cos, sin
from itertools import chain

def to_coords(spool_dist, llen, rlen):
    if llen + rlen < spool_dist:
        print("You snapped a string!")
    xf = (spool_dist*spool_dist + llen*llen - rlen*rlen) / (2.0 * spool_dist)
    return (xf, sqrt(llen*llen - xf*xf))

def to_lengths(spool_dist, x, y):
    llen = sqrt(x*x + y*y)
    rlen = sqrt((spool_dist - x)**2 + y*y)
    return (llen, rlen)

def frange(end, start = 0.0, step = 1.0):
    x = start
    while x < end:
        yield x
        x += step

def alpha_blend(x1, y1, x2, y2, step):
    #print(x1, y1, x2, y2)
    dx = x2 - x1 
    dy = y2 - y1 
    x, y = x1, y1
    xstep = ystep = step
    if abs(dx) > abs(dy):
        xstep = copysign(xstep, dx)
        ystep = copysign(step * (dy / dx), dy)
    else:
        xstep = copysign(step * (dx / dy), dx)
        ystep = copysign(ystep, dy)
    # Sometimes the floating point delta is so
    # close so zero that adding it to x makes no
    # difference. The result is an infinite loop.
    # Let's not do that k.
    if x == x + xstep: x = x2
    if y == y + ystep: y = y2
    #print("alpha_blend", dx, dy, xstep, ystep)
    while abs(x - x2) > abs(xstep) or abs(y - y2) > abs(ystep):
        yield (x,y)
        x += xstep
        y += ystep


def draw_line(x1, y1, x2, y2, spool_dist = 400):
    '''Generator that yeilds "+.-" triple strings that draw a line between
    two coordinates'''
    prev_llen = prev_rlen = 0.0
    len_step = 0.1
    cart_step = 1.0 # cartesian
    for x,y in alpha_blend(x1, y1, x2, y2, cart_step):
        (llen, rlen) = to_lengths(spool_dist, x, y)
        #print("draw_line: (%f, %f) -> (%f, %f)"%(x,y,llen, rlen))
        if prev_llen == 0 and prev_rlen == 0:
            prev_llen = llen
            prev_rlen = rlen

        while abs(llen - prev_llen) > len_step or abs(rlen - prev_rlen) > len_step:
            #print(abs(llen - prev_llen), abs(rlen - prev_rlen))
            #print(x,50,llen,rlen, prev_llen, prev_rlen)
            #print('inner')
            ch1 = ch2 = '.'
            if llen - prev_llen > len_step:
                ch1 = '+' 
                prev_llen += len_step
            elif prev_llen - llen > len_step:
                ch1 = '-' 
                prev_llen -= len_step

            if rlen - prev_rlen > len_step:
                ch2 = '+' 
                prev_rlen += len_step
            elif prev_rlen - rlen > len_step:
                ch2 = '-' 
                prev_rlen -= len_step

            yield ch1 + ch2 + '+'

def circle(x,y,r):
    first = True
    xp = yp = 0
    segments = 50.0
    # segments + 2 because we need to include both
    # the starting and ending segments
    for t in frange(segments + 2, start=1.0):
        xn = x + r * cos(2*pi * t/segments)
        yn = y + r * sin(2*pi * t/segments)
        if first:
            first = False
            (llen, rlen) = to_lengths(400.0, xn, yn)
            print("Step Distance: %f"%0.1)
            print("Start Length Left: %f"%llen)
            print("Start Length Right: %f"%rlen)
        else:
            #print('circle: %f, %f -> %f, %f'%(xp, yp, xn, yn))
            for triple in draw_line(xp, yp, xn, yn):
                yield triple
        xp, yp = xn, yn

def test_square():
    spool_dist = 400.0
    len_step = 0.1
    (llen, rlen) = to_lengths(spool_dist, 100, 100)
    #print("(%f, %f) -> (%f, %f)"%(x,y,llen, rlen))
    print("Step Distance: %f"%len_step)
    print("Start Length Left: %f"%llen)
    print("Start Length Right: %f"%rlen)
    lines = chain(draw_line(100, 100, 100, 200),
                  draw_line(100, 200, 200, 200),
                  draw_line(200, 200, 200, 100),
                  draw_line(200, 100, 100, 100),
                  draw_line(100, 100, 200, 200))
    for triple in lines:
        print(triple)



if __name__ == "__main__":
    for t in circle(200, 200, 100):
        print(t)
