/*
 * classes.h
 *
 *  Created on: Oct 7, 2017
 *      Author: xoiss
 */

#ifndef CLASSES_H_
#define CLASSES_H_

#include <string.h>
#include <sys/types.h>

namespace xgds {

class Name {
private:
    char *mchars;
public:
    inline Name(u_int alen, const char *achars) {
        mchars = new char[alen];
        memcpy(mchars, achars, alen);
    }
    inline ~Name() throw() { delete[] mchars; }
};

class Layer {
private:
    int mid;
public:
    inline Layer(int aid) throw() : mid(aid) {}
};

class Point {
public:
    int x, y;
public:
    inline Point() throw() : x(), y() {}
    inline Point(int ax, int ay) throw() : x(ax), y(ay) {}
    inline void set(int ax, int ay) throw() { x = ax; y = ay; }
    inline unsigned operator!=(const Point &apoint) const throw() {
        return x != apoint.x || y != apoint.y;
    }
};

class Chain {
private:
    Point *mpoints;
    u_int mcursor;
public:
    u_int len;
public:
    inline Chain(u_int alen) : len(alen), mcursor() {
        mpoints = new Point[alen];
    }
    inline ~Chain() throw() { delete[] mpoints; }
    inline void load(int ax, int ay) throw() { mpoints[mcursor++].set(ax, ay); }
    inline Point &operator[](u_int i) const throw() { return mpoints[i]; }
};

}

#endif
