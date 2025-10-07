#include <stdlib.h>
#include <pthread.h>

typedef struct q_edge q_edge;
typedef struct point point;
typedef struct pair pair;
typedef struct lcell lcell;

lcell* head;

extern pthread_mutex_t child_active_mtx;
extern pthread_cond_t child_continue;
extern int terminate_child;

void pause() {
    pthread_cond_wait(&child_continue, &child_active_mtx);
    if (terminate_child) {
        pthread_mutex_unlock(&child_active_mtx);
        pthread_exit(NULL);
    }
}

struct pair {
    q_edge* first;
    q_edge* second;
};

struct lcell {
    point* p1;
    point* p2;
    lcell* nxt;
};

struct point {
    float x;
    float y;
};

struct q_edge {
    point* org;
    q_edge* nxt;
    q_edge* rot;
};

int u_exists(point* p1, point* p2) {
    lcell* curr = head;
    while (curr != NULL) {
        if ((curr->p1 == p1 && curr->p2 == p2) || (curr->p1 == p2 && curr->p2 == p1)) {
            return 1;
        }
        curr = curr->nxt;
    }
    return 0;
}

void u_insert(point* p1, point* p2) {
    lcell* new = malloc(sizeof(lcell));
    new->p1 = p1;
    new->p2 = p2;
    new->nxt = head;
    head = new;
}

void u_delete(point* p1, point* p2) {
    lcell* curr = head;
    lcell* prev = NULL;
    while(curr != NULL) {
        if ((curr->p1 == p1 && curr->p2 == p2) || (curr->p1 == p2 && curr->p2 == p1)) {
            if (curr == head) {
                head = curr->nxt;
            } else {
                prev->nxt = curr->nxt;
            }
            free(curr);
            break;
        }
    prev = curr;
    curr = curr->nxt;
    }
}

q_edge* rot(q_edge* edge) {
    return edge->rot;
}

q_edge* sym(q_edge* edge) {
    return edge->rot->rot;
}

q_edge* onxt(q_edge* edge) {
    return edge->nxt;
}

q_edge* oprev(q_edge* edge) {
    return edge->rot->nxt->rot;
}

q_edge* lnxt(q_edge* edge) {
    return edge->rot->rot->rot->nxt->rot;
}

q_edge* make_edge(point* org, point* dest) {
    u_insert(org, dest);
    q_edge* e1 = malloc(sizeof(q_edge));
    q_edge* e2 = malloc(sizeof(q_edge));
    q_edge* e3 = malloc(sizeof(q_edge));
    q_edge* e4 = malloc(sizeof(q_edge));

    e1->org = org;
    e2->org = dest;

    //dual edges should never be accessed
    e3->org = e4->org = NULL;

    e1->nxt = e1;
    e2->nxt = e2;
    e3->nxt = e4;
    e4->nxt = e3;

    e1->rot = e3;
    e2->rot = e4;
    e3->rot = e2;
    e4->rot = e1;

    pause();

    return e1;
}

void swap_p(q_edge* a, q_edge* b) {
  q_edge* anxt = a->nxt;
  a->nxt = b->nxt;
  b->nxt = anxt;
}

void splice(q_edge* a, q_edge* b) {
  swap_p(a->nxt->rot, b->nxt->rot);
  swap_p(a, b);
}

q_edge* connect(q_edge* a, q_edge* b) {
    q_edge* e = make_edge(sym(a)->org, b->org);
    splice(e, lnxt(a));
    splice(sym(e), b);
    return e;
}

void delete(q_edge* e) {
    u_delete(e->org, sym(e)->org);
    splice(e, oprev(e));
    splice(sym(e), oprev(sym(e)));
    free(e->rot->rot->rot);
    free(e->rot->rot);
    free(e->rot);
    free(e);
    pause();
}

int in_circle(point* a, point* b, point* c, point* d) {
    typedef float f;

    f g = d->x*d->x + d->y*d->y;
    f m11 = a->x - d->x; f m12 = a->y - d->y; f m13 = a->x*a->x + a->y*a->y - g;
    f m21 = b->x - d->x; f m22 = b->y - d->y; f m23 = b->x*b->x + b->y*b->y - g;
    f m31 = c->x - d->x; f m32 = c->y - d->y; f m33 = c->x*c->x + c->y*c->y - g;
    return m11*m22*m33 + m12*m31*m23 + m21*m13*m32 - m13*m22*m31 - m21*m33*m12 - m32*m11*m23 < 0;
}

int ccw(point* a, point* b, point* c) {
    return (b->x - c->x)*(a->y - c->y) - (a->x - c->x)*(b->y - c->y) > 0;
}

int right_of(point* x, q_edge* e) {
    return ccw(x, sym(e)->org, e->org);
}

int left_of(point* x, q_edge* e) {
    return ccw(x, e->org, sym(e)->org);
}

int valid(q_edge* basel, q_edge* e) {
    return ccw(sym(e)->org, sym(basel)->org, basel->org);
}

pair triangulation(point* points, size_t count) {
    if (count == 2) {
        q_edge* a = make_edge(&points[0], &points[1]);
        return (pair) {a, sym(a)};
    }
    if (count == 3) {
        q_edge* a = make_edge(&points[0], &points[1]);
        q_edge* b = make_edge(&points[1], &points[2]);
        splice(sym(a), b);
        if (ccw(&points[0], &points[1], &points[2])) {
            q_edge* c = connect(b, a);
            return (pair) {a, sym(b)};
        }
        if (ccw(&points[0], &points[2], &points[1])) {
            q_edge* c = connect(b, a);
            return (pair) {sym(c), c};
        }
        return (pair) {a, sym(b)};
    }
    pair left = triangulation(points, count/2);
    pair right = triangulation(points + count/2, count - count/2);
    q_edge* ldo = left.first;
    q_edge* ldi = left.second;
    q_edge* rdi = right.first;
    q_edge* rdo = right.second;
    while (1) {
        if (left_of(rdi->org, ldi)) {
            ldi = lnxt(ldi);
        }
        else if (right_of(ldi->org, rdi)) {
            rdi = onxt(sym(rdi));
        } else {
            break;
        }
    }
    q_edge* basel = connect(sym(rdi), ldi);
    if (ldi->org == ldo->org) {
        ldo = sym(basel);
    }
    if (rdi->org == rdo->org) {
        rdo = basel;
    }
    while(1) {
        q_edge* lcand = onxt(sym(basel));
        if (valid(basel, lcand)) {
            while (in_circle(sym(basel)->org, basel->org, sym(lcand)->org, sym(onxt(lcand))->org)) {
                q_edge* t = onxt(lcand);
                delete(lcand);
                lcand = t;
            }
        }
        q_edge* rcand = oprev(basel);
        if (valid(basel, rcand)) {
            while (in_circle(sym(basel)->org, basel->org, sym(rcand)->org, sym(oprev(rcand))->org)) {
                q_edge* t = oprev(rcand);
                delete(rcand);
                rcand = t;
            }
        }
        if (!valid(basel, lcand) && !valid(basel, rcand)) {
            break;
        }
        if (!valid(basel, lcand) || (valid(basel, rcand) && in_circle(sym(lcand)->org, lcand->org, rcand->org, sym(rcand)->org))) {
            basel = connect(rcand, sym(basel));
        } else {
            basel = connect(sym(basel), sym(lcand));
        }
    }
    return (pair) {.first = ldo, .second = rdo};
}

void point_sort(point* points, size_t size, int (*cmp)(point a, point b)) {
    if (size <= 1) {
        return;
    }
    point* left = points;
    point* right = points + size/2;
    point_sort(left, size/2, cmp);
    point_sort(right, size - size/2, cmp);
    point* sorted = malloc(sizeof(point)*size);
    size_t j = 0, k = size/2;
    for (size_t i = 0; i < size; i++) {
        if (j >= size/2) {
            sorted[i] = points[k++];
            continue;
        }
        if (k >= size) {
            sorted[i] = points[j++];
            continue;
        }
        if (cmp(points[j], points[k]) <= 0) {
            sorted[i] = points[j++];
        } else {
            sorted[i] = points[k++];
        }
    }
    for (size_t i = 0; i < size; i++) {
        points[i] = sorted[i];
    }
    free(sorted);
}

int byx (point a, point b) {
    if (a.x > b.x) {
        return 1;
    }
    if (a. x < b.x) {
        return -1;
    }
    return 0;
}

int byy (point a, point b) {
    if (a.y > b.y) {
        return 1;
    }
    if (a. y < b.y) {
        return -1;
    }
    return 0;
}

void delaunay(point* points, size_t count) {
    point_sort(points, count, byy);
    point_sort(points, count, byx);
    pair res = triangulation(points, count);
    q_edge* e = res.first;
}
