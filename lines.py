from math import sqrt

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

def test():
    spool_dist = 400
    prev_llen = prev_rlen = 0.0
    step = 0.1
    for x in frange(200, 100, step):
        (llen, rlen) = to_lengths(spool_dist, x, 200)
        #print(llen, rlen)
        if prev_llen == 0 and prev_rlen == 0:
            prev_llen = llen
            prev_rlen = rlen
            print("Step Distance: %f"%step)
            print("Start Length Left: %f"%llen)
            print("Start Length Right: %f"%rlen)

        while abs(llen - prev_llen) > step or abs(rlen - prev_rlen) > step:
            #print(abs(llen - prev_llen), abs(rlen - prev_rlen))
            #print(x,50,llen,rlen, prev_llen, prev_rlen)
            #print('inner')
            ch1 = ch2 = '.'
            if llen - prev_llen > step:
                ch1 = '+' 
                prev_llen += step
            elif prev_llen - llen > step:
                ch1 = '-' 
                prev_llen -= step

            if rlen - prev_rlen > step:
                ch2 = '+' 
                prev_rlen += step
            elif prev_rlen - rlen > step:
                ch2 = '-' 
                prev_rlen -= step

            print(ch1 + ch2 + '+')

if __name__ == "__main__":
    test()
